#include "BqLogFunctionLibrary.h"
#include "BqLog.h"
#include "bq_log/bq_log.h"

void UBqLogFunctionLibrary::DoBqLogFormat(const UBqLog* LogInstance, EBqLogLevel Level, int32 CategoryIndex, const FString& FormatString, const TArray<FBqLogAny>& Args)
{
    bq::log::console(bq::log_level::error, "KSLDJFLSKDFLSD");
}

void UBqLogFunctionLibrary::DoBqLog(const UBqLog* LogInstance, EBqLogLevel Level, int32 CategoryIndex, const FString& FormatString)
{
    DoBqLogFormat(LogInstance, Level, CategoryIndex, FormatString, TArray<FBqLogAny>());
}