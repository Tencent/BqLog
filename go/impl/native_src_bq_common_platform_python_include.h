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

// The one place Python.h is included. The two Python headers include it before their own first include,
// so it stays ahead of every system header while a .cpp still leads with its own header.
// Nothing may precede Python.h, so this header includes nothing else and tests the raw _MSC_VER:
// BQ_MSVC comes with macros.h, which includes a system header itself.
#if defined(BQ_PYTHON)

// Python Stable ABI (abi3) for 3.7+, unless the build chose its own.
#if !defined(Py_LIMITED_API)
#define Py_LIMITED_API 0x03070000
#endif

#if defined(_MSC_VER) && defined(_DEBUG)
#if _DEBUG
#define BQ_PYTHON_HAD_DEBUG
#undef _DEBUG
#endif
#endif

#include <Python.h>

#if defined(BQ_PYTHON_HAD_DEBUG)
#define _DEBUG 1
#undef BQ_PYTHON_HAD_DEBUG
#endif

#endif
