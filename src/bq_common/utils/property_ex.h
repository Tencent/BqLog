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
// author: eggdai

#include "bq_common/bq_common_public_include.h"
#include "bq_common/utils/utility_types.h"

namespace bq {
    enum class enum_property_value_type {
        boolean_type,
        decimal_type,
        integral_type,
        string_type,
        array_type,
        object_type,
        null_type,
        invalid_type
    };

    class property_value {
    private:
        template <typename T>
        struct property_value_trait {
        private:
            using decay_type = bq::decay_t<T>;
            static constexpr bool is_integer = bq::is_same<decay_type, char>::value
                || bq::is_same<decay_type, unsigned char>::value
                || bq::is_same<decay_type, char16_t>::value
                || bq::is_same<decay_type, char32_t>::value
                || bq::is_same<decay_type, int8_t>::value
                || bq::is_same<decay_type, uint8_t>::value
                || bq::is_same<decay_type, int16_t>::value
                || bq::is_same<decay_type, uint16_t>::value
                || bq::is_same<decay_type, int32_t>::value
                || bq::is_same<decay_type, uint32_t>::value
                || bq::is_same<decay_type, int64_t>::value
                || bq::is_same<decay_type, uint64_t>::value;
            static constexpr bool is_decimal = bq::is_same<decay_type, float>::value
                || bq::is_same<decay_type, double>::value;

        public:
            constexpr static enum_property_value_type value = bq::condition_value<bq::is_same<decay_type, bool>::value, enum_property_value_type, enum_property_value_type::boolean_type, bq::condition_value<is_integer, enum_property_value_type, enum_property_value_type::integral_type, bq::condition_value<is_decimal, enum_property_value_type, enum_property_value_type::decimal_type, bq::condition_value<bq::is_same<decay_type, bq::string>::value, enum_property_value_type, enum_property_value_type::string_type, bq::condition_value<bq::is_null_pointer<decay_type>::value, enum_property_value_type, enum_property_value_type::null_type, enum_property_value_type::invalid_type>::value>::value>::value>::value>::value;
        };

    public:
        typedef bool bool_type;
        typedef double decimal_type;
        typedef int64_t integral_type;
        typedef bq::string string_type;
        typedef bq::array<bq::unique_ptr<property_value>> array_type;
        typedef bq::hash_map<string, bq::unique_ptr<bq::property_value>> object_type;

    private:
        template <typename... Args>
        struct max_size_st;

        template <typename T, typename... Rest>
        struct max_size_st<T, Rest...> {
            static constexpr size_t size = sizeof(T) > max_size_st<Rest...>::size ? sizeof(T) : max_size_st<Rest...>::size;
        };

        template <typename First>
        struct max_size_st<First> {
            static constexpr size_t size = sizeof(First);
        };
        // property_object
        static constexpr size_t size_ = max_size_st<bool_type, decimal_type, int64_t, string_type, array_type, object_type>::size;

    private:
        enum_property_value_type type_;
        // union do not support non-POD member.
        bq::aligned_type<char[size_], 8> data_;

#ifndef NDEBUG
        const bool* _bool_ptr_debug;
        const decimal_type* _decimal_ptr_debug;
        const int64_t* _integral_ptr_debug;
        const string_type* _string_ptr_debug;
        const array_type* _array_ptr_debug;
        const object_type* _object_ptr_debug;
#endif
    private:
        void clear_data();
        void copy_data_from(const property_value& rhs);
        void copy_data_from(property_value&& rhs);

        const bool_type& as_bool(bool skip_check_type = false) const
        {
            assert(skip_check_type || type_ == enum_property_value_type::boolean_type);
            return *(const bool_type*)(data_.get());
        }
        bool_type& as_bool(bool skip_check_type = false)
        {
            assert(skip_check_type || type_ == enum_property_value_type::boolean_type);
            return *(bool_type*)(data_.get());
        }
        const decimal_type& as_decimal(bool skip_check_type = false) const
        {
            assert(skip_check_type || type_ == enum_property_value_type::decimal_type);
            return *(const decimal_type*)(data_.get());
        }
        decimal_type& as_decimal(bool skip_check_type = false)
        {
            assert(skip_check_type || type_ == enum_property_value_type::decimal_type);
            return *(decimal_type*)(data_.get());
        }
        const int64_t& as_integral(bool skip_check_type = false) const
        {
            assert(skip_check_type || type_ == enum_property_value_type::integral_type);
            return *(const int64_t*)(data_.get());
        }
        int64_t& as_integral(bool skip_check_type = false)
        {
            assert(skip_check_type || type_ == enum_property_value_type::integral_type);
            return *(int64_t*)(data_.get());
        }
        const string_type& as_string(bool skip_check_type = false) const
        {
            assert(skip_check_type || type_ == enum_property_value_type::string_type);
            return *(const string_type*)(data_.get());
        }
        string_type& as_string(bool skip_check_type = false)
        {
            assert(skip_check_type || type_ == enum_property_value_type::string_type);
            return *(string_type*)(data_.get());
        }
        const array_type& as_array(bool skip_check_type = false) const
        {
            assert(skip_check_type || type_ == enum_property_value_type::array_type);
            return *(const array_type*)(data_.get());
        }
        array_type& as_array(bool skip_check_type = false)
        {
            assert(skip_check_type || type_ == enum_property_value_type::array_type);
            return *(array_type*)(data_.get());
        }
        const object_type& as_object(bool skip_check_type = false) const
        {
            assert(skip_check_type || type_ == enum_property_value_type::object_type);
            return *(const object_type*)(data_.get());
        }
        object_type& as_object(bool skip_check_type = false)
        {
            assert(skip_check_type || type_ == enum_property_value_type::object_type);
            return *(object_type*)(data_.get());
        }

