#include "K2Node_BqLog.h"
#include "BqLogFunctionLibrary.h"
#include "BqLog.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompiler.h"
#include "K2Node_CallFunction.h"
#include "K2Node_MakeArray.h"
#include "K2Node_MakeStruct.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/KismetTextLibrary.h"
#include "EdGraphSchema_K2.h"

#if ENGINE_MAJOR_VERSION >= 5
#include "ToolMenus.h"
#include "GraphEditorActions.h"
#include "Framework/Commands/UIAction.h"
#endif

const FName UK2Node_BqLogFormatRaw::LogInstancePinName(TEXT("LogInstance"));
const FName UK2Node_BqLogFormatRaw::LogLevelPinName(TEXT("LogLevel"));
const FName UK2Node_BqLogFormatRaw::LogFormatStringPinName(TEXT("Log Format String"));
const FName UK2Node_BqLogFormatRaw::CategoryPinName(TEXT("Category"));
const FName UK2Node_BqLogFormatRaw::ArgPinPrefix(TEXT("Arg"));

FText UK2Node_BqLogFormatRaw::GetNodeTitle(ENodeTitleType::Type) const
{
    return NSLOCTEXT("BqLog", "BqLogFormatRawTitle", "BqLog (Format Support)");
}

FText UK2Node_BqLogFormatRaw::GetTooltipText() const
{
    return NSLOCTEXT("BqLog", "BqLogFormatRawTooltip", "Collect raw typed args into FBqLogAny[], then submit to runtime without formatting.");
}

FSlateIcon UK2Node_BqLogFormatRaw::GetIconAndTint(FLinearColor& OutColor) const
{
#if ENGINE_MAJOR_VERSION >= 5
    static FSlateIcon Icon("EditorStyle", "Kismet.AllClasses.FunctionIcon");
#else
    static FSlateIcon Icon("EditorStyle", "Kismet.AllClasses.FunctionIcon");
#endif
    OutColor = FLinearColor::White;
    return Icon;
}

void UK2Node_BqLogFormatRaw::PostLoad()
{
    Super::PostLoad();
    RefreshCategoryPinStatus();
}


void UK2Node_BqLogFormatRaw::AllocateDefaultPins()
{
    UE_LOG(LogTemp, Warning, TEXT("AllocateDefaultPins called"));
    CreatePin(EGPD_Input,  UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
    CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);

    UEdGraphPin* LogInstancePin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UBqLog::StaticClass(), LogInstancePinName);
    LogInstancePin->bNotConnectable = false;
    
    // 允许连接 UBqLog 的所有子类
    LogInstancePin->PinType.PinSubCategoryObject = UBqLog::StaticClass();
    UE_LOG(LogTemp, Warning, TEXT("AllocateDefaultPins: LogInstance pin created with class: %s"), *UBqLog::StaticClass()->GetName());

    UEdGraphPin* LogLevelPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Byte, StaticEnum<EBqLogLevel>(), LogLevelPinName);
    LogLevelPin->DefaultValue = TEXT("Info"); // 默认Info级别

    UEdGraphPin* LogFormatStringPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_String, LogFormatStringPinName);
    LogFormatStringPin->DefaultValue = TEXT("");

    // 动态创建分类枚举引脚（如果当前 LogInstance 支持）
    UE_LOG(LogTemp, Warning, TEXT("AllocateDefaultPins: Checking if should show category pin"));
    bool bShouldShow = false;
    if (SavedLogType.GetDefaultObject())
    {
        bShouldShow = SavedLogType.GetDefaultObject()->SupportsCategoryEnum();
    }

    UE_LOG(LogTemp, Warning, TEXT("AllocateDefaultPins: ShouldShowCategoryPin returned: %s"), bShouldShow ? TEXT("true") : TEXT("false"));
    
    if (bShouldShow)
    {
        UE_LOG(LogTemp, Warning, TEXT("AllocateDefaultPins: Getting category enum"));
        UEnum* CategoryEnum = SavedLogType.GetDefaultObject()->GetCategoryEnum();
        UE_LOG(LogTemp, Warning, TEXT("AllocateDefaultPins: CategoryEnum: %s"), CategoryEnum ? *CategoryEnum->GetName() : TEXT("null"));
        
        if (CategoryEnum)
        {
            UEdGraphPin* CategoryPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Byte, CategoryEnum, CategoryPinName);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("AllocateDefaultPins: CategoryEnum is null!"));
        }
    }

    // 根据ArgumentNames数组创建参数引脚（默认不创建任何参数）
    for (const FName& ArgName : ArgumentNames)
    {
        UE_LOG(LogTemp, Warning, TEXT("Creating argument pin: %s"), *ArgName.ToString());
        CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, ArgName);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("AllocateDefaultPins completed, total pins: %d"), Pins.Num());
    
    // 打印所有引脚信息
    for (int32 i = 0; i < Pins.Num(); ++i)
    {
        UEdGraphPin* Pin = Pins[i];
        UE_LOG(LogTemp, Warning, TEXT("Pin[%d]: %s (%s)"), i, *Pin->PinName.ToString(), 
               Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
    }
}


