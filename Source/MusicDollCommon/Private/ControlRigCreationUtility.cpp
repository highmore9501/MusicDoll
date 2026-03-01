#include "ControlRigCreationUtility.h"

#include "Rigs/RigHierarchyController.h"
#include "Rigs/RigHierarchy.h"
#include "ControlRig.h"

bool FControlRigCreationUtility::CreateControl(
    URigHierarchyController* HierarchyController,
    URigHierarchy* RigHierarchy,
    const FString& ControlName,
    const FRigElementKey& ParentKey,
    const FString& ShapeName)
{
    // 基本参数验证
    if (!HierarchyController || !RigHierarchy || ControlName.IsEmpty())
    {
        UE_LOG(LogTemp, Error,
               TEXT("CreateControl: Invalid parameters - HierarchyController: %d, RigHierarchy: %d, ControlName: %s"),
               HierarchyController != nullptr, RigHierarchy != nullptr,
               *ControlName);
        return false;
    }

    // 严格检查 Control 是否已存在
    if (StrictControlExistenceCheck(RigHierarchy, ControlName))
    {
        UE_LOG(LogTemp, Warning, TEXT("✓ Control '%s' already exists"), *ControlName);
        return false;
    }

    // 额外的预防措施 - 再次检查可能的损坏 Control
    FRigElementKey ExistingElementKey(*ControlName, ERigElementType::Control);
    if (RigHierarchy->Contains(ExistingElementKey))
    {
        // 如果通过基础检查发现存在，但严格检查认为不存在，说明可能有损坏的 Control
        UE_LOG(LogTemp, Warning,
               TEXT("⚠️ Found potentially corrupted control '%s' - attempting cleanup before creation"),
               *ControlName);

        // 尝试删除损坏的 Control
        if (HierarchyController->RemoveElement(ExistingElementKey, true, false))
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("🧹 Successfully removed corrupted control '%s'"),
                   *ControlName);
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("❌ Failed to remove corrupted control '%s' - aborting creation"),
                   *ControlName);
            return false;
        }
    }

    // 验证父 Control 如果指定的话
    FRigElementKey VerifiedParentKey = ParentKey;
    if (ParentKey.IsValid())
    {
        if (!StrictControlExistenceCheck(RigHierarchy, ParentKey.Name.ToString()))
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("⚠️ Parent control '%s' does not exist or is corrupted - creating '%s' without parent"),
                   *ParentKey.Name.ToString(), *ControlName);
            VerifiedParentKey = FRigElementKey();  // 清空父级
        }
    }

    // 创建 Control 设置
    FRigControlSettings ControlSettings;
    ControlSettings.ControlType = ERigControlType::Transform;
    ControlSettings.DisplayName = FName(*ControlName);
    ControlSettings.ShapeName = FName(*ShapeName);

    // 创建初始变换
    FTransform InitialTransform = FTransform::Identity;
    FRigControlValue InitialValue;
    InitialValue.SetFromTransform(InitialTransform, ERigControlType::Transform, ERigControlAxis::X);

    // 添加 Control
    FRigElementKey NewControlKey = HierarchyController->AddControl(
        FName(*ControlName),
        VerifiedParentKey,
        ControlSettings,
        InitialValue,
        FTransform::Identity,  // Offset transform
        FTransform::Identity,  // Shape transform
        true,                  // bSetupUndo
        false                  // bPrintPythonCommand
    );

    if (NewControlKey.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("✅ Successfully created control: %s"), *ControlName);

        // 创建后验证 Control 确实存在且正确
        if (!StrictControlExistenceCheck(RigHierarchy, ControlName))
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("⚠️ Created control '%s' but verification failed - may need manual check"),
                   *ControlName);
        }

        return true;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to create control: %s"), *ControlName);
        return false;
    }
}

