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
#include "bq_log/bq_log.h"
#include "bq_log/log/appender/appender_base.h"
#include "bq_log/log/layout.h"
#include "bq_log/types/ring_buffer.h"
#include "bq_common/platform/atomic/atomic.h"

namespace bq {
    class appender_console : public appender_base {
    public:
        appender_console();

    public:
        static void register_console_callback(bq::type_func_ptr_console_callback callback);
        static void unregister_console_callback(bq::type_func_ptr_console_callback callback);

    protected:
        virtual bool init_impl(const bq::property_value& config_obj) override;
        virtual void log_impl(const log_entry_handle& handle) override;

    private:
        static bq::platform::mutex& get_console_mutex();
        static bq::hash_map<bq::type_func_ptr_console_callback, bool>& get_console_callbacks();

    private:
        bq::string log_name_prefix_;
        bq::string log_entry_cache_;

    private:
    };
}