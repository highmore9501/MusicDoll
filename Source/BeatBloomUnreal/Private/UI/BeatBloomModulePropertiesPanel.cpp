#include "UI/BeatBloomModulePropertiesPanel.h"

#include "BeatBloomUnreal.h"
#include "BeatBloomControlRigProcessor.h"
#include "UI/CommonPanelUtility.h"

#define LOCTEXT_NAMESPACE "BeatBloomModulePropertiesPanel"

void SBeatBloomModulePropertiesPanel::Construct(const FArguments& InArgs) {
    // 调用基类构造函数，使用基类的参数类型
    SModulePropertiesPanel::FArguments BaseArgs;
    SModulePropertiesPanel::Construct(BaseArgs);
}

void SBeatBloomModulePropertiesPanel::SetActor(AActor* InActor) {
    BeatBloomActor = Cast<ABeatBloomUnreal>(InActor);
    RefreshProperties();
}

bool SBeatBloomModulePropertiesPanel::CanHandleActor(
    const AActor* InActor) const {
    return Cast<const ABeatBloomUnreal>(InActor) != nullptr;
}

void SBeatBloomModulePropertiesPanel::RefreshProperties() {
    CreatePropertyWidgets();
}

void SBeatBloomModulePropertiesPanel::CreatePropertyWidgets() {
    auto Container = GetPropertyContainer();
    if (!Container.IsValid()) {
        return;
    }
    
    // 清空现有内容
    Container->ClearChildren();
    
    if (!BeatBloomActor.IsValid()) {
        Container->AddSlot().AutoHeight().Padding(5.0f)
            [SNew(STextBlock)
                .Text(LOCTEXT("NoActorSelected", "No BeatBloom Actor Selected"))
                .ColorAndOpacity(FLinearColor::Yellow)];
        return;
    }
    
    ABeatBloomUnreal* BeatBloom = BeatBloomActor.Get();
    
    // ========== IO Configuration ==========
    Container->AddSlot().AutoHeight().Padding(5.0f, 15.0f, 5.0f, 5.0f)
        [FCommonPanelUtility::CreateSectionHeader(TEXT("IO Configuration"))];
    
    // DrumKit Config 路径
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox)
         + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 5.0f, 0.0f)
             [FCommonPanelUtility::CreateFilePathPropertyRowWithCallback(
                 TEXT("DrumKit Config Path"),
                 BeatBloom->DrumKitConfigPath,
                 TEXT("DrumKitConfigPath"),
                 TEXT(".drumkit"),
                 [this](const FString& NewPath) {
                     if (BeatBloomActor.IsValid()) {
                         BeatBloomActor.Get()->DrumKitConfigPath = NewPath;
                         BeatBloomActor.Get()->Modify();
                     }
                 },
                 true)]
         + SHorizontalBox::Slot().AutoWidth()
             [SNew(SButton)
                 .Text(LOCTEXT("LoadDrumKitConfigButton", "Load"))
                 .OnClicked(this, &SBeatBloomModulePropertiesPanel::OnLoadDrumKitConfig)
                 .HAlign(HAlign_Center)
                 .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")]];
    
    // ========== DrumKit Info (只读) ==========
    if (BeatBloom->DrumKitConfig.Components.Num() > 0) {
        Container->AddSlot().AutoHeight().Padding(5.0f, 15.0f, 5.0f, 5.0f)
            [FCommonPanelUtility::CreateSectionHeader(TEXT("DrumKit Info"))];
        
        // 配置名称
        Container->AddSlot().AutoHeight().Padding(5.0f)
            [FCommonPanelUtility::CreateStringPropertyRow(
                TEXT("Configuration Name"),
                BeatBloom->DrumKitConfig.Name,
                TEXT("DrumKitConfigName"),
                FSimpleDelegate())];
        
        // 肢体数量
        Container->AddSlot().AutoHeight().Padding(5.0f)
            [FCommonPanelUtility::CreateNumericPropertyRow(
                TEXT("Limb Count"),
                BeatBloom->DrumKitConfig.Limbs.Num(),
                TEXT("LimbCount"),
                [this](const FString& PropertyPath, int32 NewValue) {
                    OnNumericPropertyChanged(PropertyPath, NewValue);
                })];
        
        // 鼓件数量
        Container->AddSlot().AutoHeight().Padding(5.0f)
            [FCommonPanelUtility::CreateNumericPropertyRow(
                TEXT("Component Count"),
                BeatBloom->DrumKitConfig.Components.Num(),
                TEXT("ComponentCount"),
                [this](const FString& PropertyPath, int32 NewValue) {
                    OnNumericPropertyChanged(PropertyPath, NewValue);
                })];
        
        // 特殊动作数量
        Container->AddSlot().AutoHeight().Padding(5.0f)
            [FCommonPanelUtility::CreateNumericPropertyRow(
                TEXT("Special Action Count"),
                BeatBloom->DrumKitConfig.SpecialActions.Num(),
                TEXT("SpecialActionCount"),
                [this](const FString& PropertyPath, int32 NewValue) {
                    OnNumericPropertyChanged(PropertyPath, NewValue);
                })];
    }
    
    // Control Rig Section
    Container->AddSlot().AutoHeight().Padding(5.0f, 15.0f, 5.0f, 5.0f)
        [FCommonPanelUtility::CreateSectionHeader(TEXT("Control Rig"))];
    
    // Check Objects Status Button
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SButton)
             .Text(LOCTEXT("CheckObjectsStatusButton", "Check Drummer Control Rig Status"))
             .OnClicked(this,
                &SBeatBloomModulePropertiesPanel::OnCheckObjectsStatus)
             .HAlign(HAlign_Center)
             .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];
    
    // Setup Drummer Control Rig Button
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SButton)
             .Text(LOCTEXT("SetupAllObjectsButton", "Setup Drummer Control Rig"))
             .OnClicked(
                 this,
                 &SBeatBloomModulePropertiesPanel::OnSetupAllObjects)
             .HAlign(HAlign_Center)
             .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];
    
    // ========== File Paths ==========
    Container->AddSlot().AutoHeight().Padding(5.0f, 15.0f, 5.0f, 5.0f)
        [FCommonPanelUtility::CreateSectionHeader(TEXT("File Paths"))];
    
    // IOFilePath (继承自 AInstrumentBase)
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [FCommonPanelUtility::CreateFilePathPropertyRowWithCallback(
            TEXT("IO File Path"),
            BeatBloom->IOFilePath,
            TEXT("IOFilePath"),
            TEXT(".drummer"),
            [this](const FString& NewPath) {
                if (BeatBloomActor.IsValid()) {
                    BeatBloomActor.Get()->Modify();
                    BeatBloomActor.Get()->IOFilePath = NewPath;
                    UE_LOG(LogTemp, Warning, TEXT("BeatBloom: IO File Path updated to: %s"), *NewPath);
                }
            },
            true)];
    
    // Import/Export Section
    Container->AddSlot().AutoHeight().Padding(5.0f, 15.0f, 5.0f, 5.0f)
        [FCommonPanelUtility::CreateSectionHeader(TEXT("Import/Export"))];
    
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SButton)
            .Text(LOCTEXT("ExportRecorderInfoButton", "Export Drummer Info"))
            .OnClicked(this, &SBeatBloomModulePropertiesPanel::OnExportRecorderInfo)
            .HAlign(HAlign_Center)
            .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];
    
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SButton)
            .Text(LOCTEXT("ImportRecorderInfoButton", "Import Drummer Info"))
            .OnClicked(this, &SBeatBloomModulePropertiesPanel::OnImportRecorderInfo)
            .HAlign(HAlign_Center)
            .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];
}

