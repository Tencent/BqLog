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
// author: eggdai
#include <stdint.h>
#include "bq_common/types/type_traits.h"
#include "bq_common/types/type_tools.h"

namespace bq {
    struct nullopt_t {
        explicit constexpr nullopt_t(int32_t) { }
    };
    constexpr nullopt_t nullopt { 0 };

    template <typename T>
    class optional;

    template <typename T>
    struct is_optional : public bq::false_type {
    };

    template <typename T>
    struct is_optional<bq::optional<T>> : public bq::true_type {
    };

    template <typename T>
    class optional {
        template <class>
        friend class optional;

        using value_type = typename bq::decay<T>::type;
        alignas(value_type) unsigned char storage_[sizeof(value_type)]; 
        bool has_value_;

    private:
        bq_forceinline value_type* get_pointer() noexcept
        {
            return reinterpret_cast<value_type*>(&storage_);
        }
    public:
        optional() noexcept
            : has_value_(false)
        {
            new(get_pointer()) value_type();
        }
        optional(nullopt_t) noexcept
            : optional()
        {
        }

        optional(const optional<T>& rhs);

        optional(optional<T>&& rhs) noexcept;

        template <typename U>
        optional(const optional<U>& rhs);

        template <typename U>
        optional(optional<U>&& rhs) noexcept;

        template <typename U, typename enable_if<!is_optional<typename decay<U>::type>::value>::type* = nullptr>
        optional(U&& value) noexcept;

        optional<T>& operator=(const optional<T>& rhs);

        optional<T>& operator=(optional<T>&& rhs) noexcept;

        template <typename U>
        optional<T>& operator=(const optional<U>& rhs);

        template <typename U>
        optional<T>& operator=(optional<U>&& rhs) noexcept;

        template <typename U, typename enable_if<!is_optional<typename decay<U>::type>::value>::type* = nullptr>
        optional<T>& operator=(U&& rhs) noexcept;

        ~optional()
        {
            clear_value();
        }

        inline void clear_value() noexcept
        {
            if (has_value_) {
                bq::object_destructor<value_type>::destruct(get_pointer());
                has_value_ = false;
            }
        }

        inline optional& operator=(nullopt_t) noexcept
        {
            clear_value();
            return *this;
        }

        inline T& operator*() &
        {
            assert(has_value_);
            return *get_pointer();
        }

        inline const T& operator*() const&
        {
            assert(has_value_);
            return *get_pointer();
        }

        inline explicit operator bool() const noexcept
        {
            return has_value_;
        }

        inline bool has_value() const noexcept
        {
            return has_value_;
        }

        inline T& value() &
        {
            return *get_pointer();
        }

        inline T&& value() &&
        {
            return bq::move(*get_pointer());
        }

        template <typename U>
        inline T value_or(U&& u) const&
        {
            if (has_value_)
                return *get_pointer();
            else
                return static_cast<T>(bq::forward<U>(u));
        }

        template <typename U>
        inline T value_or(U&& u) &&
        {
            if (has_value_)
                return bq::move(*get_pointer());
            else
                return static_cast<T>(bq::forward<U>(u));
        }
    };

    template <typename T>
    inline optional<T>::optional(const optional<T>& rhs)
        : has_value_(rhs.has_value_)
    {
        if (rhs.has_value_) {
            new (get_pointer(), bq::enum_new_dummy::dummy) value_type(rhs.*get_pointer());
        }
    }

    template <typename T>
    inline optional<T>::optional(optional<T>&& rhs) noexcept
        : has_value_(rhs.has_value_)
    {
        if (rhs.has_value_) {
            new (get_pointer(), bq::enum_new_dummy::dummy) value_type(bq::move(rhs.*get_pointer()));
            rhs.clear_value();
        }
    }

    template <typename T>
    template <typename U>
    inline optional<T>::optional(const optional<U>& rhs)
        : has_value_(rhs.has_value_)
    {
        if (rhs.has_value_) {
            new (get_pointer(), bq::enum_new_dummy::dummy) value_type(rhs.*get_pointer());
        }
    }

    template <typename T>
    template <typename U>
    inline optional<T>::optional(optional<U>&& rhs) noexcept
        : has_value_(rhs.has_value_)
    {
        if (rhs.has_value_) {
            new (get_pointer(), bq::enum_new_dummy::dummy) value_type(bq::move(rhs.*get_pointer()));
            rhs.clear_value();
        }
    }

    template <typename T>
    template <typename U, typename enable_if<!is_optional<typename decay<U>::type>::value>::type*>
    inline optional<T>::optional(U&& value) noexcept
        : has_value_(true)
    {
        new (get_pointer(), bq::enum_new_dummy::dummy) value_type(forward<U>(value));
    }

    template <typename T>
    inline optional<T>& optional<T>::operator=(const optional<T>& rhs)
    {
        if (has_value_) {
            if (rhs.has_value_) {
                *get_pointer() = rhs.*get_pointer();
            } else {
                clear_value();
                has_value_ = false;
            }
        } else {
            if (rhs.has_value_) {
                new (get_pointer(), bq::enum_new_dummy::dummy) value_type(rhs.*get_pointer());
                has_value_ = true;
            }
        }
        return *this;
    }

    template <typename T>
    inline optional<T>& optional<T>::operator=(optional<T>&& rhs) noexcept
    {
        if (has_value_) {
            if (rhs.has_value_) {
                *get_pointer() = bq::move(rhs.*get_pointer());
                rhs.clear_value();
            } else {
                clear_value();
                has_value_ = false;
            }
        } else {
            if (rhs.has_value_) {
                new (get_pointer(), bq::enum_new_dummy::dummy) value_type(bq::move(rhs.*get_pointer()));
                has_value_ = true;
                rhs.clear_value();
            }
        }
        return *this;
    }

    template <typename T>
    template <typename U>
    inline optional<T>& optional<T>::operator=(const optional<U>& rhs)
    {
        if (has_value_) {
            if (rhs.has_value_) {
                *get_pointer() = rhs.*get_pointer();
            } else {
                clear_value();
                has_value_ = false;
            }
        } else {
            if (rhs.has_value_) {
                new (get_pointer(), bq::enum_new_dummy::dummy) value_type(rhs.*get_pointer());
                has_value_ = true;
            }
        }
        return *this;
    }

    template <typename T>
    template <typename U>
    inline optional<T>& optional<T>::operator=(optional<U>&& rhs) noexcept
    {
        if (has_value_) {
            if (rhs.has_value_) {
                *get_pointer() = bq::move(rhs.*get_pointer());
                rhs.clear_value();
            } else {
                clear_value();
                has_value_ = false;
            }
        } else {
            if (rhs.has_value_) {
                new (get_pointer(), bq::enum_new_dummy::dummy) value_type(bq::move(rhs.*get_pointer()));
                has_value_ = true;
                rhs.clear_value();
            }
        }
        return *this;
    }

    template <typename T>
    template <typename U, typename enable_if<!is_optional<typename decay<U>::type>::value>::type*>
    inline optional<T>& optional<T>::operator=(U&& rhs) noexcept
    {
        if (has_value_) {
            *get_pointer() = bq::move(rhs);
        } else {
            new (get_pointer(), bq::enum_new_dummy::dummy) value_type(bq::move(rhs));
            has_value_ = true;
        }
        return *this;
    }

    template <typename T>
    optional<T> make_optional(T&& __t)
    {
        return optional<T> { bq::forward<T>(__t) };
    }
}
