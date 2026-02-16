# MusicDoll 动画生成原理文档

## 概述

MusicDoll 是一个专为音乐表演动画生成而设计的虚幻引擎插件系统。它包含三个核心动画类型，分别用于生成和驱动：

1. **人物演奏动画** - 通过 Level Sequencer 驱动演奏者的身体动作
2. **乐器 Morph Target 动画** - 通过乐器 Control Rig 驱动 Morph Target 变形
3. **乐器材质动画** - 通过 Level Sequencer 的材质参数轨道驱动材质参数

---

## 一、人物演奏动画

### 1.1 概念

人物演奏动画表示演奏者（如钢琴家、小提琴手）的身体动作。这些数据来自Music Doll系列软件（如 KeyRipple、StringFlow、BeatBloom、FretDance）的导出，记录了演奏过程中各个身体部位的位置和旋转。

### 1.2 在 Unreal 中的呈现逻辑

```
JSON 动画数据
    ↓
解析关键帧（位置、旋转）
    ↓
写入 Level Sequencer 的 Control Rig 轨道
    ↓
每个 Control 对应一个动画通道
    ├─ 位置（X、Y、Z 三个轨道）
    └─ 旋转（Pitch、Yaw、Roll 三个轨道）
    ↓
Sequencer 播放时驱动对应的 Control 运动
```

### 1.3 Control 列表

演奏者 Control Rig 中的主要控制器：

