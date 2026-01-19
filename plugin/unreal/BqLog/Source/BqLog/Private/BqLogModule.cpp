#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"
#include "bq_log/bq_log.h"
DEFINE_LOG_CATEGORY_STATIC(LogBqLog, Log, All);
class FBqLogModule : public IModuleInterface
{
private:
    static void BQ_STDCALL on_bq_log_console_callback(uint64_t log_id, int32_t category_idx, bq::log_level log_level, const char* content, int32_t length)
    {
        (void)log_id;
        (void)category_idx;
        const ANSICHAR* u8_str = reinterpret_cast<const ANSICHAR*>(content ? content : "");
        int32_t tchar_len = (length >= 0) ? length : FCStringAnsi::Strlen(u8_str);
        FUTF8ToTCHAR conv(u8_str, tchar_len);
        switch (log_level)
        {
        case bq::log_level::verbose:
            UE_LOG(LogBqLog, VeryVerbose, TEXT("%.*s"), conv.Length(), conv.Get());
            break;
        case bq::log_level::debug:
            UE_LOG(LogBqLog, Verbose, TEXT("%.*s"), conv.Length(), conv.Get());
            break;
        case bq::log_level::info:
            UE_LOG(LogBqLog, Display, TEXT("%.*s"), conv.Length(), conv.Get());
            break;
        case bq::log_level::warning:
            UE_LOG(LogBqLog, Warning, TEXT("%.*s"), conv.Length(), conv.Get());
            break;
        case bq::log_level::error:
            UE_LOG(LogBqLog, Error, TEXT("%.*s"), conv.Length(), conv.Get());
            break;
        case bq::log_level::fatal:
            UE_LOG(LogBqLog, Fatal, TEXT("%.*s"), conv.Length(), conv.Get());
            break;
        default:
            UE_LOG(LogBqLog, Log, TEXT("Unkown Log Level: %.*s"), conv.Length(), conv.Get());
            break;
        }
    }
public:
    virtual void StartupModule() override
    {
#if WITH_EDITOR
        bq::log::register_console_callback(&on_bq_log_console_callback);
        bq::log::console(bq::log_level::info, "BqLog Editor console callback registered");
        bq::log::console(bq::log_level::info, bq::string("BqLog Version:") + bq::log::get_version());

        FString Dir = FPaths::ProjectDir(); 
        FString Full = FPaths::ConvertRelativePathToFull(Dir);
        bq::log::reset_base_dir(0, bq::string(TCHAR_TO_UTF8(*Full)));
        bq::log::reset_base_dir(1, bq::string(TCHAR_TO_UTF8(*Full)));
#endif
        bq::log::console(bq::log_level::info, (bq::string("UE BqLog Base Dir Type 0:") + bq::log::get_file_base_dir(0)).c_str());
        bq::log::console(bq::log_level::info, (bq::string("UE BqLog Base Dir Type 1:") + bq::log::get_file_base_dir(1)).c_str());
    }
    virtual void ShutdownModule() override {}
};
IMPLEMENT_MODULE(FBqLogModule, BqLog)