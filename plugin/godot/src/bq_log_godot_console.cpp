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
#include "bq_log_godot_console.h"

#include <atomic>
#include <mutex>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "bq_log/bq_log.h"

namespace bq_godot {

    static std::mutex g_console_mutex;
    static std::atomic<bool> g_redirect_enabled { false };
    static std::atomic<bool> g_callback_registered { false };
    // Never construct a godot::Callable at static-init time (DllMain runs before GDExtension init).
    static godot::Callable* g_user_callback = nullptr;

    // Prevents infinite recursion if Godot's print/push_warning/push_error or a user
    // console callback synchronously triggers another BqLog write on the same thread.
    static thread_local bool t_in_console_callback = false;

    struct console_reentrancy_guard {
        bool entered_ = false;
        console_reentrancy_guard()
        {
            if (!t_in_console_callback) {
                t_in_console_callback = true;
                entered_ = true;
            }
        }
        ~console_reentrancy_guard()
        {
            if (entered_) {
                t_in_console_callback = false;
            }
        }
    };

    static void BQ_STDCALL on_bq_console_callback(
        uint64_t log_id,
        int32_t category_idx,
        bq::log_level level,
        const char* content,
        int32_t length)
    {
        console_reentrancy_guard guard;
        if (!guard.entered_) {
            return;
        }

        const bool do_redirect = g_redirect_enabled.load(std::memory_order_relaxed);
        godot::Callable user_cb;
        {
            std::lock_guard<std::mutex> lock(g_console_mutex);
            if (g_user_callback && g_user_callback->is_valid()) {
                user_cb = *g_user_callback;
            }
        }

        if (!do_redirect && !user_cb.is_valid()) {
            return;
        }

        const godot::String text = (content && length > 0)
            ? godot::String::utf8(content, length)
            : godot::String();

        if (do_redirect) {
            switch (level) {
            case bq::log_level::warning:
                godot::UtilityFunctions::push_warning(text);
                break;
            case bq::log_level::error:
            case bq::log_level::fatal:
                godot::UtilityFunctions::push_error(text);
                break;
            default:
                godot::UtilityFunctions::print(text);
                break;
            }
        }

        if (user_cb.is_valid()) {
            user_cb.call(
                static_cast<int64_t>(log_id),
                category_idx,
                static_cast<int32_t>(level),
                text);
        }
    }

    static void BQ_STDCALL on_bq_console_buffer_fetch(
        void* pass_through_param,
        uint64_t log_id,
        int32_t category_idx,
        bq::log_level level,
        const char* content,
        int32_t length)
    {
        console_reentrancy_guard guard;
        if (!guard.entered_ || !pass_through_param) {
            return;
        }
        const auto* cb = static_cast<const godot::Callable*>(pass_through_param);
        if (!cb->is_valid()) {
            return;
        }
        const godot::String text = (content && length > 0)
            ? godot::String::utf8(content, length)
            : godot::String();
        cb->call(
            static_cast<int64_t>(log_id),
            category_idx,
            static_cast<int32_t>(level),
            text);
    }

    // Must be called with g_console_mutex held.
    static void sync_native_callback_registration_locked()
    {
        const bool need_cb = g_redirect_enabled.load(std::memory_order_acquire)
            || (g_user_callback && g_user_callback->is_valid());
        const bool currently_registered = g_callback_registered.load(std::memory_order_acquire);
        if (need_cb && !currently_registered) {
            bq::api::__api_register_console_callbacks(&on_bq_console_callback);
            g_callback_registered.store(true, std::memory_order_release);
        } else if (!need_cb && currently_registered) {
            bq::api::__api_unregister_console_callbacks(&on_bq_console_callback);
            g_callback_registered.store(false, std::memory_order_release);
        }
    }

    void init_godot_console(bool enable_editor_redirect)
    {
        std::lock_guard<std::mutex> lock(g_console_mutex);
        g_redirect_enabled.store(enable_editor_redirect, std::memory_order_release);
        sync_native_callback_registration_locked();
    }

    void shutdown_godot_console()
    {
        {
            std::lock_guard<std::mutex> lock(g_console_mutex);
            delete g_user_callback;
            g_user_callback = nullptr;
            g_redirect_enabled.store(false, std::memory_order_release);
            sync_native_callback_registration_locked();
        }
    }

    void set_godot_console_redirect_enable(bool enable)
    {
        std::lock_guard<std::mutex> lock(g_console_mutex);
        g_redirect_enabled.store(enable, std::memory_order_release);
        sync_native_callback_registration_locked();
    }

    bool is_godot_console_redirect_enabled()
    {
        return g_redirect_enabled.load(std::memory_order_acquire);
    }

    void register_user_console_callback(const godot::Callable& callback)
    {
        std::lock_guard<std::mutex> lock(g_console_mutex);
        if (!g_user_callback) {
            g_user_callback = new godot::Callable(callback);
        } else {
            *g_user_callback = callback;
        }
        sync_native_callback_registration_locked();
    }

    void unregister_user_console_callback(const godot::Callable& callback)
    {
        std::lock_guard<std::mutex> lock(g_console_mutex);
        if (g_user_callback && (!callback.is_valid() || *g_user_callback == callback)) {
            delete g_user_callback;
            g_user_callback = nullptr;
        }
        sync_native_callback_registration_locked();
    }

    bool fetch_and_remove_godot_console_buffer(const godot::Callable& callback)
    {
        return bq::api::__api_fetch_and_remove_console_buffer(
            &on_bq_console_buffer_fetch,
            &callback);
    }

    bool is_in_console_callback()
    {
        return t_in_console_callback;
    }

}
