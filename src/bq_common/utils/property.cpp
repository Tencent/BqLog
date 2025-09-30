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
#include "property.h"
#include "file_manager.h"

namespace bq {
    bool property::load(const string& file_name, int32_t base_dir_type)
    {
        string path = TO_ABSOLUTE_PATH(file_name, base_dir_type);
        string context = file_manager::read_all_text(path);
        return load(context);
    }

    bool property::load(const string& context)
    {
        auto vv = parse(context);
        for (auto& cell : vv) {
            set(bq::get<0>(cell), bq::get<1>(cell));
        }
        return !properties.is_empty();
    }

    static array<string> find_split(string& line, const char& split_float)
    {
        // special case
        //==2 =>  key:= value:2
        //=== =>  key:= value:=
        //==  =>   novalid
        //=   =>   novalid
        array<string> kv;
        for (size_t i = 1; i < line.size() - 1; i++) {
            if (line[i] == split_float && line[i - 1] != '\\') {
                kv.push_back(line.substr(0, i));
                kv.push_back(line.substr(i + 1, line.size() - i - 1));
                break;
            }
        }
        return move(kv);
    }

    array<tuple<string, string>> property::parse(const string& context)
    {
        array<tuple<string, string>> vv;
        string file_context = context;
        file_context = file_context.replace("\r\n", "\n");
        file_context = file_context.replace("\t", "");
        array<string> lines = file_context.split("\n");
        for (size_t i = 0; i < lines.size(); i++) {
            auto& line = lines[i];
            line = line.trim();
            if (line.is_empty() || line[0] == '#' || line[0] == '!') {
                continue;
            }

            array<string> kv = find_split(line, '=');
            if (kv.size() < 2)
                kv = find_split(line, ':');
            if (kv.size() == 2) {
                string key = kv[0].trim();
                string value = line.substr(key.size() + 1, line.size() - key.size() - 1).trim();
                if (value.is_empty())
                    continue;
                while (true) {
                    if (value[value.size() - 1] != '\\') {
                        break;
                    }
                    i++;
                    if (i < lines.size()) {
                        value = value.substr(0, value.size() - 1);
                        value += lines[i].trim();
                    }
                }
                value = value.replace("\\n", "\n");
                value = value.replace("\\=", "=");
                value = value.replace("\\:", ":");
                vv.push_back(make_tuple(key, value));
            }
        }
        return vv;
    }

    bool property::store(const string& file_name, int32_t base_dir_type)
    {
        string lines;
        for (auto& cell : properties) {
            lines += cell.key() + "=" + cell.value() + "\n";
        }
        string path = TO_ABSOLUTE_PATH(file_name, base_dir_type);
        file_manager::instance().write_all_text(path, lines);
        return true;
    }

    string property::get(const string& key, const string& default_value)
    {
        auto it = properties.find(key);
        if (it != properties.end()) {
            return it->value();
        }
        return default_value;
    }

    void property::set(const string& key, const string& default_value)
    {
        if (properties.find(key) == properties.end())
            key_list.push_back(key);
        properties[key] = default_value;
    }

    bq::string property::serialize() const
    {
        string lines;
        auto itr = properties.begin();
        for (; itr != properties.end(); itr++) {
            string value = itr->value();
            value = value.replace("\n", "\\n");
            value = value.replace("=", "\\=");
            value = value.replace(":", "\\:");
            lines += itr->key() + "=" + value + "\n";
        }
        return lines;
    }
}
