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

namespace bq {
    namespace test {

        // The runtime writes one format version and reads one format version. Old files are the
        // standalone tool's job, and "the runtime refuses them, with a message that says where to
        // go" is a property worth pinning: it is the difference between a user upgrading and a user
        // staring at a decode failure.
        //
        // The samples under test/data/legacy are real files written by Release_2.3.1 (format 9) and
        // Release_2.7.3 (format 10), with the decoded text their own decoders produced. The tool's
        // job - decode them byte for byte - is checked by the CI step that runs BqLog_LogDecoder over
        // that directory; the runtime's job is only to decline them, which is what happens here.
        class test_legacy_format : public test_base {
        private:
            // The samples are source-tree data, not generated output: <repo>/test/data/legacy, as seen
            // from this file, or the directory the run was pointed at.
            static bq::string legacy_data_dir()
            {
                return bq::test::test_data_dir(__FILE__, "legacy");
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

            virtual test_result test() override
            {
                test_result result;
                case_runtime_supports_one_version(result);
                case_runtime_declines_old_versions(result);
                return result;
            }
        };
    }
}
