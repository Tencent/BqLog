#pragma once
/*
 * Copyright (C) 2025 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#include "bq_common/bq_common.h"
#include "bq_log/log/appender/appender_base.h"
#include "bq_log/log/log_imp.h"
#include "bq_log/log/log_worker.h"

namespace bq {
    class log_manager {
    private:
        enum class phase {
            invalid,
            working,
            uninitialized
        };
        friend struct log_global_vars;

    private:
        log_manager();

    public:
        ~log_manager();
        static log_manager& instance();

    public:
        uint64_t create_log(bq::string log_name, const bq::string& config_content, const bq::array<bq::string>& category_names);

        bool reset_config(const bq::string& log_name, const bq::string& config_content);

        void process_by_worker(log_imp* target_log, bool is_force_flush);

        uint32_t get_logs_count() const;

        log_imp* get_log_by_index(uint32_t index);

        inline static log_imp* get_log_by_id(uint64_t log_id)
        {
            if (!log_id) {
                return nullptr;
            }
            log_id ^= log_id_magic_number;
            return reinterpret_cast<log_imp*>((uintptr_t)(log_id));
        }

        void force_flush_all();

        bool try_flush_all();

        void force_flush(uint64_t log_id);

        bq_forceinline log_worker& get_public_worker()
        {
            return public_worker_;
        }

        void uninit();

        /// <summary>
        /// get the public layout which can only be used in public_worker
        /// </summary>
        /// <returns></returns>
        bq::layout& get_public_layout();

        void try_restart_worker(log_worker* worker_ptr);

    private:
        bq::platform::atomic<phase> phase_;
        bq::array_inline<bq::unique_ptr<log_imp>> log_imp_list_;
        bq::log_worker public_worker_;
        bq::layout public_layout_;
        bq::platform::spin_lock_rw_crazy logs_lock_;
        bq::platform::spin_lock uninit_lock_;
        bq::platform::atomic<int32_t> automatic_log_name_seq_;
    };
}
