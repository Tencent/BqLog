#include "BqLogFunctionLibrary.h"
#include "BqLog.h"
#include "bq_log/bq_log.h"
#include "bq_log/log/log_imp.h"
#include "bq_log/log/log_manager.h"

bool UBqLogFunctionLibrary::DoBqLogFormat(UBqLog* LogInstance, EBqLogLevel Level, int32 CategoryIndex, const FString& FormatString, const TArray<FBqLogAny>& Args)
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
        const char* ErrorLogCStr = TCHAR_TO_UTF8(*ErrorLog);
        bq::log::console(bq::log_level::error, ErrorLogCStr);
    }
    const uint64_t log_id = LogInstance->log_id_;

    bq::log_imp* log_impl = bq::log_manager::get_log_by_id(log_id);
    bq::log_level log_level = static_cast<bq::log_level>(static_cast<int32_t>(Level));
    if (!log_impl || !log_impl->is_enable_for(static_cast<uint32_t>(CategoryIndex), log_level))
    {
        return false;
    }
    bq::_api_u16string_def stack_string_def;
    stack_string_def.str = nullptr;
    stack_string_def.len = 0;
    if(log_impl->is_stack_trace_enable_for(log_level)){
        bq::api::__api_get_stack_trace_utf16(&stack_string_def, 0);
    }
    auto format_size_seq = bq::tools::make_size_seq<false>(FormatString);
    size_t aligned_format_data_size = bq::align_4(format_size_seq.get_element().get_value() + (stack_string_def.len << 1));
    size_t total_data_size = sizeof(bq::_log_entry_head_def) + aligned_format_data_size;
    for (FBqLogAny& Arg : const_cast<TArray<FBqLogAny>&>(Args))
    {
        switch (Arg.Type) {
        case EBqLogAnyType::None:
            total_data_size += bq::tools::make_size_seq<true>(nullptr).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Bool:
            total_data_size += bq::tools::make_size_seq<true>(true).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Int64:
            total_data_size += bq::tools::make_size_seq<true>(static_cast<int64_t>(INT64_MAX)).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Double:
            total_data_size += bq::tools::make_size_seq<true>(static_cast<double>(1.0)).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::String:
            total_data_size += bq::tools::make_size_seq<true>(Arg.S).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Name:
            total_data_size += bq::tools::make_size_seq<true>(Arg.N).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Text:
            total_data_size += bq::tools::make_size_seq<true>(Arg.T).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Object:
            if (Arg.Obj == nullptr)
            {
                total_data_size += bq::tools::make_size_seq<true>(nullptr).get_element().get_aligned_value();
            }
            else
            {
                Arg.FormattedString = Arg.Obj->GetName();
                total_data_size += bq::tools::make_size_seq<true>(Arg.FormattedString).get_element().get_aligned_value();
            }
            break;
        case EBqLogAnyType::Class:
            if (Arg.Cls.Get())
            {
                Arg.FormattedString = Arg.Cls->GetName();
                total_data_size += bq::tools::make_size_seq<true>(Arg.FormattedString).get_element().get_aligned_value();
            }
            else
            {
                total_data_size += bq::tools::make_size_seq<true>(nullptr).get_element().get_aligned_value();
            }
            break; 
#if ENGINE_MAJOR_VERSION >= 5
        case EBqLogAnyType::SoftObject:
            Arg.FormattedString = Arg.SoftPath.ToString();
            total_data_size += bq::tools::make_size_seq<true>(Arg.FormattedString).get_element().get_aligned_value();
            break;
#endif
        case EBqLogAnyType::Vector:
            Arg.FormattedString = Arg.V.ToString(); 
            total_data_size += bq::tools::make_size_seq<true>(Arg.FormattedString).get_element().get_aligned_value();
            break;  
        case EBqLogAnyType::Rotator:
            Arg.FormattedString = Arg.R.ToString(); 
            total_data_size += bq::tools::make_size_seq<true>(Arg.FormattedString).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Transform:
            Arg.FormattedString = Arg.Xform.ToString();
            total_data_size += bq::tools::make_size_seq<true>(Arg.FormattedString).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Color:
            Arg.FormattedString = Arg.Color.ToString();
            total_data_size += bq::tools::make_size_seq<true>(Arg.FormattedString).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::LinearColor:
            Arg.FormattedString = Arg.LColor.ToString();
            total_data_size += bq::tools::make_size_seq<true>(Arg.FormattedString).get_element().get_aligned_value();
            break;
        break;
        }
    }

    auto handle = bq::api::__api_log_buffer_alloc(log_id, (uint32_t)total_data_size);
    if (handle.result != bq::enum_buffer_result_code::success) {
        return false;
    }
    bq::_log_entry_head_def* head = (bq::_log_entry_head_def*)handle.data_addr;
    head->category_idx = CategoryIndex;
    head->level = static_cast<uint8_t>(log_level);
    head->log_format_str_type = static_cast<uint8_t>(bq::log_arg_type_enum::string_utf16_type);
    size_t log_args_offset = static_cast<size_t>(sizeof(bq::_log_entry_head_def) + aligned_format_data_size);
    head->log_args_offset = (uint32_t)log_args_offset;
    uint8_t* log_format_content_addr = handle.data_addr + sizeof(bq::_log_entry_head_def);
    bq::tools::_type_copy<false>(FormatString, log_format_content_addr, format_size_seq.get_element().get_value());
    if (stack_string_def.len > 0) {
        memcpy(log_format_content_addr + format_size_seq.get_element().get_value(), stack_string_def.str, stack_string_def.len << 1);
        *(uint32_t*)log_format_content_addr += (stack_string_def.len << 1);
    }
    uint8_t* log_args_addr = handle.data_addr + log_args_offset;
    for (const FBqLogAny& Arg : Args) {
        switch (Arg.Type)
        {
        case EBqLogAnyType::None:
            bq::tools::_type_copy<true>(nullptr, log_args_addr, bq::tools::make_size_seq<true>(nullptr).get_value());
            log_args_addr += bq::tools::make_size_seq<true>(nullptr).get_element().get_aligned_value();
        case EBqLogAnyType::Bool:
            bq::tools::_type_copy<true>(Arg.B, log_args_addr, bq::tools::make_size_seq<true>(Arg.B).get_value());
            log_args_addr += bq::tools::make_size_seq<true>(true).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Int64:
            bq::tools::_type_copy<true>(Arg.I64, log_args_addr, bq::tools::make_size_seq<true>(Arg.I64).get_value());
            log_args_addr += bq::tools::make_size_seq<true>(static_cast<int64_t>(Arg.I64)).get_element().get_aligned_value();
            break;
        case EBqLogAnyType::Double:
            bq::tools::_type_copy<true>(Arg.D, log_args_addr, bq::tools::make_size_seq<true>(Arg.D).get_value());
            log_args_addr += bq::tools::make_size_seq<true>(static_cast<double>(Arg.D)).get_element().get_aligned_value();
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
#if ENGINE_MAJOR_VERSION >= 5
            case EBqLogAnyType::SoftObject:
#endif
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
    bq::api::__api_log_buffer_commit(log_id, handle);
    return true;
}

bool UBqLogFunctionLibrary::DoBqLog(UBqLog* LogInstance, EBqLogLevel Level, int32 CategoryIndex, const FString& FormatString)
{
    return DoBqLogFormat(LogInstance, Level, CategoryIndex, FormatString, TArray<FBqLogAny>());
}