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
#include <gdextension_interface.h>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include "bq_log/bq_log.h"
#include "bq_log_godot_adapter.h"

class bq_godot_cpp_test_helper : public godot::RefCounted {
    GDCLASS(bq_godot_cpp_test_helper, godot::RefCounted)

protected:
    static void _bind_methods()
    {
        godot::ClassDB::bind_static_method(
            "bq_godot_cpp_test_helper",
            godot::D_METHOD("run_cpp_adapter_interop", "log_name", "config"),
            &bq_godot_cpp_test_helper::run_cpp_adapter_interop);
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

static void initialize_cpp_test_module(godot::ModuleInitializationLevel level)
{
    if (level == godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        godot::ClassDB::register_class<bq_godot_cpp_test_helper>();
    }
}

static void uninitialize_cpp_test_module(godot::ModuleInitializationLevel level)
{
    (void)level;
}

extern "C" {
GDExtensionBool GDE_EXPORT bqlog_godot_cpp_test_init(
    GDExtensionInterfaceGetProcAddress get_proc_address,
    const GDExtensionClassLibraryPtr library,
    GDExtensionInitialization* initialization)
{
    godot::GDExtensionBinding::InitObject init_obj(get_proc_address, library, initialization);
    init_obj.register_initializer(initialize_cpp_test_module);
    init_obj.register_terminator(uninitialize_cpp_test_module);
    init_obj.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_obj.init();
}
}
