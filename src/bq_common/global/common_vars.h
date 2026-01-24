#pragma once
/*
 * Copyright (C) 2025 Tencent.
 * BQLOG is licensed under the Apache License, Version 2.0.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */
//
//  common_vars.h
//  Created by Yu Cao on 2025/4/11.
//  Manages the lifecycle and initialization order of global variables uniformly,
//  used to avoid the Static Initialization Order Fiasco.
//  In theory, all global variables with constructors or destructor
//  should be managed here.

#include "bq_common/bq_common.h"
namespace bq {
    template <typename P>
    struct _global_vars_priority_var_initializer {
        static void init()
        {
            P::get();
        }
    };
    template <>
    struct _global_vars_priority_var_initializer<void> {
        static void init()
        {
        }
    };

    struct global_var_destructiable {
        friend struct global_vars_destructor;
    protected:
        virtual void partial_destruct()
        {
        }
    };

    struct global_vars_destructor {
    public:
        void register_destructible_var(global_var_destructiable* var);
        ~global_vars_destructor();
    };

    global_vars_destructor& get_global_var_destructor();

    /// <summary>
    /// Base class template for managing global objects with controlled initialization and destruction.
    /// </summary>
    /// <typeparam name="T">The type of the global object, typically the derived class itself.</typeparam>
    /// <typeparam name="Priority_Global_Var_Type">Optional type (derived from global_vars_base) that must initialize before T and destruct after T. Defaults to void if no priority is needed.</typeparam>
    /// <remarks>
    /// Provides a singleton-like mechanism for global objects, ensuring:
    /// - Safe handling of nested get() calls during T's construction.
    /// - Precise initialization order via Priority_Global_Var_Type.
    /// - Proper destruction of T and its priority dependencies.
    /// - Thread-safe initialization in C++11.
    ///
    /// Usage:
    /// - Derive a global object collection class from global_vars_base<DerivedClass, OptionalPriorityClass>.
    /// - Declare members in the correct order to enforce initialization dependencies.
    /// - Access the global instance via DerivedClass::get().
    ///
    /// Example:
    /// ```cpp
    /// struct HighPriorityVars : global_vars_base<HighPriorityVars> {
    ///     int32_t value;
    /// };
    /// struct LowPriorityVars : global_vars_base<LowPriorityVars, HighPriorityVars> {
    ///     int32_t otherValue;
    /// };
    /// ```
    /// Here, HighPriorityVars initializes before LowPriorityVars and destructs after it, with safe nested calls.
    /// </remarks>
    template <typename T, typename Priority_Global_Var_Type = void>
    struct global_vars_base : public global_var_destructiable {
    protected:
        static BQ_TLS T* global_vars_ptr_;
        alignas(8) static int32_t global_vars_init_flag_; // 0 not init, 1 initializing, 2 initialized
        static T* global_vars_buffer_;
        static bq::platform::thread::thread_id initializer_thread_id_;

    protected:
        virtual ~global_vars_base()
        {
            assert(false && "global_vars_base should not be directly destructed");
        }

    public:
        static T& get()
        {
            if (T::global_vars_ptr_) {
                return *T::global_vars_ptr_;
            }
            if (bq::platform::thread::get_current_thread_id() == initializer_thread_id_
                && initializer_thread_id_ != 0) {
                T::global_vars_ptr_ = T::global_vars_buffer_;
                return *T::global_vars_buffer_;
            }
            if (!T::global_vars_ptr_) {
                bq::platform::atomic<int32_t>& atomic_status = *reinterpret_cast<bq::platform::atomic<int32_t>*>(&T::global_vars_init_flag_);
                if (atomic_status.load_acquire() == 0) {
                    int32_t expected = 0;
                    if (atomic_status.compare_exchange_strong(expected, 1, bq::platform::memory_order::release, bq::platform::memory_order::acquire)) {
                        initializer_thread_id_ = bq::platform::thread::get_current_thread_id();
                        _global_vars_priority_var_initializer<Priority_Global_Var_Type>::init();
                        T::global_vars_buffer_ = static_cast<T*>(bq::platform::aligned_alloc(BQ_CACHE_LINE_SIZE, sizeof(T)));
                        T::global_vars_ptr_ = T::global_vars_buffer_;
                        new (T::global_vars_buffer_, bq::enum_new_dummy::dummy) T();
                        get_global_var_destructor().register_destructible_var(T::global_vars_ptr_);
                        atomic_status.store_release(2);
                        return *T::global_vars_ptr_;
                    }
                }
                while (atomic_status.load_acquire() != 2) {
                    bq::platform::thread::yield();
                }
                T::global_vars_ptr_ = T::global_vars_buffer_;
            }
            return *T::global_vars_ptr_;
        }
    };

