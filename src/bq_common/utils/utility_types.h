#pragma once
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
/*!
 * \file utility_types.h
 *
 * \author pippocao
 * \date 2022/07/14
 *
 */

#include "bq_common/bq_common_public_include.h"
#include "bq_common/platform/atomic/atomic.h"
#include "bq_common/utils/aligned_allocator.h"
namespace bq {

    static constexpr size_t BQ_CACHE_LINE_SIZE = 128;
    static constexpr size_t BQ_CACHE_LINE_SIZE_LOG2 = 7;

    // This is a wrapper struct which can define your data alignment.
    //"alignas" keyword can only ensure alignment on heap allocation when c++ standard is up to c++17
    // and it does not work correctly when object is constructed with "placement new" when starting address is not aligned.
    //***important: This implementation might introduce some performance overhead.
    template <typename T, uint32_t align>
    struct aligned_type;

    template <typename T>
    struct is_aligned_type : public bq::false_type {
    };

    template <typename T, uint32_t align>
    struct is_aligned_type<bq::aligned_type<T, align>> : public bq::true_type {
    };

    template <typename T, uint32_t align>
    struct aligned_type {
    private:
        typedef uint32_t offset_type;
        static_assert(sizeof(offset_type) <= 4, "size of offset_type should <= 4");
        static constexpr size_t offset_mask = ~(sizeof(offset_type) - 1);

        uint8_t offset_[sizeof(offset_type) + sizeof(offset_type)];
        uint8_t padding_[align + sizeof(T)];

    private:
        uint32_t& offset() const
        {
            return *(uint32_t*)(((size_t)offset_ + (sizeof(offset_type) - 1)) & offset_mask);
        }

        void init()
        {
            size_t addr = (size_t)padding_;
            uint32_t mod = (uint32_t)(addr % align);
            if (mod == 0) {
                offset() = 0;
            } else {
                offset() = (uint32_t)align - mod;
            }
        }

        const void* get_addr() const
        {
            return static_cast<const void*>(padding_ + offset());
        }

        void* get_addr()
        {
            return static_cast<void*>(padding_ + offset());
        }

    public:
        aligned_type()
        {
            init();
            new (get_addr(), bq::enum_new_dummy::dummy) T();
        }

        ~aligned_type()
        {
            bq::object_destructor<T>::destruct((T*)get_addr());
        }

        inline aligned_type(const aligned_type<T, align>& rhs)
        {
            init();
            new (get_addr(), bq::enum_new_dummy::dummy) T(rhs.get());
        }

        inline aligned_type(aligned_type<T, align>&& rhs) noexcept
        {
            init();
            new (get_addr(), bq::enum_new_dummy::dummy) T(bq::move(rhs.get()));
        }

        template <typename U, uint32_t U_align>
        inline aligned_type(const aligned_type<U, U_align>& rhs)
        {
            init();
            new (get_addr(), bq::enum_new_dummy::dummy) T(rhs.get());
        }

        template <typename U, uint32_t U_align>
        inline aligned_type(aligned_type<U, U_align>&& rhs) noexcept
        {
            init();
            new (get_addr(), bq::enum_new_dummy::dummy) T(bq::move(rhs.get()));
        }

        template <typename U, typename enable_if<!is_aligned_type<typename decay<U>::type>::value>::type* = nullptr>
        inline aligned_type(U&& init_value)
        {
            init();
            new (get_addr(), bq::enum_new_dummy::dummy) T(bq::forward<U>(init_value));
        }

        inline aligned_type<T, align>& operator=(const aligned_type<T, align>& rhs)
        {
            get() = rhs.get();
            return *this;
        }

        inline aligned_type<T, align>& operator=(aligned_type<T, align>&& rhs) noexcept
        {
            get() = bq::move(rhs.get());
            return *this;
        }

        template <typename U, uint32_t U_align>
        inline aligned_type<T, align>& operator=(const aligned_type<U, U_align>& rhs)
        {
            get() = rhs.get();
            return *this;
        }

        template <typename U, uint32_t U_align>
        inline aligned_type<T, align>& operator=(aligned_type<U, U_align>&& rhs) noexcept
        {
            get() = bq::move(rhs.get());
            return *this;
        }

        template <typename U, typename enable_if<!is_aligned_type<typename decay<U>::type>::value>::type* = nullptr>
        inline aligned_type<T, align>& operator=(U&& value)
        {
            get() = bq::forward<U>(value);
            return *this;
        }

        inline T& get()
        {
            return *(T*)get_addr();
        }
        inline const T& get() const
        {
            return *(const T*)get_addr();
        }
    };