UEdGraphPin* UK2Node_BqLogFormatRaw::GetLogExecutePin() const { return FindPinChecked(UEdGraphSchema_K2::PN_Execute, EGPD_Input); }
UEdGraphPin* UK2Node_BqLogFormatRaw::GetLogThenPin() const { return FindPinChecked(UEdGraphSchema_K2::PN_Then, EGPD_Output); }
UEdGraphPin* UK2Node_BqLogFormatRaw::GetLogInstancePin() const { return FindPinChecked(LogInstancePinName, EGPD_Input); }
UEdGraphPin* UK2Node_BqLogFormatRaw::GetLogLevelPin() const { return FindPinChecked(LogLevelPinName, EGPD_Input); }
UEdGraphPin* UK2Node_BqLogFormatRaw::GetLogFormatStringPin() const { return FindPinChecked(LogFormatStringPinName, EGPD_Input); }

void UK2Node_BqLogFormatRaw::AddInputPin()
{
    UE_LOG(LogTemp, Warning, TEXT("AddInputPin called"));
    
    // 生成新的参数名称
    FName NewArgName = GetUniqueArgumentName();
    ArgumentNames.Add(NewArgName);
    
    UE_LOG(LogTemp, Warning, TEXT("Added argument: %s"), *NewArgName.ToString());
    
    // 重新构建节点
    ReconstructNode();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
}

// UE4/UE5 兼容的动态pin接口实现
bool UK2Node_BqLogFormatRaw::CanAddPin() const
{
    UE_LOG(LogTemp, Warning, TEXT("CanAddPin called - returning true"));
    return true;
}

#if ENGINE_MAJOR_VERSION >= 5
// UE5 特有的接口方法实现
bool UK2Node_BqLogFormatRaw::CanRemovePin(const UEdGraphPin* Pin) const
{
    UE_LOG(LogTemp, Warning, TEXT("CanRemovePin (UE5+) called for pin: %s"), Pin ? *Pin->PinName.ToString() : TEXT("NULL"));
    
    if (!Pin || Pin->Direction != EGPD_Input) 
    {
        UE_LOG(LogTemp, Warning, TEXT("CanRemovePin: Pin is null or not input pin"));
        return false;
    }
    
    // 不能删除核心引脚
    if (Pin->PinName == UEdGraphSchema_K2::PN_Execute || Pin->PinName == LogInstancePinName || Pin->PinName == LogFormatStringPinName) 
    {
        UE_LOG(LogTemp, Warning, TEXT("CanRemovePin: Cannot remove core pin"));
        return false;
    }
    
    // 检查是否是参数引脚
    if (!Pin->PinName.ToString().StartsWith(ArgPinPrefix.ToString()))
    {
        UE_LOG(LogTemp, Warning, TEXT("CanRemovePin: Not an argument pin"));
        return false;
    }
    
    // 参数引脚允许删除到 0 个
    UE_LOG(LogTemp, Warning, TEXT("CanRemovePin: argument pin, allow remove"));
    return true;
}

void UK2Node_BqLogFormatRaw::RemoveArgumentPin(UEdGraphPin* Pin)
{
    UE_LOG(LogTemp, Warning, TEXT("RemovePin (UE5+) called for pin: %s"), Pin ? *Pin->PinName.ToString() : TEXT("NULL"));
    
    if (!Pin)
    {
        UE_LOG(LogTemp, Warning, TEXT("RemovePin: Pin is null"));
        return;
    }
    if (!CanRemovePin(Pin))
    {
        UE_LOG(LogTemp, Warning, TEXT("RemovePin: Cannot remove this pin"));
        return;
    }
    RemoveInputPin(Pin);
}

// UE5.6+ Details面板支持
void UK2Node_BqLogFormatRaw::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
    UE_LOG(LogTemp, Warning, TEXT("GetNodeContextMenuActions called"));
    
    // 调用父类实现
    Super::GetNodeContextMenuActions(Menu, Context);
    
    // 添加自定义菜单项
    if (Context && Context->Node)
    {
        FToolMenuSection& Section = Menu->AddSection("BqLogActions", NSLOCTEXT("BqLog", "BqLogActions", "BqLog Actions"));
        
        Section.AddMenuEntry(
            "AddArgument",
            NSLOCTEXT("BqLog", "AddArgument", "Add Argument"),
            NSLOCTEXT("BqLog", "AddArgumentTooltip", "Add a new argument pin"),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateLambda([this]() {
                UE_LOG(LogTemp, Warning, TEXT("Add Argument menu clicked"));
                const_cast<UK2Node_BqLogFormatRaw*>(this)->AddInputPin();
            }))
        );
    }
}

