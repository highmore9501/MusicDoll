# 实时同步系统指南

## 概述

本文档详细说明了 StringFlow（弦乐器）系统中的实时同步机制，包括人物 Control Rig、小提琴 Control Rig 和琴弓之间的协调关系。

---

## 第一部分：系统架构与实现原理

### 1.1 三个关键 Control Rig

在 StringFlow 系统中存在三个独立的 Control Rig 实例：

#### A. 人物 Control Rig（Character Control Rig）
- **所有者**: 人物骨骼网格 Actor（`SkeletalMeshActor`）
- **核心控制器**: `controller_root`
  - 用于驱动整个演奏的基础框架
  - 所有人物相关的演奏动作都以此为参考
- **其他关键控制器**:
  - `bow_controller`: 右手弓的控制器
  - `string_touch_point`: 弦触点的参考位置

#### B. 小提琴 Control Rig（Instrument Control Rig）
- **所有者**: 小提琴骨骼网格 Actor（`StringInstrument`）
- **核心控制器**: `violin_root`
  - 驱动整个小提琴模型
  - 通过实时同步与人物 `controller_root` 关联

#### C. 琴弓 Control Rig（Bow Control Rig）
- **所有者**: 琴弓骨骼网格 Actor（`Bow`）
- **核心控制器**: `bow_ctrl`
  - 驱动琴弓模型的位置和旋转
  - 由人物的右手控制器驱动

### 1.2 实时同步机制

#### 小提琴与人物的同步（ParentBetweenControlRig）

实时同步使用一个称为 **"父子关系"** 的机制：

```
人物 Control Rig          小提琴 Control Rig
└─ controller_root    →    └─ violin_root
   (父控制器)                (子控制器)
```

**核心算法**:

1. **计算父控制器的偏移矩阵**
   ```
   offset = controller_root_init^(-1) × controller_root_current
   ```
   - `controller_root_init`: 蓝图中定义的初始化变换
   - `controller_root_current`: 运行时的当前变换
   - 这个 offset 表示相对于初始化位置的偏移

2. **应用到子控制器**
   ```
   violin_root_new = violin_root_init × offset
   ```
   - `violin_root_init`: 蓝图中定义的初始化变换（相对于 controller_root 的相对变换）
   - 小提琴自动跟随人物的所有运动

**关键特性**:
- ✅ 即使 `controller_root` 的初始化变换不是零值，算法也能正确处理
- ✅ 小提琴保持与人物的初始相对关系
- ✅ 人物的任何旋转、位置、缩放变化都会自动传递到小提琴
- ✅ 完全无需在两个 Control Rig 之间来回切换坐标系

#### 琴弓的同步（SyncBowTransform）

琴弓由人物的右手控制驱动：

1. **获取驱动源**
   - 位置源: `bow_controller`（右手弓的控制器位置）
   - 朝向源: `string_touch_point`（弦触点位置）

2. **计算目标旋转**
   - 计算从弓位置指向弦触点的方向向量
   - 使用 `BowAxisTowardString` 参数定义弓的哪个轴应该朝向琴弦
   - 通过 `FQuat::FindBetweenNormals` 计算必要的旋转增量
   - 最终旋转 = 增量旋转 × 当前旋转

3. **应用到琴弓**
   - 位置: 直接使用 `bow_controller` 的位置
   - 旋转: 使用计算后的目标旋转

### 1.3 实现细节

#### 关键方法：FInstrumentControlRigUtility::ParentBetweenControlRig

这是实现父子关系的核心方法，位于 `Common` 模块中：

```cpp
static bool ParentBetweenControlRig(
    ASkeletalMeshActor* ParentControlRig,
    const FString& ParentControlName,
    ASkeletalMeshActor* ChildControlRig,
    const FString& ChildControlName);
```

**用途**: 在两个不同的 Control Rig 中建立 Control 的父子关系

