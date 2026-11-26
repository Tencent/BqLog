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
#include "register_types.h"

#include <gdextension_interface.h>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/string.hpp>
#include "bq_category_log_godot.h"
#include "bq_log/bq_log.h"
#include "bq_log_godot.h"
#include "bq_log_godot_console.h"
#include "bq_log_godot_runtime.h"

void initialize_bqlog_godot_module(godot::ModuleInitializationLevel level)
{
    if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    bq_godot::retain_bqlog_runtime();

    godot::ClassDB::register_class<bq_log_category>();
    godot::ClassDB::register_class<bq_log_base>();
    godot::ClassDB::register_class<bq_log>();
    godot::ClassDB::register_class<bq_category_log>();

    // Route default relative log paths to Godot's writable user:// directory.
    bool is_editor_env = false;
    if (godot::OS* os = godot::OS::get_singleton()) {
        const godot::String user_dir = os->get_user_data_dir();
        if (!user_dir.is_empty()) {
            const godot::CharString utf8 = user_dir.utf8();
            bq::api::__api_reset_base_dir(0, utf8.get_data());
            bq::api::__api_reset_base_dir(1, utf8.get_data());
        }
        is_editor_env = os->has_feature("editor");
    }
    if (!is_editor_env) {
        if (godot::Engine* engine = godot::Engine::get_singleton()) {
            is_editor_env = engine->is_editor_hint();
        }
    }

    bq_godot::init_godot_console(is_editor_env);
}

void uninitialize_bqlog_godot_module(godot::ModuleInitializationLevel level)
{
    if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    bq_godot::shutdown_godot_console();
    bq::api::__api_force_flush(0u);
}

extern "C" {
GDExtensionBool GDE_EXPORT bqlog_godot_library_init(
    GDExtensionInterfaceGetProcAddress get_proc_address,
    const GDExtensionClassLibraryPtr library,
    GDExtensionInitialization* initialization)
{
    godot::GDExtensionBinding::InitObject init_obj(get_proc_address, library, initialization);

    init_obj.register_initializer(initialize_bqlog_godot_module);
    init_obj.register_terminator(uninitialize_bqlog_godot_module);
    init_obj.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);

    return init_obj.init();
}
}
