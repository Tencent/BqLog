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

#include <napi.h>
#include <string>
#include <vector>

// Include BqLog headers
#include "bq_log/misc/bq_log_api.h"
#include "bq_log/bq_log.h"

class BqLogWrapper : public Napi::ObjectWrap<BqLogWrapper> {
private:
    uint64_t log_id_;

public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    BqLogWrapper(const Napi::CallbackInfo& info);

    // Core API methods
    Napi::Value GetVersion(const Napi::CallbackInfo& info);
    void EnableAutoCrashHandler(const Napi::CallbackInfo& info);
    Napi::Value CreateLog(const Napi::CallbackInfo& info);
    Napi::Value GetLogByName(const Napi::CallbackInfo& info);
    void ResetConfig(const Napi::CallbackInfo& info);
    void SetAppendersEnable(const Napi::CallbackInfo& info);
    void ForceFlush(const Napi::CallbackInfo& info);
    void Log(const Napi::CallbackInfo& info);
    void Uninit(const Napi::CallbackInfo& info);

    // Console callback support
    Napi::Value SetConsoleCallback(const Napi::CallbackInfo& info);
    Napi::Value TakeSnapshotString(const Napi::CallbackInfo& info);

    // Utility methods
    Napi::Value IsValid(const Napi::CallbackInfo& info);
    Napi::Value GetLogId(const Napi::CallbackInfo& info);
};

// Static callback for console output
static Napi::ThreadSafeFunction console_callback_tsfn;

static void console_callback_handler(uint64_t log_id, int32_t category_idx, int32_t log_level, const char* content, int32_t length) {
    if (console_callback_tsfn) {
        auto callback = [=](Napi::Env env, Napi::Function jsCallback) {
            Napi::Object result = Napi::Object::New(env);
            result.Set("logId", Napi::Number::New(env, static_cast<double>(log_id)));
            result.Set("categoryIdx", Napi::Number::New(env, category_idx));
            result.Set("logLevel", Napi::Number::New(env, log_level));
            result.Set("content", Napi::String::New(env, content, length));
            
            jsCallback.Call({result});
        };
        console_callback_tsfn.BlockingCall(callback);
    }
}

Napi::Object BqLogWrapper::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "BqLog", {
        StaticMethod("getVersion", &BqLogWrapper::GetVersion),
        StaticMethod("enableAutoCrashHandler", &BqLogWrapper::EnableAutoCrashHandler),
        StaticMethod("createLog", &BqLogWrapper::CreateLog),
        StaticMethod("getLogByName", &BqLogWrapper::GetLogByName),
        StaticMethod("uninit", &BqLogWrapper::Uninit),
        
        InstanceMethod("resetConfig", &BqLogWrapper::ResetConfig),
        InstanceMethod("setAppendersEnable", &BqLogWrapper::SetAppendersEnable),
        InstanceMethod("forceFlush", &BqLogWrapper::ForceFlush),
        InstanceMethod("log", &BqLogWrapper::Log),
        InstanceMethod("setConsoleCallback", &BqLogWrapper::SetConsoleCallback),
        InstanceMethod("takeSnapshotString", &BqLogWrapper::TakeSnapshotString),
        InstanceMethod("isValid", &BqLogWrapper::IsValid),
        InstanceMethod("getLogId", &BqLogWrapper::GetLogId)
    });

    Napi::FunctionReference* constructor = new Napi::FunctionReference();
    *constructor = Napi::Persistent(func);
    env.SetInstanceData(constructor);

    exports.Set("BqLog", func);
    return exports;
}

BqLogWrapper::BqLogWrapper(const Napi::CallbackInfo& info) : Napi::ObjectWrap<BqLogWrapper>(info), log_id_(0) {
    // Constructor can be used to wrap existing log instances
    if (info.Length() > 0 && info[0].IsNumber()) {
        log_id_ = info[0].As<Napi::Number>().Int64Value();
    }
}

Napi::Value BqLogWrapper::GetVersion(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    const char* version = bq::api::__api_get_log_version();
    return Napi::String::New(env, version ? version : "unknown");
}

void BqLogWrapper::EnableAutoCrashHandler(const Napi::CallbackInfo& info) {
    bq::api::__api_enable_auto_crash_handler();
}