    // This is a wrapper struct which can ensure your data monopolize L1 cache line.
    // It works in C++ 11 and later version, regardless whether object is allocated in heap or stack.
    // Warning : There maybe some memory waste.
    template <typename T>
    struct cache_friendly_type {
    private:
        uint8_t padding_left_[BQ_CACHE_LINE_SIZE];
        T value_;
        uint8_t padding_right_[BQ_CACHE_LINE_SIZE];

    public:
        template <typename U>
        cache_friendly_type(U&& rhs)
            : value_(bq::forward<U>(rhs))
        {
        }

        inline T& get()
        {
            return value_;
        }

        inline const T& get() const
        {
            return value_;
        }
    };

    // simple substitude of std::unique_ptr
    // which can handle the vast majority of cases.
    template <typename T>
    class unique_ptr {
    public:
        using value_type = T;

    public:
        unique_ptr()
            : ptr(nullptr)
        {
        }
        ~unique_ptr() { reset(); }
        template <typename D>
        unique_ptr(unique_ptr<D>&& rhs) noexcept
            : ptr(rhs.ptr)
        {
            rhs.ptr = nullptr;
        }

        unique_ptr(const unique_ptr&) = delete;
        unique_ptr& operator=(T*) = delete;
        unique_ptr& operator=(const unique_ptr&) = delete;

        template <typename D>
        unique_ptr& operator=(unique_ptr<D>&& rhs)
        {
            if (ptr != rhs.ptr) {
                reset();
                ptr = rhs.ptr;
                rhs.ptr = nullptr;
            }
            return *this;
        }
        T* operator->()
        {
            return ptr;
        }
        const T* operator->() const
        {
            return ptr;
        }
        T& operator*()
        {
            return *ptr;
        }
        const T& operator*() const
        {
            return *ptr;
        }

        operator void*() const
        {
            return static_cast<void*>(ptr);
        }

        operator bool() const {
            return ptr;
        }

        template <typename D>
        bool operator==(const unique_ptr<D>& rhs) const
        {
            return ptr == rhs.operator->();
        }
        bool operator==(decltype(nullptr)) const
        {
            return ptr == nullptr;
        }
        template <typename D>
        bool operator!=(const unique_ptr<D>& rhs) const
        {
            return ptr != rhs.operator->();
        }
        template <typename D>
        bool operator<(const unique_ptr<D>& rhs) const
        {
            return ptr < rhs.operator->();
        }
        template <typename D>
        bool operator<=(const unique_ptr<D>& rhs) const
        {
            return ptr <= rhs.operator->();
        }
        template <typename D>
        bool operator>(const unique_ptr<D>& rhs) const
        {
            return ptr > rhs.operator->();
        }
        template <typename D>
        bool operator>=(const unique_ptr<D>& rhs) const
        {
            return ptr >= rhs.operator->();
        }
        T* get()
        {
            return ptr;
        }
        bq::enable_if_t<!bq::is_const<T>::value, const T*> get() const
        {
            return ptr;
        }
        template <typename D>
        void swap(unique_ptr<D>& rhs)
        {
            auto old = ptr;
            ptr = rhs.ptr;
            rhs.ptr = old;
        }
        void reset()
        {
            if (ptr) {
                ptr->~T();
                bq::aligned_free(ptr);
                ptr = nullptr;
            }
        }

    private:
        template <typename>
        friend class unique_ptr;

        template <typename U, typename... Args>
        friend unique_ptr<U> make_unique(Args&&...);

        template <typename U>
        friend unique_ptr<U> make_unique();

        explicit unique_ptr(T* new_ptr) : ptr(new_ptr) {}

        T* ptr;
    };

    template <typename T, typename... Ts>
    unique_ptr<T> make_unique(Ts&&... params)
    {
        void* addr = bq::aligned_alloc(alignof(T), sizeof(T));
        new (addr, bq::enum_new_dummy::dummy)T(bq::forward<Ts>(params)...);
        return unique_ptr<T>(static_cast<T*>(addr));
    }

    // make a unique_ptr with default initialization
    template <class T>
    unique_ptr<T> make_unique()
    {
        void* addr = bq::aligned_alloc(alignof(T), sizeof(T));
        new (addr, bq::enum_new_dummy::dummy)T();
        return unique_ptr<T>(static_cast<T*>(addr));
    }

    // simple substitude of std::shared_ptr
    // which can handle the vast majority of cases.
    template <typename T>
    class shared_ptr {
    public:
        shared_ptr() noexcept
            : ptr_(nullptr)
            , ref_count_(nullptr)
        {
        }

