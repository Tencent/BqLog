#pragma once

#include "CoreMinimal.h"
#include "BqLog.h"
#include "K2Node.h"
#include "K2Node_AddPinInterface.h"
#include "K2Node_BqLog.generated.h"

UCLASS()
class BQLOGEDITOR_API UK2Node_BqLogFormat : public UK2Node, public IK2Node_AddPinInterface
{
	GENERATED_BODY()
public:
	static const FName LogInstancePinName;
	static const FName LogLevelPinName;
	static const FName LogFormatStringPinName;
	static const FName CategoryPinName; // 新增：动态分类枚举引脚
	static const FName ArgPinPrefix;
	static const FName ReturnPinName; // 新增：返回值引脚名

	// UK2Node 接口
	virtual void PostLoad() override;
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual void GetMenuActions(class FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, class UEdGraph* SourceGraph) override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;

	// UE4/UE5/UE6 兼容的动态pin接口
	virtual bool CanAddPin() const override;
	virtual void AddInputPin() override;
	void RemoveArgumentPin(UEdGraphPin* Pin);

#if ENGINE_MAJOR_VERSION >= 5
	// UE5/UE6 特有的接口方法
	virtual void AddPin() { AddInputPin(); }
	virtual bool CanRemovePin(const UEdGraphPin* Pin) const override;
	virtual void RemoveInputPin(class UEdGraphPin* Pin) override;
	
	// UE5.6+ Details面板支持
	virtual void GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;
	
	// UE5.6+ Details面板属性支持
	virtual bool ShouldShowNodeProperties() const override { return true; }
	virtual bool IsNodePure() const override { return false; }
	virtual bool CanUserDeleteNode() const override { return true; }
	virtual bool CanDuplicateNode() const override { return true; }
	
	// UE5.6+ 参数管理UI支持
	virtual bool SupportsAddPin() const { return true; }
	virtual bool SupportsRemovePin() const { return true; }
	
	// UE5.6+ Details面板参数管理
	virtual int32 GetNumArguments() const;
	virtual void AddArgument();
	virtual void RemoveArgument(int32 Index);
	virtual bool CanRemoveArgument(int32 Index) const;
#endif

	// Wildcard 类型协商
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;

	// 动态分类枚举支持
	void OnLogInstanceChanged();
	void RefreshCategoryPinStatus();

    virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;

	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface

private:
	/** When adding arguments to the node, their names are placed here and are generated as pins during construction */
	UPROPERTY(EditAnywhere, Category = "Arguments", meta = (DisplayName = "Arguments"))
	TArray<FName> ArgumentNames;

	
	UEdGraphPin* GetLogExecutePin() const;
	UEdGraphPin* GetLogThenPin() const;
	UEdGraphPin* GetLogInstancePin() const;
	UEdGraphPin* GetLogLevelPin() const;
	UEdGraphPin* GetLogFormatStringPin() const;

	UEdGraphPin* CreateNextArgPin();
	void GetArgPins(TArray<UEdGraphPin*>& OutPins) const;
	static FName GetArgNameFromPin(const UEdGraphPin* Pin);
	FName GetUniqueArgumentName() const;

private:
	UPROPERTY()
	TSubclassOf<UBqLog> SavedLogType = nullptr;
	UPROPERTY(Transient)
	TSet<FName> PinsToPurge;
};