// UE5.6+ Details面板参数管理实现
int32 UK2Node_BqLogFormatRaw::GetNumArguments() const
{
    int32 Count = 0;
    for (UEdGraphPin* Pin : Pins)
    {
        if (Pin->Direction == EGPD_Input && Pin->PinName.ToString().StartsWith(ArgPinPrefix.ToString()))
        {
            ++Count;
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("GetNumArguments: %d"), Count);
    return Count;
}

void UK2Node_BqLogFormatRaw::AddArgument()
{
    UE_LOG(LogTemp, Warning, TEXT("AddArgument called"));
    AddInputPin();
}

void UK2Node_BqLogFormatRaw::RemoveArgument(int32 Index)
{
    UE_LOG(LogTemp, Warning, TEXT("RemoveArgument called for index: %d"), Index);
    
    // 找到指定索引的参数引脚
    int32 CurrentIndex = 0;
    for (UEdGraphPin* Pin : Pins)
    {
        if (Pin->Direction == EGPD_Input && Pin->PinName.ToString().StartsWith(ArgPinPrefix.ToString()))
        {
            if (CurrentIndex == Index)
            {
                UE_LOG(LogTemp, Warning, TEXT("Removing argument pin: %s"), *Pin->PinName.ToString());
                RemoveArgumentPin(Pin);
                break;
            }
            ++CurrentIndex;
        }
    }
}

bool UK2Node_BqLogFormatRaw::CanRemoveArgument(int32 Index) const
{
    int32 ArgCount = GetNumArguments();
    bool CanRemove = (ArgCount > 0 && Index >= 0 && Index < ArgCount);
    UE_LOG(LogTemp, Warning, TEXT("CanRemoveArgument: Index=%d, ArgCount=%d, CanRemove=%s"), 
           Index, ArgCount, CanRemove ? TEXT("true") : TEXT("false"));
    return CanRemove;
}
#endif

void UK2Node_BqLogFormatRaw::RemoveInputPin(UEdGraphPin* Pin)
{
    UE_LOG(LogTemp, Warning, TEXT("RemoveInputPin called for pin: %s"), Pin ? *Pin->PinName.ToString() : TEXT("NULL"));
    if (!Pin || Pin->Direction != EGPD_Input) return;
    if (Pin->PinName == UEdGraphSchema_K2::PN_Execute || Pin->PinName == LogInstancePinName || Pin->PinName == LogFormatStringPinName) return;
    if (Pin->PinName.ToString().StartsWith(ArgPinPrefix.ToString()) || Pin->PinName.ToString().Equals(CategoryPinName.ToString()))
    {
        ArgumentNames.Remove(Pin->PinName);
        PinsToPurge.Add(Pin->PinName);
        Modify();
        UE_LOG(LogTemp, Warning, TEXT("Removing pin: %s"), *Pin->PinName.ToString());
        Pin->BreakAllPinLinks();
        ReconstructNode();
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
        if (UEdGraph* Graph = GetGraph()) { Graph->NotifyNodeChanged(this); }
    }
}

void UK2Node_BqLogFormatRaw::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
    UClass* ActionKey = GetClass();
    if (ActionRegistrar.IsOpenForRegistration(ActionKey))
    {
        ActionRegistrar.AddBlueprintAction(ActionKey, UBlueprintNodeSpawner::Create(ActionKey));
    }
}

FText UK2Node_BqLogFormatRaw::GetMenuCategory() const
{
    return NSLOCTEXT("BqLog", "BqLogCategory", "BqLog");
}

void UK2Node_BqLogFormatRaw::ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const
{
    Super::ValidateNodeDuringCompilation(MessageLog);
    if (UEdGraphPin* CatPin = const_cast<UK2Node_BqLogFormatRaw*>(this)->FindPin(CategoryPinName, EGPD_Input))
    {
        UEnum* CategoryEnum = Cast<UEnum>(CatPin->PinType.PinSubCategoryObject.Get());
        if (!CategoryEnum)
        {
            return; // 未显示Category或无效，忽略
        }
        // 若已连线，值来源于上游数据流，不在此处做常量合法性校验
        if (CatPin->LinkedTo.Num() > 0)
        {
            return;
        }
        // 名字->值 映射，失败则报错
        const FString NameStr = CatPin->DefaultValue;
        int32 Index = INDEX_NONE;

        // 兼容 "EnumName::Member" 或仅 "Member"
        FString CleanName = NameStr;
        int32 SepPos = INDEX_NONE;
        if (CleanName.FindChar(':', SepPos))
        {
            int32 Pos = CleanName.Find(TEXT("::"));
            if (Pos != INDEX_NONE) CleanName = CleanName.Mid(Pos + 2);
        }

#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MINOR_VERSION >= 22
        Index = CategoryEnum->GetIndexByName(FName(CategoryEnum->GenerateEnumPrefix() + FString(TEXT("::")) + *CleanName));
#else
        Index = CategoryEnum->GetIndexByNameString(CategoryEnum->GenerateEnumPrefix() + FString(TEXT("::")) + CleanName);
#endif
        if (Index == INDEX_NONE)
        {
            MessageLog.Error(*NSLOCTEXT("BqLog", "Error_InvalidCategory", "Invalid Category selection on @@").ToString(), this);
            return;
        }
    }
}

