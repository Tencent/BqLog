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
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include "bq_common/bq_common.h"

namespace bq {
    static bool parse_boolean_value(bool& out, const char* str)
    {
        size_t true_token_size = sizeof("true") - 1;
        size_t false_token_size = sizeof("false") - 1;
        if (strncmp("true", str, true_token_size) == 0) {
            out = true;
            return true;
        } else if (strncmp("false", str, false_token_size) == 0) {
            out = false;
            return true;
        }
        return false;
    }

    static bool is_decimal(const char* string, size_t length)
    {
        if (length > 1 && string[0] == '0' && string[1] != '.') {
            return false;
        }
        if (length > 2 && !strncmp(string, "-0", 2) && string[2] != '.') {
            return false;
        }
        while (length--) {
            if (strchr("xX", string[length])) {
                return false;
            }
        }
        return true;
    }

    static bool parse_number_value(double& out, const char* str)
    {

        switch (*str) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            break;
        default:
            return false;
            break;
        }
        char* end;
        double number = 0;
        errno = 0;
        number = strtod(str, &end);
        if (errno == ERANGE && (number <= -HUGE_VAL || number >= HUGE_VAL)) {
            return false;
        }
        if ((errno && errno != ERANGE) || !is_decimal(str, static_cast<size_t>(end - str))) {
            return false;
        }
        out = number;
        return true;
    }

    static bool parse_null_value(const char* str)
    {
        size_t token_size = sizeof("null") - 1;
        if (strncmp("null", str, token_size) == 0) {

            return true;
        }
        return false;
    }

    static bool parse_array_key(string str)
    {
        return str.find("[]") != string::npos;
    }

    static bool parse_array_value(string str)
    {
        if (str.size() < 2)
            return false;
        return str[0] == '[' && str[str.size() - 1] == ']';
    }

    static property_value parse_to_property_value(property_value& root, string key, const string& value)
    {
        property_value pv;
        bool bvalue = false;
        double dvalue = 0;
        int64_t ivalue = 0;
        if (parse_array_key(key.c_str())) {
            key = key.substr(0, key.size() - 2); // remove []
            if (!root[key].is_array())
                root[key] = property_value(enum_property_value_type::array);
            root[key].add_array_item(parse_to_property_value(root, "", value));
            pv = property_value(enum_property_value_type::array);
        } else if (parse_array_value(value.c_str())) {
            if (!root[key].is_array())
                root[key] = property_value(enum_property_value_type::array);

            if (value.size() >= 3) {
                string new_value = value.substr(1, value.size() - 2);
                auto values = new_value.split(",");
                for (auto& cell_value : values) {
                    root[key].add_array_item(parse_to_property_value(root, "", cell_value));
                }
            }
            pv = property_value(enum_property_value_type::array);
        } else if (parse_number_value(dvalue, value.c_str())) {
            if (fmod(dvalue, 1.0) == 0.0) {
                ivalue = (int64_t)(dvalue);
                pv = ivalue;
            } else {
                pv = dvalue;
            }
        } else if (parse_boolean_value(bvalue, value.c_str())) {
            pv = bvalue;
        } else if (parse_null_value(value.c_str())) {
            pv = property_value(enum_property_value_type::null);
        } else {
            pv = value;
        }
        return pv;
    }

    static void trans_parson_to_property_value(property_value& root, const string key, const string& value)
    {
        array<string> keys = key.split(".");
        switch (keys.size()) {
        case 0:
            return;
            break;
        case 1: {
            string new_key = keys[0];
            property_value pv = move(parse_to_property_value(root, key, value));
            if (!pv.is_array()) {
                root[new_key] = move(pv);
            }
        } break;
        default:
            // retain the following content for new key
            // old key=a.b.c -> new key = b.c
            string new_key = key.substr(keys[0].size() + 1, key.size() - keys[0].size() - 1);
            trans_parson_to_property_value(root[keys[0]], new_key, value);
            break;
        }
    }

    property_value property_value::create_from_string(const bq::string& property_string)
    {
        property_value pv_root;
        property pbase;
        pbase.load(property_string);
        auto& maps = pbase.maps();
        for (auto& cell : maps) {
            trans_parson_to_property_value(pv_root, cell.key(), cell.value());
        }
        return pv_root;
    }

    static string serialize_recursive(string key, const bq::property_value& object)
    {
        string line;
        if (object.is_object()) {
            auto keys = object.get_object_key_set();
            for (auto obj_iter = keys.begin(); obj_iter != keys.end(); ++obj_iter) {
                string new_key = (*obj_iter);
                if (!key.is_empty())
                    new_key = key + "." + new_key;

                auto& value = object[*obj_iter];
                if (!value.is_object())
                    line += new_key + "=";
                line += serialize_recursive(new_key, value);
                if (!value.is_object())
                    line += "\n";
            }
        } else if (object.is_array()) {
            line += "[";
            for (typename property_value::array_type::size_type i = 0; i < object.array_size(); ++i) {
                auto& value = object[i];
                line += serialize_recursive("", value);
                if (object.array_size() - i > 1)
                    line += ",";
            }
            line += "]";
        } else {
            if (object.is_null()) {
                line += "null";
            } else if (object.is_bool()) {
                line += (bool)object ? "true" : "false";
            } else if (object.is_decimal()) {
                char fmt[32] = {};
                snprintf(fmt, sizeof(fmt), "%lf", (double)object);
                line += fmt;
            } else if (object.is_integral()) {
                char fmt[32] = {};
                snprintf(fmt, sizeof(fmt), "%" PRId64, (int64_t)object);
                line += fmt;
            } else if (object.is_string()) {
                string value = (string)object;
                value = value.replace("\n", "\\n");
                value = value.replace("=", "\\=");
                value = value.replace(":", "\\:");
                line += value;
            }
        }
        return line;
    }

    bq::string property_value::serialize() const
    {
        string lines = serialize_recursive("", *this);
        return lines;
    }

    void property_value::clear_data()
    {
        switch (type_) {
        case enum_property_value_type::boolean:
            break;
        case enum_property_value_type::decimal:
            break;
        case enum_property_value_type::integral:
            break;
        case enum_property_value_type::string:
            (&as_string())->~string_type();
            break;
        case enum_property_value_type::array:
            (&as_array())->~array_type();
            break;
        case enum_property_value_type::object:
            (&as_object())->~object_type();
            break;
        default:
            break;
        }
        type_ = enum_property_value_type::null;
    }

    void property_value::copy_data_from(const property_value& rhs)
    {
        type_ = rhs.type_;
        switch (type_) {
        case enum_property_value_type::boolean:
            *this = (bool_type)rhs;
            break;
        case enum_property_value_type::decimal:
            *this = (decimal_type)rhs;
            break;
        case enum_property_value_type::integral:
            *this = (integral_type)rhs;
            break;
        case enum_property_value_type::string:
            *this = (string_type)rhs;
            break;
        case enum_property_value_type::array: {
            auto& src_array = rhs.as_array(true);
            auto& dest_array = as_array();
            new (&dest_array, bq::enum_new_dummy::dummy) array_type();
            for (auto& item : src_array) {
                auto& jv = *item;
                dest_array.push_back(new property_value(jv));
            }
            break;
        }
        case enum_property_value_type::object: {
            auto& src_obj = rhs.as_object(true);
            auto& dest_obj = as_object();
            new (&dest_obj, bq::enum_new_dummy::dummy) object_type();
            for (auto& item : src_obj) {
                auto& jk = item.key();
                auto& jv = *item.value();
                dest_obj.add(jk, new property_value(jv));
            }
            break;
        }
        default:
            break;
        }
    }

    void property_value::copy_data_from(property_value&& rhs)
    {
        type_ = rhs.type_;
        switch (type_) {
        case enum_property_value_type::boolean:
            as_bool() = bq::move(rhs.as_bool());
            break;
        case enum_property_value_type::decimal:
            as_decimal() = bq::move(rhs.as_decimal());
            break;
        case enum_property_value_type::integral:
            as_integral() = bq::move(rhs.as_integral());
            break;
        case enum_property_value_type::string:
            new (&as_string(), bq::enum_new_dummy::dummy) string_type(bq::move(rhs.as_string()));
            break;
        case enum_property_value_type::array:
            new (&as_array(), bq::enum_new_dummy::dummy) array_type(bq::move(rhs.as_array()));
            break;
        case enum_property_value_type::object:
            new (&as_object(), bq::enum_new_dummy::dummy) object_type(bq::move(rhs.as_object()));
            break;
        default:
            break;
        }
    }

    bool property_value::check_and_switch_type(enum_property_value_type type, bool skip_check_type)
    {
        assert(skip_check_type || type_ == enum_property_value_type::boolean);
        if (type_ != type) {
            clear_data();
            type_ = type;
            return false;
        }
        return true;
    }

    const bq::property_value& property_value::get_null_value()
    {
        return common_global_vars::get().null_property_value_;
    }

    property_value& property_value::add_null_array_item()
    {
        if (!is_array()) {
            return this->operator[](0);
        }
        return this->operator[](as_array(true).size());
    }

    property_value& property_value::add_null_object_item(const bq::string& key)
    {
        return this->operator[](key);
    }

    property_value::property_value(enum_property_value_type value_type)
        : type_(value_type)
    {
        init_debug_ptrs();
        switch (type_) {
        case enum_property_value_type::boolean:
            as_bool() = false;
            break;
        case enum_property_value_type::decimal:
            as_decimal() = 0.0f;
            break;
        case enum_property_value_type::integral:
            as_integral() = 0;
            break;
        case enum_property_value_type::string:
            new (&as_string(), bq::enum_new_dummy::dummy) string_type("");
            break;
        case enum_property_value_type::array:
            new (&as_array(), bq::enum_new_dummy::dummy) array_type();
            break;
        case enum_property_value_type::object:
            new (&as_object(), bq::enum_new_dummy::dummy) object_type();
            break;
        default:
            break;
        }
    }

    property_value::property_value(const property_value& rhs)
    {
        init_debug_ptrs();
        copy_data_from(rhs);
    }

    property_value::property_value(property_value&& rhs) noexcept
        : type_(rhs.type_)
    {
        init_debug_ptrs();
        copy_data_from(bq::move(rhs));
    }

    property_value::~property_value()
    {
        clear_data();
    }

    const property_value& property_value::operator[](const bq::string& name) const
    {
        if (!is_object()) {
            return get_null_value();
        }
        auto iter = as_object(true).find(name);
        if (iter) {
            return *iter->value().get();
        }
        return get_null_value();
    }

    property_value& property_value::operator[](const bq::string& name)
    {
        if (!is_object()) {
            check_and_set_template(bq::move(object_type()), as_object(true), true, enum_property_value_type::object);
            return *as_object().add(name, new property_value())->value();
        }
        auto iter = as_object().find(name);
        if (iter == as_object().end()) {
            iter = as_object().add(name, new property_value());
        }
        return *iter->value();
    }

    property_value& property_value::add_array_item(const bq::string& key, array<property_value>& value)
    {
        property_value& jarray = (*this)[key];
        jarray.set_array(value);
        return jarray;
    }

    void property_value::set_array(array<property_value>& items)
    {
        for (auto& item : items) {
            add_array_item(item);
        }
    }

    void property_value::erase_object_item(const bq::string& key)
    {
        as_object().erase(key);
    }

    void property_value::erase_array_item(property_value::array_type::size_type idx)
    {
        if (idx == (property_value::array_type::size_type)-1) {
            as_array().clear();
            return;
        }
        as_array().erase(as_array().begin() + idx);
    }

    void property_value::clear_array_item()
    {
        if (is_null())
            add_null_array_item();
        as_array().clear();
    }

    const property_value& property_value::operator[](typename array_type::size_type idx) const
    {
        if (!is_array()) {
            return get_null_value();
        }
        if (idx < as_array(true).size()) {
            return *as_array(true)[idx];
        }
        return get_null_value();
    }

    property_value& property_value::operator[](typename array_type::size_type idx)
    {
        if (!is_array()) {
            check_and_set_template(bq::move(array_type()), as_array(true), true, enum_property_value_type::array);
        }
        as_array().set_capacity(idx + 1);
        for (typename array_type::size_type i = as_array().size(); i <= idx; ++i) {
            as_array(true).push_back(new property_value());
        }
        return *as_array(true)[idx];
    }

    bool property_value::has_object_key(const bq::string& name) const
    {
        if (!is_object()) {
            return false;
        }
        return !(*this)[name].is_null();
    }

    bq::array<bq::string> property_value::get_object_key_set() const
    {
        bq::array<bq::string> result;
        if (!is_object()) {
            return result;
        }
        auto iter = as_object(true).begin();
        for (; iter != as_object(true).end(); ++iter) {
            result.push_back(iter->key());
        }
        return result;
    }

    typename property_value::array_type::size_type property_value::array_size() const
    {
        if (!is_array()) {
            return 0;
        }
        return as_array(true).size();
    }

    typename property_value::object_type::size_type property_value::object_size() const
    {
        if (!is_object()) {
            return 0;
        }
        return as_object(true).size();
    }

    property_value& property_value::operator=(const property_value& rhs)
    {
        clear_data();
        copy_data_from(rhs);
        return *this;
    }

    property_value& property_value::operator=(property_value&& rhs) noexcept
    {
        clear_data();
        copy_data_from(bq::move(rhs));
        return *this;
    }

    property_value& property_value::operator=(bool_type value)
    {
        check_and_set_template(value, as_bool(true), true, enum_property_value_type::boolean);
        return *this;
    }

    property_value::operator decimal_type() const
    {
        return as_decimal();
    }
    property_value& property_value::operator=(decimal_type value)
    {
        check_and_set_template(value, as_decimal(true), true, enum_property_value_type::decimal);
        return *this;
    }

    property_value::operator integral_type() const
    {
        return as_integral();
    }
    property_value& property_value::operator=(integral_type value)
    {
        check_and_set_template(value, as_integral(true), true, enum_property_value_type::integral);
        return *this;
    }

    property_value::operator string_type() const
    {
        return as_string();
    }
    property_value& property_value::operator=(const char* value)
    {
        string str = value;
        check_and_set_template(move(str), as_string(true), true, enum_property_value_type::string);
        return *this;
    }
    property_value& property_value::operator=(const string_type& value)
    {
        check_and_set_template(value, as_string(true), true, enum_property_value_type::string);
        return *this;
    }
    property_value& property_value::operator=(string_type&& value)
    {
        check_and_set_template(bq::move(value), as_string(true), true, enum_property_value_type::string);
        return *this;
    }
}