Napi::Value BqLogWrapper::CreateLog(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (info.Length() < 2) {
        Napi::TypeError::New(env, "Expected at least 2 arguments: logName, config").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (!info[0].IsString() || !info[1].IsString()) {
        Napi::TypeError::New(env, "logName and config must be strings").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::string log_name = info[0].As<Napi::String>().Utf8Value();
    std::string config = info[1].As<Napi::String>().Utf8Value();
    
    std::vector<const char*> categories;
    std::vector<std::string> category_strings;
    
    if (info.Length() > 2 && info[2].IsArray()) {
        Napi::Array category_array = info[2].As<Napi::Array>();
        uint32_t length = category_array.Length();
        
        category_strings.reserve(length);
        categories.reserve(length);
        
        for (uint32_t i = 0; i < length; i++) {
            Napi::Value val = category_array[i];
            if (val.IsString()) {
                category_strings.push_back(val.As<Napi::String>().Utf8Value());
                categories.push_back(category_strings.back().c_str());
            }
        }
    }

    uint64_t log_id = bq::api::__api_create_log(
        log_name.c_str(), 
        config.c_str(), 
        static_cast<uint32_t>(categories.size()), 
        categories.empty() ? nullptr : categories.data()
    );

    if (log_id == 0) {
        return env.Null();
    }

    // Create new instance with log_id
    Napi::FunctionReference* constructor = env.GetInstanceData<Napi::FunctionReference>();
    return constructor->New({Napi::Number::New(env, static_cast<double>(log_id))});
}

Napi::Value BqLogWrapper::GetLogByName(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "Expected string argument: logName").ThrowAsJavaScriptException();
        return env.Null();
    }

    std::string log_name = info[0].As<Napi::String>().Utf8Value();
    uint64_t log_id = bq::api::__api_get_log_by_name(log_name.c_str());

    if (log_id == 0) {
        return env.Null();
    }

    // Create new instance with log_id
    Napi::FunctionReference* constructor = env.GetInstanceData<Napi::FunctionReference>();
    return constructor->New({Napi::Number::New(env, static_cast<double>(log_id))});
}

void BqLogWrapper::ResetConfig(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (info.Length() < 2 || !info[0].IsString() || !info[1].IsString()) {
        Napi::TypeError::New(env, "Expected 2 string arguments: logName, config").ThrowAsJavaScriptException();
        return;
    }

    std::string log_name = info[0].As<Napi::String>().Utf8Value();
    std::string config = info[1].As<Napi::String>().Utf8Value();
    
    bq::api::__api_log_reset_config(log_name.c_str(), config.c_str());
}

void BqLogWrapper::SetAppendersEnable(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (info.Length() < 2 || !info[0].IsString() || !info[1].IsBoolean()) {
        Napi::TypeError::New(env, "Expected string and boolean arguments: appenderName, enable").ThrowAsJavaScriptException();
        return;
    }

    std::string appender_name = info[0].As<Napi::String>().Utf8Value();
    bool enable = info[1].As<Napi::Boolean>().Value();
    
    bq::api::__api_set_appenders_enable(log_id_, appender_name.c_str(), enable);
}

void BqLogWrapper::ForceFlush(const Napi::CallbackInfo& info) {
    bq::api::__api_force_flush(log_id_);
}

void BqLogWrapper::Log(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (info.Length() < 3) {
        Napi::TypeError::New(env, "Expected at least 3 arguments: categoryIdx, level, message").ThrowAsJavaScriptException();
        return;
    }

    if (!info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsString()) {
        Napi::TypeError::New(env, "Invalid argument types").ThrowAsJavaScriptException();
        return;
    }

    int32_t category_idx = info[0].As<Napi::Number>().Int32Value();
    int32_t level = info[1].As<Napi::Number>().Int32Value();
    std::string message = info[2].As<Napi::String>().Utf8Value();

    // Allocate buffer and write log
    auto write_handle = bq::api::__api_log_buffer_alloc(log_id_, static_cast<uint32_t>(message.length() + 1));
    if (write_handle.result_code == bq::log_buffer_result_code::success) {
        memcpy(write_handle.data_ptr, message.c_str(), message.length());
        write_handle.data_ptr[message.length()] = '\0';
        bq::api::__api_log_buffer_commit(log_id_, write_handle);
    }
}

Napi::Value BqLogWrapper::SetConsoleCallback(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (info.Length() < 1 || !info[0].IsFunction()) {
        Napi::TypeError::New(env, "Expected function argument: callback").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Function callback = info[0].As<Napi::Function>();
    
    // Create thread-safe function
    console_callback_tsfn = Napi::ThreadSafeFunction::New(
        env,
        callback,
        "ConsoleCallback",
        0,
        1
    );

    // Register the callback
    bq::api::__api_register_console_callbacks(console_callback_handler);
    
    return env.Undefined();
}

Napi::Value BqLogWrapper::TakeSnapshotString(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    bool use_gmt_time = info.Length() > 0 && info[0].IsBoolean() ? info[0].As<Napi::Boolean>().Value() : false;
    
    const char* snapshot = bq::api::__api_take_snapshot_string(log_id_, use_gmt_time);
    return snapshot ? Napi::String::New(env, snapshot) : env.Null();
}

Napi::Value BqLogWrapper::IsValid(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    return Napi::Boolean::New(env, log_id_ != 0);
}

Napi::Value BqLogWrapper::GetLogId(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    return Napi::Number::New(env, static_cast<double>(log_id_));
}

void BqLogWrapper::Uninit(const Napi::CallbackInfo& info) {
    bq::api::__api_uninit();
    
    // Clean up thread-safe function
    if (console_callback_tsfn) {
        console_callback_tsfn.Release();
    }
}

// Module initialization
Napi::Object Init(Napi::Env env, Napi::Object exports) {
    return BqLogWrapper::Init(env, exports);
}

NODE_API_MODULE(bqlog, Init)