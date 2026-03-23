# 实时同步系统指南

## 概述

本文档详细说明了 StringFlow（弦乐器）系统中的实时同步机制，包括人物 Control Rig、小提琴 Control Rig 和琴弓之间的协调关系。

---

## 第一部分：系统架构与实现原理

### 1.1 三个关键 Control Rig

在 StringFlow 系统中存在三个独立的 Control Rig 实例：

#### A. 人物 Control Rig（Character Control Rig）
- **所有者**: 人物骨骼网格 Actor（`SkeletalMeshActor`）
- **组件名称**: `Performer`（用于在代码中引用）
- **核心控制器**: `controller_root`
  - 用于驱动整个演奏的基础框架
  - 所有人物相关的演奏动作都以此为参考
  - 父级：`base_root`（在 Control Rig Blueprint 中自动创建）
- **其他关键控制器**:
  - `bow_controller`: 右手弓的控制器
  - `string_touch_point`: 弦触点的参考位置

#### B. 小提琴 Control Rig（Instrument Control Rig）
- **所有者**: 小提琴骨骼网格 Actor（`StringInstrument`）
- **组件名称**: `StringInstrument`（用于在代码中引用）
- **核心控制器**: `violin_root`
  - 驱动整个小提琴模型
  - 通过实时同步与人物 `controller_root` 关联

#### C. 琴弓 Control Rig（Bow Control Rig）
- **所有者**: 琴弓骨骼网格 Actor（`Bow`）
- **组件名称**: `Bow`（用于在代码中引用）
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

#### 重要提示：关于 base_root

在人物 Control Rig 中，`controller_root` 实际上有一个父级控制器叫做 `base_root`：

```
Control Rig Hierarchy:
└─ base_root           (自动创建的根节点)
   └─ controller_root  (用户控制的根节点)
      └─ ... 其他控制器
```

**base_root 的作用**:
- 作为 Control Rig 的绝对根节点，提供稳定的参考系
- 在同步计算中，`base_root` 通常保持不变（Identity Transform）
- `controller_root` 的所有变换都是相对于 `base_root` 的

**代码中的使用**:
```cpp
// 在创建 Control Rig hierarchy 时
FControlRigCreationUtility::CreateControl(ControlRigBlueprint, TEXT("base_root"), TEXT(""));
FControlRigCreationUtility::CreateControl(ControlRigBlueprint, TEXT("controller_root"), TEXT("base_root"));
```

**注意事项**:
- ✅ 实时同步算法会自动处理 `base_root` 的存在
- ✅ 用户只需要关注 `controller_root` 的动画
- ❌ 不要手动修改 `base_root` 的变换，这会破坏同步计算

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
   - **默认值**: `(1.0f, 0.0f, 0.0f)` - X 轴
   - 典型值：
     - `(1, 0, 0)` - X 轴指向琴弦
     - `(0, 1, 0)` - Y 轴指向琴弦
     - `(0, 0, 1)` - Z 轴指向琴弦
   - 选择使琴弓看起来"自然"的轴

3. **设置琴弓向上轴** (`BowUpAxis`)
   - 定义琴弓的"上"方向
   - 这有助于消除旋转的歧义性
   - **默认值**: `(0.0f, 0.0f, 1.0f)` - Z 轴
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

## 第三部分：硬编码名称与配置参考

### 3.1 Control Rig 组件名称

在代码和 Sequencer 中使用的组件名称（用于 `GetCachedControlRig`）：

| 组件 | 名称 | 说明 |
|------|------|------|
| 人物 Control Rig | `Performer` | 演奏者角色的 Control Rig |
| 小提琴 Control Rig | `StringInstrument` | 弦乐器的 Control Rig |
| 琴弓 Control Rig | `Bow` | 琴弓的 Control Rig |

**用途示例**:
```cpp
// 获取人物的 ControlRig 实例
UControlRig* PerformerControlRig = StringFlowActor->GetCachedControlRig(TEXT("Performer"));

// 获取小提琴的 ControlRig 实例
UControlRig* InstrumentControlRig = StringFlowActor->GetCachedControlRig(TEXT("StringInstrument"));
```

### 3.2 控制器名称硬编码

以下控制器名称在代码中硬编码使用，必须与 Control Rig Blueprint 中的名称完全匹配：

#### 人物 Control Rig 控制器
| 控制器名称 | 用途 | 备注 |
|-----------|------|------|
| `base_root` | `controller_root` 的父级 | 在 Control Rig 中自动创建 |
| `controller_root` | 人物根控制器 | 驱动整个演奏的基础框架 |
| `bow_controller` | 右手弓控制器 | 驱动琴弓位置 |
| `string_touch_point` | 弦触点位置 | 驱动琴弓朝向 |

#### 小提琴 Control Rig 控制器
| 控制器名称 | 用途 | 备注 |
|-----------|------|------|
| `violin_root` | 小提琴根控制器 | 跟随 `controller_root` 运动 |

#### 琴弓 Control Rig 控制器
| 控制器名称 | 用途 | 备注 |
|-----------|------|------|
| `bow_ctrl` | 琴弓控制器 | 接收来自 `bow_controller` 的驱动 |

### 3.3 手指与手掌控制器命名规则

系统自动生成以下控制器和记录器名称：

