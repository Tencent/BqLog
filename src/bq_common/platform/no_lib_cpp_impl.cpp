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
#include <pthread.h>
#include "bq_common/bq_common.h"

const std::nothrow_t std::nothrow = {};

void* operator new(size_t size)
{
    return bq::platform::aligned_alloc(8, size);
}

void* operator new[](size_t size)
{
    return bq::platform::aligned_alloc(8, size);
}

void operator delete(void* ptr) noexcept
{
    bq::platform::aligned_free(ptr);
}

void operator delete[](void* ptr) noexcept
{
    bq::platform::aligned_free(ptr);
}

void* operator new(size_t size, std::align_val_t alignment)
{
    return bq::platform::aligned_alloc(static_cast<size_t>(alignment), size);
}

void* operator new[](size_t size, std::align_val_t alignment)
{
    return bq::platform::aligned_alloc(static_cast<size_t>(alignment), size);
}

void operator delete(void* ptr, std::align_val_t) noexcept
{
    bq::platform::aligned_free(ptr);
}

void operator delete[](void* ptr, std::align_val_t) noexcept
{
    bq::platform::aligned_free(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept
{
    bq::platform::aligned_free(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept
{
    bq::platform::aligned_free(ptr);
}

void operator delete(void* ptr, std::size_t, std::align_val_t) noexcept
{
    bq::platform::aligned_free(ptr);
}

void* operator new(size_t size, const std::nothrow_t&) noexcept
{
    return bq::platform::aligned_alloc(8, size);
}

void* operator new[](size_t size, const std::nothrow_t&) noexcept
{
    return bq::platform::aligned_alloc(8, size);
}

void* operator new(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    return bq::platform::aligned_alloc(static_cast<size_t>(alignment), size);
}

void* operator new[](size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    return bq::platform::aligned_alloc(static_cast<size_t>(alignment), size);
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept
{
    bq::platform::aligned_free(ptr);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept
{
    bq::platform::aligned_free(ptr);
}

void operator delete(void* ptr, std::align_val_t, const std::nothrow_t&) noexcept
{
    bq::platform::aligned_free(ptr);
}

void operator delete[](void* ptr, std::align_val_t, const std::nothrow_t&) noexcept
{
    bq::platform::aligned_free(ptr);
}

extern "C" void __cxa_pure_virtual()
{
    assert(false && "you are calling a pure virtual function!");
}

// inspired by google bionic
//////////////////////////////////////////////cxa_guard begin/////////////////////////////////////
union _guard_t {
    int32_t state;
#if defined(__arm__)
    // The ARM C++ ABI mandates that guard variables are 32-bit aligned, 32-bit
    // values. The LSB is tested by the compiler-generated code before calling
    // __cxa_guard_acquire.
    int32_t aligner;
#else
    // The Itanium/x86 C++ ABI (used by all other architectures) mandates that
    // guard variables are 64-bit aligned, 64-bit values. The LSB is tested by
    // the compiler-generated code before calling __cxa_guard_acquire.
    int64_t aligner;
#endif
public:
    bq_forceinline bq::platform::atomic<int32_t>& get_atomic_state()
    {
        return *reinterpret_cast<bq::platform::atomic<int32_t>*>((void*)&state);
    }
};

constexpr int32_t CONSTRUCTION_NOT_YET_STARTED = 0;
constexpr int32_t CONSTRUCTION_COMPLETE = 1;
constexpr int32_t CONSTRUCTION_UNDERWAY_WITHOUT_WAITER = 0x100;
constexpr int32_t CONSTRUCTION_UNDERWAY_WITH_WAITER = 0x200;

extern "C" int32_t __cxa_guard_acquire(_guard_t* gv)
{
    int32_t old_value = gv->get_atomic_state().load(bq::platform::memory_order::acquire);
    while (true) {
        if (old_value == CONSTRUCTION_COMPLETE) {
            return 0;
        } else if (old_value == CONSTRUCTION_NOT_YET_STARTED) {
            if (!gv->get_atomic_state().compare_exchange_weak(old_value, CONSTRUCTION_UNDERWAY_WITHOUT_WAITER, bq::platform::memory_order::release, bq::platform::memory_order::acquire)) {
                continue;
            }
            return 1;
        } else if (old_value == CONSTRUCTION_UNDERWAY_WITHOUT_WAITER) {
            if (!gv->get_atomic_state().compare_exchange_weak(old_value, CONSTRUCTION_UNDERWAY_WITH_WAITER, bq::platform::memory_order::release, bq::platform::memory_order::acquire)) {
                continue;
            }
        }
        // todo yield ?
        old_value = gv->get_atomic_state().load(bq::platform::memory_order::acquire);
    }
}

extern "C" void __cxa_guard_release(_guard_t* gv)
{
    gv->get_atomic_state().store(CONSTRUCTION_COMPLETE, bq::platform::memory_order::release);
}

extern "C" void __cxa_guard_abort(_guard_t* gv)
{
    // Release fence is used to make all stores performed by the construction function
    // visible in other threads.
    gv->get_atomic_state().store(CONSTRUCTION_NOT_YET_STARTED, bq::platform::memory_order::release);
}
//////////////////////////////////////////////cxa_guard end/////////////////////////////////////

//////////////////////////////////////////////__cxa_thread_atexit begin/////////////////////////////////////
struct tls_entry {
    void (*destructor_)(void*);
    void* obj_;
    tls_entry* next_;
};

static pthread_key_t stl_key;

static BQ_TLS tls_entry* tls_entry_head_ = nullptr;

static void on_thread_exit(void* param)
{
    (void)param;
    while (tls_entry_head_) {
        tls_entry_head_->destructor_(tls_entry_head_->obj_);
        auto next = tls_entry_head_->next_;
        free(tls_entry_head_);
        tls_entry_head_ = next;
    }
}

struct st_stl_key_initer {
    st_stl_key_initer()
    {
        pthread_key_create(&stl_key, &on_thread_exit);
    }
};

extern "C" int32_t __cxa_thread_atexit(void (*destructor)(void*), void* obj, void* destructor_handle)
{
    (void)destructor_handle;
    static st_stl_key_initer key_initer;

    tls_entry* entry = (tls_entry*)malloc(sizeof(tls_entry));
    if (!entry) {
        return -1;
    }
    entry->destructor_ = destructor;
    entry->obj_ = obj;
    entry->next_ = tls_entry_head_;
    tls_entry_head_ = entry;

    return 0;
}
//////////////////////////////////////////////__cxa_thread_atexit end/////////////////////////////////////

#endif