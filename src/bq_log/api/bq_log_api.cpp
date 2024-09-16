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
#include <string.h>
#include <stdio.h>
#include "bq_common/bq_common.h"
#include "bq_log/bq_log.h"
#include "bq_log/log/log_types.h"
#include "bq_log/log/log_manager.h"
#include "bq_log/log/log_imp.h"
#include "bq_log/log/appender/appender_file_base.h"
#include "bq_log/log/appender/appender_console.h"
#include "bq_log/log/decoder/appender_decoder_manager.h"
#include "bq_log/log/decoder/appender_decoder_helper.h"
#include "bq_log/types/ring_buffer.h"
#include "bq_log/log/appender/appender_console.h"

#if BQ_POSIX
#ifdef BQ_PS
// TODO
#else
#include <signal.h>
#endif
#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>

// The BSDs don't define these so define them as their equivalents
#if !defined(BQ_APPLE) && !defined(BQ_PS) && defined(BQ_UNIX)
#ifndef __NR_tgkill
#define __NR_tgkill SYS_thr_kill
#endif

#ifndef __NR_gettid
#define __NR_gettid SYS_getgid
#endif
#endif

#if BQ_APPLE
pid_t bq_gettid()
{
    return 0;
}

int bq_tgkill(pid_t tgid, pid_t tid, int sig)
{
    (void)tid;
    return kill(tgid, sig);
}
#else
pid_t bq_gettid()
{
    return syscall(__NR_gettid);
}

int bq_tgkill(pid_t tgid, pid_t tid, int sig)
{
    return syscall(__NR_tgkill, tgid, tid, sig);
}
#endif

#endif

namespace bq {
    namespace api {
        static_assert(sizeof(uintptr_t) == sizeof(log_imp*), "invalid pointer size");

        BQ_API const char* __api_get_log_version()
        {
            return "1.4.4";
        }

#if BQ_POSIX

        // compatible to siginfo_t or siginfo
        static struct sigaction tmp_action_static;
        using siginfo_ptr_type = function_argument_type_t<decltype(tmp_action_static.sa_sigaction), 1>;

        static struct signal_stack_holder_type {
            char* signal_stack = NULL;
            signal_stack_holder_type()
            {
                signal_stack = (char*)malloc(SIGSTKSZ);
            }
            ~signal_stack_holder_type()
            {
                free(signal_stack);
            }
        } signal_stack_holder;

        template <int SIG>
        class log_signal_handler {
        public:
            typedef void (*sigaction_func_type)(int, siginfo_ptr_type, void*);

        private:
            static bool registered;
            static struct sigaction original_sigaction;
            static sigaction_func_type handler;

        private:
            static void on_signal(int sig, siginfo_ptr_type info, void* context)
            {
#ifdef BQ_PS
                // TODO
#else
                (void)tmp_action_static;
                struct sigaction tmp;
                sigaction(SIG, &original_sigaction, &tmp);
                registered = false;
                handler(sig, info, context);
#if BQ_APPLE
                kill(getpid(), sig);
#else
                bq_tgkill(getpid(), bq_gettid(), sig);
#endif
#endif
            }

        public:
            static void set_handler(sigaction_func_type func)
            {
#ifdef BQ_PS
                // TODO
#else
                stack_t ss;

                ss.ss_sp = signal_stack_holder.signal_stack;
                ss.ss_size = SIGSTKSZ;
                ss.ss_flags = 0;
                if (sigaltstack(&ss, NULL) == -1) {
                    bq::util::log_device_console(log_level::error, "sigaltstack failed");
                }
                struct sigaction new_sig;
                new_sig.sa_sigaction = on_signal;
                new_sig.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;
                sigemptyset(&new_sig.sa_mask);

                int result = 0;
                if (registered) {
                    struct sigaction tmp;
                    result = sigaction(SIG, &new_sig, &tmp);
                } else {
                    result = sigaction(SIG, &new_sig, &original_sigaction);
                }
                if (result == 0) {
                    registered = true;
                    handler = func;
                }
#endif
            }
        };

