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

#include <string>
#include "test_base.h"
#include "bq_common/bq_common.h"
#include "bq_log/bq_log.h"
#include "bq_log/types/storage/instance_storage.h"
#include "bq_log/types/storage/log_storage.h"
#include "bq_log/log/log_manager.h"

namespace bq {
    namespace test {

        // The base destructor's flush lands on the base transform and would write the pending bytes
        // unencrypted; a correctly written tail decodes back into the record.
        class test_security_destruct : public test_base {
        private:
            static const bq::string& appender_pub_key()
            {
                static const bq::string k = bq::string(
                                                "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCwv3QtDXB/fQN+FonyOHuS2uC6IZc16bfd6qQk4ykBOt3nTfBFc")
                    + "Nr8ZWvvcf4H0hFkrpMtQ0AJO057GhVTQCCfnvfStSq2Yra+O5VGpI5Q6NLrUuVERimjNgwtxbXt3P8Nw87jEIJiY/8m2FUXhZE"
                    + "PwoA7t+2/953cNE1itJskJtojwaUlMN0dXBJxs4NP8MfBPPZQ5vNV8xgEf1SCQzQBAJsofy1kPHHqJNBXUBsNA44SP5H95JOz+"
                    + "r0oaNkYxT88Zk4tbk5N3hk5aXyZVp49OqhrXCPf5owDa4Lqk4UzVTk9EimxvtSuiUTzr7IJhHYy7jsGnSgq6dH0xlUfxKeX pippocao@PIPPOCAO-PC6";
                return k;
            }

            static const bq::string& appender_priv_key()
            {
                static const bq::string k = bq::string("-----BEGIN RSA PRIVATE KEY-----\n")
                    + "MIIEpAIBAAKCAQEAsL90LQ1wf30DfhaJ8jh7ktrguiGXNem33eqkJOMpATrd503w\n"
                    + "RXDa/GVr73H+B9IRZK6TLUNACTtOexoVU0Agn5730rUqtmK2vjuVRqSOUOjS61Ll\n"
                    + "REYpozYMLcW17dz/DcPO4xCCYmP/JthVF4WRD8KAO7ftv/ed3DRNYrSbJCbaI8Gl\n"
                    + "JTDdHVwScbODT/DHwTz2UObzVfMYBH9UgkM0AQCbKH8tZDxx6iTQV1AbDQOOEj+R\n"
                    + "/eSTs/q9KGjZGMU/PGZOLW5OTd4ZOWl8mVaePTqoa1wj3+aMA2uC6pOFM1U5PRIp\n"
                    + "sb7UrolE86+yCYR2Mu47Bp0oKunR9MZVH8SnlwIDAQABAoIBACtaWpmuYTi0JkYo\n"
                    + "Kx/hoNXtoA+nq5pKwJHLOwXdPjKSCNnycQvnWZ9tFSN/V2r9qMyEUY9ZnnxlMqPZ\n"
                    + "Sv/Hi/j7Ghhx3Y8s+VwB62SPemT4JrwX8ipj91SULjqP80br3Re4PqfNZd3SX0Rc\n"
                    + "7co+Nc2izKdZPxTGHM9leNHMMP2VrVbZeSlQBnqlFVMqi2g9ukMGZG10vPdIJV7z\n"
                    + "5dqWaKuW/2F8dp/o6i/uUDWAH4fITLD1PLqx5/kP8ohOXup8wxYaY8jhKlvswuCh\n"
                    + "qlY773SamnrIGctQWe63Fe9q7hzs3vcCOfciFYsVX2qfHPvGORzd8DgMwe3TFbrM\n"
                    + "nmUijcECgYEA5zyarIPQfbEsmMtKgY7OSz+M1SaOE9r/I0Hl2dz7f0r7JocZ6KUj\n"
                    + "NinbX/zvJ7AMzDUefbptjf9F4vNa0eBZmSZDAiWm3byvMKS5uLboFNrKnYptIc3c\n"
                    + "0CzDzC+nMi4NrPoAZZrUyJw1Emr7gWVVG2FW2NatOVqfPq3XBbk9dncCgYEAw60H\n"
                    + "FqcvrSTAVSk9L+TB8Fn92p5YQtaV7CSZj9GQdvfs+pIkPlq+jYvSH7nYjK2Qlq+h\n"
                    + "sn+3YcVVczbGuhSLuq4bHPd46HOjya3rbAq39RxlpFRFjZ4hici4XIFKnB7Kylta\n"
                    + "Ph6nNq9m5tdFZ1SurgLlOaxg2AwLAia2V5L5/+ECgYAzmhmmP/Ap7HzYSB2DVfwB\n"
                    + "XNgvxN/V3HwtQQprGN5i5LexPFryyM9XyfVzsT0pbScd9wir5AuIsZvF7qqoxVkZ\n"
                    + "TSmM9BwNxYqO32O2rdKSvNSUXYzHC2qoZiT3jvbPwuk4Xb3y7p9neTx6tLcVhCh+\n"
                    + "6LT5xMZ5UxjQYvjmBRWLNwKBgQC5Ta9rzXHB5w7Y1w/hviHHKoHTOabdzPb3RQXD\n"
                    + "g7LqZwkdla4K+sZ/pwybDNU9C9TkTnizYG1agpTUYeg6KeDVLbHxcY4nm/Nct349\n"
                    + "t7zTu0uqHkArx7d9Ev88Yxgz1pk2nuJL951klSC+tNg97Zzqn0VSo6KmlmkKZXzC\n"
                    + "XCayIQKBgQC+hPlGE7T6WLKvsmaH3IJI/TAjbuSu25o+aar7ecPoeax6YnSgyF/8\n"
                    + "Xx7f0BFCYEzprftfqDBcfa6D8GKkcsqAMDeJRz8meD+55o3qdL6LKFkjCKvmRxuv\n"
                    + "OmO+42HqCD4mqxMU1rgcWOn+LLW3HSqbE5kYA9XDwEtxCnMiKqP7sA==\n"
                    + "-----END RSA PRIVATE KEY-----\n";
                return k;
            }