#### 手指控制器
格式：`{手指编号}_{手部标识}`
- 左手：`1_L`, `2_L`, `3_L`, `4_L`
- 右手：`1_R`, `2_R`, `3_R`, `4_R`

#### 手掌控制器
| 类型 | 左手 | 右手 | 说明 |
|------|------|------|------|
| hand_controller | `H_L` | `H_R` | 主手掌控制器 |
| hand_pivot_controller | `HP_L` | `HP_R` | 手掌枢轴控制器 |
| hand_rotation_controller | `H_rotation_L` | `H_rotation_R` | 手掌旋转控制器 |
| thumb_controller | `T_L` | `T_R` | 拇指控制器 |
| thumb_pivot_controller | `TP_L` | `TP_R` | 拇指枢轴控制器 |

#### 记录器命名规则
格式：`p_s{弦索引}_f{品格索引}_{手指编号}_L_{位置类型}`
- 示例：`p_s0_f1_1_L_normal`（左手食指在第 0 弦第 1 品格正常位置）

### 3.4 配置参数默认值

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `BowAxisTowardString` | `(1.0f, 0.0f, 0.0f)` | 琴弓指向琴弦的轴向（X 轴） |
| `BowUpAxis` | `(0.0f, 0.0f, 1.0f)` | 琴弓的向上方向（Z 轴） |
| `bEnableRealtimeSync` | `false` | 是否启用实时同步 |

### 3.5 重要提示

⚠️ **不要修改硬编码名称**

以上所有名称（组件名、控制器名）都是硬编码在源码中的，如果要修改：
1. 必须同时修改源代码中的所有引用
2. 必须确保 Control Rig Blueprint 中的名称与代码一致
3. 推荐保持默认名称不变，避免不必要的兼容性问题

⚠️ **组件名称大小写敏感**

所有名称都是大小写敏感的，例如 `Performer` 不能写成 `performer` 或 `PERFORMER`。

### 3.6 代码使用示例

#### 示例 1：在蓝图中获取 Control Rig

```
// 获取人物的 ControlRig 实例（用于访问控制器）
UControlRig* PerformerControlRig = StringFlowActor->GetCachedControlRig(TEXT("Performer"));

// 获取小提琴的 ControlRig 实例
UControlRig* InstrumentControlRig = StringFlowActor->GetCachedControlRig(TEXT("StringInstrument"));

// 获取琴弓的 ControlRig 实例
UControlRig* BowControlRig = StringFlowActor->GetCachedControlRig(TEXT("Bow"));
```

#### 示例 2：在 Sequencer 中使用

在 Sequencer 中创建 Transform 轨道时，需要使用以下组件名称：

1. 展开 `AStringFlowUnreal` Actor
2. 找到 `Performer` 组件 → 添加 `controller_root` 的动画轨道
3. 找到 `StringInstrument` 组件 → 添加 `violin_root` 的动画轨道（如果需要手动覆盖）
4. 找到 `Bow` 组件 → 添加 `bow_ctrl` 的动画轨道（如果需要手动覆盖）

**注意**：启用实时同步后，`violin_root` 和 `bow_ctrl` 会自动跟随，通常不需要手动设置动画。

### 3.7 常见问题排查

#### 问题 1：Error - Failed to get ControlRig for component Performer

**原因**: CacheSubsystem 未正确初始化或 LevelSequence 未加载

**解决方法**:
1. 确保场景中已放置了 LevelSequenceActor
2. 确保 LevelSequence 正在播放或已加载
3. 检查 GEngine 和 UControlRigCacheSubsystem 是否正常工作

#### 问题 2：小提琴位置不正确

**可能原因**:
- `violin_root` 的 Init Transform 设置错误
- `controller_root` 与 `violin_root` 的相对关系未正确缓存

**解决方法**:
1. 禁用实时同步 (`bEnableRealtimeSync = false`)
2. 手动调整 `violin_root` 到正确位置
3. 将当前变换值复制到 Init Transform
4. 重新启用实时同步
5. 调用 `InitializeStringInstrumentSync()` 重新初始化

#### 问题 3：琴弓不跟随右手运动

**可能原因**:
- 实时同步未启用
- `bow_controller` 或 `string_touch_point` 控制器不存在
- Bow Control Rig 中 `bow_ctrl` 未正确创建

**解决方法**:
1. 确认 `bEnableRealtimeSync = true`
2. 在 Sequencer 中检查是否存在 `bow_controller` 和 `string_touch_point` 控制器
3. 打开 Bow 的 Control Rig Blueprint，确认 `bow_ctrl` 存在
4. 如果缺失，使用 `FControlRigCreationUtility::CreateControl` 创建

#### 问题 4：BowAxisTowardString 设置后琴弓朝向仍然错误

**可能原因**:
- 琴弓模型的本地坐标系轴向与预期不符
- 需要使用不同的轴组合

**解决方法**:
尝试以下常见配置：
- 小提琴/中提琴：`BowAxisTowardString=(1,0,0)`, `BowUpAxis=(0,0,1)`
- 大提琴：`BowAxisTowardString=(0,1,0)`, `BowUpAxis=(0,0,1)`
- 如果都不对，逐轴测试 `(1,0,0)`, `(-1,0,0)`, `(0,1,0)`, `(0,-1,0)`, `(0,0,1)`, `(0,0,-1)`

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

