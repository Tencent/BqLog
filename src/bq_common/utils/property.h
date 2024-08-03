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
#include <inttypes.h>
#include <stddef.h>
#include "bq_common/bq_common_public_include.h"

namespace bq {
    class property {
        typedef hash_map<string, string> hash_type;

    public:
        bool load(const string& file_name, bool in_sand_box);
        bool load(const string& context);
        bool store(const string& file_name, bool in_sand_box);
        void set(const string& key, const string& default_value = "");
        string get(const string& key, const string& default_value = "");
        hash_type& maps() { return properties; }
        array<string>& keys() { return key_list; }
        bq::string serialize() const;

        static array<tuple<string, string>> parse(const string& context);

    private:
        hash_type properties;
        array<string> key_list;
    };
}
