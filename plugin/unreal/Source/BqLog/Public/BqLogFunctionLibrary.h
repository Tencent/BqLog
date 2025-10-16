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

public:
    // 统一使用 UBqLog，支持各类扩展子类；新增 CategoryValue 数值参数
    UFUNCTION(BlueprintCallable, Category="BqLog", meta=(BlueprintInternalUseOnly="true"))
    static void DoBqLogFormat(const UBqLog* LogInstance, EBqLogLevel Level, int32 CategoryIndex, const FString& FormatString, const TArray<FBqLogAny>& Args);
    UFUNCTION(BlueprintCallable, Category="BqLog", meta=(BlueprintInternalUseOnly="true"))
    static void DoBqLog(const UBqLog* LogInstance, EBqLogLevel Level, int32 CategoryIndex, const FString& FormatString);
};
