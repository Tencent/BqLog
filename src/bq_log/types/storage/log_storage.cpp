/* Copyright (C) 2026 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
#include "bq_log/types/storage/log_storage.h"
#include "bq_common/bq_common.h"
#include "bq_log/types/storage/instance_storage.h"

namespace bq {
    bq::unique_ptr<log_storage> log_storage::create(const bq::string& log_name, bool need_recovery)
    {
        if (!need_recovery) {
            return bq::unique_ptr<log_storage>();
        }
        auto inst = instance_storage::acquire();
        if (!inst) {
            return bq::unique_ptr<log_storage>();
        }
        auto storage = bq::make_unique<log_storage>();
        storage->dir_ = inst->logs_dir() + "/" + escape_name(log_name);
        file_manager::create_directory(storage->dir_);
        return storage;
    }

    bq::string log_storage::escape_name(const bq::string& name)
    {
        static const char* hex_chars = "0123456789ABCDEF";
        if (name.is_empty()) {
            return "%";
        }
        bq::string result;
        for (size_t i = 0; i < name.size(); ++i) {
            char c = name.c_str()[i];
            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-') {
                result += bq::string(&c, 1);
            } else if (c >= 'A' && c <= 'Z') {
                result += "^";
                result += bq::string(&c, 1);
            } else {
                result += "%";
                char hex[2] = { hex_chars[(static_cast<uint8_t>(c) >> 4) & 0x0F], hex_chars[static_cast<uint8_t>(c) & 0x0F] };
                result += bq::string(hex, 2);
            }
        }
        return result;
    }

    bq::string log_storage::lp_path() const
    {
        return dir_ + "/lp.mmap";
    }

    bq::string log_storage::hp_dir() const
    {
        return dir_ + "/hp";
    }

    bq::string log_storage::hp_path(uint64_t index) const
    {
        char name[32];
        snprintf(name, sizeof(name), "/%" PRIu64 ".mmap", index);
        return hp_dir() + name;
    }

    bq::string log_storage::os_dir() const
    {
        return dir_ + "/os";
    }

    bq::string log_storage::os_path(uint64_t index) const
    {
        char name[32];
        snprintf(name, sizeof(name), "/%" PRIu64 ".mmap", index);
        return os_dir() + name;
    }

    bq::string log_storage::appender_path(const bq::string& appender_name) const
    {
        return dir_ + "/appenders/" + escape_name(appender_name) + ".mmap";
    }

    void log_storage::remove(const bq::string& abs_path) const
    {
        auto inst = instance_storage::get();
        if (!inst) {
            util::log_device_console(log_level::error, "refusing to remove %s, no bqlog instance claimed", abs_path.c_str());
            return;
        }
        inst->remove(abs_path);
    }

}