        bool check_and_switch_type(enum_property_value_type type, bool skip_check_type);

        template <typename SRC, typename TARGET>
        void check_and_set_template(SRC&& value, TARGET& target, bool skip_check_type, enum_property_value_type type)
        {
            if (!check_and_switch_type(type, skip_check_type)) {
                new (&target, bq::enum_new_dummy::dummy) TARGET(bq::forward<SRC>(value));
            } else {
                target = bq::forward<SRC>(value);
            }
        }

        static const property_value& get_null_value();

        property_value& add_null_array_item();

        property_value& add_null_object_item(const bq::string& key);

        void init_debug_ptrs()
        {
#ifndef NDEBUG
            _bool_ptr_debug = &as_bool(true);
            _decimal_ptr_debug = &as_decimal(true);
            _integral_ptr_debug = &as_integral(true);
            _string_ptr_debug = &as_string(true);
            _array_ptr_debug = &as_array(true);
            _object_ptr_debug = &as_object(true);
#endif
        }

    public:
        static property_value create_from_string(const bq::string& property_string);

        property_value(enum_property_value_type value_type = enum_property_value_type::null_type);

        property_value(const property_value& rhs);

        property_value(property_value&& rhs) noexcept;

        ~property_value();

        static const property_value& null()
        {
            return get_null_value();
        }

        property_value& operator=(const property_value& rhs);

        property_value& operator=(property_value&& rhs) noexcept;

        bool is_null() const
        {
            return get_type() == enum_property_value_type::null_type;
        }

        bool is_bool() const
        {
            return get_type() == enum_property_value_type::boolean_type;
        }

        bool is_decimal() const
        {
            return get_type() == enum_property_value_type::decimal_type;
        }

        bool is_integral() const
        {
            return get_type() == enum_property_value_type::integral_type;
        }

        bool is_string() const
        {
            return get_type() == enum_property_value_type::string_type;
        }

        bool is_array() const
        {
            return get_type() == enum_property_value_type::array_type;
        }

        bool is_object() const
        {
            return get_type() == enum_property_value_type::object_type;
        }

        operator string_type() const;

        explicit operator bool_type() const { return as_bool(); }
        explicit operator decimal_type() const;
        explicit operator integral_type() const;
        explicit operator uint64_t() const { return (uint64_t)as_integral(); };
        explicit operator int32_t() const { return (int32_t)as_integral(); };
        explicit operator uint32_t() const { return (uint32_t)as_integral(); };

        property_value& operator=(bool_type value);
        property_value& operator=(decimal_type value);
        property_value& operator=(integral_type value);
        property_value& operator=(const char* value);
        property_value& operator=(const string_type& value);
        property_value& operator=(string_type&& value);

        const property_value& operator[](const bq::string& name) const;
        property_value& operator[](const bq::string& name);

        const property_value& operator[](const typename array_type::size_type idx) const;
        property_value& operator[](const typename array_type::size_type idx);

        template <typename T>
        bq::enable_if_t<property_value_trait<T>::value == enum_property_value_type::null_type, bq::property_value&> operator=(T value)
        {
            (void)value;
            clear_data();
            type_ = enum_property_value_type::null_type;
        }

        template <typename T>
            bq::enable_if_t < property_value_trait<T>::value<enum_property_value_type::invalid_type, bq::property_value&> add_array_item(T&& value)
        {
            auto& new_item = add_null_array_item();
            new_item = bq::forward<T>(value);
            return new_item;
        }
        template <typename T>
        bq::enable_if_t<bq::is_same<bq::decay_t<T>, property_value>::value, bq::property_value&> add_array_item(T&& value)
        {
            auto& new_item = add_null_array_item();
            new_item = bq::forward<T>(value);
            return new_item;
        }

        template <typename T>
            bq::enable_if_t < property_value_trait<T>::value<enum_property_value_type::invalid_type, bq::property_value&> add_array_item(const bq::string& key, T&& value)
        {
            auto& obj = (*this)[key];
            obj.add_array_item(value);
            return obj;
        }

        template <typename T>
        bq::enable_if_t<bq::is_same<bq::decay_t<T>, property_value>::value, bq::property_value&> add_array_item(const bq::string& key, T&& value)
        {
            auto& obj = (*this)[key];
            obj.add_array_item(value);
            return obj;
        }

        property_value& add_array_item(const bq::string& key, array<property_value>& value);
        void set_array(array<property_value>& items);
        void erase_array_item(array_type::size_type idx);
        void clear_array_item();

        template <typename T>
            bq::enable_if_t < property_value_trait<T>::value<enum_property_value_type::invalid_type, bq::property_value&> add_object_item(const bq::string& key, T&& value)
        {
            auto& new_item = add_null_object_item(key);
            new_item = bq::forward<T>(value);
            return new_item;
        }
        template <typename T>
        bq::enable_if_t<bq::is_same<bq::decay_t<T>, property_value>::value, bq::property_value&> add_object_item(const bq::string& key, T&& value)
        {
            auto& new_item = add_null_object_item(key);
            new_item = bq::forward<T>(value);
            return new_item;
        }
        void erase_object_item(const bq::string& key);

        bool has_object_key(const bq::string& name) const;

        bq::array<bq::string> get_object_key_set() const;

        typename array_type::size_type array_size() const;

        typename object_type::size_type object_size() const;

        enum_property_value_type get_type() const { return type_; };

        bq::string serialize() const;
    };
}