void UK2Node_BqLogFormatRaw::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
    if (!SavedLogType)
    {
        for (UEdGraphPin* Pin : OldPins)
        {
            if (Pin && Pin->Direction == EGPD_Input && Pin->PinName == LogInstancePinName)
            {
                UClass* Detected = nullptr;
                if (UObject* DefObj = Pin->DefaultObject)
                {
                    if (auto* AsLog = Cast<UBqLog>(DefObj))
                        Detected = AsLog->GetClass();
                }
                if (!Detected && Pin->LinkedTo.Num() > 0)
                {
                    if (UEdGraphPin* L = Pin->LinkedTo[0])
                    {
                        if (UClass* LinkedCls = Cast<UClass>(L->PinType.PinSubCategoryObject.Get()))
                        {
                            if (LinkedCls->IsChildOf(UBqLog::StaticClass()))
                                Detected = LinkedCls;
                        }
                    }
                }
                if (Detected)
                {
                    SavedLogType = Detected;
                }
                break;
            }
        }
    }
    for (int32 i = OldPins.Num() - 1; i >= 0; --i)
    {
        UEdGraphPin* P = OldPins[i];
        if (!P)
        {
            continue;
        }
        if (PinsToPurge.Contains(P->PinName))
        {
            P->BreakAllPinLinks();
            OldPins.RemoveAt(i);
        }
    }
    Super::ReallocatePinsDuringReconstruction(OldPins);
    PinsToPurge.Empty();
}

UEdGraphPin* UK2Node_BqLogFormatRaw::CreateNextArgPin()
{
    UE_LOG(LogTemp, Warning, TEXT("CreateNextArgPin called"));
    int32 MaxIdx = -1;
    for (UEdGraphPin* P : Pins)
    {
        if (P->Direction == EGPD_Input &&
            P->PinName.ToString().StartsWith(ArgPinPrefix.ToString()))
        {
            int32 N = -1;
            LexTryParseString(N, *P->PinName.ToString().RightChop(ArgPinPrefix.ToString().Len()));
            MaxIdx = FMath::Max(MaxIdx, N);
        }
    }
    const int32 NewIdx = MaxIdx + 1;
    UE_LOG(LogTemp, Warning, TEXT("Creating new pin with index: %d"), NewIdx);
    UEdGraphPin* NewPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, *FString::Printf(TEXT("%s%d"), *ArgPinPrefix.ToString(), NewIdx));
    NewPin->bNotConnectable = false;
    UE_LOG(LogTemp, Warning, TEXT("Created pin: %s"), *NewPin->PinName.ToString());
    return NewPin;
}

void UK2Node_BqLogFormatRaw::GetArgPins(TArray<UEdGraphPin*>& OutPins) const
{
    for (UEdGraphPin* P : Pins)
    {
        if (P->Direction == EGPD_Input &&
            P->PinName.ToString().StartsWith(ArgPinPrefix.ToString()))
        {
            OutPins.Add(P);
        }
    }
    OutPins.Sort([this](const UEdGraphPin& A, const UEdGraphPin& B){
        int32 IA = -1, IB = -1;
        LexTryParseString(IA, *A.PinName.ToString().RightChop(ArgPinPrefix.ToString().Len()));
        LexTryParseString(IB, *B.PinName.ToString().RightChop(ArgPinPrefix.ToString().Len()));
        return IA < IB;
    });
}

FName UK2Node_BqLogFormatRaw::GetArgNameFromPin(const UEdGraphPin* Pin)
{
    if (Pin && Pin->PinFriendlyName.IsEmpty() == false)
        return FName(*Pin->PinFriendlyName.ToString());
    return Pin ? Pin->PinName : NAME_None;
}

void UK2Node_BqLogFormatRaw::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
    Super::NotifyPinConnectionListChanged(Pin);
    if (!Pin || Pin->Direction != EGPD_Input) return;
    
    // 如果 LogInstance 引脚发生变化，重新构建节点以更新分类枚举
    if (Pin->PinName == LogInstancePinName)
    {
        OnLogInstanceChanged();
        return;
    }
    
    if (Pin->PinName == LogFormatStringPinName || Pin->PinName == UEdGraphSchema_K2::PN_Execute || Pin->PinName == LogInstancePinName) return;

    if (Pin->LinkedTo.Num() > 0)
    {
        const UEdGraphPin* Other = Pin->LinkedTo[0];
        Pin->PinType = Other->PinType; // 采样类型
    }
    else
    {
        Pin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
        Pin->PinType.PinSubCategoryObject = nullptr;
        Pin->DefaultValue.Reset();
    }
}

