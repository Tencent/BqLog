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
 * The placement new/delete pair the containers construct objects with, and the tag that keeps it
 * from colliding with any other overload.
 *
 * It is a header of its own because a translation unit that instantiates one of those containers
 * through `import bq.log;` needs the declarations in its own scope: gcc resolves the allocation
 * function of a new-expression whose type is still dependent in the instantiating translation
 * unit, where a declaration that came from the module's global module fragment is not visible.
 * bq_common/module_system_headers.h includes this header for that reason, and a module consumer
 * that does not pre-include that file includes this one.
 */
#if !defined(BQ_MODULE_SYSTEM_HEADERS_PREINCLUDED)
#include <stddef.h>
#endif

// we must supply a custom placement new, because we can compile without libc++.
// but in order to avoid pollute users code,
// we don't include <new> or overload default operator new.
namespace bq {
    enum class enum_new_dummy {
        dummy
    };
}
inline void* operator new(size_t, void* p, bq::enum_new_dummy) noexcept { return p; }
inline void* operator new[](size_t, void* p, bq::enum_new_dummy) noexcept { return p; }
inline void operator delete(void*, void*, bq::enum_new_dummy) noexcept {}
inline void operator delete[](void*, void*, bq::enum_new_dummy) noexcept {}