            static bq::string marker() { return "D1_PENDING_RECORD_7C5A"; }

            static bq::string output_dir() { return TO_ABSOLUTE_PATH("security_destruct_out", 0); }

            static bq::string output_file() { return output_dir() + "/d1_1.logcompr"; }

            static bq::string log_internal_dir(const bq::string& log_name)
            {
                auto* inst = bq::instance_storage::get();
                if (!inst) {
                    return bq::string();
                }
                return inst->logs_dir() + "/" + bq::log_storage::escape_name(log_name);
            }

            // Forces the instance into existence, or logs_dir() is empty and nothing is cleaned.
            static void ensure_instance_storage()
            {
                if (!bq::instance_storage::get()) {
                    bq::log::force_flush_all_logs();
                }
            }

            static bq::string make_log_conf()
            {
                bq::string conf = "log.recovery=true\n"
                                  "log.thread_mode=sync\n"
                                  "appenders_config.sec.type=compressed_file\n"
                                  "appenders_config.sec.levels=[all]\n"
                                  "appenders_config.sec.max_file_size=0\n"
                                  "appenders_config.sec.enable_rolling_log_file=false\n"
                                  "appenders_config.sec.file_name=security_destruct_out/d1\n"
                                  "appenders_config.sec.pub_key=";
                conf += appender_pub_key();
                conf += "\n";
                return conf;
            }

            // Resets every process-global injection on both construction and destruction, so a
            // failure in the middle of a case cannot leak the injected disk-full state into the next.
            struct scoped_injection_guard {
                scoped_injection_guard()
                {
                    bq::platform::test_inject::set_fault(bq::platform::test_inject::fault_kind::none);
                    bq::platform::test_inject::set_path_filter(nullptr);
                }
                ~scoped_injection_guard()
                {
                    bq::platform::test_inject::set_fault(bq::platform::test_inject::fault_kind::none);
                    bq::platform::test_inject::set_path_filter(nullptr);
                }
            };

            // first_error is the first non-success result: an undecryptable tail stops the entries there.
            static bq::string decode_all(bq::appender_decode_result& first_error, uint32_t& entry_count)
            {
                bq::tools::log_decoder decoder(output_file(), appender_priv_key());
                bq::string all;
                first_error = bq::appender_decode_result::success;
                entry_count = 0;
                for (;;) {
                    const bq::appender_decode_result r = decoder.decode();
                    if (r == bq::appender_decode_result::eof) {
                        break;
                    }
                    if (r != bq::appender_decode_result::success) {
                        first_error = r;
                        break;
                    }
                    all += decoder.get_last_decoded_log_entry();
                    all += "\n";
                    if (++entry_count > 100000) {
                        break;
                    }
                }
                return all;
            }

        public:
            void case_pending_cache_at_destruct(test_result& result)
            {
                scoped_injection_guard guard;
                const bq::string log_name = "security_destruct_d1";

                ensure_instance_storage();
                bq::file_manager::remove_file_or_dir(output_dir());
                const bq::string internal_dir = log_internal_dir(log_name);
                if (!internal_dir.is_empty()) {
                    bq::file_manager::remove_file_or_dir(internal_dir);
                }

                bq::log log_obj = bq::log::create_log(log_name.c_str(), make_log_conf().c_str());
                result.add_result(log_obj.is_valid(), "encrypted log created");

                // Warm up so the appender has its output file open before the fault is armed.
                log_obj.info("warm up line");
                bq::log::force_flush_all_logs();
                result.add_result(bq::file_manager::is_file(output_file()), "output file created");

                // The write fails with ENOSPC, so the record stays in the write cache.
                bq::platform::test_inject::set_path_filter("security_destruct_out");
                bq::platform::test_inject::set_fault(bq::platform::test_inject::fault_kind::enospc_on_write);
                log_obj.info("pending {}", marker());
                bq::log::force_flush_all_logs();
                bq::platform::test_inject::set_fault(bq::platform::test_inject::fault_kind::none);
                bq::platform::test_inject::set_path_filter(nullptr);

                // Destroy the appenders as teardown does, without flushing first.
                bq::log_manager::instance().test_teardown_appenders(log_obj.get_id());

                bq::appender_decode_result first_error = bq::appender_decode_result::success;
                uint32_t entry_count = 0;
                const bq::string decoded = decode_all(first_error, entry_count);

                result.add_result(first_error == bq::appender_decode_result::success,
                    "every record in the file decodes (result %d)",
                    static_cast<int32_t>(first_error));
                result.add_result(entry_count >= 2, "both records decoded (got %d)",
                    static_cast<int32_t>(entry_count));
                result.add_result(decoded.find(marker()) != bq::string::npos,
                    "the record pending at destruction decodes back to its text");

                bq::file_manager::remove_file_or_dir(output_dir());
                if (!internal_dir.is_empty()) {
                    bq::file_manager::remove_file_or_dir(internal_dir);
                }
            }

            virtual test_result test() override
            {
                test_result result;
                case_pending_cache_at_destruct(result);
                return result;
            }
        };
    }
}
