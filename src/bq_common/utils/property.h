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
#include <inttypes.h>
#include <stddef.h>
#include "bq_common/bq_common_public_include.h"

namespace bq {
    class property {
        typedef hash_map<bq::string, bq::string> hash_type;

    public:
        bool load(const bq::string& file_name, int32_t base_dir_type);
        bool load(const bq::string& context);
        bool store(const bq::string& file_name, int32_t base_dir_type);
        void set(const bq::string& key, const bq::string& default_value = "");
        bq::string get(const bq::string& key, const bq::string& default_value = "");
        hash_type& maps() { return properties; }
        array<bq::string>& keys() { return key_list; }
        bq::string serialize() const;

        static bq::array<bq::tuple<bq::string, bq::string>> parse(const bq::string& context);

    private:
        hash_type properties;
        bq::array<bq::string> key_list;
    };
}
