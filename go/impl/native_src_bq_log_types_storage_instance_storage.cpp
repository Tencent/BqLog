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
#include "native_src_bq_log_types_storage_instance_storage.h"
#include "native_src_bq_common_bq_common.h"
#include "native_src_bq_log_global_log_vars.h"
#include "native_src_bq_log_types_buffer_normal_buffer.h"

namespace bq {
    namespace {
        constexpr uint64_t instance_magic = 0x42514C4F47494E53ULL;
        constexpr uint32_t instance_format_version = 1;
    }

    instance_storage* instance_storage::acquire()
    {
        auto& vars = log_global_vars::get();
        bq::platform::scoped_spin_lock lock(vars.instance_storage_lock_);
        if (vars.instance_storage_) {
            return vars.instance_storage_.get();
        }
        if (!bq::memory_map::is_platform_support()) {
            return nullptr;
        }
        const bq::string base = TO_ABSOLUTE_PATH("bqlog_mmap", 0);
        if (!file_manager::create_directory(base + "/locks")) {
            util::log_device_console(log_level::warning, "bqlog instance storage disabled: cannot create directory %s", (base + "/locks").c_str());
            return nullptr;
        }
        for (uint32_t k = 0;; ++k) {
            char name[48];
            snprintf(name, sizeof(name), "/locks/p%" PRIu32 ".lock", k);
            platform::platform_file_handle h;
            auto lock_result = platform::try_lock_instance_file((base + name).c_str(), h);
            if (lock_result == platform::instance_lock_result::error) {
                // e.g. the base directory is not writable: the loop would never succeed.
                util::log_device_console(log_level::warning, "bqlog instance storage disabled: cannot create lock file %s", (base + name).c_str());
                return nullptr;
            }
            if (lock_result == platform::instance_lock_result::held_by_other) {
                continue;
            }
            auto inst = bq::make_unique<instance_storage>();
            inst->index_ = k;
            char dir[32];
            snprintf(dir, sizeof(dir), "/p%" PRIu32, k);
            inst->root_ = base + dir;
            inst->lock_handle_ = h;
            file_manager::create_directory(inst->logs_dir());
            file_manager::create_directory(inst->meta_dir());
            // the head records this run's run_version / run_serial; it must persist,
            // so a normal_buffer backed by instance.mmap
            inst->head_buffer_ = bq::make_unique<normal_buffer>(sizeof(instance_head), inst->instance_file(), true);
            if (!inst->head_buffer_->is_memory_mapped()) {
                util::log_device_console(log_level::warning, "bqlog instance storage disabled: cannot memory map %s", inst->instance_file().c_str());
                // inst is dropped here: ~instance_storage releases the lock file handle
                return nullptr;
            }
            inst->head_ = static_cast<instance_head*>(inst->head_buffer_->data());
            auto& h_head = *inst->head_;
            if (h_head.magic != instance_magic || h_head.format_version != instance_format_version) {
                memset(&h_head, 0, sizeof(h_head));
                h_head.magic = instance_magic;
                h_head.format_version = instance_format_version;
            }
            ++h_head.run_serial;
            if (++h_head.run_version == 0) {
                h_head.run_version = 1; // 0 is the zero-init value everywhere; never a live version
            }
            cleanup_legacy_dirs(base);
            vars.instance_storage_ = bq::move(inst);
            return vars.instance_storage_.get();
        }
    }

    instance_storage* instance_storage::get()
    {
        auto& vars = log_global_vars::get();
        bq::platform::scoped_spin_lock lock(vars.instance_storage_lock_);
        return vars.instance_storage_.get();
    }

    bq::string instance_storage::logs_dir() const
    {
        return root_ + "/logs";
    }

    bq::string instance_storage::meta_dir() const
    {
        return root_ + "/meta";
    }

    bq::string instance_storage::instance_file() const
    {
        return root_ + "/instance.mmap";
    }

    bool instance_storage::owns(const bq::string& abs_path) const
    {
        // compare the normalized form: a path such as "<root>/a/../../elsewhere" must not pass
        // the prefix test just because it starts with root_
        const bq::string normalized = bq::file_manager::get_lexically_path(abs_path);
        return root_.size() > 0 && normalized.size() > root_.size() && normalized.find(root_) == 0
            && normalized.c_str()[root_.size()] == '/';
    }

    void instance_storage::remove(const bq::string& abs_path) const
    {
        if (!owns(abs_path)) {
            util::log_device_console(log_level::error, "refusing to remove %s, it is not owned by this bqlog instance", abs_path.c_str());
            return;
        }
        file_manager::remove_file_or_dir(abs_path);
    }

    void instance_storage::cleanup_legacy_dirs(const bq::string& base)
    {
        auto sub_names = file_manager::get_sub_dirs_and_files_name(base);
        for (const auto& name : sub_names) {
            if (!name.begin_with("mmap_")) {
                continue;
            }
            const bq::string dir = file_manager::combine_path(base, name);
            const bq::string lp_dir = file_manager::combine_path(dir, "lp");
            bool in_use = false;
            auto lp_names = file_manager::get_sub_dirs_and_files_name(lp_dir);
            for (const auto& lp_name : lp_names) {
                if (!lp_name.end_with(".mmap")) {
                    continue;
                }
                auto h = file_manager::instance().open_file(file_manager::combine_path(lp_dir, lp_name), file_open_mode_enum::read_write | file_open_mode_enum::exclusive);
                if (!h.is_valid()) {
                    in_use = true;
                    break;
                }
                file_manager::instance().close_file(h);
            }
            if (!in_use) {
                file_manager::remove_file_or_dir(dir);
            }
        }
    }

#if defined(BQ_UNIT_TEST)
    void instance_storage::test_begin_new_run()
    {
        auto& vars = log_global_vars::get();
        bq::platform::scoped_spin_lock lock(vars.instance_storage_lock_);
        if (!head_) {
            return;
        }
        ++head_->run_serial;
        if (++head_->run_version == 0) {
            head_->run_version = 1;
        }
    }

    void instance_storage::test_set_run(uint16_t version, uint64_t serial)
    {
        auto& vars = log_global_vars::get();
        bq::platform::scoped_spin_lock lock(vars.instance_storage_lock_);
        if (!head_) {
            return;
        }
        head_->run_version = version;
        head_->run_serial = serial;
    }
#endif

    instance_storage::~instance_storage()
    {
        platform::unlock_instance_file(lock_handle_);
    }
}
