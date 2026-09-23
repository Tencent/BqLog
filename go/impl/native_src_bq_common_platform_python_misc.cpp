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

#include "native_src_bq_common_platform_python_misc.h"
#if defined(BQ_PYTHON)

// Use Python Stable ABI (abi3) targeting Python 3.7+.
// A single compiled extension works across all Python 3.7+ versions.
#define Py_LIMITED_API 0x03070000

#if defined(BQ_MSVC) && defined(_DEBUG)
#if _DEBUG
#define _BQ_PYTHON_HAD_DEBUG
#undef _DEBUG
#endif
#endif

#include <Python.h>

#if defined(_BQ_PYTHON_HAD_DEBUG)
#define _DEBUG 1
#undef _BQ_PYTHON_HAD_DEBUG
#endif

// ---- Module name: default "_bqlog", overridable via BQ_PYTHON_MODULE_NAME macro ----
#if !defined(BQ_PYTHON_MODULE_NAME)
#define BQ_PYTHON_MODULE_NAME _bqlog
#endif

// Stringify helpers
#define BQ_PYTHON_STRINGIFY_IMPL(x) #x
#define BQ_PYTHON_STRINGIFY(x) BQ_PYTHON_STRINGIFY_IMPL(x)

// Build PyInit_<name> entry-point symbol via token pasting
#define BQ_PYTHON_PYINIT_IMPL(name) PyInit_##name
#define BQ_PYTHON_PYINIT(name) BQ_PYTHON_PYINIT_IMPL(name)

#include "native_src_bq_common_bq_common.h"

namespace bq {
    namespace platform {

        python_method_register::python_method_register(PyMethodDef* methods)
        {
            common_global_vars::get().python_registered_method_arrays_.push_back(static_cast<void*>(methods));
        }

        PyObject* python_init()
        {
            auto& method_arrays = common_global_vars::get().python_registered_method_arrays_;

            // Count total methods across all registered arrays
            size_t total = 0;
            for (auto* opaque_arr : method_arrays) {
                for (PyMethodDef* m = static_cast<PyMethodDef*>(opaque_arr); m->ml_name != nullptr; ++m) {
                    ++total;
                }
            }

            // Build a single merged PyMethodDef array (static so it outlives the call)
            static bq::array<PyMethodDef> merged;
            merged.clear();
            merged.set_capacity(static_cast<uint32_t>(total + 1));

            for (auto* opaque_arr : method_arrays) {
                for (PyMethodDef* m = static_cast<PyMethodDef*>(opaque_arr); m->ml_name != nullptr; ++m) {
                    merged.push_back(*m);
                }
            }
            // Sentinel
            PyMethodDef sentinel = { nullptr, nullptr, 0, nullptr };
            merged.push_back(sentinel);

            static struct PyModuleDef module_def = {
                PyModuleDef_HEAD_INIT,
                BQ_PYTHON_STRINGIFY(BQ_PYTHON_MODULE_NAME),
                "BqLog CPython C Extension bindings",
                -1,
                merged.begin(),
                nullptr,    // m_slots
                nullptr,    // m_traverse
                nullptr,    // m_clear
                nullptr     // m_free
            };
            // Update m_methods pointer in case the array was reallocated
            module_def.m_methods = merged.begin();

            return PyModule_Create(&module_def);
        }

#ifdef BQ_DYNAMIC_LIB
#ifdef __cplusplus
        extern "C" {
#endif
        BQ_API PyObject* BQ_PYTHON_PYINIT(BQ_PYTHON_MODULE_NAME)(void)
        {
            return python_init();
        }
#ifdef __cplusplus
        }
#endif
#endif
    }
}
#endif // BQ_PYTHON