void UK2Node_BqLogFormatRaw::PinDefaultValueChanged(UEdGraphPin* Pin)
{
    Super::PinDefaultValueChanged(Pin);
    if (!Pin)
    {
        return;
    }
    if (Pin->PinName == LogInstancePinName)
    {
        OnLogInstanceChanged();
        return;
    }
}

void UK2Node_BqLogFormatRaw::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
    Super::ExpandNode(CompilerContext, SourceGraph);

    // 1) 基本校验：LogInstance 必须存在（连线或默认对象）
    UEdGraphPin* LogInstancePin = GetLogInstancePin();
    const bool bHasConnection = (LogInstancePin && LogInstancePin->LinkedTo.Num() > 0);
    const bool bHasDefaultObj = (LogInstancePin && LogInstancePin->DefaultObject != nullptr);
    if (!bHasConnection && !bHasDefaultObj)
    {
        CompilerContext.MessageLog.Error(TEXT("LogInstance must be set on @@"), this);
        return;
    }

    // 2) 收集自定义参数 pins（保持你现有的 ArgX 识别）
    TArray<UEdGraphPin*> ArgPins;
    GetArgPins(ArgPins);

    // 3) 为每个参数生成 MakeStruct(FBqLogAny)
    TArray<UEdGraphPin*> AnyStructOutputs;
    AnyStructOutputs.Reserve(ArgPins.Num());

    for (UEdGraphPin* ArgPin : ArgPins)
    {
        UK2Node_MakeStruct* MakeAny = CompilerContext.SpawnIntermediateNode<UK2Node_MakeStruct>(this, SourceGraph);
        MakeAny->StructType = FBqLogAny::StaticStruct();
        MakeAny->AllocateDefaultPins();

        const FName Cat = ArgPin->PinType.PinCategory;
        UObject* SubObj = ArgPin->PinType.PinSubCategoryObject.Get();

        auto SetTypeEnum = [&](EBqLogAnyType E)
        {
            if (UEdGraphPin* TypePin = MakeAny->FindPin(TEXT("Type"), EGPD_Input))
            {
                TypePin->DefaultValue = StaticEnum<EBqLogAnyType>()->GetNameStringByValue((int64)E);
            }
        };

        if (Cat == UEdGraphSchema_K2::PC_Boolean)
        {
            SetTypeEnum(EBqLogAnyType::Bool);
            if (UEdGraphPin* P = MakeAny->FindPin(TEXT("B"), EGPD_Input)) CompilerContext.MovePinLinksToIntermediate(*ArgPin, *P);
        }
        else if (Cat == UEdGraphSchema_K2::PC_Int)
        {
            SetTypeEnum(EBqLogAnyType::Int64);
            if (UEdGraphPin* P = MakeAny->FindPin(TEXT("I64"), EGPD_Input)) CompilerContext.MovePinLinksToIntermediate(*ArgPin, *P);
        }
#if ENGINE_MAJOR_VERSION >= 5
        else if (Cat == UEdGraphSchema_K2::PC_Int64)
        {
            SetTypeEnum(EBqLogAnyType::Int64);
            if (UEdGraphPin* P = MakeAny->FindPin(TEXT("I64"), EGPD_Input)) CompilerContext.MovePinLinksToIntermediate(*ArgPin, *P);
        }
#endif
        else if (Cat == UEdGraphSchema_K2::PC_Float || Cat == UEdGraphSchema_K2::PC_Real)
        {
            SetTypeEnum(EBqLogAnyType::Double);
            if (UEdGraphPin* P = MakeAny->FindPin(TEXT("D"), EGPD_Input)) CompilerContext.MovePinLinksToIntermediate(*ArgPin, *P);
        }
        else if (Cat == UEdGraphSchema_K2::PC_String)
        {
            SetTypeEnum(EBqLogAnyType::String);
            if (UEdGraphPin* P = MakeAny->FindPin(TEXT("S"), EGPD_Input)) CompilerContext.MovePinLinksToIntermediate(*ArgPin, *P);
        }
        else if (Cat == UEdGraphSchema_K2::PC_Name)
        {
            SetTypeEnum(EBqLogAnyType::Name);
            if (UEdGraphPin* P = MakeAny->FindPin(TEXT("N"), EGPD_Input)) CompilerContext.MovePinLinksToIntermediate(*ArgPin, *P);
        }
        else if (Cat == UEdGraphSchema_K2::PC_Text)
        {
            SetTypeEnum(EBqLogAnyType::Text);
            if (UEdGraphPin* P = MakeAny->FindPin(TEXT("T"), EGPD_Input)) CompilerContext.MovePinLinksToIntermediate(*ArgPin, *P);
        }
        else if (Cat == UEdGraphSchema_K2::PC_Object)
        {
            SetTypeEnum(EBqLogAnyType::Object);
            if (UEdGraphPin* P = MakeAny->FindPin(TEXT("Obj"), EGPD_Input)) CompilerContext.MovePinLinksToIntermediate(*ArgPin, *P);
        }
        else if (Cat == UEdGraphSchema_K2::PC_Class)
        {
            SetTypeEnum(EBqLogAnyType::Class);
            if (UEdGraphPin* P = MakeAny->FindPin(TEXT("Cls"), EGPD_Input)) CompilerContext.MovePinLinksToIntermediate(*ArgPin, *P);
        }
        else if (Cat == UEdGraphSchema_K2::PC_SoftObject
              || (Cat == UEdGraphSchema_K2::PC_Struct && SubObj == TBaseStructure<FSoftObjectPath>::Get()))
        {
            SetTypeEnum(EBqLogAnyType::SoftObject);
            if (UEdGraphPin* P = MakeAny->FindPin(TEXT("SoftPath"), EGPD_Input)) CompilerContext.MovePinLinksToIntermediate(*ArgPin, *P);
        }
        else if (Cat == UEdGraphSchema_K2::PC_Struct && SubObj)
        {
            UScriptStruct* SStruct = Cast<UScriptStruct>(SubObj);
            if (SStruct == TBaseStructure<FVector>::Get())
            {
                SetTypeEnum(EBqLogAnyType::Vector);
                if (UEdGraphPin* P = MakeAny->FindPin(TEXT("V"), EGPD_Input)) CompilerContext.MovePinLinksToIntermediate(*ArgPin, *P);
            }
            else if (SStruct == TBaseStructure<FRotator>::Get())
            {
                SetTypeEnum(EBqLogAnyType::Rotator);
                if (UEdGraphPin* P = MakeAny->FindPin(TEXT("R"), EGPD_Input)) CompilerContext.MovePinLinksToIntermediate(*ArgPin, *P);
            }
            else if (SStruct == TBaseStructure<FTransform>::Get())
            {
                SetTypeEnum(EBqLogAnyType::Transform);
                if (UEdGraphPin* P = MakeAny->FindPin(TEXT("Xform"), EGPD_Input)) CompilerContext.MovePinLinksToIntermediate(*ArgPin, *P);
            }
            else if (SStruct == TBaseStructure<FColor>::Get())
            {
                SetTypeEnum(EBqLogAnyType::Color);
                if (UEdGraphPin* P = MakeAny->FindPin(TEXT("Color"), EGPD_Input)) CompilerContext.MovePinLinksToIntermediate(*ArgPin, *P);
            }
            else if (SStruct == TBaseStructure<FLinearColor>::Get())
            {
                SetTypeEnum(EBqLogAnyType::LinearColor);
                if (UEdGraphPin* P = MakeAny->FindPin(TEXT("LColor"), EGPD_Input)) CompilerContext.MovePinLinksToIntermediate(*ArgPin, *P);
            }
            else
            {
                SetTypeEnum(EBqLogAnyType::String);
                if (UEdGraphPin* P = MakeAny->FindPin(TEXT("S"), EGPD_Input))
                {
                    CompilerContext.GetSchema()->TrySetDefaultValue(*P, TEXT(""));
                }
            }
        }
        else
        {
            SetTypeEnum(EBqLogAnyType::String);
            if (UEdGraphPin* P = MakeAny->FindPin(TEXT("S"), EGPD_Input))
            {
                CompilerContext.GetSchema()->TrySetDefaultValue(*P, TEXT(""));
            }
        }

        // 输出 pin（第一个输出）
        UEdGraphPin* OutPin = nullptr;
        for (UEdGraphPin* P : MakeAny->Pins)
        {
            if (P->Direction == EGPD_Output) { OutPin = P; break; }
        }
        check(OutPin);
        AnyStructOutputs.Add(OutPin);
    }

    // 4) 构建 Args 数组（仅当有参数）
    const int32 Desired = AnyStructOutputs.Num();
    UK2Node_MakeArray* MakeArrayAny = nullptr;

    if (Desired > 0)
    {
        MakeArrayAny = CompilerContext.SpawnIntermediateNode<UK2Node_MakeArray>(this, SourceGraph);
        MakeArrayAny->AllocateDefaultPins();

        // 输出 pin：TArray<FBqLogAny>
        UEdGraphPin* OutArr = MakeArrayAny->GetOutputPin();
        OutArr->PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        OutArr->PinType.PinSubCategoryObject = FBqLogAny::StaticStruct();
        OutArr->PinType.ContainerType = EPinContainerType::Array;

        // 收集并增补输入 pin
        auto CollectArrayInputs = [](UK2Node_MakeArray* Node, TArray<UEdGraphPin*>& OutInputs)
        {
            OutInputs.Reset();
            for (UEdGraphPin* P : Node->Pins)
            {
                if (P && P->Direction == EGPD_Input)
                {
                    OutInputs.Add(P);
                }
            }
        };

        TArray<UEdGraphPin*> ArrayInputs;
        CollectArrayInputs(MakeArrayAny, ArrayInputs);

        int32 Safety = 128;
        while (ArrayInputs.Num() < Desired && Safety-- > 0)
        {
            MakeArrayAny->AddInputPin();
            CollectArrayInputs(MakeArrayAny, ArrayInputs);
        }

        // 输入 pins 设为“元素类型”
        for (UEdGraphPin* InPin : ArrayInputs)
        {
            InPin->PinType = OutArr->PinType;
            InPin->PinType.ContainerType = EPinContainerType::None;
        }

        // 连接元素
        const int32 ConnectCount = FMath::Min(Desired, ArrayInputs.Num());
        for (int32 i = 0; i < ConnectCount; ++i)
        {
            CompilerContext.MovePinLinksToIntermediate(*AnyStructOutputs[i], *ArrayInputs[i]);
        }
    }

    // 5) 生成调用节点（有/无参数两个函数）
    UK2Node_CallFunction* CallSubmit = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
    CallSubmit->SetFromFunction(UBqLogFunctionLibrary::StaticClass()->FindFunctionByName(
        Desired > 0 ? GET_FUNCTION_NAME_CHECKED(UBqLogFunctionLibrary, DoBqLogFormat)
                    : GET_FUNCTION_NAME_CHECKED(UBqLogFunctionLibrary, DoBqLog)));
    CallSubmit->AllocateDefaultPins();

    UEdGraphPin* CallLogInstancePin   = CallSubmit->FindPin(TEXT("LogInstance"),   EGPD_Input);
    UEdGraphPin* CallLogLevelPin      = CallSubmit->FindPin(TEXT("Level"),         EGPD_Input);
    UEdGraphPin* CallCategoryIndexPin = CallSubmit->FindPin(TEXT("CategoryIndex"), EGPD_Input);
    UEdGraphPin* CallFormatStringPin  = CallSubmit->FindPin(TEXT("FormatString"),  EGPD_Input);

    if (!(CallLogInstancePin && CallLogLevelPin && CallCategoryIndexPin && CallFormatStringPin))
    {
        CompilerContext.MessageLog.Error(TEXT("Internal error: Missing pins on function node @@"), this);
        BreakAllNodeLinks();
        return;
    }

    // 搬运数据 pins：连线+默认值一起搬（比 TryCreateConnection 更稳）
    CompilerContext.MovePinLinksToIntermediate(*GetLogInstancePin(),    *CallLogInstancePin);
    CompilerContext.MovePinLinksToIntermediate(*GetLogLevelPin(),       *CallLogLevelPin);
    CompilerContext.MovePinLinksToIntermediate(*GetLogFormatStringPin(),*CallFormatStringPin);

    // CategoryValue：连线优先；未连线则把枚举名解析成底层值写常量
    if (UEdGraphPin* CategoryPin = FindPin(CategoryPinName, EGPD_Input))
    {
        if (CategoryPin->LinkedTo.Num() > 0)
        {
            // 让引擎做 Enum(Byte)->Int 的隐式转换
            CompilerContext.MovePinLinksToIntermediate(*CategoryPin, *CallCategoryIndexPin);
        }
        else
        {
            UEnum* CategoryEnum = Cast<UEnum>(CategoryPin->PinType.PinSubCategoryObject.Get());
            if (!CategoryEnum)
            {
                CompilerContext.MessageLog.Error(TEXT("Category enum type is missing on @@"), this);
                BreakAllNodeLinks();
                return;
            }

            // 严格按名字解析（支持 "EnumName::Member" 和 "Member" 两种形态：取最后一段）
            auto LastToken = [](const FString& S)->FString {
                int32 Pos = S.Find(TEXT("::"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
                return (Pos != INDEX_NONE) ? S.Mid(Pos + 2) : S;
            };

            const FString RawName = CategoryPin->DefaultValue;
            const FString ShortName = LastToken(RawName);

#if (ENGINE_MAJOR_VERSION > 4) || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 22)
            int32 Index = CategoryEnum->GetIndexByName(FName(*ShortName));
#else
            int32 Index = CategoryEnum->GetIndexByNameString(ShortName);
#endif
            if (Index == INDEX_NONE)
            {
                CompilerContext.MessageLog.Error(TEXT("Invalid Category selection on @@"), this);
                BreakAllNodeLinks();
                return;
            }
            const int64 Value = CategoryEnum->GetValueByIndex(Index);
            CompilerContext.GetSchema()->TrySetDefaultValue(*CallCategoryIndexPin, LexToString((int32)Value));
        }
    }
    else
    {
        // 本次没有 Category 引脚：按你的规则，写 0（或保留空）
        CompilerContext.GetSchema()->TrySetDefaultValue(*CallCategoryIndexPin, TEXT("0"));
    }

    // 连接 Args（仅在有参数时）
    if (Desired > 0)
    {
        if (UEdGraphPin* CallArgsPin = CallSubmit->FindPin(TEXT("Args"), EGPD_Input))
        {
            CompilerContext.MovePinLinksToIntermediate(*MakeArrayAny->GetOutputPin(), *CallArgsPin);
        }
    }

    // 6) 执行流搬运（带校验）
    UEdGraphPin* ThisExec = GetLogExecutePin();
    UEdGraphPin* ThisThen = GetLogThenPin();
    if (!ThisExec || !ThisThen || !CallSubmit->GetExecPin() || !CallSubmit->GetThenPin())
    {
        CompilerContext.MessageLog.Error(TEXT("Internal error: Exec/Then pins missing on @@"), this);
        BreakAllNodeLinks();
        return;
    }
    CompilerContext.MovePinLinksToIntermediate(*ThisExec, *CallSubmit->GetExecPin());
    CompilerContext.MovePinLinksToIntermediate(*ThisThen, *CallSubmit->GetThenPin());

    // 清理本节点链接
    BreakAllNodeLinks();
}

void UK2Node_BqLogFormatRaw::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
    const FName PropertyName = (PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None);
    UE_LOG(LogTemp, Warning, TEXT("PostEditChangeProperty called for property: %s"), *PropertyName.ToString());
    
    if (PropertyName == GET_MEMBER_NAME_CHECKED(UK2Node_BqLogFormatRaw, ArgumentNames))
    {
        UE_LOG(LogTemp, Warning, TEXT("ArgumentNames changed, reconstructing node"));
        ReconstructNode();
    }
    
    Super::PostEditChangeProperty(PropertyChangedEvent);
    GetGraph()->NotifyNodeChanged(this);
}

