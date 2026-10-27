// clang-format off
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
/*!
 * Generated module entry for test_category_log
 *
 * It exports the same class as test_category_log.h, so import it instead of including the header:
 *   import bq.test_category_log;
 *
 * The two entries are mutually exclusive: a translation unit may not import and include the
 * same library, so a project picks one of them.
 */
module;

// The system headers the class is written in terms of, plus the language-level macros the header
// uses (BQ_CPP_17, BQ_CPP_20, bq_forceinline, BQ_EBCO): a macro is not exported by a module, so
// they come from macros.h inside that one list, never from a list copied here.
#include "bq_log/module_system_headers.h"

export module bq.test_category_log;

import bq.log;

// The header carries the whole class. These two markers turn its export block on, and because
// BQ_MODULE_EXPORT_BEGIN is defined while it is read, the header skips its own bq_log.h include:
// the library arrives through the import above. Everything the header defines out of class stays
// outside the export block, which is what C++20 requires.
#define BQ_MODULE_EXPORT_BEGIN export {
#define BQ_MODULE_EXPORT_END }
#include "test_category_log.h"
#undef BQ_MODULE_EXPORT_BEGIN
#undef BQ_MODULE_EXPORT_END
// clang-format on
        