    template <typename T, typename Priority_Global_Var_Type>
    BQ_TLS T* global_vars_base<T, Priority_Global_Var_Type>::global_vars_ptr_;

    template <typename T, typename Priority_Global_Var_Type>
    alignas(8) int32_t global_vars_base<T, Priority_Global_Var_Type>::global_vars_init_flag_; // Init by Zero Initialization to avoid static init order fiasco

    template <typename T, typename Priority_Global_Var_Type>
    T* global_vars_base<T, Priority_Global_Var_Type>::global_vars_buffer_;

    template <typename T, typename Priority_Global_Var_Type>
    bq::platform::thread::thread_id global_vars_base<T, Priority_Global_Var_Type>::initializer_thread_id_;

    struct common_global_vars : public global_vars_base<common_global_vars> {
#if defined(BQ_WIN)
        bq::platform::mutex win_api_mutex_;
#endif
        bq::array<char> device_console_buffer_ = { '\0' };
        bq::platform::mutex console_mutex_;
#if defined(BQ_X86)
        bool avx2_support_;
#endif
        bool crc32_supported_;
        bq::platform::base_dir_initializer base_dir_init_inst_;
        bq::hash_map<bq::platform::file_node_info, bq::platform::file_open_mode_enum> file_exclusive_cache_;
        bq::platform::mutex file_exclusive_mutex_;
        bq::platform::mutex stack_trace_mutex_;

#if defined(BQ_JAVA)
        bq::array<void (*)()> jni_onload_callbacks_inst_;
        bq::array<jobject> remove_jni_global_ref_list_array_;
        bq::platform::mutex remove_jni_global_ref_list_mutex_;
        class remove_jni_global_ref_thread : public bq::platform::thread {
        protected:
            virtual void run() override;
        };
        remove_jni_global_ref_thread remove_jni_global_ref_thread_;
        bool is_jvm_destroyed() const;
#endif
#if defined(BQ_NAPI)
        bq::platform::mutex napi_init_mutex_;
        bq::platform::mutex napi_env_mutex_;
        bq::array<void (*)(napi_env env, napi_value exports)> napi_init_native_callbacks_inst_;
        bq::array<napi_property_descriptor> napi_registered_functions_;
#if defined(BQ_NAPI)
        napi_threadsafe_function napi_tsfn_native_ = nullptr;
        napi_threadsafe_function napi_tsfn_js_ = nullptr;
        napi_env napi_main_env_ = nullptr;
        bool napi_is_initialized_ = false;
        bq::array<bq::platform::napi_callback_dispatcher*> napi_dispatchers_;
#endif
#endif
#if defined(BQ_ANDROID)
        jobject android_asset_manager_java_instance_ = nullptr;
        AAssetManager* android_asset_manager_inst_ = nullptr;
        bq::string android_id_;
        bq::string android_package_name_;
        bq::string apk_path_;
        bq::platform::spin_lock apk_path_spin_lock_;
#endif
#if defined(BQ_WIN)
        bq::platform::platform_file_handle stack_trace_process_ = bq::platform::invalid_platform_file_handle;
        bq::platform::atomic<bool> stack_trace_sym_initialized_ = false;
#endif
        property_value null_property_value_;
        file_manager* file_manager_inst_ = new file_manager();

        common_global_vars();

        virtual ~common_global_vars() override { }

    protected:
        virtual void partial_destruct() override;
    };

}