// 动态分类枚举支持实现
void UK2Node_BqLogFormatRaw::OnLogInstanceChanged()
{
    UE_LOG(LogTemp, Warning, TEXT("OnLogInstanceChanged called"));
    
    // 检查是否需要重新构建节点
    RefreshCategoryPinStatus();
    const bool bShouldShowCategory = SavedLogType.GetDefaultObject() ? SavedLogType.GetDefaultObject()->SupportsCategoryEnum() : false;
    UEdGraphPin* ExistingCategoryPin = FindPin(CategoryPinName, EGPD_Input);
    const bool bCurrentlyHasCategory = (ExistingCategoryPin != nullptr);

    bool bNeedReconstruct = false;
    if (bShouldShowCategory && ExistingCategoryPin)
    {
        UEnum* NewEnum = SavedLogType.GetDefaultObject()->GetCategoryEnum();
        UEnum* OldEnum = Cast<UEnum>(ExistingCategoryPin->PinType.PinSubCategoryObject.Get());
        if (NewEnum != OldEnum)
        {
            bNeedReconstruct = true;
        }
    }
    if (bShouldShowCategory != bCurrentlyHasCategory)
    {
        bNeedReconstruct = true;
    }
    if (bNeedReconstruct)
    {
        UEdGraphPin* CategoryPin = FindPin(CategoryPinName, EGPD_Input);
        if (CategoryPin)
        {
            RemoveInputPin(CategoryPin);
        }
        ReconstructNode();
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
    }
}


