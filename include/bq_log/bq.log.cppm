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
// The module entry of the library: import bq.log; says what #include "bq_log/bq_log.h" says.
module;

// The system headers the headers below pull in, so that the includes inside them are no-ops, plus
// the switch that tells them so. The list lives in that one file so every entry preincludes the
// same set.
#include "bq_log/module_system_headers.h"

export module bq.log;

// The headers mark their interface with these; the include entry sees them empty.
#define BQ_MODULE_EXPORT_BEGIN export {
#define BQ_MODULE_EXPORT_END }
#include "bq_log/bq_log.h"
#undef BQ_MODULE_EXPORT_BEGIN
#undef BQ_MODULE_EXPORT_END
