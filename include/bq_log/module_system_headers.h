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
// The system headers a C++20 module entry of bq_log preincludes. bq_log is built on bq_common, so
// this forwards to bq_common's list instead of repeating it; a bq_log specific system header goes
// here, never in a *.cppm.
//
// Include this only from the global module fragment of a module entry: after `module;` and before
// `export module`.
#include "bq_common/module_system_headers.h"
