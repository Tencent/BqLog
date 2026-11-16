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

// Before bq_common_public_include.h: Python.h has to come before any system header.
#include "bq_common/platform/python_include.h"

#include "bq_common/bq_common_public_include.h"

// This header is intentionally minimal.
// BqLog Python API functions are registered via
// bq::platform::python_method_register in the .cpp file.
