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
#include "bq_common/platform/no_lib_cpp_impl.h"
#if defined(BQ_NO_LIBCPP)
#include "bq_common/bq_common.h"

void* operator new(size_t size)
{
    void* p = nullptr;
    if (posix_memalign(&p, 8, size) != 0) {
        assert(false && "bad alloc");
    }
    return p;
}

void* operator new[](size_t size)
{
    void* p = nullptr;
    if (posix_memalign(&p, 8, size) != 0) {
        assert(false && "bad alloc");
    }
    return p;
}

void operator delete(void* ptr) noexcept
{
    free(ptr);
}

void operator delete[](void* ptr) noexcept
{
    free(ptr);
}

extern "C" void __cxa_pure_virtual()
{
    assert(false && "you are calling a pure virtual function!");
}

// inspired by google bionic
//////////////////////////////////////////////cxa_guard begin/////////////////////////////////////
#if defined(__arm__)
// The ARM C++ ABI mandates that guard variables are 32-bit aligned, 32-bit
// values. The LSB is tested by the compiler-generated code before calling
// __cxa_guard_acquire.
union _guard_t {
    bq::platform::atomic<int32_t> state;
    int32_t aligner;
};
#else
// The Itanium/x86 C++ ABI (used by all other architectures) mandates that
// guard variables are 64-bit aligned, 64-bit values. The LSB is tested by
// the compiler-generated code before calling __cxa_guard_acquire.
union _guard_t {
    bq::platform::atomic<int32_t> state;
    int64_t aligner;
};
#endif

constexpr int32_t CONSTRUCTION_NOT_YET_STARTED = 0;
constexpr int32_t CONSTRUCTION_COMPLETE = 1;
constexpr int32_t CONSTRUCTION_UNDERWAY_WITHOUT_WAITER = 0x100;
constexpr int32_t CONSTRUCTION_UNDERWAY_WITH_WAITER = 0x200;

extern "C" int __cxa_guard_acquire(_guard_t* gv)
{
    int32_t old_value = gv->state.load(bq::platform::memory_order::acquire);
    while (true) {
        if (old_value == CONSTRUCTION_COMPLETE) {
            return 0;
        } else if (old_value == CONSTRUCTION_NOT_YET_STARTED) {
            if (!gv->state.compare_exchange_weak(old_value, CONSTRUCTION_UNDERWAY_WITHOUT_WAITER, bq::platform::memory_order::release, bq::platform::memory_order::acquire)) {
                continue;
            }
            return 1;
        } else if (old_value == CONSTRUCTION_UNDERWAY_WITHOUT_WAITER) {
            if (!gv->state.compare_exchange_weak(old_value, CONSTRUCTION_UNDERWAY_WITH_WAITER, bq::platform::memory_order::release, bq::platform::memory_order::acquire)) {
                continue;
            }
        }
        // todo yield ?
        old_value = gv->state.load(bq::platform::memory_order::acquire);
    }
}

extern "C" void __cxa_guard_release(_guard_t* gv)
{
    gv->state.store(CONSTRUCTION_COMPLETE, bq::platform::memory_order::release);
}

extern "C" void __cxa_guard_abort(_guard_t* gv)
{
    // Release fence is used to make all stores performed by the construction function
    // visible in other threads.
    gv->state.store(CONSTRUCTION_NOT_YET_STARTED, bq::platform::memory_order::release);
    //////////////////////////////////////////////cxa_guard end/////////////////////////////////////
}
#endif