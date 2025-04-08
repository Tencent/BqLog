#pragma once
/*
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#include "bq_log/log/appender/appender_base.h"
#include "bq_log/types/buffer/log_buffer.h"
#include "bq_common/bq_common.h"

namespace bq {
    class appender_console : public appender_base {
    private:
        class console_callbacks {
        private:
            bq::platform::mutex mutex_;
            bq::hash_map<bq::type_func_ptr_console_callback, bool> callbacks_;

        public:
            void register_callback(bq::type_func_ptr_console_callback callback);
            void erase_callback(bq::type_func_ptr_console_callback callback);
            void call(uint64_t log_id, int32_t category_idx, int32_t log_level, const char* content, int32_t length);
        };

        class console_buffer {
        private:
            bool enable_;
            bq::platform::atomic<log_buffer*> buffer_;
            bq::platform::thread::thread_id fetch_thread_id_;
        public:
            console_buffer();
            ~console_buffer();
            void insert(uint64_t epoch_ms, uint64_t log_id, int32_t category_idx, int32_t log_level, const char* content, int32_t length);
            bool fetch_and_remove(bq::type_func_ptr_console_buffer_fetch_callback callback, const void* pass_through_param);
            void set_enable(bool enalbe);
            bq_forceinline bool is_enable() const { return enable_; }
        };

        class console_static_misc {
        private:
            console_callbacks callbacks_;
            console_buffer buffer_;

        public:
            bq_forceinline console_callbacks& callback() { return callbacks_; }
            bq_forceinline console_buffer& buffer() { return buffer_; }
        };

    public:
        appender_console();

    public:
        static void register_console_callback(bq::type_func_ptr_console_callback callback);
        static void unregister_console_callback(bq::type_func_ptr_console_callback callback);

        static void set_console_buffer_enable(bool enable);
        static bool fetch_and_remove_from_console_buffer(bq::type_func_ptr_console_buffer_fetch_callback callback, const void* pass_through_param);

    protected:
        virtual bool init_impl(const bq::property_value& config_obj) override;
        virtual void log_impl(const log_entry_handle& handle) override;

    private:
        static console_static_misc& get_console_misc();

    private:
        bq::string log_name_prefix_;
        bq::string log_entry_cache_;

    private:
    };
}