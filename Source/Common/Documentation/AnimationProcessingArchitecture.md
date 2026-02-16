# 动画数据读取->分发->生成架构文档

## 概述

本文档详细描述了KeyRipple和StringFlow两个模块中动画数据从读取到最终生成的完整架构流程。

## 整体架构模式

两个模块都遵循统一的三层架构模式：
1. **配置文件读取层** - 解析JSON配置文件获取动画数据路径
2. **数据分发层** - 根据路径将不同类型动画数据分发给对应处理器
3. **动画生成层** - 各处理器负责具体的动画生成实现

## KeyRipple模块架构

### 1. 入口点
- **KeyRippleUnreal.h** - 定义`AnimationFilePath`属性存储配置文件路径
- **KeyRippleOperationsPanel** - 提供UI操作入口

### 2. 配置文件读取层
**KeyRippleAnimationProcessor::GeneratePerformerAnimation()**
```cpp
// 1. 从Actor获取配置文件路径
FString AnimationPath = KeyRippleActor->AnimationFilePath;

// 2. 调用直接处理方法
GeneratePerformerAnimationDirect(KeyRippleActor, AnimationPath);
```

### 3. 数据分发层
**KeyRippleAnimationProcessor::GeneratePerformerAnimationDirect()**
```cpp
// 1. 解析配置文件获取各动画路径
FString LeftHandPath, RightHandPath, PianoKeyPath;
ParseKeyRippleConfigFile(KeyRippleActor, LeftHandPath, RightHandPath, PianoKeyPath);

// 2. 分发给不同处理器
if (!LeftHandPath.IsEmpty()) {
    MakePerformerAnimation(KeyRippleActor, LeftHandPath, LevelSequence);
}
if (!RightHandPath.IsEmpty()) {
    MakePerformerAnimation(KeyRippleActor, RightHandPath, LevelSequence);
}
if (!PianoKeyPath.IsEmpty()) {
    UKeyRipplePianoProcessor::GenerateInstrumentAnimation(KeyRippleActor, PianoKeyPath);
}
```

### 4. 动画生成层

#### 演奏者动画生成 (KeyRippleAnimationProcessor)
**MakePerformerAnimation()**
- 处理演奏者模型(SkeletalMeshActor)的Control Rig动画
- 使用通用的JSON解析和关键帧批量插入工具

#### 乐器动画生成 (KeyRipplePianoProcessor)
**GenerateInstrumentAnimation()**
- 处理钢琴Morph Target动画
- 处理钢琴材质参数动画

## StringFlow模块架构

### 1. 入口点
- **StringFlowUnreal.h** - 定义`AnimationFilePath`属性存储配置文件路径
- **StringFlowOperationsPanel** - 提供UI操作入口

### 2. 配置文件读取层
**StringFlowAnimationProcessor::GeneratePerformerAnimation()**
```cpp
// 1. 从Actor获取配置文件路径
FString AnimationPath = StringFlowActor->AnimationFilePath;

// 2. 调用直接处理方法
GeneratePerformerAnimationDirect(StringFlowActor, AnimationPath);
```

### 3. 数据分发层
**StringFlowAnimationProcessor::GeneratePerformerAnimationDirect()**
```cpp
// 1. 解析配置文件获取各动画路径
FString LeftHandPath, RightHandPath, StringVibrationPath;
ParseStringFlowConfigFile(StringFlowActor, LeftHandPath, RightHandPath, StringVibrationPath);

// 2. 分发给不同处理器
if (!LeftHandPath.IsEmpty()) {
    MakePerformerAnimation(StringFlowActor, LeftHandPath, LevelSequence);
}
if (!RightHandPath.IsEmpty()) {
    MakePerformerAnimation(StringFlowActor, RightHandPath, LevelSequence);
}
if (!StringVibrationPath.IsEmpty()) {
    UStringFlowMusicInstrumentProcessor::GenerateInstrumentAnimation(StringFlowActor);
}
```

### 4. 动画生成层

#### 演奏者动画生成 (StringFlowAnimationProcessor)
**MakePerformerAnimation()**
- 处理演奏者模型(SkeletalMeshActor)的Control Rig动画
- 使用通用的JSON解析和关键帧批量插入工具

#### 乐器动画生成 (StringFlowMusicInstrumentProcessor)
**GenerateInstrumentAnimation()**
- 处理弦乐器Morph Target动画（使用新的方法替代旧的弦振动动画）
- 处理弦乐器材质参数动画

## 配置文件格式

### KeyRipple配置文件 (keyripple_config.json)
```json
{
    "left_hand_animation": "path/to/left_hand.json",
    "right_hand_animation": "path/to/right_hand.json", 
    "piano_key_animation": "path/to/piano_keys.json"
}
```

### StringFlow配置文件 (stringflow_config.json)
```json
{
    "left_hand_animation": "path/to/left_hand.json",
    "right_hand_animation": "path/to/right_hand.json",
    "string_vibration_animation": "path/to/string_vibration.json"
}
```

## 通用工具类复用

两个模块都大量复用Common模块提供的工具类：

### InstrumentAnimationUtility
- `ProcessControlsContainer()` - 通用控件容器处理
- `BatchInsertControlRigKeys()` - 批量关键帧插入
- `ParseAnimationJsonFile()` - 通用JSON动画文件解析

### InstrumentControlRigUtility
- Control Rig相关通用操作

### InstrumentMorphTargetUtility
- Morph Target动画处理工具

### InstrumentMaterialUtility
- 材质参数动画处理工具

## 架构优势

1. **统一性** - 两个模块采用完全相同的架构模式
2. **可维护性** - 通用逻辑集中在Common模块，减少重复代码
3. **扩展性** - 新增动画类型只需在对应处理器中实现
4. **清晰性** - 职责分离明确，数据流向清晰

## 注意事项

1. 所有处理器方法都应该是静态方法便于调用
2. 配置文件解析应该统一接口规范
3. 错误处理要完善，提供清晰的日志输出
4. 遵循"配置文件读取 → 路径分发 → 分类处理"的标准流程