# Control Rig动画烘焙模块使用指南

## 概述

Control Rig动画烘焙模块是一个强大的工具集，用于将实时Control Rig动画转换为静态关键帧动画。该模块支持扫描Level Sequence中的Control Rig轨道，并对指定的Control进行动画曲线烘焙。

## 模块架构

### 核心组件

#### 1. Common模块（通用基础组件）

**ControlRigScanner（扫描器）**
- 文件路径：`Plugins/MusicDoll/Source/Common/Public/Private/Baking/ControlRigScanner.h/.cpp`
- 功能：扫描当前Level Sequence中所有Control Rig轨道
- 主要方法：
  - `ScanLevelSequenceControlRigs()` - 扫描所有Control Rig轨道
  - `GetActorControlRigInfo()` - 获取指定Actor的Control Rig信息
  - `GetControlNamesFromBlueprint()` - 从蓝图获取Control名称列表

**AnimationBaker（烘焙器）**
- 文件路径：`Plugins/MusicDoll/Source/Common/Public/Private/Baking/AnimationBaker.h/.cpp`
- 功能：实现动画模拟播放和关键帧提取的核心逻辑
- 主要方法：
  - `BakeControlAnimation()` - 烘焙单个Control动画
  - `BakeMultipleControlAnimations()` - 批量烘焙多个Control
  - `GetControlValueAtFrame()` - 获取指定帧的Control值

**BakeOperationsPanelBase（通用操作面板基类）**
- 文件路径：`Plugins/MusicDoll/Source/Common/Public/Private/Baking/BakeOperationsPanelBase.h/.cpp`
- 功能：提供通用的烘焙操作界面
- 特性：
  - 扫描结果显示
  - 烘焙参数设置（帧范围、步长等）
  - 进度显示和状态管理

#### 2. KeyRippleUnreal模块（钢琴演奏者专用）

**SKeyRippleBakeOperationsPanel**
- 文件路径：`Plugins/MusicDoll/Source/KeyRippleUnreal/Public/Private/KeyRippleBakeOperationsPanel.h/.cpp`
- 特色功能：
  - 为演奏者和钢琴分别提供Control选择下拉菜单
  - 预设常用的KeyRipple控制器分组
  - 支持同时选择多个Control进行批量烘焙

**UKeyRippleBakeProcessor**
- 文件路径：`Plugins/MusicDoll/Source/KeyRippleUnreal/Public/Private/KeyRippleBakeProcessor.h/.cpp`
- 集成特点：
  - 利用现有的ControlRigCacheSubsystem
  - 复用KeyRippleAnimationHelper中的控制器名称管理
  - 与现有动画生成功能保持一致性

#### 3. StringFlowUnreal模块（弦乐器专用）

**SStringFlowBakeOperationsPanel**
- 文件路径：`Plugins/MusicDoll/Source/StringFlowUnreal/Public/Private/StringFlowBakeOperationsPanel.h/.cpp`
- 特色功能：
  - 为演奏者/琴/弓三个骨骼分别提供Control选择
  - 支持StringFlow特有的控制器命名规则
  - 提供弦乐器专用的预设Control分组

**UStringFlowBakeProcessor**
- 文件路径：`Plugins/MusicDoll/Source/StringFlowUnreal/Public/Private/StringFlowBakeProcessor.h/.cpp`
- 集成特点：
  - 充分利用StringFlowUnreal中已有的Control Rig缓存机制
  - 支持多组件(ControlRig)的独立烘焙操作
  - 与现有StringFlow动画处理流程无缝集成

## 使用方法

### 基本操作流程

1. **准备工作**
   - 确保已打开包含Control Rig动画的Level Sequence
   - 选择要进行烘焙的目标Actor（KeyRipple或StringFlow）

2. **扫描Control Rig轨道**
   - 点击"Scan Control Rigs"按钮
   - 系统将自动扫描当前Level Sequence中的所有Control Rig轨道
   - 扫描结果将显示在界面中，包括绑定的Actor和可用的Control列表

3. **设置烘焙参数**
   - **Start Frame**: 烘焙开始帧号
   - **End Frame**: 烘焙结束帧号
   - **Frame Step**: 帧采样步长（默认为1，即逐帧采样）
   - **Overwrite existing keyframes**: 是否覆盖现有关键帧

4. **选择要烘焙的Control**
   - KeyRipple模块：分别选择演奏者和钢琴的Control
   - StringFlow模块：分别选择演奏者、乐器和琴弓的Control
   - 可以选择多个Control进行批量烘焙

5. **执行烘焙**
   - 点击"Bake Animation"按钮开始烘焙过程
   - 界面将显示烘焙进度
   - 烘焙完成后，动画关键帧将被写入Level Sequence

### 高级功能

#### 批量烘焙
```cpp
// KeyRipple批量烘焙示例
TArray<FString> PerformerControls = {"LeftHand_Control", "RightHand_Control"};
TArray<FString> PianoControls = {"PianoPedal_Control"};

FAnimationBakeSettings Settings;
Settings.StartFrame = 0;
Settings.EndFrame = 100;
Settings.FrameStep = 1;

int32 BakedCount = UKeyRippleBakeProcessor::BakeAllKeyRippleAnimation(
    KeyRippleActor, 
    PerformerControls, 
    PianoControls, 
    Settings);
```

