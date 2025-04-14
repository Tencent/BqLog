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
//
//  vars.cpp
//  Created by Yu Cao on 2025/4/11.
//
#include "bq_common/bq_common.h"

namespace bq {
    common_global_vars* common_global_vars_holder::global_vars_ptr_ = &init_common_global_vars();;
}