void UK2Node_BqLogFormatRaw::RefreshCategoryPinStatus()
{
    SavedLogType = nullptr;
    UEdGraphPin* LogInstancePin = GetLogInstancePin();
    if (LogInstancePin)
    {
        if (LogInstancePin->LinkedTo.Num() > 0)
        {
            UEdGraphPin* ConnectedPin = LogInstancePin->LinkedTo[0];
            if (ConnectedPin)
            {
                // 检查连接的引脚类型是否为 UBqLog 的子类
                if (UClass* Cls = Cast<UClass>(ConnectedPin->PinType.PinSubCategoryObject.Get()))
                {
                    if (Cls->IsChildOf(UBqLog::StaticClass()))
                    {
                        SavedLogType = Cls;
                    }
                }
            }
        }

        // 2) 若无连接：检查默认对象
        if (LogInstancePin->DefaultObject)
        {
            const UBqLog* LogInstance = Cast<UBqLog>(LogInstancePin->DefaultObject);
            auto Cls = LogInstancePin->DefaultObject->GetClass();
            if (Cls->IsChildOf(UBqLog::StaticClass()))
            {
                SavedLogType = Cls;
            }
        }
    }
}

FName UK2Node_BqLogFormatRaw::GetUniqueArgumentName() const
{
    FName NewArgName;
    int32 i = 0;
    while (true)
    {
        NewArgName = *FString::Printf(TEXT("Arg%d"), i++);
        if (!ArgumentNames.Contains(NewArgName))
        {
            break;
        }
    }
    return NewArgName;
}