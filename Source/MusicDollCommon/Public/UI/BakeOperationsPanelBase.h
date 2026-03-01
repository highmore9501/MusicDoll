#pragma once

#include "Baking/AnimationBaker.h"
#include "CoreMinimal.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 已选择的Control条目结构
 * 用于跟踪用户选择的不同Control Rig Instance上的Control
 */
struct FSelectedControlEntry
{
    UControlRig* ControlRigInstance;
    FString ControlName;
    FString DisplayName; // 格式: "InstanceName.ControlName"
    
    FSelectedControlEntry() 
        : ControlRigInstance(nullptr)
    {}
    
    FSelectedControlEntry(UControlRig* InControlRig, const FString& InControlName, const FString& InDisplayName)
        : ControlRigInstance(InControlRig)
        , ControlName(InControlName)
        , DisplayName(InDisplayName)
    {}
};

/**
 * Control Rig烘焙操作面板基类
 * 提供通用的烘焙操作界面和功能
 */
class MUSICDOLLCOMMON_API SBakeOperationsPanelBase : public SCompoundWidget {
   public:
    SLATE_BEGIN_ARGS(SBakeOperationsPanelBase) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual ~SBakeOperationsPanelBase();

    // 基础功能接口
    virtual void SetActor(AActor* InActor);
    virtual void RefreshScanResults() = 0;  // 纯虚函数，子类必须实现
    virtual void OnBakeButtonClicked();
    virtual void OnCancelButtonClicked();
    virtual void OnClearSelectedTracksButtonClicked();
    virtual bool IsBakingInProgress() const { return bIsBaking; }
    
    // Control选择管理
    virtual void AddSelectedControl();
    virtual void RemoveSelectedControl(int32 Index);
    virtual TArray<FSelectedControlEntry> GetSelectedControls() const { return SelectedControls; }
    
    // 通用的帮助方法 - 子类可以复用
    bool AddControlToSelection(UControlRig* ControlRig, const FString& ControlName, const FString& DisplayName);
    void FinalizeAddSelectedControl(bool bAddedAny);
    
    // UI事件处理
    FReply HandleAddSelectedClicked();
    TSharedRef<ITableRow> GenerateSelectedControlRow(
        TSharedPtr<FSelectedControlEntry> Item,
        const TSharedRef<STableViewBase>& OwnerTable);
    void RefreshSelectedControlsList();

   protected:
    // Actor引用
    TWeakObjectPtr<AActor> CurrentActor;

    // 已选择的Control管理
    TArray<FSelectedControlEntry> SelectedControls;
    
    // 扫描相关
    TMap<FString, FControlRigScanResult> ScanResults;
    bool bHasValidScanResults;
    
    // UI组件 - 已选择Control显示
    TSharedPtr<SListView<TSharedPtr<FSelectedControlEntry>>> SelectedControlsListView;
    TArray<TSharedPtr<FSelectedControlEntry>> SelectedControlsItems;

    // 烘焙相关
    bool bIsBaking;
    FAnimationBakeSettings CurrentBakeSettings;

    // UI控件
    TSharedPtr<SVerticalBox> ScanResultsContainer;
    TSharedPtr<SEditableTextBox> StartFrameTextBox;
    TSharedPtr<SEditableTextBox> EndFrameTextBox;
    TSharedPtr<SEditableTextBox> FrameStepTextBox;
    TSharedPtr<SCheckBox> OverwriteCheckBox;
    TSharedPtr<SButton> ScanButton;
    TSharedPtr<SButton> BakeButton;
    TSharedPtr<SButton> CancelButton;
    TSharedPtr<SButton> ClearTracksButton;
    TSharedPtr<STextBlock> StatusTextBlock;
    TSharedPtr<SProgressBar> Progressbar;

    // 子类需要实现的纯虚函数
    virtual TSharedRef<SWidget> CreateControlSelectionWidget() = 0;
    virtual TArray<FString> GetSelectedControlNames() const = 0;
        
    // 通用UI创建方法
    TSharedRef<SWidget> CreateScanSection();
    TSharedRef<SWidget> CreateBakeSettingsSection();
    TSharedRef<SWidget> CreateControlSelectionSection();
    TSharedRef<SWidget> CreateActionButtonsSection();
    TSharedRef<SWidget> CreateStatusSection();

    // 事件处理
    FReply HandleScanButtonClicked();
    FReply HandleBakeButtonClicked();
    FReply HandleCancelButtonClicked();
    FReply HandleClearSelectedTracksButtonClicked();
    void HandleBakeProgress(int32 Current, int32 Total);

    // 状态管理
    void UpdateStatusText(const FString& Status);
    void SetBakingState(bool bBaking);
    void UpdateButtonStates();
    bool ValidateBakeSettings();

    // 辅助方法
    int32 ParseIntFromTextBox(TSharedPtr<SEditableTextBox> TextBox,
                              int32 DefaultValue) const;
    
    
    // 模块标识方法
    virtual FName GetModuleName() const { return NAME_None; }

   private:
    FString CurrentStatusText;
};