void SBeatBloomModulePropertiesPanel::OnNumericPropertyChanged(
    const FString& PropertyPath, int32 NewValue) {
    if (!BeatBloomActor.IsValid()) {
        return;
    }

    ABeatBloomUnreal* BeatBloom = BeatBloomActor.Get();
    BeatBloom->Modify();

    // Note: These properties are read-only displays of DrumKitConfig data
    // They cannot be directly modified through the UI
    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: Numeric property change attempted for %s = %d (read-only)"),
           *PropertyPath, NewValue);
}

void SBeatBloomModulePropertiesPanel::OnFilePathChanged(
    const FString& PropertyPath, const FString& NewFilePath) {
    if (!BeatBloomActor.IsValid()) {
        return;
    }
    
    ABeatBloomUnreal* BeatBloom = BeatBloomActor.Get();
    
    // 根据 PropertyPath 更新对应的属性
    if (PropertyPath == TEXT("DrumKitConfigPath")) {
        BeatBloom->DrumKitConfigPath = NewFilePath;
        BeatBloom->Modify();
        UE_LOG(LogTemp, Warning, TEXT("BeatBloom: DrumKit Config Path updated to: %s"), *NewFilePath);
    }
}

FReply SBeatBloomModulePropertiesPanel::OnLoadDrumKitConfig() {
    // 加载 .drumkit 配置文件
    if (!BeatBloomActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: No actor selected for loading drumkit config"));
        return FReply::Handled();
    }
    
    ABeatBloomUnreal* BeatBloom = BeatBloomActor.Get();
    FString FilePath = BeatBloom->DrumKitConfigPath;
    
    // 如果路径为空，弹出文件选择对话框
    if (FilePath.IsEmpty()) {
        if (FCommonPanelUtility::BrowseForFile(TEXT(".drumkit"), FilePath, false)) {
            BeatBloom->DrumKitConfigPath = FilePath;
            BeatBloom->Modify();
        } else {
            // 用户取消了选择
            return FReply::Handled();
        }
    }
    
    // 验证文件是否存在
    if (!FPaths::FileExists(FilePath)) {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: DrumKit config file does not exist: %s"), *FilePath);
        return FReply::Handled();
    }
    
    // 调用 LoadDrumKitConfig 加载配置
    if (BeatBloom->LoadDrumKitConfig(FilePath)) {
        UE_LOG(LogTemp, Warning,
               TEXT("BeatBloom: Successfully loaded drumkit config from %s"), *FilePath);
        
        // 刷新面板显示
        RefreshProperties();
        
        // 通知操作面板刷新鼓件下拉选项
        OnDrumKitConfigLoaded.ExecuteIfBound();
    } else {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: Failed to load drumkit config from %s"), *FilePath);
    }
    
    return FReply::Handled();
}

