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
#include "bq_common/bq_common_public_include.h"
#if defined(BQ_OHOS)
#include <hitrace/trace.h>
#include "napi/native_api.h"

// On OHOS (and most non-Windows platforms), calling convention macro is not needed.
// Node's node_api.h defines NAPI_CDECL (notably for Windows). If it's missing, make it empty.
#ifndef NAPI_CDECL
  #define NAPI_CDECL
#endif

namespace bq {
    namespace platform {
        const bq::string get_bundle_name();

    }
}
#endif
