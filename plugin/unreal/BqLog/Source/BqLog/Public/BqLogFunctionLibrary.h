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
/*!
 * bq_log.h UE Adapter
 *
 * \brief
 *
 * \author pippocao
 * \date 2025.10.14
 */

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BqLog.h"
#include "BqLogFunctionLibrary.generated.h"

UCLASS()
class BQLOG_API UBqLogFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

private:
    static bool EnsureLogInstance(UBqLog* LogInstance);

public:
    UFUNCTION(BlueprintCallable, Category="BqLog", meta=(BlueprintInternalUseOnly="true"))
    static bool DoBqLogFormat(UBqLog* LogInstance, EBqLogLevel Level, int32 CategoryIndex, const FString& FormatString, const TArray<FBqLogAny>& Args);
    UFUNCTION(BlueprintCallable, Category="BqLog", meta=(BlueprintInternalUseOnly="true"))
    static bool DoBqLog(UBqLog* LogInstance, EBqLogLevel Level, int32 CategoryIndex, const FString& FormatString);

    UFUNCTION(BlueprintCallable, Category="BqLog")
    static void BqLogForceFlushAllLogs();
    
    UFUNCTION(BlueprintCallable, Category="BqLog")
    static void BqLogForceFlush(UBqLog* LogInstance);

    UFUNCTION(BlueprintCallable, Category="BqLog")
    static bool BqLogResetConfig(UBqLog* LogInstance, const FString& Config);

    UFUNCTION(BlueprintCallable, Category="BqLog")
    static void BqLogSetAppenderEnable(UBqLog* LogInstance, const FString& AppenderName, bool bEnable);

    UFUNCTION(BlueprintCallable, Category="BqLog")
    static bool BqLogIsValid(UBqLog* LogInstance);

    UFUNCTION(BlueprintCallable, Category="BqLog")
    static FString BqLogTakeSnapshot(UBqLog* LogInstance, const FString& TimeZoneConfigStr);
};
