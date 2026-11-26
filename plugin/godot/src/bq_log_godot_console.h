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

#include <godot_cpp/variant/callable.hpp>

namespace bq_godot {

    void init_godot_console(bool enable_editor_redirect);
    void shutdown_godot_console();

    void set_godot_console_redirect_enable(bool enable);
    bool is_godot_console_redirect_enabled();

    void register_user_console_callback(const godot::Callable& callback);
    void unregister_user_console_callback(const godot::Callable& callback);
    bool fetch_and_remove_godot_console_buffer(const godot::Callable& callback);

    // True while the current thread is inside a BqLog console callback/redirect;
    // checked at the logging entry point to prevent both infinite loops and sync-mode deadlocks.
    bool is_in_console_callback();

}
