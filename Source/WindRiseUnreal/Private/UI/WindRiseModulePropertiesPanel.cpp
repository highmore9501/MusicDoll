#include "UI/WindRiseModulePropertiesPanel.h"

#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "InstrumentMorphTargetUtility.h"
#include "UI/CommonPanelUtility.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "WindRiseUnreal.h"

#define LOCTEXT_NAMESPACE "WindRiseModulePropertiesPanel"

void SWindRiseModulePropertiesPanel::Construct(const FArguments& InArgs) {
    SModulePropertiesPanel::FArguments BaseArgs;
    SModulePropertiesPanel::Construct(BaseArgs);
}

void SWindRiseModulePropertiesPanel::SetActor(AActor* InActor) {
    WindRiseActor = Cast<AWindRiseUnreal>(InActor);
    RefreshProperties();
}

bool SWindRiseModulePropertiesPanel::CanHandleActor(
    const AActor* InActor) const {
    return Cast<const AWindRiseUnreal>(InActor) != nullptr;
}

void SWindRiseModulePropertiesPanel::RefreshProperties() {
    RefreshMorphTargetOptions();
    CreatePropertyWidgets();
}

// ============================================================
// MT 下拉选项刷新
// ============================================================

void SWindRiseModulePropertiesPanel::RefreshMorphTargetOptions() {
    CharacterMorphTargetOptions.Empty();
    InstrumentMorphTargetOptions.Empty();

    if (!WindRiseActor.IsValid()) return;

    AWindRiseUnreal* WindRise = WindRiseActor.Get();

    // 从 Performer SkeletalMesh 获取所有可用 MT
    if (WindRise->SkeletalMeshActor &&
        WindRise->SkeletalMeshActor->GetSkeletalMeshComponent()) {
        USkeletalMeshComponent* SkelComp =
            WindRise->SkeletalMeshActor->GetSkeletalMeshComponent();
        TArray<FString> AllMTNames;
        UInstrumentMorphTargetUtility::GetMorphTargetNames(SkelComp,
                                                           AllMTNames);
        for (const FString& Name : AllMTNames) {
            if (!WindRise->CharacterMorphTargets.Contains(Name)) {
                CharacterMorphTargetOptions.Add(
                    MakeShareable(new FString(Name)));
            }
        }
    }

    // 从 Instrument SkeletalMesh 获取所有可用 MT
    if (WindRise->InstrumentMesh &&
        WindRise->InstrumentMesh->GetSkeletalMeshComponent()) {
        USkeletalMeshComponent* SkelComp =
            WindRise->InstrumentMesh->GetSkeletalMeshComponent();
        TArray<FString> AllMTNames;
        UInstrumentMorphTargetUtility::GetMorphTargetNames(SkelComp,
                                                           AllMTNames);
        for (const FString& Name : AllMTNames) {
            if (!WindRise->InstrumentMorphTargets.Contains(Name)) {
                InstrumentMorphTargetOptions.Add(
                    MakeShareable(new FString(Name)));
            }
        }
    }

    if (CharacterMorphTargetOptions.Num() > 0) {
        SelectedCharacterMT = CharacterMorphTargetOptions[0];
    }
    if (InstrumentMorphTargetOptions.Num() > 0) {
        SelectedInstrumentMT = InstrumentMorphTargetOptions[0];
    }
}

// ============================================================
// 绘制 MT 编辑器
// ============================================================

