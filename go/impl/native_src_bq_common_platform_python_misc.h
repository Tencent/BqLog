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

#include "native_include_bq_common_bq_common_public_include.h"
#if defined(BQ_PYTHON)

// Forward declare PyObject to avoid pulling in Python.h in the header.
struct _object;
typedef _object PyObject;

// Forward declare PyMethodDef (defined in Python.h as a simple C struct).
struct PyMethodDef;

namespace bq {
    namespace platform {

        /// Register a batch of PyMethodDef entries.
        /// All registered entries are collected by python_init() into a
        /// single module. Multiple modules (BqLog, other cloud-control
        /// components, etc.) can each register their own methods; they
        /// will all end up in the same _bqlog extension module.
        ///
        /// Typical usage – in the .cpp file that defines the methods:
        ///
        ///     static PyMethodDef my_methods[] = { ... {nullptr,nullptr,0,nullptr} };
        ///     static bq::platform::python_method_register _reg(my_methods);
        ///
        struct python_method_register {
            /// @param methods  A null-terminated PyMethodDef array.
            ///                 The pointer must remain valid for the
            ///                 lifetime of the programme (i.e. a static
            ///                 array).
            python_method_register(PyMethodDef* methods);
        };

        /// The actual module initialisation implementation.
        /// Returns a new-reference PyObject* (the module), or nullptr on failure.
        /// Users who embed BqLog as a static library or source code can call
        /// this function from their own PyInit_xxx entry point.
        PyObject* python_init();
    }
}
#endif