**参数说明**:
- `ParentControlRig`: 父 Control 所属的 Control Rig（通常是人物）
- `ParentControlName`: 父 Control 的名称（如 "controller_root"）
- `ChildControlRig`: 子 Control 所属的 Control Rig（通常是乐器）
- `ChildControlName`: 子 Control 的名称（如 "violin_root"）

#### 关键方法：GetControlRigControlInitTransform

获取 Control Rig 蓝图中定义的初始化变换：

```cpp
static bool GetControlRigControlInitTransform(
    ASkeletalMeshActor* InSkeletalMeshActor,
    const FString& ControlName,
    FTransform& OutInitTransform);
```

这个方法从蓝图的 Hierarchy 中读取初始值，这对于建立正确的父子关系至关重要。

---

## 第二部分：使用指南

### 2.1 初始化流程

按照以下步骤在编辑器中正确初始化小提琴与人物的关系：

#### 步骤 1: 禁用实时同步

1. 选择包含小提琴的 Actor（`AStringFlowUnreal`）
2. 在 Details 面板中找到 **"Transform Sync"** 部分
3. 确保 **`bEnableRealtimeSync`** 为 **`false`**（默认状态）

#### 步骤 2: 手动调整小提琴位置

1. 在 Sequencer 或直接在 3D 视口中编辑 `controller_root` 的位置和旋转
2. 手动调整小提琴的 `violin_root` 变换，使其与人物处于合适的位置
   - 小提琴应该看起来像是被人物正确地"拿着"
   - 位置、旋转都应该看起来自然合理
3. **不要触发任何同步逻辑**——此时我们只是在做视觉微调

#### 步骤 3: 保存初始化变换

这是关键步骤！**复制当前的 `violin_root` 变换值**：

1. 在 Control Rig Blueprint 编辑器中打开小提琴的 Control Rig
2. 找到 `violin_root` 这个 Control
3. 在 Details 面板中查看其当前的变换值：
   - **Location** (位置)
   - **Rotation** (旋转)
   - **Scale** (缩放，通常为 1,1,1)
4. **复制这些值到 `violin_root` 的 "Init Transform" 字段**
   - 这定义了小提琴相对于 `controller_root` 的初始相对关系
   - 这个值在实时同步启动后将作为基础参考

**为什么这很重要**:
- 初始化变换定义了小提琴与人物的 "偏移关系"
- 实时同步启用后，无论 `controller_root` 如何移动，小提琴都会保持这个偏移关系
- 如果初始化变换设置不正确，小提琴会"飞走"或位置错误

#### 步骤 4: 启用实时同步

1. 返回包含小提琴的 Actor（`AStringFlowUnreal`）
2. 在 Details 面板中将 **`bEnableRealtimeSync`** 设置为 **`true`**
3. 此时小提琴应该保持在你设置的位置，并开始跟随 `controller_root` 的运动

#### 步骤 5: 验证同步效果

1. 在 Sequencer 中移动 `controller_root` 的位置或旋转
2. 观察小提琴是否正确地跟随
3. 小提琴应该显示为 `controller_root` 的"子级"，保持初始偏移关系

### 2.2 琴弓的设置（必须在启用实时同步后）

**重要**：琴弓相关的设置 **必须在启用实时同步的情况下进行**！

这是因为琴弓的位置和旋转依赖于人物的右手控制器（`bow_controller`），只有在实时同步启用后，这些关系才能正确建立和验证。

#### 琴弓设置步骤

1. **确保实时同步已启用** (`bEnableRealtimeSync = true`)

2. **设置琴弓驱动轴** (`BowAxisTowardString`)
   - 这定义了琴弓的哪个轴应该指向琴弦
   - 典型值：
     - `(1, 0, 0)` - X 轴指向琴弦
     - `(0, 1, 0)` - Y 轴指向琴弦
     - `(0, 0, 1)` - Z 轴指向琴弦
   - 选择使琴弓看起来"自然"的轴

