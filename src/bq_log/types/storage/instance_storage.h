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
    class normal_buffer;

    BQ_PACK_BEGIN
    struct alignas(8) instance_head {
        uint64_t magic; // 0x42514C4F47494E53 ("BQLOGINS")
        uint32_t format_version; // 1
        uint16_t run_version;
        uint16_t reserved0;
        uint64_t run_serial;
        uint8_t reserved[40];
    } BQ_PACK_END
    static_assert(sizeof(instance_head) == 64, "instance head size");

    // One claimed instance per process: a process lock file plus the instance's mmap root.
    // The lock is held by an open file description, so the kernel releases it when the
    // process dies, and two runtimes in one process exclude each other.
    class instance_storage {
    public:
        // claims an instance on first call; thread safe; returns nullptr if mmap is unsupported
        static instance_storage* acquire();
        // nullptr if not claimed yet (no recovery log created)
        static instance_storage* get();

        // A forked child closes its copy of the lock descriptor, which releases nothing the parent
        // holds; it then drops this claim with the rest of the runtime.
        void close_lock_after_fork();

        // the version number every recovery log_buffer of this run shares; bumped once per acquire()
        uint16_t run_version() const { return head_->run_version; }
        // monotonically increasing run counter; never wraps
        uint64_t run_serial() const { return head_->run_serial; }

        uint32_t index() const { return index_; }
        const bq::string& root() const { return root_; } // <base>/bqlog_mmap/p<k>
        bq::string logs_dir() const; // root/logs
        bq::string meta_dir() const; // root/meta
        bq::string instance_file() const; // root/instance.mmap
        bool owns(const bq::string& abs_path) const; // abs_path is inside root_
        void remove(const bq::string& abs_path) const; // only removes paths owned() by this instance

        // deletes unused mmap_<log> legacy directories under base
        static void cleanup_legacy_dirs(const bq::string& base);

#if defined(BQ_UNIT_TEST)
        // simulates a relaunch: bumps run_version / run_serial as acquire() does
        void test_begin_new_run();
        // sets the counters directly (wrap-around tests)
        void test_set_run(uint16_t version, uint64_t serial);
#endif

        // bq::make_unique instantiates placement new, so the ctor can't be private
        instance_storage() = default;
        ~instance_storage(); // unlock_instance_file
        // one instance per process: copying would duplicate the lock handle and the root path
        instance_storage(const instance_storage&) = delete;
        instance_storage& operator=(const instance_storage&) = delete;

    private:
        uint32_t index_ = 0;
        bq::string root_;
        platform::platform_file_handle lock_handle_ = platform::invalid_platform_file_handle;
        bq::unique_ptr<normal_buffer> head_buffer_;
        instance_head* head_ = nullptr;
    };
}
