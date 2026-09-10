#include "UI/SingerModulePropertiesPanel.h"

#include "SingerUnreal.h"
#include "Styling/AppStyle.h"
#include "UI/CommonPanelUtility.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SSingerModulePropertiesPanel"

void SSingerModulePropertiesPanel::Construct(const FArguments& InArgs) {
    // 调用基类构造函数，使用基类的参数类型
    SModulePropertiesPanel::FArguments BaseArgs;
    SModulePropertiesPanel::Construct(BaseArgs);
}

void SSingerModulePropertiesPanel::SetActor(AActor* InActor) {
    SingerActor = Cast<ASingerUnreal>(InActor);
    RefreshProperties();
}

bool SSingerModulePropertiesPanel::CanHandleActor(
    const AActor* InActor) const {
    return InActor && InActor->IsA<ASingerUnreal>();
}

void SSingerModulePropertiesPanel::RefreshProperties() {
    CreatePropertyWidgets();
}

void SSingerModulePropertiesPanel::CreatePropertyWidgets() {
    auto Container = GetPropertyContainer();
    if (!Container.IsValid()) {
        return;
    }

    Container->ClearChildren();

    if (!SingerActor.IsValid()) {
        Container->AddSlot().AutoHeight().Padding(
            5.0f)[SNew(STextBlock)
                      .Text(LOCTEXT("NoActorSelected",
                                    "No Singer Actor Selected"))
                      .ColorAndOpacity(FLinearColor::Yellow)];
        return;
    }

    // ===== Control Rig 初始化 =====
    Container->AddSlot().AutoHeight().Padding(
        5.0f, 15.0f, 5.0f, 5.0f)[FCommonPanelUtility::CreateSectionHeader(
        TEXT("Control Rig Initialization"))];

    Container->AddSlot().AutoHeight().Padding(5.0f, 0.0f, 5.0f, 5.0f)
        [SNew(STextBlock)
             .Text(LOCTEXT(
                 "AddEyeControlHint",
                 "确保 base_root 存在，并检测 Control Rig Curves 中名称含 "
                 "「なごみ」的眼睛睁开曲线；存在时在 base_root 下添加同名 "
                 "Float Channel（多个候选取最短名称）。"
                 "\n执行后请在 Control Rig 编辑器中 Compile。"))
             .AutoWrapText(true)];

    Container->AddSlot().AutoHeight().Padding(
        5.0f)[SNew(SButton)
                  .Text(LOCTEXT("AddEyeControlButton", "Add Eye Control"))
                  .OnClicked(this,
                             &SSingerModulePropertiesPanel::OnAddEyeControl)
                  .HAlign(HAlign_Center)
                  .ButtonStyle(FAppStyle::Get(), "FlatButton.Default")];
}

FReply SSingerModulePropertiesPanel::OnAddEyeControl() {
    if (!SingerActor.IsValid()) {
        UE_LOG(LogTemp, Error,
               TEXT("Singer: No actor selected for Add Eye Control"));
        return FReply::Handled();
    }

    const bool bSuccess = SingerActor->InitializeControlRig();
    UE_LOG(LogTemp, Warning,
           TEXT("Singer: Add Eye Control operation %s"),
           bSuccess ? TEXT("succeeded") : TEXT("failed"));

    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
