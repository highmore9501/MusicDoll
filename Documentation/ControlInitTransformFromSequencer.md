# Control Initial Transform From Sequencer 功能说明

## 概述

此功能允许用户在 Sequencer 中调整 Control 位置后，一键将当前变换写入 ControlRigBlueprint 的初始值（Initial Transform），无需手动打开 ControlRig Blueprint 进行操作。
此功能有两个按键，一个是将transform写入初始值，一个是将transform写入offset值，任何情况下都可以使用前者。只有在不需要读取control数据进行计算时，才可以使用后者(比如说一些pole target 控件的位置初始化)。

## 使用方法

### 1. 在 Sequencer 中调整 Control

1. 打开 Level Sequence
2. 选中需要调整初始值的 Control（可以多选）
3. 调整这些 Control 到期望的位置和旋转

### 2. 应用到初始值

1. 在 Bone Control Mapping 面板中，点击左侧操作栏最下方的 **"Apply Selected To Init"** 按钮
2. 系统会自动：
   - 遍历当前 Sequence 中所有 ControlRig
   - 找出其中被选中的 Control
   - 读取它们的当前全局变换
   - 将变换写入对应 ControlRigBlueprint 的初始全局变换
   - 标记 Blueprint 为已修改并触发重新编译
3. 完成后会显示通知，告知成功应用了多少个 Control

## 技术实现

### 新增文件

#### `ControlInitTransformUtility.h/cpp`

核心工具类，提供以下功能：

```cpp
static bool ApplySelectedControlsTransformToInitial(
    int32& OutAppliedCount,
    int32& OutSkippedCount);
```

**工作流程：**

1. 通过 `UInstrumentAnimationUtility::GetCurrentLevelSequence()` 获取当前 Sequence
2. 通过 `UControlRigSequencerEditorLibrary::GetControlRigs()` 获取所有 ControlRig 绑定
3. 遍历每个 ControlRig：
   - 获取 ControlRig 实例和对应的 Blueprint
   - 调用 `ControlRig->GetHierarchy()->GetSelectedKeys()` 获取选中的 Controls
   - 对每个选中的 Control：
     - 从运行时 Hierarchy 读取当前全局变换：`RuntimeHierarchy->GetGlobalTransform()`
     - 写入 Blueprint Hierarchy 的初始全局变换：`BlueprintHierarchy->SetInitialGlobalTransform()`
4. 标记 Blueprint 为已修改：`FBlueprintEditorUtils::MarkBlueprintAsModified()`

### UI 集成

#### `SBoneControlMappingEditPanel.h/cpp`

在左侧操作栏添加新按钮：

```cpp
+ SVerticalBox::Slot().AutoHeight().Padding(5.0f)
      [SNew(SButton)
           .Text(LOCTEXT("ApplySelectedControlsInitButton", "Apply Selected To Init"))
           .OnClicked(this, &SBoneControlMappingEditPanel::OnApplySelectedControlsInitTransformClicked)
           .ToolTipText(LOCTEXT("ApplySelectedControlsInitTooltip",
               "将当前Sequence中选中的Control的变换写入ControlRigBlueprint的初始值"))]
```

**按钮处理函数：**

```cpp
FReply OnApplySelectedControlsInitTransformClicked();
```

调用 `FControlInitTransformUtility::ApplySelectedControlsTransformToInitial()` 并显示结果通知。

## 注意事项

1. **必须在编辑器中使用**：此功能依赖 Sequencer 的选择状态，只能在编辑器模式下使用
2. **选择 Control**：必须先在 Sequencer 中选中至少一个 Control
3. **Blueprint 会被修改**：操作会直接修改 ControlRigBlueprint，建议操作前备份
4. **自动编译**：修改后 Blueprint 会自动重新编译，可能需要等待片刻
5. **全局变换**：写入的是全局变换（相对于 ControlRig 根），不是局部变换

## 应用场景

- 快速设置多个 Control 的初始位置
- 批量调整 Control 的默认姿态
- 从动画帧提取姿态作为初始值
- 无需逐个打开 ControlRig Blueprint 手动调整

## 相关文件

### 新增文件

- `Plugins\MusicDoll\Source\MusicDollCommon\Public\ControlInitTransformUtility.h`
- `Plugins\MusicDoll\Source\MusicDollCommon\Private\ControlInitTransformUtility.cpp`

### 修改文件

- `Plugins\MusicDoll\Source\MusicDollCommon\Public\UI\SBoneControlMappingEditPanel.h`
- `Plugins\MusicDoll\Source\MusicDollCommon\Private\UI\SBoneControlMappingEditPanel.cpp`
