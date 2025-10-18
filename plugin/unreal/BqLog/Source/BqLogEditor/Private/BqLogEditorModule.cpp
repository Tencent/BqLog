#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"
#include "bq_log/bq_log.h"

DEFINE_LOG_CATEGORY_STATIC(LogBqLogEditor, Log, All);

class FBqLogEditorModule : public IModuleInterface
{
private:
    static void BQ_STDCALL on_bq_log_console_callback(uint64_t log_id, int32_t category_idx, int32_t log_level, const char* content, int32_t length)
    {
        (void)log_id;
        (void)category_idx;
        const ANSICHAR* u8_str = reinterpret_cast<const ANSICHAR*>(content ? content : "");
        int32_t tchar_len = (length >= 0) ? length : FCStringAnsi::Strlen(u8_str);
        FUTF8ToTCHAR conv(u8_str, tchar_len);
        switch (static_cast<bq::log_level>(log_level))
        {
        case bq::log_level::verbose:
            UE_LOG(LogBqLogEditor, VeryVerbose, TEXT("%.*s"), conv.Length(), conv.Get());
            break;
        case bq::log_level::debug:
            UE_LOG(LogBqLogEditor, Verbose, TEXT("%.*s"), conv.Length(), conv.Get());
            break;
        case bq::log_level::info:
            UE_LOG(LogBqLogEditor, Display, TEXT("%.*s"), conv.Length(), conv.Get());
            break;
        case bq::log_level::warning:
            UE_LOG(LogBqLogEditor, Warning, TEXT("%.*s"), conv.Length(), conv.Get());
            break;
        case bq::log_level::error:
            UE_LOG(LogBqLogEditor, Error, TEXT("%.*s"), conv.Length(), conv.Get());
            break;
        case bq::log_level::fatal:
            UE_LOG(LogBqLogEditor, Fatal, TEXT("%.*s"), conv.Length(), conv.Get());
            break;
        default:
            UE_LOG(LogBqLogEditor, Log, TEXT("Unkown Log Level: %.*s"), conv.Length(), conv.Get());
            break;
        }
    }
public:
    virtual void StartupModule() override
    {
        UE_LOG(LogBqLogEditor, Log, TEXT("BqLog Editor module started"));
        bq::log::register_console_callback(&on_bq_log_console_callback);
        bq::log::console(bq::log_level::info, "BqLog Editor console callback registered");
        bq::log::console(bq::log_level::info, bq::string("BqLog Version:") + bq::log::get_version());
    }
    virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FBqLogEditorModule, BqLogEditor)