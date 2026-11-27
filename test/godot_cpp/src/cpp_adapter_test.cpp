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
#include <mutex>
#include <string>
#include <gdextension_interface.h>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
// Godot C++ users include only the single Godot adapter header (it includes bq_log/bq_log.h).
#include "bq_log_godot_adapter.h"

namespace bq {

    static std::mutex cpp_cb_mutex;
    static std::string cpp_cb_captured;

    static void BQ_STDCALL on_cpp_test_console_callback(
        uint64_t log_id,
        int32_t category_idx,
        bq::log_level level,
        const char* content,
        int32_t length)
    {
        (void)log_id;
        (void)category_idx;
        (void)level;
        if (content && length > 0) {
            std::lock_guard<std::mutex> lock(cpp_cb_mutex);
            cpp_cb_captured.append(content, static_cast<size_t>(length));
            cpp_cb_captured.push_back('\n');
        }
    }

    static bool check_captured_contains(const std::string& captured, const char* needle)
    {
        if (captured.find(needle) == std::string::npos) {
            godot::UtilityFunctions::printerr("[CPP_TEST_FAIL] Missing expected text in console callback: ", needle);
            return false;
        }
        return true;
    }

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
                godot::UtilityFunctions::printerr("[CPP_TEST_FAIL] create_log failed");
                return false;
            }

            {
                std::lock_guard<std::mutex> lock(cpp_cb_mutex);
                cpp_cb_captured.clear();
            }
            bq::log::register_console_callback(&on_cpp_test_console_callback);

            const godot::String gd_str = godot::String::utf8("cpp_gd_str_你好");
            const godot::StringName gd_sn("cpp_gd_sname");
            const godot::NodePath gd_np("/root/Main/Player");
            const godot::Vector2 gd_v2(1.5f, -2.5f);
            const godot::Vector2i gd_v2i(10, -20);
            const godot::Vector3 gd_v3(1.0f, 2.0f, 3.0f);
            const godot::Vector3i gd_v3i(4, 5, 6);
            const godot::Vector4 gd_v4(1.0f, 2.0f, 3.0f, 4.0f);
            const godot::Vector4i gd_v4i(7, 8, 9, 10);
            const godot::Color gd_col(0.5f, 0.25f, 1.0f, 1.0f);
            const godot::Quaternion gd_quat(0.0f, 0.0f, 0.0f, 1.0f);
            const godot::Rect2 gd_r2(1.0f, 2.0f, 3.0f, 4.0f);
            const godot::Rect2i gd_r2i(5, 6, 7, 8);

            cpp_log.info(godot::String::utf8("cpp_adapter: str={}, sn={}, np={}"), gd_str, gd_sn, gd_np);
            cpp_log.info("cpp_math_a: v2={}, v2i={}, v3={}, v3i={}", gd_v2, gd_v2i, gd_v3, gd_v3i);
            cpp_log.info("cpp_math_b: v4={}, v4i={}, col={}, quat={}", gd_v4, gd_v4i, gd_col, gd_quat);
            cpp_log.info("cpp_rect: r2={}, r2i={}", gd_r2, gd_r2i);
            BQ_LOG_FAST_INFO(cpp_log, "cpp_fast_mode: str={}, v3={}, num={}", gd_str, gd_v3, 777);
            cpp_log.force_flush();

            bq::log::unregister_console_callback(&on_cpp_test_console_callback);

            std::string captured;
            {
                std::lock_guard<std::mutex> lock(cpp_cb_mutex);
                captured = cpp_cb_captured;
            }

            bool ok = true;
            ok = check_captured_contains(captured, "cpp_adapter: str=cpp_gd_str_你好, sn=cpp_gd_sname, np=/root/Main/Player") && ok;
            ok = check_captured_contains(captured, "cpp_math_a: v2=(1.5, -2.5), v2i=(10, -20), v3=(1, 2, 3), v3i=(4, 5, 6)") && ok;
            ok = check_captured_contains(captured, "cpp_math_b: v4=(1, 2, 3, 4), v4i=(7, 8, 9, 10), col=(0.5, 0.25, 1, 1), quat=(0, 0, 0, 1)") && ok;
            ok = check_captured_contains(captured, "cpp_rect: r2=[P: (1, 2), S: (3, 4)], r2i=[P: (5, 6), S: (7, 8)]") && ok;
            ok = check_captured_contains(captured, "cpp_fast_mode: str=cpp_gd_str_你好, v3=(1, 2, 3), num=777") && ok;
            return ok;
        }
    };

}

static void initialize_cpp_test_module(godot::ModuleInitializationLevel level)
{
    if (level == godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        godot::ClassDB::register_class<bq::bq_godot_cpp_test_helper>();
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
