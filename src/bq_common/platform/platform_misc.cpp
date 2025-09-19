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
#include "bq_common/bq_common.h"

namespace bq {
    namespace platform {
        const bq::string& get_base_dir(bool is_sandbox)
        {
            if (is_sandbox) {
                return common_global_vars::get().base_dir_init_inst_.get_base_dir_0();
            }
            else {
                return common_global_vars::get().base_dir_init_inst_.get_base_dir_1();
            }
        }

        void base_dir_initializer::set_base_dir_0(const bq::string& dir)
        {
            base_dir_0_ = dir;
            bq::util::log_device_console(log_level::info, "set base dir in sandbox: %s", dir.c_str());
        }
        void base_dir_initializer::set_base_dir_1(const bq::string& dir)
        {
            base_dir_1_ = dir;
            bq::util::log_device_console(log_level::info, "set base dir : %s", dir.c_str());
        }
    }
}


