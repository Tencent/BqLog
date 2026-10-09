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
#pragma once
#include "bq_common/bq_common.h"

namespace bq {
    // One claimed instance per process: a process lock file plus the instance's mmap root.
    // The lock is held by an open file description, so the kernel releases it when the
    // process dies, and two runtimes in one process exclude each other.
    class instance_storage {
    public:
        // claims an instance on first call; thread safe; returns nullptr if mmap is unsupported
        static instance_storage* acquire();
        // nullptr if not claimed yet (no recovery log created)
        static instance_storage* get();

        uint32_t index() const { return index_; }
        const bq::string& root() const { return root_; } // <base>/bqlog_mmap/p<k>
        bq::string logs_dir() const; // root/logs
        bq::string meta_dir() const; // root/meta
        bq::string instance_file() const; // root/instance.mmap
        bool owns(const bq::string& abs_path) const; // abs_path is inside root_
        void remove(const bq::string& abs_path) const; // only removes paths owned() by this instance

        // deletes unused mmap_<log> legacy directories under base
        static void cleanup_legacy_dirs(const bq::string& base);

        // bq::make_unique instantiates placement new, so the ctor can't be private
        instance_storage() = default;
        ~instance_storage(); // unlock_instance_file

    private:
        uint32_t index_ = 0;
        bq::string root_;
        platform::platform_file_handle lock_handle_ = platform::invalid_platform_file_handle;
    };
}
