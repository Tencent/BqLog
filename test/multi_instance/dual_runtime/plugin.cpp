/* Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0. */
// One BqLog runtime per plugin: each plugin statically links its own copy and
// creates a recovery log with the same name. The host loads both and reports
// the two instance indices, which must differ (0 and 1).
#include "bq_log/bq_log.h"

#if defined(_WIN32)
#define PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

PLUGIN_EXPORT int plugin_run(const char* log_name)
{
    const char* config = "log.thread_mode=async\n"
                         "log.recovery=true\n"
                         "appenders_config.out.type=text_file\n"
                         "appenders_config.out.levels=[all]\n"
                         "appenders_config.out.file_name=output/plugin\n"
                         "appenders_config.out.base_dir_type=0\n"
                         "appenders_config.out.enable_rolling_log_file=false\n";
    bq::log log = bq::log::create_log(log_name, config);
    if (!log.is_valid()) {
        return -1;
    }
    log.info("plugin log written");
    log.force_flush();
    uint32_t instance = UINT32_MAX;
    bool lp_mapped = false;
    bool hp_mapped = false;
    bq::api::__api_test_get_storage_report(log.get_id(), &instance, &lp_mapped, &hp_mapped);
    return static_cast<int>(instance);
}
