/* Copyright (C) 2025 Tencent.
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
/*!
 * Don't call functions defined in this file!!
 *
 * \brief
 *
 * \author pippocao
 * \date 2022.08.03
 */
#include "bq_common/bq_common_public_include.h"
#include "bq_log/misc/bq_log_def.h"

namespace bq {
    namespace api {
        /////////////////////////////////////////////////////////////DYNAMIC LIB APIS BEGIN////////////////////////////////////////////////////
#include "bq_log/misc/bq_log_c_api.inc"
    }
}
