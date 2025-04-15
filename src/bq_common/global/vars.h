#pragma once
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
//
//  vars.h
//  Created by Yu Cao on 2025/4/11.
//  Manages the lifecycle and initialization order of global variables uniformly,
//  used to avoid the Static Initialization Order Fiasco.
//  In theory, all global variables with constructors or destructor
//  should be managed here.
#include "bq_common/bq_common.h"

namespace bq {
    template<typename P>
    struct _global_vars_priority_var_initializer {
        static void init() {
            P::get();
        }
    };
    template <>
    struct _global_vars_priority_var_initializer<void> {
        static void init()
        {
        }
    };

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
    ///     int value; 
    /// };
    /// struct LowPriorityVars : global_vars_base<LowPriorityVars, HighPriorityVars> {
    ///     int otherValue;
    /// };
    /// ```
    /// Here, HighPriorityVars initializes before LowPriorityVars and destructs after it, with safe nested calls.
    /// </remarks>
    template<typename T, typename Priority_Global_Var_Type = void>
    struct global_vars_base {
        friend struct global_var_holder;
    protected:
        static BQ_TLS T* global_vars_ptr_;
        static bq::platform::atomic<int32_t> global_vars_init_flag_;   // 0 not init, 1 initializing, 2 initialized
        static T* global_vars_buffer_;
    private:
        struct global_var_holder {
            global_var_holder()
            {
            }
            ~global_var_holder()
            {
                //"To ensure these variables can be used at any time without being destroyed, we accept a small amount of memory leakage."
                if (global_vars_ptr_) {
                    (reinterpret_cast<global_vars_base*>(global_vars_ptr_))->partial_destruct();
                    /*global_vars_ptr_->~T();
                    bq::platform::aligned_free(global_vars_ptr_);
                    global_vars_ptr_ = nullptr;*/
                } else if (global_vars_buffer_) {
                    (reinterpret_cast<global_vars_base*>(global_vars_buffer_))->partial_destruct();
                }
            }
        };
    protected:
        virtual void partial_destruct() {

        }
    public:
        static T& get() {
            if (!global_vars_ptr_) {
                if (global_vars_init_flag_.load_acquire() == 0) {
                    int32_t expected = 0;
                    if (global_vars_init_flag_.compare_exchange_strong(expected, 1, bq::platform::memory_order::release, bq::platform::memory_order::acquire)) {
                        _global_vars_priority_var_initializer<Priority_Global_Var_Type>::init();
                        static global_var_holder holder;
                        global_vars_buffer_ = reinterpret_cast<T*>(bq::platform::aligned_alloc(8, sizeof(T)));
                        global_vars_ptr_ = global_vars_buffer_;
                        new (global_vars_buffer_, bq::enum_new_dummy::dummy) T();
                        global_vars_init_flag_.store_release(2);
                        return *global_vars_ptr_; 
                    }
                }
                while (global_vars_init_flag_.load_acquire() != 2) {
                    bq::platform::thread::yield();
                }
                global_vars_ptr_ = global_vars_buffer_;
            }
            return *global_vars_ptr_; 
        }
    };

    template<typename T, typename Priority_Global_Var_Type>
    BQ_TLS T* global_vars_base<T, Priority_Global_Var_Type>::global_vars_ptr_;

    template <typename T, typename Priority_Global_Var_Type>
    bq::platform::atomic<int32_t> global_vars_base<T, Priority_Global_Var_Type>::global_vars_init_flag_; // Init by Zero Initialization to avoid static init order fiasco

    template <typename T, typename Priority_Global_Var_Type>
    T* global_vars_base<T, Priority_Global_Var_Type>::global_vars_buffer_;


    struct common_global_vars : public global_vars_base<common_global_vars>{
        bq::platform::base_dir_initializer base_dir_init_inst_;
        bq::hash_map<bq::platform::file_node_info, bq::platform::file_open_mode_enum> file_exclusive_cache_;
        bq::platform::mutex file_exclusive_mutex_;
        bq::platform::mutex stack_trace_mutex_;
        bq::array<char> device_console_buffer_ = { '\0' };
        bq::platform::mutex console_mutex_;

#if BQ_JAVA
        bq::array<void (*)()> jni_onload_callbacks_inst_;
#endif
#if BQ_ANDROID
        jobject android_asset_manager_java_instance_ = nullptr;
        AAssetManager* android_asset_manager_inst_ = nullptr;
        bq::string android_id_;
        bq::string android_package_name_;
        bq::string apk_path_;
        bq::platform::spin_lock apk_path_spin_lock_;
#endif
#if BQ_WIN
        HANDLE stack_trace_process_ = GetCurrentProcess();
        bq::platform::atomic<bool> stack_trace_sym_initialized_ = false;
#endif
        property_value null_property_value_;
        file_manager* file_manager_inst_ = new file_manager();

        common_global_vars()
        {
        }
    protected:
        virtual void partial_destruct() override
        {
            delete file_manager_inst_;
            file_manager_inst_ = nullptr;
        }
    };

}