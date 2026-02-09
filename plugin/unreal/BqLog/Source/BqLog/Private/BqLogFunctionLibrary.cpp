#include "BqLogFunctionLibrary.h"
#include "BqLog.h"
#include "bq_log/bq_log.h"


bool UBqLogFunctionLibrary::EnsureLogInstance(UBqLog* LogInstance)
{
    if (!LogInstance)
    {
        bq::log::console(bq::log_level::error, "LogInstance is null");
        return false;
    }
    LogInstance->Ensure();
    if (LogInstance->log_id_ == 0)
    {
        FSoftObjectPath SoftPath(LogInstance);
        const FString ErrorLog = FString(TEXT("LogInstance is invalid: ")) + SoftPath.ToString();
        bq::string error_str = TCHAR_TO_UTF8(*ErrorLog);
        bq::log::console(bq::log_level::error, error_str.c_str());
        return false;
    }
    return true;
}

bool UBqLogFunctionLibrary::DoBqLogFormat(UBqLog* LogInstance, EBqLogLevel Level, int32 CategoryIndex, const FString& FormatString, const TArray<FBqLogAny>& Args)
{
    if (!EnsureLogInstance(LogInstance))
    {
        return false;
    }
    const uint64_t log_id = LogInstance->log_id_;
    if (!(((*LogInstance->merged_log_level_bitmap_ & (1U << static_cast<int32_t>(Level))) != 0) && LogInstance->categories_mask_array_[CategoryIndex]))
    {
        return false;
    }
    bq::_api_u16string_def stack_string_def;
    stack_string_def.str = nullptr;
    stack_string_def.len = 0;
    bool should_print_stack = (*LogInstance->print_stack_level_bitmap_ & (1 << static_cast<int32_t>(Level)));
    if(should_print_stack){
        bq::api::__api_get_stack_trace_utf16(&stack_string_def, 0);
    }
    size_t format_size = bq::tools::_serialize_str_helper_by_type<FString>::get_storage_data_size(FormatString);
    size_t total_format_data_size = format_size + stack_string_def.len;
    size_t total_args_size = 0;
    for (FBqLogAny& Arg : const_cast<TArray<FBqLogAny>&>(Args))
    {
        switch (Arg.Type) {
        case EBqLogAnyType::None:
            total_args_size += bq::tools::make_size_seq<true>(nullptr).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Bool:
            total_args_size += bq::tools::make_size_seq<true>(true).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Int32:
            total_args_size += bq::tools::make_size_seq<true>(static_cast<int32_t>(Arg.I32)).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Int64:
            total_args_size += bq::tools::make_size_seq<true>(static_cast<int64_t>(Arg.I64)).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Float:
            total_args_size += bq::tools::make_size_seq<true>(Arg.F).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Double:
            total_args_size += bq::tools::make_size_seq<true>(Arg.D).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::String:
            total_args_size += bq::tools::make_size_seq<true>(Arg.S).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Name:
            total_args_size += bq::tools::make_size_seq<true>(Arg.N).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Text:
            total_args_size += bq::tools::make_size_seq<true>(Arg.T).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Object:
            if (Arg.Obj == nullptr)
            {
                total_args_size += bq::tools::make_size_seq<true>(nullptr).get_element().get_aligned_value();
            }
            else
            {
                Arg.FormattedString = Arg.Obj->GetName();
                total_args_size += bq::tools::make_size_seq<true>(Arg.FormattedString).get_element().get_aligned_value();
            }
            break;
        case EBqLogAnyType::Class:
            if (Arg.Cls.Get())
            {
                Arg.FormattedString = Arg.Cls->GetName();
                total_args_size += bq::tools::make_size_seq<true>(Arg.FormattedString).get_element().get_aligned_value();
            }
            else
            {
                total_args_size += bq::tools::make_size_seq<true>(nullptr).get_element().get_aligned_value();
            }
            break;
        case EBqLogAnyType::SoftObject:
            Arg.FormattedString = Arg.SoftPath.ToString();
            total_args_size += bq::tools::make_size_seq<true>(Arg.FormattedString).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Vector:
            Arg.FormattedString = Arg.V.ToString();
            total_args_size += bq::tools::make_size_seq<true>(Arg.FormattedString).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Rotator:
            Arg.FormattedString = Arg.R.ToString();
            total_args_size += bq::tools::make_size_seq<true>(Arg.FormattedString).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Transform:
            Arg.FormattedString = Arg.Xform.ToString();
            total_args_size += bq::tools::make_size_seq<true>(Arg.FormattedString).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Color:
            Arg.FormattedString = Arg.Color.ToString();
            total_args_size += bq::tools::make_size_seq<true>(Arg.FormattedString).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::LinearColor:
            Arg.FormattedString = Arg.LColor.ToString();
            total_args_size += bq::tools::make_size_seq<true>(Arg.FormattedString).get_element().get_aligned_value();
            break;
        break;
        }
    }
    const void* format_data_ptr = should_print_stack ? nullptr : bq::tools::_serialize_str_helper_by_type<FString>::get_storage_data_addr(FormatString);
    auto handle = bq::api::__api_log_write_begin(log_id
        , static_cast<uint8_t>(static_cast<int32_t>(Level))
        , static_cast<uint32_t>(CategoryIndex)
        , static_cast<uint8_t>(bq::log_arg_type_enum::string_utf16_type)
        , static_cast<uint32_t>(total_format_data_size)
        , format_data_ptr
        , static_cast<uint32_t>(total_args_size));
    if (handle.result != bq::enum_buffer_result_code::success) {
        return false;
    }
    if(!format_data_ptr){
        //Ugly hack
        bq::tools::_type_copy<false>(FormatString, handle.format_data_addr - sizeof(uint32_t), total_format_data_size + sizeof(uint32_t));
        memcpy(handle.format_data_addr + format_size, stack_string_def.str, stack_string_def.len);
        *reinterpret_cast<uint32_t*>(handle.format_data_addr - sizeof(uint32_t)) = static_cast<uint32_t>(total_format_data_size);
    }
    uint8_t* log_args_addr = handle.format_data_addr + bq::align_4(total_format_data_size);
    for (const FBqLogAny& Arg : Args) {
        switch (Arg.Type)
        {
        case EBqLogAnyType::None:
            bq::tools::_type_copy<true>(nullptr, log_args_addr, bq::tools::make_size_seq<true>(nullptr).get_value());
            log_args_addr += bq::tools::make_size_seq<true>(nullptr).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Bool:
            bq::tools::_type_copy<true>(Arg.B, log_args_addr, bq::tools::make_size_seq<true>(Arg.B).get_value());
            log_args_addr += bq::tools::make_size_seq<true>(Arg.B).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Int32:
            bq::tools::_type_copy<true>(Arg.I32, log_args_addr, bq::tools::make_size_seq<true>(Arg.I32).get_value());
            log_args_addr += bq::tools::make_size_seq<true>(static_cast<int32_t>(Arg.I32)).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Int64:
            bq::tools::_type_copy<true>(Arg.I64, log_args_addr, bq::tools::make_size_seq<true>(Arg.I64).get_value());
            log_args_addr += bq::tools::make_size_seq<true>(static_cast<int64_t>(Arg.I64)).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Float:
            bq::tools::_type_copy<true>(Arg.F, log_args_addr, bq::tools::make_size_seq<true>(Arg.F).get_value());
            log_args_addr += bq::tools::make_size_seq<true>(Arg.F).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Double:
            bq::tools::_type_copy<true>(Arg.D, log_args_addr, bq::tools::make_size_seq<true>(Arg.D).get_value());
            log_args_addr += bq::tools::make_size_seq<true>(Arg.D).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::String:
            bq::tools::_type_copy<true>(Arg.S, log_args_addr, bq::tools::make_size_seq<true>(Arg.S).get_value());
            log_args_addr += bq::tools::make_size_seq<true>(Arg.S).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Name:
            bq::tools::_type_copy<true>(Arg.N, log_args_addr, bq::tools::make_size_seq<true>(Arg.N).get_value());
            log_args_addr += bq::tools::make_size_seq<true>(Arg.N).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Text:
            bq::tools::_type_copy<true>(Arg.T, log_args_addr, bq::tools::make_size_seq<true>(Arg.T).get_value());
            log_args_addr += bq::tools::make_size_seq<true>(Arg.T).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Object:
            if (Arg.Obj == nullptr)
            {
                bq::tools::_type_copy<true>(nullptr, log_args_addr, bq::tools::make_size_seq<true>(nullptr).get_value());
                log_args_addr += bq::tools::make_size_seq<true>(nullptr).get_element().get_aligned_value();
            }
            else {
                bq::tools::_type_copy<true>(Arg.FormattedString, log_args_addr, bq::tools::make_size_seq<true>(Arg.FormattedString).get_value());
                log_args_addr += bq::tools::make_size_seq<true>(Arg.FormattedString).get_element().get_aligned_value();
            }
            break;
        case EBqLogAnyType::Class:
            if (!Arg.Cls.Get())
            {
                bq::tools::_type_copy<true>(nullptr, log_args_addr, bq::tools::make_size_seq<true>(nullptr).get_value());
                log_args_addr += bq::tools::make_size_seq<true>(nullptr).get_element().get_aligned_value();
            }else{
                bq::tools::_type_copy<true>(Arg.FormattedString, log_args_addr, bq::tools::make_size_seq<true>(Arg.FormattedString).get_value());
                log_args_addr += bq::tools::make_size_seq<true>(Arg.FormattedString).get_element().get_aligned_value();
            }
            break;
            case EBqLogAnyType::SoftObject:
            case EBqLogAnyType::Vector:
            case EBqLogAnyType::Rotator:
            case EBqLogAnyType::Transform:
            case EBqLogAnyType::Color:
            case EBqLogAnyType::LinearColor:
            {
                bq::tools::_type_copy<true>(Arg.FormattedString, log_args_addr, bq::tools::make_size_seq<true>(Arg.FormattedString).get_value());
                log_args_addr += bq::tools::make_size_seq<true>(Arg.FormattedString).get_element().get_aligned_value();
            }
        default:
            break;
        }
    }
    bq::api::__api_log_write_finish(log_id, handle);
    return true;
}

