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
#include "Engine/DataAsset.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/Engine.h"
#include "bq_log/bq_log.h"
#include "BqLog.generated.h"

UENUM(BlueprintType)
enum class EBqLogInitType : uint8
{
    BqLog_Create UMETA(DisplayName="Create New Log"),
    BqLog_Get UMETA(DisplayName="Get Log By Name")
};

UENUM(BlueprintType)
enum class EBqLogLevel : uint8
{
    Verbose UMETA(DisplayName="Verbose"),
    Debug   UMETA(DisplayName="Debug"),
    Info    UMETA(DisplayName="Info"),
    Warning UMETA(DisplayName="Warning"),
    Error   UMETA(DisplayName="Error"),
    Fatal   UMETA(DisplayName="Fatal")
};

UCLASS(BlueprintType)
class BQLOG_API UBqLog : public UDataAsset
{
    GENERATED_BODY()
    friend class UBqLogFunctionLibrary;
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BqLog")
    FName LogName = TEXT("");
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BqLog")
    EBqLogInitType CreateType = EBqLogInitType::BqLog_Create;

    UPROPERTY(Transient)
    bool IsCreateMode = true;

    // 受 bIsCreateMode 控制显示
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BqLog",
              meta=(MultiLine="true", EditCondition="IsCreateMode", EditConditionHides))
    FText LogConfig;

    UFUNCTION()
    void Ensure();

    // 实现基础接口
    virtual bool SupportsCategoryEnum() const { return false; }
    virtual UEnum* GetCategoryEnum() const { return nullptr; }

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
    virtual void PostLoad() override;

protected:
    virtual uint32_t get_category_count() const {return 0; }
    virtual const char* const* get_category_names() const {return nullptr;}
protected:
    uint64_t log_id_ = 0;
    bool need_renew_inst_ = true;
};

UENUM(BlueprintType)
enum class EBqLogAnyType : uint8
{
    None        UMETA(DisplayName="None"),
    Bool        UMETA(DisplayName="Bool"),
    Int64       UMETA(DisplayName="Int64"),
    Double      UMETA(DisplayName="Double"),
    String      UMETA(DisplayName="String"),
    Name        UMETA(DisplayName="Name"),
    Text        UMETA(DisplayName="Text"),
    Object      UMETA(DisplayName="Object"),
    Class       UMETA(DisplayName="Class"),
#if ENGINE_MAJOR_VERSION >= 5
    SoftObject  UMETA(DisplayName="SoftObjectPath"),
#endif
    Vector      UMETA(DisplayName="Vector"),
    Rotator     UMETA(DisplayName="Rotator"),
    Transform   UMETA(DisplayName="Transform"),
    Color       UMETA(DisplayName="Color"),
    LinearColor UMETA(DisplayName="LinearColor"),
};

USTRUCT(BlueprintType)
struct BQLOG_API FBqLogAny
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBqLogAnyType Type = EBqLogAnyType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool         B = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int64        I64 = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double       D = 0.0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString      S;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName        N;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText        T;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) UObject*     Obj = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<UObject> Cls;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FSoftObjectPath SoftPath;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector      V = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRotator     R = FRotator::ZeroRotator;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform   Xform = FTransform::Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FColor       Color = FColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor LColor = FLinearColor::White;

    UPROPERTY(Transient)    FString FormattedString;
};

UENUM()
enum ELOG_CATEGORY
{
    LOG_CATEGORY_None = 1 UMETA(DisplayName="CATEGORY_None"),
    LOG_CATEGORY_TANK_AA = 2 UMETA(DisplayName="TANK.AA"),
};