        template <int SIG>
        bool log_signal_handler<SIG>::registered = false;
        template <int SIG>
        struct sigaction log_signal_handler<SIG>::original_sigaction;
        template <int SIG>
        typename log_signal_handler<SIG>::sigaction_func_type log_signal_handler<SIG>::handler = nullptr;

        static void log_crash_handler(int signal, siginfo_ptr_type info, void*)
        {
            (void)info;
            bq::util::log_device_console(log_level::error, "crash occurred, signal:%d, now try to force flush logs", signal);
            log_manager::instance().force_flush_all();
        }
#endif

        BQ_API void __api_enable_auto_crash_handler()
        {
#if BQ_POSIX
            log_signal_handler<SIGSEGV>::set_handler(log_crash_handler);
            log_signal_handler<SIGABRT>::set_handler(log_crash_handler);
            log_signal_handler<SIGFPE>::set_handler(log_crash_handler);
            log_signal_handler<SIGILL>::set_handler(log_crash_handler);
            log_signal_handler<SIGTRAP>::set_handler(log_crash_handler);
            log_signal_handler<SIGBUS>::set_handler(log_crash_handler);
#else
            bq::util::log_device_console(log_level::warning, "Crash Handler is only working on POSIX system now, such as Android, Linux, iOS and Mac");
#endif
        }

        BQ_API uint64_t __api_create_log(const char* log_name_utf8, const char* config_content_utf8, uint32_t category_count, const char** category_names_array_utf8)
        {
            bq::array<bq::string> category_names;
            category_names.set_capacity(category_count);
            if (category_count == 0) {
                category_names.push_back("");
            }
            for (uint32_t i = 0; i < category_count; ++i) {
                category_names.push_back(category_names_array_utf8[i]);
            }
            uint64_t log_handle = log_manager::instance().create_log(log_name_utf8, config_content_utf8, category_names);
            return log_handle;
        }

        BQ_API bool __api_log_reset_config(const char* log_name_utf8, const char* config_content_utf8)
        {
            return log_manager::instance().reset_config(log_name_utf8, config_content_utf8);
        }

        static BQ_TLS thread_info thread_info_;
        BQ_API bq::_api_ring_buffer_chunk_write_handle __api_log_buffer_alloc(uint64_t log_id, uint32_t length)
        {
            auto log = bq::log_manager::get_log_by_id(log_id);
            if (!log) {
                bq::_api_ring_buffer_chunk_write_handle handle;
                handle.result = enum_buffer_result_code::err_buffer_not_inited;
                return handle;
            }
            if (!thread_info_.is_inited()) {
                thread_info_.init();
            }
            auto& ring_buffer = log->get_ring_buffer();
            auto reliable_level = log->get_reliable_level();
            uint32_t ext_info_length = sizeof(ext_log_entry_info_head) + thread_info_.thread_name_len_;
            auto write_handle = ring_buffer.alloc_write_chunk(length + ext_info_length);
            bool need_awake_worker = (write_handle.result == enum_buffer_result_code::err_not_enough_space
                || (write_handle.approximate_used_blocks_count << 1) >= ring_buffer.get_total_blocks_count());
            if (need_awake_worker) {
                log_manager::instance().awake_worker();
            }
            if (reliable_level != log_reliable_level::low) {
                while (write_handle.result == enum_buffer_result_code::err_not_enough_space
                    || write_handle.result == enum_buffer_result_code::err_alloc_failed_by_race_condition) {
                    bq::platform::thread::cpu_relax();
                    write_handle = ring_buffer.alloc_write_chunk(length + ext_info_length);
                }
            }
            bq::_api_ring_buffer_chunk_write_handle handle;
            handle.result = write_handle.result;
            handle.data_addr = write_handle.data_addr;

            if (write_handle.result == enum_buffer_result_code::success) {
                bq::_log_entry_head_def* head = (bq::_log_entry_head_def*)handle.data_addr;
                head->ext_info_offset = length;
                bq::ext_log_entry_info_head* ext_info = (bq::ext_log_entry_info_head*)(handle.data_addr + length);
                ext_info->thread_id_ = thread_info_.thread_id_;
                ext_info->thread_name_len_ = thread_info_.thread_name_len_;
                memcpy((uint8_t*)ext_info + sizeof(ext_log_entry_info_head), thread_info_.thread_name_, thread_info_.thread_name_len_);
            }
            return handle;
        }

