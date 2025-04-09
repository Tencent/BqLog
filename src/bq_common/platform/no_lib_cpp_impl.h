#pragma once
/*
 * Copyright (C) 2024 THL A29 Limited, a Tencent company.
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
 * \class no_lib_cpp_impl.cpp
 *
 * \To pursue the minimum size of final binary executable and library, we may exclude stdlibc++ on some platforms(such as android and ios).
 * \So we had to implement some useful features by ourselves, you can see <type_traits.h>, <array.h>, <hash_map.h>, <atomic.h> and so on in our repository.
 * \But thats not enough, there are still some c++ language features depending on the implementation of stdlibc++.
 *
 * \Includes:
 * \1. When you define a pure virtual class, you must have a error handler "__cxa_pure_virtual" implementation when linking on GNU CPP compilers.
 * \2. global "operator new", and other functions.
 * \3. "__cxa_guard_acquire" and "__cxa_guard_release" to enable the "scoped static"("Magic Static") feature.
 *
 * \author pippocao
 * \date 2022/09/21
 */
#include "bq_common/platform/macros.h"
#include "bq_common/misc/assert.h"
#if defined(BQ_NO_LIBCPP)
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

namespace std {
	enum class align_val_t : size_t {};
	struct nothrow_t {
	};
	extern const nothrow_t nothrow;
}

inline void* operator new(size_t, void* p) noexcept { return p; }
inline void* operator new[](size_t, void* p) noexcept { return p; }
inline void operator delete(void*, void*) noexcept { }
inline void operator delete[](void*, void*) noexcept { }

void* operator new(size_t size);
void* operator new[](size_t size);
void operator delete(void* ptr) noexcept;
void operator delete[](void* ptr) noexcept;

void* operator new(size_t size, std::align_val_t alignment);
void* operator new[](size_t size, std::align_val_t alignment);
void operator delete(void* ptr, std::align_val_t alignment) noexcept;
void operator delete[](void* ptr, std::align_val_t alignment) noexcept;

void* operator new(size_t size, const std::nothrow_t&) noexcept;
void* operator new[](size_t size, const std::nothrow_t&) noexcept;
void* operator new(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept;
void* operator new[](size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept;

void operator delete(void* ptr, const std::nothrow_t&) noexcept;
void operator delete[](void* ptr, const std::nothrow_t&) noexcept;
void operator delete(void* ptr, std::align_val_t alignment, const std::nothrow_t&) noexcept;
void operator delete[](void* ptr, std::align_val_t alignment, const std::nothrow_t&) noexcept;
#else
#include <new>
#endif