FReply SBeatBloomModulePropertiesPanel::OnSetupAllObjects() {
    if (!BeatBloomActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: No actor selected for setup drummer control rig"));
        return FReply::Handled();
    }
    
    UBeatBloomControlRigProcessor::SetupAllObjects(BeatBloomActor.Get());
    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: Setup Drummer Control Rig operation triggered"));
    return FReply::Handled();
}

FReply SBeatBloomModulePropertiesPanel::OnCheckObjectsStatus() {
    if (!BeatBloomActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("BeatBloom: No actor selected for check drummer control rig status"));
        return FReply::Handled();
    }
    
    UBeatBloomControlRigProcessor::CheckObjectsStatus(BeatBloomActor.Get());
    UE_LOG(LogTemp, Warning,
           TEXT("BeatBloom: Check Drummer Control Rig Status operation triggered"));
    return FReply::Handled();
}

FReply SBeatBloomModulePropertiesPanel::OnExportRecorderInfo() {
if (!BeatBloomActor.IsValid()) {
    UE_LOG(LogTemp, Error, TEXT("BeatBloom: No actor selected for export drummer info"));
    return FReply::Handled();
}
    
ABeatBloomUnreal* BeatBloom = BeatBloomActor.Get();
    
// 检查是否设置了 IO File Path
if (BeatBloom->IOFilePath.IsEmpty()) {
    UE_LOG(LogTemp, Error, TEXT("BeatBloom: IO File Path is not set. Please set IO File Path first."));
    return FReply::Handled();
}

if (!FCommonPanelUtility::ConfirmExportOverwrite(BeatBloom->IOFilePath)) {
    return FReply::Handled();
}

// 直接使用 IO File Path 进行导出
BeatBloom->ExportRecorderInfo(BeatBloom->IOFilePath);
    UE_LOG(LogTemp, Warning, TEXT("BeatBloom: Export Drummer Info to %s"), *BeatBloom->IOFilePath);
    
    return FReply::Handled();
}

FReply SBeatBloomModulePropertiesPanel::OnImportRecorderInfo() {
    if (!BeatBloomActor.IsValid()) {
        UE_LOG(LogTemp, Error, TEXT("BeatBloom: No actor selected for import drummer info"));
        return FReply::Handled();
    }
    
    ABeatBloomUnreal* BeatBloom = BeatBloomActor.Get();
    
    // 检查是否设置了 IO File Path
    if (BeatBloom->IOFilePath.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("BeatBloom: IO File Path is not set. Please set IO File Path first."));
        return FReply::Handled();
    }
    
    // 验证文件是否存在
    if (!FPaths::FileExists(BeatBloom->IOFilePath)) {
        UE_LOG(LogTemp, Error, TEXT("BeatBloom: IO file does not exist: %s"), *BeatBloom->IOFilePath);
        return FReply::Handled();
    }
    
    // 直接使用 IO File Path 进行导入
    if (BeatBloom->ImportRecorderInfo(BeatBloom->IOFilePath)) {
        UE_LOG(LogTemp, Warning, TEXT("BeatBloom: Import Drummer Info from %s"), *BeatBloom->IOFilePath);
        RefreshProperties();
    } else {
        UE_LOG(LogTemp, Error, TEXT("BeatBloom: Failed to import drummer info from %s"), *BeatBloom->IOFilePath);
    }
    
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