3. **设置琴弓向上轴** (`BowUpAxis`)
   - 定义琴弓的"上"方向
   - 这有助于消除旋转的歧义性
   - 典型值：`(0, 0, 1)` 或 `(0, 1, 0)`

4. **在 Sequencer 中移动右手控制器**
   - 在启用实时同步的状态下编辑 `bow_controller` 和 `string_touch_point`
   - 实时观察琴弓的位置和旋转如何响应
   - 琴弓应该：
     - 位置跟随 `bow_controller`
     - 朝向始终指向 `string_touch_point`
     - 旋转看起来自然合理

5. **微调参数直到满意**
   - 如果琴弓的朝向不对，调整 `BowAxisTowardString`
   - 如果琴弓旋转有歧义，调整 `BowUpAxis`

### 2.3 重要提示与常见问题

#### 为什么要在启用实时同步后再设置琴弓？

因为琴弓的同步在 `Tick` 中每帧运行。只有在启用实时同步后，你才能：
- 实时看到琴弓的位置和旋转变化
- 验证 `BowAxisTowardString` 和 `BowUpAxis` 的设置是否正确
- 在编辑右手控制器时立即看到视觉反馈

如果在禁用实时同步的状态下调整，你将无法看到正确的效果。

#### 如果小提琴"飞走"了怎么办？

这通常意味着初始化变换设置不正确。解决方法：

1. 禁用实时同步
2. 手动调整 `violin_root` 回到正确位置
3. 重新复制其变换值到 Init Transform
4. 重新启用实时同步

#### 如果琴弓位置或旋转错误怎么办？

检查以下几点：

1. **实时同步是否启用**？琴弓同步需要启用才能工作
2. **`BowAxisTowardString` 是否设置正确**？尝试不同的轴 `(1,0,0)`, `(0,1,0)`, `(0,0,1)`
3. **`BowUpAxis` 是否合理**？应该垂直于向前轴
4. **右手控制器是否正确驱动**？在 Sequencer 中验证 `bow_controller` 和 `string_touch_point` 的位置

### 2.4 总结工作流程

| 阶段 | 实时同步状态 | 任务 | 目标 |
|------|-------------|------|------|
| 初始化 | ❌ 禁用 | 调整小提琴位置至合适位置 | 确定初始相对关系 |
| 保存 | ❌ 禁用 | 复制 `violin_root` 变换到 Init Transform | 锁定初始偏移 |
| 激活 | ✅ 启用 | 验证小提琴跟随人物运动 | 确认同步有效 |
| 琴弓设置 | ✅ 启用 | 调整右手控制器和琴弓参数 | 实现正确的琴弓动作 |
| 完成 | ✅ 启用 | 在 Sequencer 中编辑动画 | 记录演奏动作 |

---

## 技术细节补充

### 实时同步的数学原理

实时同步的核心是矩阵变换的链式计算：

```
Step 1: 获取 controller_root 的变换
  init_transform = controller_root 蓝图中的初始化变换
  current_transform = controller_root 当前的变换

Step 2: 计算偏移
  offset = init_transform^(-1) × current_transform

Step 3: 应用到 violin_root
  violin_root_new = violin_root_init × offset
```

这确保了：
- 如果 `controller_root` 在原地不动，`offset` = Identity，`violin_root` = `violin_root_init`
- 如果 `controller_root` 移动，`offset` 包含该移动，小提琴相应移动
- 所有变换都是相对的，完全避免了坐标系转换的复杂性

---

## 相关代码位置

- **核心实现**: `Plugins/MusicDoll/Source/Common/Private/InstrumentControlRigUtility.cpp`
- **同步处理**: `Plugins/MusicDoll/Source/StringFlowUnreal/Private/StringFlowTransformSyncProcessor.cpp`
- **配置类**: `Plugins/MusicDoll/Source/StringFlowUnreal/Public/StringFlowUnreal.h`