        shared_ptr(const shared_ptr& rhs) noexcept
        {
            if (rhs.ref_count_) {
                int64_t prev_value = rhs.ref_count_->fetch_add_seq_cst(1);
                if ((prev_value >> 32) == 0) {
                    ptr_ = rhs.ptr_;
                    ref_count_ = rhs.ref_count_;
                    return;
                }
            }
            ptr_ = nullptr;
            ref_count_ = nullptr;
        }

        template <typename D>
        shared_ptr(shared_ptr<D>&& rhs) noexcept
            : ptr_(rhs.ptr_)
            , ref_count_(rhs.ref_count_)
        {
            rhs.ptr_ = nullptr;
            rhs.ref_count_ = nullptr;
        }

        ~shared_ptr()
        {
            release();
        }

        shared_ptr& operator=(const shared_ptr& rhs)
        {
            if (this != &rhs) {
                release();
                if (rhs.ref_count_) {
                    int64_t prev_value = rhs.ref_count_->fetch_add_seq_cst(1);
                    if ((prev_value >> 32) == 0) {
                        ptr_ = rhs.ptr_;
                        ref_count_ = rhs.ref_count_;
                        return *this;
                    }
                }
            }
            return *this;
        }

        template <typename D>
        shared_ptr& operator=(shared_ptr<D>&& rhs) noexcept
        {
            if (this != &rhs) {
                release();
                ptr_ = rhs.ptr_;
                ref_count_ = rhs.ref_count_;
                rhs.ptr_ = nullptr;
                rhs.ref_count_ = nullptr;
            }
            return *this;
        }

        T* operator->() const { return ptr_; }
        T& operator*() const { return *ptr_; }

        T* get() const noexcept { return ptr_; }

        int32_t use_count() const noexcept
        {
            if (ref_count_) {
                int64_t state = ref_count_->load_relaxed();
                if ((state >> 32) == 0) {
                    return static_cast<int32_t>(state & 0xFFFFFFFF);
                }
            }
            return 0;
        }

        template <typename D>
        bool operator==(const shared_ptr<D>& rhs) const noexcept
        {
            return ptr_ == rhs.ptr;
        }
        bool operator==(decltype(nullptr)) const noexcept
        {
            return ptr_ == nullptr;
        }
        template <typename D>
        bool operator!=(const shared_ptr<D>& rhs) const noexcept
        {
            return ptr_ != rhs.ptr;
        }
        template <typename D>
        bool operator!=(decltype(nullptr)) const noexcept
        {
            return ptr_ != nullptr;
        }

        void reset() noexcept
        {
            release();
        }

        void reset(T* new_ptr)
        {
            release();
            ptr_ = new_ptr;
            ref_count_ = new_ptr ? new bq::platform::atomic<int64_t>(1) : nullptr;
        }

    private:
        template <typename>
        friend class shared_ptr;

        template <typename U, typename... Args>
        friend shared_ptr<U> make_shared(Args&&...);

        template <typename U>
        friend shared_ptr<U> make_shared();

        explicit shared_ptr(T* new_ptr)
            : ptr_(new_ptr)
            , ref_count_(new_ptr ? new bq::platform::atomic<int64_t>(1) : nullptr)
        {
        }

        void release() noexcept
        {
            if (ref_count_) {
                int64_t expected_value = ref_count_->load_relaxed();
                while (true) {
                    if (expected_value == 1) {
                        if (ref_count_->compare_exchange_strong(expected_value, static_cast<int64_t>(1) << 32, bq::platform::memory_order::seq_cst, bq::platform::memory_order::seq_cst)) {
                            ptr_->~T();
                            bq::aligned_free(ptr_);
                            delete ref_count_;
                            break;
                        }
                    } else {
                        if (ref_count_->compare_exchange_strong(expected_value, expected_value - 1, bq::platform::memory_order::seq_cst, bq::platform::memory_order::seq_cst)) {
                            break;
                        }
                    }
                }
            }
            ptr_ = nullptr;
            ref_count_ = nullptr;
        }

        T* ptr_;
        bq::platform::atomic<int64_t>* ref_count_;
    };

    // make_shared equivalent (simplified, no allocator support)
    template <typename T, typename... Ts>
    shared_ptr<T> make_shared(Ts&&... params)
    {
        void* addr = bq::aligned_alloc(alignof(T), sizeof(T));
        new (addr, bq::enum_new_dummy::dummy)T(bq::forward<Ts>(params)...);
        return shared_ptr<T>(static_cast<T*>(addr));
    }

    // make_shared with default initialization
    template <typename T>
    shared_ptr<T> make_shared()
    {
        void* addr = bq::aligned_alloc(alignof(T), sizeof(T));
        new (addr, bq::enum_new_dummy::dummy)T();
        return shared_ptr<T>(static_cast<T*>(addr));
    }
}
