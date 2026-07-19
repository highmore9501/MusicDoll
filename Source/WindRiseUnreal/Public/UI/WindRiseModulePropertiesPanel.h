#pragma once

#include "CoreMinimal.h"
#include "UI/ModulePropertiesPanelBase.h"

class AWindRiseUnreal;

/**
 * WindRise 属性面板
 * 显示和编辑 AWindRiseUnreal 的基础属性
 *
 * 布局：
 * - Config：InstrumentType, Description, MinNote, MaxNote
 * - 人物 Morph Target（嘴唇/口腔）添加/删除
 * - 乐器 Morph Target 添加/删除
 * - Control Rig：Check Status / Initialize Performer CR
 * - .wind 文件：路径浏览 + Import / Export
 */
class WINDRISEUNREAL_API SWindRiseModulePropertiesPanel
    : public SModulePropertiesPanel {
   public:
    SLATE_BEGIN_ARGS(SWindRiseModulePropertiesPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // SModulePropertiesPanel interface
    virtual void SetActor(AActor* InActor) override;
    virtual bool CanHandleActor(const AActor* InActor) const override;
    virtual void RefreshProperties() override;

   protected:
    virtual void CreatePropertyWidgets() override;

   private:
    TWeakObjectPtr<AWindRiseUnreal> WindRiseActor;

    // 回调
    FReply OnCheckObjectsStatus();
    FReply OnInitializePerformerCR();
    FReply OnImportWindFile();
    FReply OnExportWindFile();

    // 人物 MT 编辑
    FReply OnAddCharacterMorphTarget();
    FReply OnRemoveCharacterMorphTarget(FString MorphTargetName);

    // 乐器 MT 编辑
    FReply OnAddInstrumentMorphTarget();
    FReply OnRemoveInstrumentMorphTarget(FString MorphTargetName);

    // MT 下拉选项
    TArray<TSharedPtr<FString>> CharacterMorphTargetOptions;
    TSharedPtr<FString> SelectedCharacterMT;
    TArray<TSharedPtr<FString>> InstrumentMorphTargetOptions;
    TSharedPtr<FString> SelectedInstrumentMT;

    // 刷新 MT 下拉选项
    void RefreshMorphTargetOptions();

    // 绘制 MT 编辑器
    void DrawMorphTargetEditor(TSharedPtr<SVerticalBox> Container,
                               const FString& SectionTitle,
                               TArray<FString>& TargetNames,
                               TArray<TSharedPtr<FString>>& DropdownOptions,
                               TSharedPtr<FString>& SelectedOption,
                               const TFunction<FReply()>& OnAdd,
                               const TFunction<FReply(FString)>& OnRemove,
                               class USkeletalMeshComponent* SourceSkelComp);
};