bool UBqLogFunctionLibrary::DoBqLog(UBqLog* LogInstance, EBqLogLevel Level, int32 CategoryIndex, const FString& FormatString)
{
    return DoBqLogFormat(LogInstance, Level, CategoryIndex, FormatString, TArray<FBqLogAny>());
}

void UBqLogFunctionLibrary::BqLogForceFlushAllLogs(){
    bq::log::force_flush_all_logs();
}
    
void UBqLogFunctionLibrary::BqLogForceFlush(UBqLog* LogInstance)
{
    if (!EnsureLogInstance(LogInstance))
    {
        return;
    }
    const uint64_t log_id = LogInstance->log_id_;
    bq::api::__api_force_flush(log_id);
}

bool UBqLogFunctionLibrary::BqLogResetConfig(UBqLog* LogInstance, const FString& Config)
{
    if (!EnsureLogInstance(LogInstance))
    {
        return false;
    }
    const uint64_t log_id = LogInstance->log_id_;
    return bq::api::__api_log_reset_config(TCHAR_TO_UTF8(*LogInstance->LogName.ToString()), TCHAR_TO_UTF8(*Config));
}

void UBqLogFunctionLibrary::BqLogSetAppenderEnable(UBqLog* LogInstance, const FString& AppenderName, bool bEnable)
{
    if (!EnsureLogInstance(LogInstance))
    {
        return;
    }
    const uint64_t log_id = LogInstance->log_id_;
    bq::api::__api_set_appender_enable(log_id, TCHAR_TO_UTF8(*AppenderName), bEnable);
}

bool UBqLogFunctionLibrary::BqLogIsValid(UBqLog* LogInstance)
{
    if (!EnsureLogInstance(LogInstance))
    {
        return false;
    }
    const uint64_t log_id = LogInstance->log_id_;
    return log_id != 0;
}

FString UBqLogFunctionLibrary::BqLogTakeSnapshot(UBqLog* LogInstance, const FString& TimeZoneConfigStr)
{
    if (!EnsureLogInstance(LogInstance))
    {
        return FString();
    }
    const uint64_t log_id = LogInstance->log_id_;
    bq::_api_string_def snapshot_str_def;
    snapshot_str_def.str = nullptr;
    snapshot_str_def.len = 0;
    bq::api::__api_take_snapshot_string(log_id, TCHAR_TO_UTF8(*TimeZoneConfigStr), &snapshot_str_def);
    FString SnapshotString;
    if (snapshot_str_def.len > 0 && snapshot_str_def.str != nullptr)
    {
        SnapshotString = snapshot_str_def.str;
        FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(snapshot_str_def.str), snapshot_str_def.len);
        SnapshotString = FString(Converter.Get(), Converter.Length());
    }
    bq::api::__api_release_snapshot_string(log_id, &snapshot_str_def);
    return SnapshotString;
}