void SWindRiseModulePropertiesPanel::DrawMorphTargetEditor(
    TSharedPtr<SVerticalBox> Container, const FString& SectionTitle,
    TArray<FString>& TargetNames, TArray<TSharedPtr<FString>>& DropdownOptions,
    TSharedPtr<FString>& SelectedOption, const TFunction<FReply()>& OnAdd,
    const TFunction<FReply(FString)>& OnRemove,
    USkeletalMeshComponent* SourceSkelComp) {
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[FCommonPanelUtility::CreateSectionHeader(SectionTitle)];

    // 下拉 + 添加按钮
    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 5.0f, 0.0f)
             [SNew(SComboBox<TSharedPtr<FString>>)
                  .OptionsSource(&DropdownOptions)
                  .OnSelectionChanged_Lambda(
                      [&SelectedOption](TSharedPtr<FString> NewValue,
                                        ESelectInfo::Type) {
                          if (NewValue.IsValid()) {
                              SelectedOption = NewValue;
                          }
                      })
                  .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                      return SNew(STextBlock).Text(FText::FromString(*Item));
                  })[SNew(STextBlock).Text_Lambda([&SelectedOption]() -> FText {
                      if (SelectedOption.IsValid())
                          return FText::FromString(*SelectedOption);
                      return FText::GetEmpty();
                  })]] +
         SHorizontalBox::Slot().AutoWidth()
             [SNew(SButton)
                  .Text(LOCTEXT("AddMT", "Add"))
                  .OnClicked_Lambda([OnAdd]() -> FReply { return OnAdd(); })
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")]];

    // 已选 MT 列表
    if (TargetNames.Num() == 0) {
        Container->AddSlot().AutoHeight().Padding(
            5.0f)[SNew(STextBlock)
                      .Text(LOCTEXT("NoMT", "(None)"))
                      .ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))];
    } else {
        for (const FString& MTName : TargetNames) {
            FString NameCopy = MTName;
            Container->AddSlot().AutoHeight().Padding(2.0f, 1.0f)
                [SNew(SHorizontalBox) +
                 SHorizontalBox::Slot().FillWidth(1.0f).VAlign(
                     VAlign_Center)[SNew(STextBlock)
                                        .Text(FText::FromString(MTName))
                                        .Font(FAppStyle::GetFontStyle(
                                            "PropertyWindow.NormalFont"))] +
                 SHorizontalBox::Slot().AutoWidth()
                     [SNew(SButton)
                          .Text(LOCTEXT("RemoveMT", "✕"))
                          .OnClicked_Lambda([this, NameCopy]() -> FReply {
                              return OnRemoveCharacterMorphTarget(NameCopy);
                          })
                          .ButtonStyle(FAppStyle::Get(),
                                       "FlatButton.Default")]];
        }
    }
}

// ============================================================
// 属性面板布局
// ============================================================

