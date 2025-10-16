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
 * BqLogExtended.h - Extended BqLog with Category Support
 *
 * \brief Example of how to extend BqLog with custom category enums
 *
 * \author pippocao
 * \date 2025.10.14
 */

#include "CoreMinimal.h"
#include "BqLog.h"
#include "BqLogExtended.generated.h"

// 示例：游戏日志分类枚举
UENUM(BlueprintType)
enum class EGameLogCategory : uint8
{
    Player    UMETA(DisplayName="Player"),
    Combat    UMETA(DisplayName="Combat"),
    Inventory UMETA(DisplayName="Inve.ntory"),
    Quest     UMETA(DisplayName="Quest"),
    System    UMETA(DisplayName="System")
};

// 示例：网络日志分类枚举
UENUM(BlueprintType)
enum class ENetworkLogCategory : uint8
{
    Connection UMETA(DisplayName="Connec.tion"),
    Data       UMETA(DisplayName="Data Transfer"),
    Error      UMETA(DisplayName="Network.Error"),
    Security   UMETA(DisplayName="Security")
};

// 游戏日志扩展类
UCLASS(BlueprintType)
class BQLOG_API UBqLogGame : public UBqLog
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Game Log")
    FName GameLogName = TEXT("GameLog");
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Game Log")
    EGameLogCategory DefaultCategory = EGameLogCategory::System;

    // 实现基础接口
    virtual FName GetLogTypeName() const override { return TEXT("Game"); }
    virtual bool SupportsCategoryEnum() const override { return true; }
    virtual UEnum* GetCategoryEnum() const override { return StaticEnum<EGameLogCategory>(); }
};

// 网络日志扩展类
UCLASS(BlueprintType)
class BQLOG_API UBqLogNetwork : public UBqLog
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network Log")
    FName NetworkLogName = TEXT("NetworkLog");
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network Log")
    ENetworkLogCategory DefaultCategory = ENetworkLogCategory::Connection;

    // 实现基础接口
    virtual FName GetLogTypeName() const override { return TEXT("Network"); }
    virtual bool SupportsCategoryEnum() const override { return true; }
    virtual UEnum* GetCategoryEnum() const override { return StaticEnum<ENetworkLogCategory>(); }
    
};