        BQ_API void __api_log_buffer_commit(uint64_t log_id, bq::_api_ring_buffer_chunk_write_handle write_handle)
        {
            auto log = bq::log_manager::get_log_by_id(log_id);
            if (!log) {
                assert(false && "commit invalid log buffer");
                return;
            }
            bq::ring_buffer_write_handle handle;
            handle.data_addr = write_handle.data_addr;
            handle.result = write_handle.result;
            bq::_log_entry_head_def* head = (bq::_log_entry_head_def*)handle.data_addr;
            head->timestamp_epoch = bq::platform::high_performance_epoch_ms();
            auto& ring_buffer = log->get_ring_buffer();
            ring_buffer.commit_write_chunk(handle);

            if (log->get_thread_mode() == log_thread_mode::sync) {
                log->sync_process();
            }
        }

        BQ_API void __api_set_appenders_enable(uint64_t log_id, const char* appender_name, bool enable)
        {
            bq::log_imp* log = bq::log_manager::get_log_by_id(log_id);
            if (log) {
                auto appenders = log->get_appender_by_vague_name(appender_name);
                for (auto appender : appenders) {
                    const_cast<appender_base*>(appender)->set_enable(enable);
                }
            }
        }

        BQ_API uint32_t __api_get_logs_count()
        {
            return bq::log_manager::instance().get_logs_count();
        }

        BQ_API uint64_t __api_get_log_id_by_index(uint32_t index)
        {
            bq::log_imp* log = bq::log_manager::instance().get_log_by_index(index);
            if (log) {
                return log->id();
            }
            return 0;
        }

        BQ_API bool __api_get_log_name_by_id(uint64_t log_id, bq::_api_string_def* name_ptr)
        {
            if (!name_ptr) {
                return false;
            }
            bq::log_imp* log = bq::log_manager::get_log_by_id(log_id);
            if (log) {
                const auto& name = log->get_name();
                name_ptr->str = name.c_str();
                name_ptr->len = (uint32_t)name.size();
                return true;
            }
            return false;
        }

        BQ_API uint32_t __api_get_log_categories_count(uint64_t log_id)
        {
            bq::log_imp* log = bq::log_manager::get_log_by_id(log_id);
            if (log) {
                return log->get_categories_count();
            }
            return 0;
        }

        BQ_API bool __api_get_log_category_name_by_index(uint64_t log_id, uint32_t category_index, bq::_api_string_def* category_name_ptr)
        {
            if (!category_name_ptr) {
                return false;
            }
            bq::log_imp* log = bq::log_manager::get_log_by_id(log_id);
            if (!log) {
                return false;
            }
            uint32_t count = log->get_categories_count();
            if (category_index >= count) {
                return false;
            }
            const auto& category_name_str = log->get_category_name_by_index(category_index);
            category_name_ptr->str = category_name_str.c_str();
            category_name_ptr->len = (uint32_t)category_name_str.size();
            return true;
        }

        BQ_API const uint32_t* __api_get_log_merged_log_level_bitmap_by_log_id(uint64_t log_id)
        {
            bq::log_imp* log = bq::log_manager::get_log_by_id(log_id);
            if (log) {
                return log->merged_log_level_bitmap_.get_bitmap_ptr();
            }
            return nullptr;
        }

        BQ_API const uint8_t* __api_get_log_category_masks_array_by_log_id(uint64_t log_id)
        {
            bq::log_imp* log = bq::log_manager::get_log_by_id(log_id);
            if (!log || log->get_categories_count() == 0) {
                return nullptr;
            }
            return &(log->categories_mask_array_[0]);
        }

        BQ_API const uint32_t* __api_get_log_print_stack_level_bitmap_by_log_id(uint64_t log_id)
        {
            bq::log_imp* log = bq::log_manager::get_log_by_id(log_id);
            if (log) {
                return log->print_stack_level_bitmap_.get_bitmap_ptr();
            }
            return nullptr;
        }

        BQ_API void __api_log_device_console(bq::log_level level, const char* content)
        {
            bq::util::log_device_console_plain_text(level, content);
        }