bool FControlRigCreationUtility::StrictControlExistenceCheck(
    URigHierarchy* RigHierarchy,
    const FString& ControllerName)
{
    if (!RigHierarchy)
    {
        return false;
    }

    FRigElementKey ElementKey(*ControllerName, ERigElementType::Control);

    // 基本存在性检查
    if (!RigHierarchy->Contains(ElementKey))
    {
        return false;
    }

    // 获取 Control 元素并验证其完整性
    FRigControlElement* ControlElement =
        RigHierarchy->Find<FRigControlElement>(ElementKey);
    if (!ControlElement)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("Control '%s' exists in hierarchy but element is null - considering as non-existent"),
               *ControllerName);
        return false;
    }

    // 基本的完整性检查（可在子类中扩展）
    // 例如检查 ControlType 是否合理等

    return true;
}

FString FControlRigCreationUtility::DetermineShapeName(const FString& ControlName)
{
    // 根据控制器名称确定形状
    if (ControlName.Contains(TEXT("hand"), ESearchCase::IgnoreCase) &&
        !ControlName.Contains(TEXT("rotation"), ESearchCase::IgnoreCase))
    {
        return TEXT("Cube");
    }
    else if (ControlName.Contains(TEXT("rotation"), ESearchCase::IgnoreCase))
    {
        return TEXT("Circle");
    }
    else if (ControlName.StartsWith(TEXT("pole_")))
    {
        return TEXT("Diamond");
    }
    else
    {
        return TEXT("Sphere");
    }
}

int32 FControlRigCreationUtility::CleanupDuplicateControls(
    URigHierarchy* RigHierarchy,
    const TSet<FString>& ExpectedControllerNames,
    bool bLogVerbose)
{
    if (!RigHierarchy)
    {
        return 0;
    }

    URigHierarchyController* HierarchyController = RigHierarchy->GetController();
    if (!HierarchyController)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("Cannot get HierarchyController for cleanup"));
        return 0;
    }

    if (bLogVerbose)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("Starting cleanup of duplicate/corrupted controls..."));
    }

    // 获取所有现有的 Control 元素
    TArray<FRigElementKey> ExistingControlKeys = RigHierarchy->GetAllKeys(false);
    TArray<FRigElementKey> FilteredControlKeys;

    // 筛选出 Control 类型的元素
    for (const FRigElementKey& Key : ExistingControlKeys)
    {
        if (Key.Type == ERigElementType::Control)
        {
            FilteredControlKeys.Add(Key);
        }
    }

    int32 DuplicatesFound = 0;
    TMap<FString, TArray<FRigElementKey>> ControlGroups;

    // 将控件按名称分组
    for (const FRigElementKey& ControlKey : FilteredControlKeys)
    {
        FString ControlName = ControlKey.Name.ToString();

        // 只处理我们期望的控制器
        if (ExpectedControllerNames.Contains(ControlName))
        {
            ControlGroups.FindOrAdd(ControlName).Add(ControlKey);
        }
    }

    // 删除除第一个外的所有重复实例
    for (const auto& GroupPair : ControlGroups)
    {
        const FString& ControlName = GroupPair.Key;
        const TArray<FRigElementKey>& ControlInstances = GroupPair.Value;

        if (ControlInstances.Num() > 1)
        {
            if (bLogVerbose)
            {
                UE_LOG(LogTemp, Warning,
                       TEXT("  🔍 Found %d instances of control '%s' - removing duplicates"),
                       ControlInstances.Num(), *ControlName);
            }

            // 保留第一个，删除其余的
            for (int32 i = 1; i < ControlInstances.Num(); i++)
            {
                bool bRemoved = HierarchyController->RemoveElement(
                    ControlInstances[i], true, false);
                if (bRemoved)
                {
                    DuplicatesFound++;
                    if (bLogVerbose)
                    {
                        UE_LOG(LogTemp, Warning,
                               TEXT("    ✅ Removed duplicate control '%s' instance %d"),
                               *ControlName, i + 1);
                    }
                }
                else if (bLogVerbose)
                {
                    UE_LOG(LogTemp, Warning,
                           TEXT("    ❌ Failed to remove duplicate control '%s' instance %d"),
                           *ControlName, i + 1);
                }
            }
        }
    }

    if (bLogVerbose)
    {
        if (DuplicatesFound > 0)
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("Cleanup completed: Removed %d duplicate control instances"),
                   DuplicatesFound);
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("Cleanup completed: No duplicates found"));
        }
    }

    return DuplicatesFound;
}