#### 左手
- `H_L` - 左手手掌
- `H_rotation_L` - 左手手掌旋转(Unreal中弃用）
- `HP_L` - 左手手臂极向量
- `T_L` - 左手拇指
- `TP_L` - 左手拇指极向量
- `1_L`、`2_L`、`3_L`、`4_L` - 一般左手四个手指
- `0_L`、`1_L`、`2_L`、`3_L`、`4_L` - 钢琴左手五个手指，0表示小拇指

#### 右手
- `H_R` - 右手手掌
- `H_rotation_R` - 右手手掌旋转(Unreal中弃用）
- `HP_R` - 右手手臂极向量
- `T_R` - 右手拇指
- `TP_R` - 右手拇指极向量
- `1_R`、`2_R`、`3_R`、`4_R` - 一般右手四个手指
- `5_R`、`6_R`、`7_R`、`8_R`、`9_R` - 钢琴右手五个手指，5表示大拇指

#### 其他（取决于具体乐器）
- `String_Touch_Point` - 触弦点
- `Bow_Controller` - 琴弓控制器
- `Tar_` - 一系列用于控制人物朝向的点

### 1.4 生成流程

1. **导出动画数据**
   - 在 KeyRipple、StringFlow 等Music Doll工具中分析乐曲
   - 导出 JSON 格式的动画数据（left_hand.json、right_hand.json）

2. **导入 Unreal**
   - 在场景中放入对应的Actor，钢琴放KeyRippleUnrealActor，小提琴放StringFlowUnrealActor
   - 设置Actor里必要的参数，如人物骨骼，乐器骨骼，乐器材质等
   - 为人物骨骼和乐器骨骼，创建对应的 Control Rig
   - 打开对应的 Level Sequence，放入人物骨骼、乐器骨骼
   - 把人物骨骼和乐器骨骼的Control rig 绑定到 Level Sequence 中
   - 初始化乐器，包括生成材质轨道，生成Morph Target轨道等
   - 调用动画处理器（AnimationProcessor）
   - 自动解析 JSON 并写入 Control Rig 轨道

3. **预览**
   - 在 Sequencer 中按播放按钮
   - 演奏者角色按照动画数据运动

### 1.5 数据格式示例

```json
[
  {
    "frame": 0,
    "hand_infos": {
      "H_L": [x, y, z],              // 左手位置
      "H_rotation_L": [w, x, y, z],  // 左手旋转（四元数）
      "1_L": [x, y, z],              // 左手食指位置
      "1_rotation_L": [w, x, y, z],  // 左手食指旋转
      ...
    }
  },
  {
    "frame": 1,
    "hand_infos": { ... }
  }
]
```

---

## 二、乐器 Morph Target 动画

### 2.1 概念

乐器 Morph Target 动画驱动乐器的几何变形，表现乐器本身的机械运动。例如：

- **钢琴** - 琴键的按下和抬起
- **小提琴** - 弦的振动和形状变化

### 2.2 在 Unreal 中的呈现逻辑

这是一个**两步骤的过程**：

#### 步骤 1：动画轨道的设置（代码自动完成）

```
乐器 JSON 动画数据
    ↓
提取 Morph Target 名称列表
    ↓
获取或创建乐器 Control Rig
    ↓
在 Root Control 下创建 Float Animation Channels
（每个通道与一个 Morph Target 同名）
    ↓
解析 JSON 关键帧数据
    ↓
写入 Level Sequencer 中 Control Rig 的对应通道
```

此时 Sequencer 中的 Control Rig 轨道结构如下：

```
Control Rig: Violin
├─ Root Control (violin_root)
│  ├─ s0fret0 (Float Channel) → 关键帧数据
│  ├─ s0fret1 (Float Channel) → 关键帧数据
│  ├─ s0fret2 (Float Channel) → 关键帧数据
│  └─ ...
```

对于钢琴：

```
Control Rig: Piano
├─ Root Control (piano_key_root)
│  ├─ key_0_pressed (Float Channel) → 关键帧数据
│  ├─ key_1_pressed (Float Channel) → 关键帧数据
│  ├─ key_2_pressed (Float Channel) → 关键帧数据
│  └─ ...（88 个琴键）
```

#### 步骤 2：同步浮点值到 Morph Target（用户在蓝图中实现）

**重点：这一步需要使用者在蓝图中手动实现！**

Control Rig 中的 Float Channel 只是数据容器，并不直接驱动 Morph Target。使用者需要创建一个同步函数，将这些浮动值应用到对应的 Morph Target 上。

**蓝图实现步骤：**

1. **在 EventTick 或其他事件中执行**
   ```
   获取乐器的 SkeletalMeshComponent
   获取 Control Rig 实例
   ```

2. **逐个同步 Morph Target**
   ```
   对于每个 Morph Target（如 "key_51_pressed"）：
     ├─ 从 Control Rig 的 Float Channel 读取当前值
     │  （使用 "Get Control Value" 或类似节点）
     ├─ 使用 "Set Morph Target" 节点
     └─ 输入参数：
         ├─ Skeletal Mesh Component
         ├─ Morph Target Name: "key_51_pressed"
         └─ Value: （从 Control Rig 读取的浮点值）
   ```

3. **关键节点**
   - `Get Control Value` - 从 Control Rig 读取浮点值
   - `Set Morph Target` - 设置 Morph Target 权重
   - 在 Tick 事件中调用，确保每帧都同步

**蓝图示意（伪代码）：**

```
Event Tick
├─ Get SkeletalMeshComponent (Piano)
├─ Get Control Rig (Piano)
├─ For Each Morph Target Name in Morph Target List
│  ├─ Get Control Value (ControlRig, MorphTargetName)
│  ├─ Set Morph Target (SkeletalMeshComp, MorphTargetName, Value)
│  └─ Next
```

### 2.3 数据格式

JSON 动画数据示例：

```json
{
  "keys": [
    {
      "shape_key_name": "key_0_pressed",
      "keyframes": [
        {"frame": 0, "shape_key_value": 0.0},
        {"frame": 5, "shape_key_value": 0.8},
        {"frame": 10, "shape_key_value": 0.0}
      ]
    },
    {
      "shape_key_name": "key_1_pressed",
      "keyframes": [ ... ]
    }
  ]
}
```

### 2.4 使用者的工作

1. **自动生成**
   - 调用 `GenerateInstrumentAnimation()` 函数
   - 系统自动创建 Control Rig 的 Float Channels
   - 系统自动在 Sequencer 中写入关键帧

2. **手动实现**（蓝图中）
   - 创建一个 Function 或 Blueprint 事件
   - 每帧读取 Control Rig 的浮点值
   - 使用 `Set Curve` 节点应用到对应的Morph Target

---

## 三、乐器材质动画

### 3.1 概念

乐器材质动画驱动材质参数的变化，为乐器提供视觉反馈。例如：

- **钢琴键按压指示** - 按下时改变颜色或发光
- **弦张力可视化** - 显示弦的振动状态

### 3.2 在 Unreal 中的呈现逻辑

```
乐器 Morph Target 动画数据
    ↓
识别每个数据对应的乐器部件（如琴键号）
    ↓
在 Level Sequencer 中创建或查找对应的材质轨道
    ↓
将 Morph Target 数据同步到材质的标量参数
    │
    ├─ 参数名通常为："Pressed"（对于钢琴）
    │              或 "Vibration"（对于弦乐器）
    │
    └─ 为每个部件的材质实例创建独立的参数轨道
    ↓
Sequencer 播放时同时驱动两个轨道：
├─ Morph Target 曲线 (key_51_pressed)
└─ 材质参数曲线 (Mat_Key_51.Pressed)
    ↓
最终效果：琴键既变形又改变视觉表现
```

### 3.3 具体例子（钢琴）

#### 场景：钢琴键 51 被按下

**数据来源：**
```json
{
  "shape_key_name": "key_51_pressed",
  "keyframes": [
    {"frame": 0, "shape_key_value": 0.0},
    {"frame": 5, "shape_key_value": 1.0},
    {"frame": 10, "shape_key_value": 0.0}
  ]
}
```

**在 Unreal 中的结果：**

1. **Control Rig 轨道（自动创建）**
   ```
   Control Rig: Piano
   └─ piano_key_root
      └─ key_51_pressed (Float Channel)
         ├─ Frame 0: Value = 0.0
         ├─ Frame 5: Value = 1.0
         └─ Frame 10: Value = 0.0
   ```

2. **蓝图同步（用户实现）**
   - 每帧读取 `key_51_pressed` 的值
   - 调用 `Set Morph Target(Piano, "key_51_pressed", Value)`
   - 琴键网格发生几何变形

3. **材质参数轨道（代码自动创建）**
   ```
   Material Track: Mat_Key_51
   └─ Parameter: "Pressed"
      ├─ Frame 0: Value = 0.0
      ├─ Frame 5: Value = 1.0
      └─ Frame 10: Value = 0.0
   ```

4. **最终效果**
   - 琴键按下时：Morph Target 使键盘向下，材质参数使按键发光或变色
   - 琴键抬起时：Morph Target 恢复原位，材质参数恢复默认表现

### 3.4 材质设置

为了实现材质动画效果，材质需要包含可驱动的参数：

**在材质编辑器中：**

1. **添加标量参数**
   ```
   参数名称：Pressed
   类型：Float
   默认值：0.0
   范围：0.0 到 1.0
   ```

2. **在材质网络中使用**
   ```
   示例逻辑：
   - 当 Pressed = 0.0 时：使用默认颜色（如白色）
   - 当 Pressed = 1.0 时：使用按压颜色（如深灰色或发光）
   
   节点设置：
   Base Color = Lerp(WhiteColor, GreyColor, Pressed)
   Emissive = Pressed * IntensityValue
   ```

3. **创建材质实例**
   - 基于包含 Pressed 参数的材质创建实例
   - 为每个琴键创建独立的材质实例（如 Mat_Key_0、Mat_Key_1...）
   - 应用到对应的琴键网格槽位

### 3.5 工作流总结

| 阶段 | 谁负责 | 具体工作 |
|------|-------|---------|
| 1. 数据导出 | 用户 | 在分析工具中导出 JSON 动画数据 |
| 2. 动画生成 | MusicDoll 代码 | 创建 Control Rig Channels、写入 Sequencer |
| 3. Morph Target 同步 | 用户蓝图 | 实现每帧从 Control Rig 读值、应用到 Morph Target |
| 4. 材质参数轨道 | MusicDoll 代码 | 自动创建和写入材质参数曲线 |
| 5. 预览 | 用户 | 在 Sequencer 中播放，观看最终效果 |

---

## 四、三类动画的同步

### 4.1 同步原理

三类动画在同一个 Level Sequencer 中完成同步：

```
Timeline
   ↓
Frame 0
├─ 人物演奏动画 → Control Rig 轨道（演奏者位置/旋转）
├─ Morph Target 动画 → Control Rig 轨道（乐器 Float Channels）
│                    ↓（蓝图驱动）
│                    Set Morph Target（乐器变形）
└─ 材质动画 → 材质参数轨道（材质参数值）
   ↓
Frame 1
├─ 人物演奏动画
├─ Morph Target 动画
└─ 材质动画
   ↓
...依次类推...
```

### 4.2 关键要点

1. **所有动画都使用相同的 TickResolution**
   - 确保不同轨道的帧数正确对齐
   - 自动由 Sequencer 处理

2. **Morph Target 同步需要用户参与**
   - Control Rig 的 Float Channel 只是数据
   - 蓝图必须每帧读取这些数据并应用到实际的 Morph Target
   - 这是架构设计中的必要步骤，提供了灵活性

3. **材质动画由代码自动驱动**
   - 不需要用户干预
   - 与 Morph Target 数据同步

---

## 五、常见问题

### Q1: Morph Target 没有变形

**可能原因：**
- 蓝图中的同步函数没有实现或没有运行
- 没有在 EventTick 中调用同步逻辑

**解决方案：**
- 检查蓝图中是否有 "Set Curve" 节点
- 确保函数在 EventTick 中被调用
- 验证从 Control Rig 读取的值是否正确

### Q2: 材质参数没有更新

**可能原因：**
- 材质没有 "Pressed" 等参数定义
- 材质实例没有正确应用到骨骼网格
- 参数轨道没有关键帧

**解决方案：**
- 检查材质编辑器，确保存在相应的标量参数
- 检查骨骼网格的材质槽位设置
- 在 Sequencer 中查看材质参数轨道是否有关键帧

### Q3: 演奏动画和乐器动画不同步

**可能原因：**
- 使用了不同的 Level Sequence
- 帧率设置不一致

**解决方案：**
- 确保所有动画数据都写入同一个 LevelSequence
- 检查 MovieScene 的 TickResolution 是否一致

---

## 六、总结

### 三类动画的角色

| 动画类型 | 驱动对象 | 实现方式 | 用户工作 |
|---------|---------|---------|---------|
| **人物演奏动画** | 演奏者的骨骼/Control | Sequencer 的 Control Rig 轨道 | 无（自动处理） |
| **乐器 Morph Target** | 乐器的网格变形 | Control Rig Float Channels + 蓝图 | 实现同步蓝图 |
| **乐器材质动画** | 乐器的材质参数 | Sequencer 的材质参数轨道 | 无（自动处理） |

### 工作流概览

```
1. 导出动画数据（JSON）
          ↓
2. 导入 Unreal，调用生成函数
          ↓
3. 系统自动创建 Sequencer 轨道
          ↓
4. 用户在蓝图中实现 Morph Target 同步（关键一步！）
          ↓
5. 在 Sequencer 中播放预览
          ↓
6. 三类动画同时运行，完整呈现音乐表演
```

---

**文档版本：2.0**  
**最后更新：2026/02/06**

---
**声明：以上内容是AI写的，有不明白还是直接问作者更准确**