        BQ_API void __api_force_flush(uint64_t log_id)
        {
            if (0 == log_id) {
                bq::log_manager::instance().force_flush_all();
            } else {
                bq::log_manager::instance().force_flush(log_id);
            }
        }

        BQ_API const char* __api_get_file_base_dir(bool is_in_sandbox)
        {
            return bq::file_manager::get_base_dir(is_in_sandbox).c_str();
        }

        BQ_API bq::appender_decode_result __api_log_decoder_create(const char* log_file_path, uint32_t* out_handle)
        {
            return bq::appender_decoder_manager::instance().create_decoder(log_file_path, *out_handle);
        }

        BQ_API bq::appender_decode_result __api_log_decoder_decode(uint32_t handle, bq::_api_string_def* out_decoded_log_text)
        {
            const bq::string* decoded_text;
            auto result = bq::appender_decoder_manager::instance().decode_single_item(handle, decoded_text);
            if (result == bq::appender_decode_result::success) {
                out_decoded_log_text->str = decoded_text->c_str();
                out_decoded_log_text->len = (uint32_t)decoded_text->size();
            }
            return result;
        }

        BQ_API void __api_log_decoder_destroy(uint32_t handle)
        {
            bq::appender_decoder_manager::instance().destroy_decoder(handle);
        }

        BQ_API bool __api_log_decode(const char* in_file_path, const char* out_file_path)
        {
            return appender_decoder_helper::decode(in_file_path, out_file_path);
        }

        BQ_API void __api_register_console_callbacks(bq::type_func_ptr_console_callback on_console_callback)
        {
            appender_console::register_console_callback(on_console_callback);
        }

        BQ_API void __api_unregister_console_callbacks(bq::type_func_ptr_console_callback on_console_callback)
        {
            appender_console::unregister_console_callback(on_console_callback);
        }

        BQ_API void __api_set_console_buffer_enable(bool enable)
        {
            appender_console::set_console_buffer_enable(enable);
        }

        BQ_API bool __api_fetch_and_remove_console_buffer(bq::type_func_ptr_console_buffer_fetch_callback on_console_callback, const void* pass_through_param)
        {
            return appender_console::fetch_and_remove_from_console_buffer(on_console_callback, pass_through_param);
        }

        BQ_API void __api_enable_snapshot(uint64_t log_id, uint32_t snapshot_buffer_size)
        {
            bq::log_imp* log = bq::log_manager::get_log_by_id(log_id);
            if (log) {
                log->enable_snapshot(snapshot_buffer_size);
            }
        }

        BQ_API void __api_take_snapshot_string(uint64_t log_id, bool use_gmt_time, bq::_api_string_def* out_snapshot_string)
        {
            bq::log_manager::instance().force_flush(log_id);
            bq::log_imp* log = bq::log_manager::get_log_by_id(log_id);
            if (!log) {
                return;
            }
            const bq::string& result = log->take_snapshot_string(use_gmt_time);
            out_snapshot_string->str = result.c_str();
            out_snapshot_string->len = (uint32_t)result.size();
        }

        BQ_API void __api_release_snapshot_string(uint64_t log_id, bq::_api_string_def* snapshot_string)
        {
            bq::log_imp* log = bq::log_manager::get_log_by_id(log_id);
            if (!log) {
                return;
            }
            snapshot_string->len = 0;
            snapshot_string->str = nullptr;
            log->release_snapshot_string();
        }

        BQ_API void __api_get_stack_trace(bq::_api_string_def* out_name_ptr, uint32_t skip_frame_count)
        {
            const char* str;
            uint32_t len;
            bq::platform::get_stack_trace(skip_frame_count, str, len);
            out_name_ptr->str = str;
            out_name_ptr->len = len;
        }

        BQ_API void __api_get_stack_trace_utf16(bq::_api_u16string_def* out_name_ptr, uint32_t skip_frame_count)
        {
            const char16_t* str;
            uint32_t len;
            bq::platform::get_stack_trace_utf16(skip_frame_count, str, len);
            out_name_ptr->str = str;
            out_name_ptr->len = len;
        }

        BQ_API void __api_uninit()
        {
            bq::log_manager::instance().uninit();
        }
    }
}