bool FControlRigCreationUtility::CreateRootController(
    URigHierarchyController* HierarchyController,
    URigHierarchy* RigHierarchy,
    const FString& RootName,
    const FString& ShapeName) {
    if (!HierarchyController || !RigHierarchy) {
        UE_LOG(LogTemp, Error, TEXT("CreateRootController: Invalid HierarchyController or RigHierarchy"));
        return false;
    }

    // 检查根控制器是否已存在
    if (StrictControlExistenceCheck(RigHierarchy, RootName)) {
        UE_LOG(LogTemp, Warning, TEXT("✓ Root controller '%s' already exists"), *RootName);
        return true;
    }

    UE_LOG(LogTemp, Warning, TEXT("Creating root controller '%s'..."), *RootName);

    // 创建根控制器的设置
    FRigControlSettings ControlSettings;
    ControlSettings.ControlType = ERigControlType::Transform;
    ControlSettings.DisplayName = FName(*RootName);
    ControlSettings.ShapeName = FName(*ShapeName);

    // 初始化Transform和Value
    FTransform InitialTransform = FTransform::Identity;
    FRigControlValue InitialValue;
    InitialValue.SetFromTransform(InitialTransform, ERigControlType::Transform, ERigControlAxis::X);

    // 创建根控制器（没有父级）
    FRigElementKey NewControlKey = HierarchyController->AddControl(
        FName(*RootName),
        FRigElementKey(),  // 空的父级
        ControlSettings,
        InitialValue,
        FTransform::Identity,
        FTransform::Identity,
        true,   // bSetupUndo
        false   // bPrintPythonCommand
    );

    if (NewControlKey.IsValid()) {
        UE_LOG(LogTemp, Warning, TEXT("✅ Successfully created root controller '%s'"), *RootName);
        return true;
    } else {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to create root controller '%s'"), *RootName);
        return false;
    }
}

bool FControlRigCreationUtility::CreateInstrumentRootController(
    URigHierarchyController* HierarchyController,
    URigHierarchy* RigHierarchy,
    const FString& ControllerRootName,
    const FString& ParentName,
    const FString& ShapeName) {
    if (!HierarchyController || !RigHierarchy) {
        UE_LOG(LogTemp, Error, TEXT("CreateInstrumentRootController: Invalid HierarchyController or RigHierarchy"));
        return false;
    }

    // 检查乐器根控制器是否已存在
    if (StrictControlExistenceCheck(RigHierarchy, ControllerRootName)) {
        UE_LOG(LogTemp, Warning, TEXT("✓ Instrument root controller '%s' already exists"), *ControllerRootName);
        return true;
    }

    // 检查父控制器是否存在
    if (!StrictControlExistenceCheck(RigHierarchy, ParentName)) {
        UE_LOG(LogTemp, Error, TEXT("❌ Parent controller '%s' does not exist"), *ParentName);
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("Creating instrument root controller '%s' under '%s'..."), *ControllerRootName, *ParentName);

    // 创建乐器根控制器的设置
    FRigControlSettings ControlSettings;
    ControlSettings.ControlType = ERigControlType::Transform;
    ControlSettings.DisplayName = FName(*ControllerRootName);
    ControlSettings.ShapeName = FName(*ShapeName);

    // 初始化Transform和Value
    FTransform InitialTransform = FTransform::Identity;
    FRigControlValue InitialValue;
    InitialValue.SetFromTransform(InitialTransform, ERigControlType::Transform, ERigControlAxis::X);

    // 确定父控制器键
    FRigElementKey ParentKey(*ParentName, ERigElementType::Control);

    // 创建乐器根控制器
    FRigElementKey NewControlKey = HierarchyController->AddControl(
        FName(*ControllerRootName),
        ParentKey,
        ControlSettings,
        InitialValue,
        FTransform::Identity,
        FTransform::Identity,
        true,   // bSetupUndo
        false   // bPrintPythonCommand
    );

    if (NewControlKey.IsValid()) {
        UE_LOG(LogTemp, Warning, TEXT("✅ Successfully created instrument root controller '%s'"), *ControllerRootName);
        return true;
    } else {
        UE_LOG(LogTemp, Error, TEXT("❌ Failed to create instrument root controller '%s'"), *ControllerRootName);
        return false;
    }
}
