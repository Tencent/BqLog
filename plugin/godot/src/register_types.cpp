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
#if defined(BQLOG_GODOT_ENABLE_TEST_HELPER)
#include "bq_log_godot_adapter.h"
#endif
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#if defined(BQLOG_GODOT_ENABLE_TEST_HELPER)
class BqLogGodotCppTestHelper : public godot::RefCounted {
    GDCLASS(BqLogGodotCppTestHelper, godot::RefCounted)

protected:
    static void _bind_methods()
    {
        godot::ClassDB::bind_static_method(
            "BqLogGodotCppTestHelper",
            godot::D_METHOD("run_cpp_adapter_interop", "log_name", "config"),
            &BqLogGodotCppTestHelper::run_cpp_adapter_interop);
    }

public:
    static bool run_cpp_adapter_interop(const godot::String& log_name, const godot::String& config)
    {
        const godot::CharString name_utf8 = log_name.utf8();
        const godot::CharString cfg_utf8 = config.utf8();
        bq::log cpp_log = bq::log::create_log(name_utf8.get_data(), cfg_utf8.get_data());
        if (!cpp_log.is_valid()) {
            return false;
        }
        const godot::String gd_str = godot::String::utf8("cpp_gd_str_你好");
        const godot::StringName gd_sn("cpp_gd_sname");
        cpp_log.info(godot::String::utf8("cpp_adapter: str={}, sn={}"), gd_str, gd_sn);
        BQ_LOG_FAST_INFO(cpp_log, "cpp_fast_mode: str={}, num={}", gd_str, 777);
        cpp_log.force_flush();
        return true;
    }
};
#endif

void initialize_bqlog_godot_module(godot::ModuleInitializationLevel p_level)
{
    if (p_level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

#if defined(_WIN32)
    // Pin BqLog.dll for the process lifetime so Godot's FreeLibrary(BqLogGodot.dll)
    // during GDExtension teardown never unmaps BqLog.dll while its async/independent
    // worker threads are still running.
    HMODULE bqlog_module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, L"BqLog.dll", &bqlog_module)) {
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, L"libBqLog.dll", &bqlog_module);
    }
#endif

    godot::ClassDB::register_class<BqLogCategory>();
    godot::ClassDB::register_class<BqLogBase>();
    godot::ClassDB::register_class<BqLog>();
    godot::ClassDB::register_class<BqCategoryLog>();
#if defined(BQLOG_GODOT_ENABLE_TEST_HELPER)
    godot::ClassDB::register_class<BqLogGodotCppTestHelper>();
#endif

    // Route default relative log paths to Godot's writable user:// directory.
    bool is_editor_env = false;
    if (godot::OS* os = godot::OS::get_singleton()) {
        const godot::String user_dir = os->get_user_data_dir();
        if (!user_dir.is_empty()) {
            const godot::CharString utf8 = user_dir.utf8();
            bq::api::__api_reset_base_dir(0, utf8.get_data());
            bq::api::__api_reset_base_dir(1, utf8.get_data());
        }
        // "editor" feature is present in the Godot Editor and in projects launched from it (F5),
        // and absent in exported runtime builds (matching Unity's #if UNITY_EDITOR behavior).
        is_editor_env = os->has_feature("editor");
    }
    if (!is_editor_env) {
        if (godot::Engine* engine = godot::Engine::get_singleton()) {
            is_editor_env = engine->is_editor_hint();
        }
    }

    bq_godot::init_godot_console(is_editor_env);
}

void uninitialize_bqlog_godot_module(godot::ModuleInitializationLevel p_level)
{
    if (p_level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    bq_godot::shutdown_godot_console();
    bq::api::__api_force_flush(0u);
}

extern "C" {
GDExtensionBool GDE_EXPORT bqlog_godot_library_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    const GDExtensionClassLibraryPtr p_library,
    GDExtensionInitialization* r_initialization)
{
    godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

    init_obj.register_initializer(initialize_bqlog_godot_module);
    init_obj.register_terminator(uninitialize_bqlog_godot_module);
    init_obj.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);

    return init_obj.init();
}
}
