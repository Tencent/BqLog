/* Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#pragma once

#include "test_base.h"
#include "bq_common/bq_common.h"
#include "bq_log/log/decoder/appender_decoder_manager.h"
#include "bq_log/log/appender/appender_file_compressed.h"
#include "legacy/legacy_decoders.h"

namespace bq {
    namespace test {

        // The runtime writes one format version and reads one format version. Old files are the
        // standalone tool's job, and "the runtime refuses them, with a message that says where to
        // go" is a property worth pinning: it is the difference between a user upgrading and a user
        // staring at a decode failure.
        //
        // The samples under test/data/legacy are real files written by Release_2.3.1 (format 9) and
        // Release_2.7.3 (format 10), with the decoded text their own decoders produced. Two things
        // are checked here: the runtime declines them, and the tool's segment layers - registered in
        // this binary exactly as the tool registers them at startup, and nowhere else in the runtime -
        // reproduce their golden text byte for byte.
        class test_legacy_format : public test_base {
        private:
            // The samples are source-tree data, not generated output: <repo>/test/data/legacy, as seen
            // from this file, or the directory the run was pointed at.
            static bq::string legacy_data_dir()
            {
                return bq::test::test_data_dir(__FILE__, "legacy");
            }

            // What the standalone decoder does at startup, and what the runtime deliberately does
            // not do. Compiling the two segment layers into the test (see test/cpp/CMakeLists.txt) is
            // what lets the golden comparison below run at all.
            static void register_legacy_decoders()
            {
                bq::appender_decoder_manager::instance().register_decoder_factory(9, []() -> bq::unique_ptr<bq::appender_decoder_base> {
                    return bq::make_unique<bq::appender_decoder_legacy_v9>();
                });
                bq::appender_decoder_manager::instance().register_decoder_factory(10, []() -> bq::unique_ptr<bq::appender_decoder_base> {
                    return bq::make_unique<bq::appender_decoder_legacy_v10>();
                });
            }

        public:
            void case_runtime_supports_one_version(test_result& result)
            {
                const bq::array<uint32_t> versions = bq::appender_decoder_manager::instance().get_supported_format_versions();
                result.add_result(versions.size() == 1, "runtime reports exactly one supported format version (got %d)",
                    static_cast<int32_t>(versions.size()));
                if (versions.size() == 1) {
                    result.add_result(versions[0] == bq::appender_file_compressed::format_version,
                        "the supported version is the one this build writes (%" PRIu32 ")", versions[0]);
                }
            }

            void case_runtime_declines_old_versions(test_result& result)
            {
                struct sample_case {
                    const char* path;
                    const char* label;
                };
                const sample_case samples[] = {
                    { "/v9/plain.logcompr", "v9 plaintext" },
                    { "/v10/enc.logcompr", "v10 encrypted" },
                };
                for (const auto& sample : samples) {
                    const bq::string path = legacy_data_dir() + sample.path;
                    result.add_result(bq::file_manager::is_file(path), "%s sample exists", sample.label);
                    if (!bq::file_manager::is_file(path)) {
                        continue;
                    }
                    uint32_t handle = 0;
                    const auto decode_result = bq::appender_decoder_manager::instance().create_decoder(path, bq::string(), handle);
                    result.add_result(decode_result == bq::appender_decode_result::failed_decode_error,
                        "%s is declined by the runtime, not misread (result %d)", sample.label,
                        static_cast<int32_t>(decode_result));
                }
            }

            // Byte for byte, against the text the release that wrote the sample produced. A format
            // this old is a promise to files that already exist, and the only way to keep it honest
            // is to decode the files.
            void case_samples_decode_byte_for_byte(test_result& result)
            {
                struct sample_case {
                    const char* dir;
                    const char* name;
                    uint32_t version;
                };
                const sample_case samples[] = {
                    { "/v9", "plain", 9 },
                    { "/v9", "enc", 9 },
                    { "/v9", "enc_recover_buffer", 9 },
                    { "/v9", "enc_recover_appender", 9 },
                    { "/v10", "plain", 10 },
                    { "/v10", "enc", 10 },
                    { "/v10", "enc_260", 10 },
                    { "/v10", "enc_recover_buffer", 10 },
                    { "/v10", "enc_recover_appender", 10 },
                };
                const bq::string base_dir = legacy_data_dir();
                // Every encrypted sample was wrapped with this key. It is the pair the unit tests
                // already carry, and it protects nothing: the samples are public by construction.
                const bq::string priv_key = bq::file_manager::instance().read_all_text(base_dir + "/legacy_key.pem");
                result.add_result(!priv_key.is_empty(), "the sample private key is readable");
                if (priv_key.is_empty()) {
                    return;
                }

                for (const auto& sample : samples) {
                    const bq::string sample_path = base_dir + sample.dir + "/" + sample.name + ".logcompr";
                    const bq::string expected_path = base_dir + sample.dir + "/" + sample.name + ".expected.txt";
                    const bq::string expected = bq::file_manager::instance().read_all_text(expected_path);
                    if (expected.is_empty()) {
                        result.add_result(false, "v%" PRIu32 " %s: the golden text is readable", sample.version, sample.name);
                        continue;
                    }
                    // The same loop the tool runs: one entry per call, one newline after each.
                    bq::string decoded;
                    bq::tools::log_decoder decoder(sample_path, priv_key);
                    while (true) {
                        const bq::appender_decode_result decode_result = decoder.decode();
                        if (decode_result == bq::appender_decode_result::eof) {
                            break;
                        }
                        decoded += decoder.get_last_decoded_log_entry();
                        decoded.push_back('\n');
                        if (decode_result != bq::appender_decode_result::success) {
                            break;
                        }
                    }
                    result.add_result(decoded == expected,
                        "v%" PRIu32 " %s decodes to its golden text (%" PRIu64 " bytes decoded, %" PRIu64 " expected)",
                        sample.version, sample.name, static_cast<uint64_t>(decoded.size()),
                        static_cast<uint64_t>(expected.size()));
                }
            }

            virtual test_result test() override
            {
                test_result result;
                case_runtime_supports_one_version(result);
                case_runtime_declines_old_versions(result);
                // Only after the runtime has been asked what it supports: from here on this binary
                // can decode 9 and 10, and the two cases above would no longer describe the runtime.
                register_legacy_decoders();
                case_samples_decode_byte_for_byte(result);
                return result;
            }
        };
    }
}