#### 自定义进度回调
```cpp
auto ProgressCallback = [](int32 Current, int32 Total, const FString& ControlName)
{
    UE_LOG(LogTemp, Log, TEXT("Baking %s: %d/%d"), *ControlName, Current, Total);
};

UAnimationBaker::BakeMultipleControlAnimations(
    LevelSequence,
    ControlRigInstance,
    ControlNames,
    Settings,
    ProgressCallback);
```

## API参考

### 核心结构体

**FAnimationBakeSettings**
```cpp
struct FAnimationBakeSettings
{
    int32 StartFrame;           // 开始帧
    int32 EndFrame;             // 结束帧
    int32 FrameStep;            // 帧步长
    bool bOverwriteExistingKeys; // 是否覆盖现有关键帧
    float BakePrecision;        // 烘焙精度
};
```

**FControlRigScanResult**
```cpp
struct FControlRigScanResult
{
    ASkeletalMeshActor* BoundActor;        // 绑定的Actor
    UControlRig* ControlRigInstance;       // Control Rig实例
    UControlRigBlueprint* ControlRigBlueprint; // Control Rig蓝图
    TArray<FString> AvailableControls;     // 可用Control列表
    FString TrackName;                     // 轨道名称
};
```

### 主要方法

**ControlRigScanner**
```cpp
// 扫描Level Sequence中的所有Control Rig
static bool ScanLevelSequenceControlRigs(TMap<FString, FControlRigScanResult>& OutResults);

// 获取指定Actor的Control Rig信息
static bool GetActorControlRigInfo(ASkeletalMeshActor* Actor, FControlRigScanResult& OutResult);
```

**AnimationBaker**
```cpp
// 烘焙单个Control动画
static bool BakeControlAnimation(
    ULevelSequence* LevelSequence,
    UControlRig* ControlRigInstance,
    const FString& ControlName,
    const FAnimationBakeSettings& Settings,
    TFunction<void(int32, int32)> OutProgressCallback = nullptr);

// 批量烘焙多个Control动画
static int32 BakeMultipleControlAnimations(
    ULevelSequence* LevelSequence,
    UControlRig* ControlRigInstance,
    const TArray<FString>& ControlNames,
    const FAnimationBakeSettings& Settings,
    TFunction<void(int32, int32, const FString&)> OutProgressCallback = nullptr);
```

## 最佳实践

### 性能优化建议

1. **合理设置帧步长**
   - 对于高频动画，可以适当增加FrameStep以减少关键帧数量
   - 对于精细动作，保持FrameStep=1以保证动画质量

2. **分批处理大型动画**
   - 对于长时间序列，建议分段烘焙
   - 可以按Control类型分组进行批量处理

3. **及时清理临时数据**
   - 烘焙完成后及时释放不需要的资源
   - 避免在编辑器中积累过多的Undo历史

### 质量保证

1. **预览检查**
   - 烘焙前先预览动画效果
   - 确认Control选择正确无误

2. **备份重要数据**
   - 对重要的Level Sequence建议先备份
   - 烘焙前保存当前工作进度

3. **验证烘焙结果**
   - 烘焙后检查关键帧分布是否合理
   - 验证动画播放是否符合预期

## 故障排除

### 常见问题

**Q: 扫描时找不到Control Rig轨道**
A: 
- 确保Level Sequence已正确打开
- 检查Actor是否已正确绑定到Control Rig轨道
- 验证Control Rig实例是否正常加载

**Q: 烘焙过程中出现错误**
A:
- 检查Control名称是否正确
- 确认帧范围设置是否合理
- 验证是否有足够的权限修改Level Sequence

**Q: 烘焙后的动画效果不理想**
A:
- 尝试减小FrameStep值提高采样密度
- 检查Control Rig的初始状态是否正确
- 验证动画曲线的插值设置

### 调试技巧

1. **启用详细日志**
```cpp
// 在控制台中输入以下命令启用详细日志
log LogTemp Verbose
```

2. **使用调试断点**
- 在关键方法中设置断点观察数据流
- 检查中间结果是否符合预期

3. **逐步验证**
- 先测试简单的单Control烘焙
- 逐步增加复杂度和Control数量

## 扩展开发

### 添加新的烘焙目标

要为新的模块添加烘焙支持，需要：

1. 继承`SBakeOperationsPanelBase`创建专用面板
2. 实现`CreateControlSelectionWidget()`和`GetSelectedControlNames()`
3. 创建对应的处理器类继承`UObject`
4. 实现具体的烘焙逻辑和Actor验证方法

### 自定义烘焙算法

可以通过继承`UAnimationBaker`来实现自定义的烘焙算法：

```cpp
class CUSTOM_API UCustomAnimationBaker : public UAnimationBaker
{
    // 实现自定义的烘焙逻辑
};
```

## 版本历史

- v1.0.0: 初始版本，支持基础的Control Rig动画烘焙功能
- v1.1.0: 添加批量烘焙和进度回调支持
- v1.2.0: 完善KeyRipple和StringFlow模块的专用支持

---
*本文档最后更新时间：2026年 2月 26日*