void SWindRiseModulePropertiesPanel::CreatePropertyWidgets() {
    auto Container = GetPropertyContainer();
    if (!Container.IsValid()) return;

    Container->ClearChildren();

    if (!WindRiseActor.IsValid()) {
        Container->AddSlot().AutoHeight().Padding(
            5.0f)[SNew(STextBlock)
                      .Text(LOCTEXT("NoActorSelected",
                                    "No WindRise Actor Selected"))
                      .ColorAndOpacity(FLinearColor::Yellow)];
        return;
    }

    AWindRiseUnreal* WindRise = WindRiseActor.Get();

    // ========== Config ==========
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("Config"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateStringPropertyRow(
        TEXT("Instrument Type"), WindRise->InstrumentType,
        TEXT("InstrumentType"), FSimpleDelegate())];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateStringPropertyRow(
        TEXT("Description"), WindRise->Description, TEXT("Description"),
        FSimpleDelegate())];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("Min Note"), WindRise->MinNote, TEXT("MinNote"),
        [this](const FString&, int32 NewValue) {
            if (WindRiseActor.IsValid()) {
                WindRiseActor.Get()->MinNote = NewValue;
                WindRiseActor.Get()->Modify();
            }
        })];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateNumericPropertyRow(
        TEXT("Max Note"), WindRise->MaxNote, TEXT("MaxNote"),
        [this](const FString&, int32 NewValue) {
            if (WindRiseActor.IsValid()) {
                WindRiseActor.Get()->MaxNote = NewValue;
                WindRiseActor.Get()->Modify();
            }
        })];

    // ========== 人物 Morph Target ==========
    DrawMorphTargetEditor(
        Container, TEXT("Character Morph Targets (Lips/Mouth)"),
        WindRise->CharacterMorphTargets, CharacterMorphTargetOptions,
        SelectedCharacterMT, [this]() { return OnAddCharacterMorphTarget(); },
        [this](FString Name) { return OnRemoveCharacterMorphTarget(Name); },
        WindRise->SkeletalMeshActor
            ? WindRise->SkeletalMeshActor->GetSkeletalMeshComponent()
            : nullptr);

    // ========== 乐器 Morph Target ==========
    DrawMorphTargetEditor(
        Container, TEXT("Instrument Morph Targets"),
        WindRise->InstrumentMorphTargets, InstrumentMorphTargetOptions,
        SelectedInstrumentMT, [this]() { return OnAddInstrumentMorphTarget(); },
        [this](FString Name) { return OnRemoveInstrumentMorphTarget(Name); },
        WindRise->InstrumentMesh
            ? WindRise->InstrumentMesh->GetSkeletalMeshComponent()
            : nullptr);

    // ========== Control Rig ==========
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT("Control Rig"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("CheckObjectsStatusButton",
                                "Check Performer Control Rig Status"))
                  .OnClicked(
                      this,
                      &SWindRiseModulePropertiesPanel::OnCheckObjectsStatus)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("SetupPerformerCRButton",
                                "Initialize Performer Control Rig"))
                  .OnClicked(
                      this,
                      &SWindRiseModulePropertiesPanel::OnInitializePerformerCR)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];

    // ========== .wind 文件 ==========
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f,
        5.0f)[FCommonPanelUtility::CreateSectionHeader(TEXT(".wind File"))];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[FCommonPanelUtility::CreateFilePathPropertyRowWithCallback(
        TEXT("IO File Path"), WindRise->IOFilePath, TEXT("IOFilePath"),
        TEXT(".wind"),
        [this](const FString& NewPath) {
            if (WindRiseActor.IsValid()) {
                WindRiseActor.Get()->IOFilePath = NewPath;
                WindRiseActor.Get()->Modify();
            }
        },
        true)];

    Container->AddSlot().AutoHeight().Padding(5.0f)
        [SNew(SHorizontalBox) +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(0.0f, 0.0f, 5.0f, 0.0f)
             [SNew(SButton)
                  .Text(LOCTEXT("ImportWindFile", "Import .wind"))
                  .OnClicked(this,
                             &SWindRiseModulePropertiesPanel::OnImportWindFile)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")] +
         SHorizontalBox::Slot().FillWidth(0.5f).Padding(5.0f, 0.0f, 0.0f, 0.0f)
             [SNew(SButton)
                  .Text(LOCTEXT("ExportWindFile", "Export .wind"))
                  .OnClicked(this,
                             &SWindRiseModulePropertiesPanel::OnExportWindFile)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")]];
}

// ============================================================
// 回调
// ============================================================

FReply SWindRiseModulePropertiesPanel::OnCheckObjectsStatus() {
    if (WindRiseActor.IsValid()) {
        WindRiseActor.Get()->CheckControlRigStatus();
    }
    return FReply::Handled();
}

FReply SWindRiseModulePropertiesPanel::OnInitializePerformerCR() {
    if (WindRiseActor.IsValid()) {
        WindRiseActor.Get()->InitializePerformerControlRig();
    }
    return FReply::Handled();
}

FReply SWindRiseModulePropertiesPanel::OnImportWindFile() {
    if (!WindRiseActor.IsValid()) return FReply::Handled();

    AWindRiseUnreal* WindRise = WindRiseActor.Get();
    if (!WindRise->IOFilePath.IsEmpty()) {
        WindRise->ImportWindFile(WindRise->IOFilePath);
        RefreshProperties();
    }
    return FReply::Handled();
}

FReply SWindRiseModulePropertiesPanel::OnExportWindFile() {
    if (!WindRiseActor.IsValid()) return FReply::Handled();

    AWindRiseUnreal* WindRise = WindRiseActor.Get();
    if (!WindRise->IOFilePath.IsEmpty()) {
        WindRise->ExportWindFile(WindRise->IOFilePath);
    }
    return FReply::Handled();
}

FReply SWindRiseModulePropertiesPanel::OnAddCharacterMorphTarget() {
    if (!WindRiseActor.IsValid() || !SelectedCharacterMT.IsValid()) {
        return FReply::Handled();
    }
    AWindRiseUnreal* WindRise = WindRiseActor.Get();
    WindRise->CharacterMorphTargets.Add(*SelectedCharacterMT);
    WindRise->Modify();
    RefreshProperties();
    return FReply::Handled();
}

FReply SWindRiseModulePropertiesPanel::OnRemoveCharacterMorphTarget(
    FString MorphTargetName) {
    if (!WindRiseActor.IsValid()) return FReply::Handled();
    WindRiseActor.Get()->CharacterMorphTargets.Remove(MorphTargetName);
    WindRiseActor.Get()->Modify();
    RefreshProperties();
    return FReply::Handled();
}

FReply SWindRiseModulePropertiesPanel::OnAddInstrumentMorphTarget() {
    if (!WindRiseActor.IsValid() || !SelectedInstrumentMT.IsValid()) {
        return FReply::Handled();
    }
    AWindRiseUnreal* WindRise = WindRiseActor.Get();
    WindRise->InstrumentMorphTargets.Add(*SelectedInstrumentMT);
    WindRise->Modify();
    RefreshProperties();
    return FReply::Handled();
}

FReply SWindRiseModulePropertiesPanel::OnRemoveInstrumentMorphTarget(
    FString MorphTargetName) {
    if (!WindRiseActor.IsValid()) return FReply::Handled();
    WindRiseActor.Get()->InstrumentMorphTargets.Remove(MorphTargetName);
    WindRiseActor.Get()->Modify();
    RefreshProperties();
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
