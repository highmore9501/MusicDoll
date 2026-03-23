# FretDance 实时同步系统指南

## 概述

本文档详细说明了 FretDance（吉他）系统中的实时同步机制，包括人物 Control Rig 和吉他之间的协调关系。

---

## 第一部分：系统架构与实现原理

### 1.1 两个关键 Control Rig

在 FretDance 系统中存在两个独立的 Control Rig 实例：

#### A. 人物 Control Rig（Character Control Rig）
- **所有者**: 人物骨骼网格 Actor（`SkeletalMeshActor`）
- **组件名称**: `Performer`（用于在代码中引用）
- **核心控制器**: `controller_root`
  - 用于驱动整个演奏的基础框架
  - 所有人物相关的演奏动作都以此为参考
  - 父级：`base_root`（在 Control Rig Blueprint 中自动创建）
- **其他关键控制器**:
  - 左手手指控制器：`I_L`, `M_L`, `R_L`, `P_L`
  - 右手手指控制器：`I_R`, `M_R`, `R_R`, `P_R`（以及电吉他的特殊层级）
  - 手掌控制器：`H_L`, `H_R`, `HP_L`, `HP_R`等

#### B. 吉他 Control Rig（Instrument Control Rig）
- **所有者**: 吉他骨骼网格 Actor（`Guitar`）
- **组件名称**: `Guitar`（用于在代码中引用）
- **核心控制器**: `guitar_root`
  - 这是吉他的根**控制器**（注意：不是骨骼）
  - 驱动整个吉他模型
  - 通过实时同步与人物 `controller_root` 关联

### 1.2 实时同步机制

#### 吉他与人物的同步（Control-to-Control）

实时同步使用 **"父子关系"** 机制，与 StringFlow 完全一致：

```
人物 Control Rig          吉他 Control Rig
└─ controller_root    →    └─ guitar_root
   (父控制器)                (子控制器)
```

**核心算法**:

1. **初始化阶段**（`InitializeGuitarSync`）
   
   使用 `FInstrumentControlRigUtility::InitializeControlRelationship`:
   
   ```
   Step 1: 获取 controller_root 的初始化全局变换
     parent_init = controller_root 在蓝图中的初始化变换
   
   Step 2: 获取 guitar_root 的初始化全局变换
     child_init = guitar_root 在蓝图中的初始化变换
   
   Step 3: 获取两个 Actor 的世界变换
     parent_actor_world = Performer Actor 的世界变换
     child_actor_world = Guitar Actor 的世界变换
   
   Step 4: 计算世界空间下的初始变换
     parent_world = parent_init × parent_actor_world
     child_world = child_init × child_actor_world
   
   Step 5: 计算相对变换
     CachedGuitarRelativeTransform = child_world.GetRelativeTransform(parent_world)
   
   Step 6: 缓存结果
     CachedGuitarRelativeTransform 在整个生命周期中保持不变
   ```

2. **每帧更新**（`SyncGuitarTransform`）
   
   使用 `FInstrumentControlRigUtility::UpdateChildControlFromParent`:
   
   ```
   Step 1: 获取 controller_root 的当前世界变换
     parent_current = controller_root 的当前世界变换
   
   Step 2: 计算 guitar_root 的新世界变换
     child_new = CachedGuitarRelativeTransform × parent_current
   
   Step 3: 应用变换到 guitar_root
     SetControlRigWorldTransform(guitar_root, child_new)
   ```

**关键特性**:
- ✅ 吉他自动跟随人物的所有运动
- ✅ 保持初始的相对位置和姿态
- ✅ 人物的任何旋转、位置、缩放变化都会自动传递到吉他
- ✅ 完全无需在两个 Control Rig 之间来回切换坐标系
- ✅ 与 StringFlow 使用相同的算法和工具函数

### 1.3 实现细节

#### 重要提示：关于 guitar_root 控制器

**guitar_root 是硬编码的控制器名称**，这是吉他 Control Rig 中的根控制器：

```cpp
// 在 InitializeGuitarSync 中检查并创建
if (!GuitarBlueprint->Hierarchy->Contains(FRigElementKey(
    TEXT("guitar_root"), ERigElementType::Control))) {
    // 如果不存在，自动创建
    FControlRigCreationUtility::CreateControl(GuitarBlueprint, TEXT("guitar_root"), TEXT(""));
}
```

**guitar_root 的作用**:
- 作为吉他 Control Rig 的根控制器，控制整个吉他的变换
- 在同步计算中，这个控制器的初始化变换被用作参考点
- 这个名称必须与吉他 Control Rig Blueprint 中的控制器名称完全匹配

**注意事项**:
- ⚠️ **guitar_root 是硬编码的** - 如果你的吉他 Control Rig 使用不同的根控制器名称，需要修改代码或重命名控制器
- ✅ 推荐使用 `guitar_root` 作为标准命名
- ✅ 如果控制器不存在，`InitializeGuitarSync` 会自动创建它

#### 关键方法：UFretDanceTransformSyncProcessor::InitializeGuitarSync

这是初始化吉他同步的核心方法：

```cpp
static bool InitializeGuitarSync(AFretDanceUnreal* FretDanceActor);
```

**用途**: 计算并缓存吉他相对于 Performer 的初始变换

**执行步骤**:
1. 检查 Performer 的 `controller_root` 是否存在
2. 检查 Guitar 的 `guitar_root` 是否存在，不存在则自动创建
3. 调用 `FInstrumentControlRigUtility::InitializeControlRelationship` 计算相对变换
4. 缓存到 `CachedGuitarRelativeTransform`

#### 关键方法：FInstrumentControlRigUtility::InitializeControlRelationship

这是通用的 Control Rig 同步工具方法（与 StringFlow 共用）：

```cpp
static bool InitializeControlRelationship(
    ASkeletalMeshActor* ParentControlRig,
    const FString& ParentControlName,
    ASkeletalMeshActor* ChildControlRig,
    const FString& ChildControlName,
    FTransform& OutRelativeTransform);
```

**参数说明**:
- `ParentControlRig`: 父 Control 所属的 Control Rig（Performer）
- `ParentControlName`: 父 Control 的名称（`"controller_root"`）
- `ChildControlRig`: 子 Control 所属的 Control Rig（Guitar）
- `ChildControlName`: 子 Control 的名称（`"guitar_root"`）
- `OutRelativeTransform`: 输出的相对变换（缓存后复用）

#### 关键方法：FInstrumentControlRigUtility::UpdateChildControlFromParent

这是每帧更新的通用方法（与 StringFlow 共用）：

```cpp
static bool UpdateChildControlFromParent(
    UControlRig* ParentControlRigInstance,
    const FString& ParentControlName,
    ASkeletalMeshActor* ParentSkeletalMeshActor,
    ASkeletalMeshActor* ChildSkeletalMeshActor,
    const FString& ChildControlName,
    const FTransform& RelativeTransform);
```

**用途**: 根据父 Control 的当前变换更新子 Control

---

## 第二部分：使用指南

### 2.1 初始化流程

按照以下步骤在编辑器中正确初始化吉他与人物的关系：

#### 步骤 1: 禁用实时同步

1. 选择包含吉他的 Actor（`AFretDanceUnreal`）
2. 在 Details 面板中找到 **"Transform Sync"** 部分
3. 确保 **`bEnableRealtimeSync`** 为 **`false`**（默认状态）

#### 步骤 2: 手动调整吉他位置

1. 在 3D 视口中将吉他放置到人物手中合适的位置
2. 调整吉他的位置、旋转，使其看起来像是被人物正确地"拿着"
3. **不要触发任何同步逻辑**——此时我们只是在做视觉微调

#### 步骤 3: 初始化同步关系

这是关键步骤！调用 `InitializeGuitarSync` 来缓存初始变换：

**方法 A：通过蓝图调用**
1. 打开你的 FretDance 蓝图
2. 添加节点：`Initialize Guitar Sync`
3. 在场景设置完成后调用一次

**方法 B：通过代码调用**
```cpp
// 在你的设置代码中
if (FretDanceActor) {
    UFretDanceTransformSyncProcessor::InitializeGuitarSync(FretDanceActor);
}
```

**方法 C：在启用实时同步时自动调用**
- 可以在 `PostEditChangeProperty` 或其他初始化函数中，在首次启用时调用

#### 步骤 4: 启用实时同步

1. 选择 `AFretDanceUnreal` Actor
2. 在 Details 面板中将 **`bEnableRealtimeSync`** 设置为 **`true`**
3. 此时吉他应该保持在你设置的位置，并开始跟随 `controller_root` 的运动

#### 步骤 5: 验证同步效果

1. 在 Sequencer 中移动 `controller_root` 的位置或旋转
2. 观察吉他是否正确地跟随
3. 吉他应该显示为 `controller_root` 的"子级"，保持初始偏移关系

### 2.2 重要提示与常见问题

#### 如果吉他"飞走"了怎么办？

这通常意味着初始化变换计算不正确。解决方法：

1. 禁用实时同步
2. 检查吉他的 `guitar_root` 控制器是否存在且命名正确
3. 重新手动调整吉他位置
4. 重新调用 `InitializeGuitarSync`
5. 重新启用实时同步

#### 如果吉他不跟随人物运动怎么办？

检查以下几点：

1. **实时同步是否启用**？(`bEnableRealtimeSync = true`)
2. **Performer Control Rig 是否正确加载**？
   - 检查场景中是否有 LevelSequence
   - 检查 Control Rig Cache Subsystem 是否工作正常
3. **controller_root 控制器是否存在**？
   - 在 Sequencer 中查看 Performer 的控制器列表
4. **Guitar 的 guitar_root 控制器是否存在**？
   - 打开吉他的 Control Rig Blueprint 检查

#### 关于 guitar_root 控制器的硬编码

⚠️ **重要**：代码中硬编码了 `"guitar_root"` 这个控制器名称：

```cpp
// 检查并创建 guitar_root
if (!GuitarBlueprint->Hierarchy->Contains(FRigElementKey(
    TEXT("guitar_root"), ERigElementType::Control))) {
    FControlRigCreationUtility::CreateControl(GuitarBlueprint, TEXT("guitar_root"), TEXT(""));
}
```

**如果你的吉他 Control Rig 使用不同的根控制器名称**：

1. 推荐统一使用 `guitar_root` 作为标准命名
2. 或者修改代码中的硬编码名称
3. 或者在 Control Rig Blueprint 中重命名控制器为 `guitar_root`

**推荐的命名约定**：
- `guitar_root` - 所有吉他类型的标准根控制器名称

但无论使用什么名称，都必须确保：
- 与 Control Rig Blueprint 中的控制器名称完全匹配
- 在代码中统一使用同一个名称

### 2.3 总结工作流程

| 阶段 | 实时同步状态 | 任务 | 目标 |
|------|-------------|------|------|
| 准备 | ❌ 禁用 | 将吉他放置到人物手中合适位置 | 确定初始相对关系 |
| 初始化 | ❌ 禁用 | 调用 `InitializeGuitarSync()` | 计算并缓存相对变换 |
| 激活 | ✅ 启用 | 验证吉他跟随人物运动 | 确认同步有效 |
| 动画 | ✅ 启用 | 在 Sequencer 中编辑 `controller_root` 动画 | 记录演奏动作 |

---

## 第三部分：硬编码名称与配置参考

### 3.1 Control Rig 组件名称

在代码和 Sequencer 中使用的组件名称（用于 `GetCachedControlRig`）：

| 组件 | 名称 | 说明 |
|------|------|------|
| 人物 Control Rig | `Performer` | 演奏者角色的 Control Rig |
| 吉他 Control Rig | `Guitar` | 吉他模型的 Control Rig |

**用途示例**:
```cpp
// 获取人物的 ControlRig 实例
UControlRig* PerformerControlRig = FretDanceActor->GetCachedControlRig(TEXT("Performer"));

// 获取吉他的 ControlRig 实例
UControlRig* GuitarControlRig = FretDanceActor->GetCachedControlRig(TEXT("Guitar"));
```

### 3.2 控制器名称硬编码

以下控制器名称在代码中硬编码使用，必须与 Control Rig Blueprint 中的名称完全匹配：

#### 人物 Control Rig 控制器
| 控制器名称 | 用途 | 备注 |
|-----------|------|------|
| `base_root` | `controller_root` 的父级 | 在 Control Rig 中自动创建 |
| `controller_root` | 人物根控制器 | 驱动整个演奏的基础框架 |

#### 吉他 Control Rig 控制器
| 控制器名称 | 用途 | 备注 |
|-----------|------|------|
| `guitar_root` | 吉他根控制器 | 接收来自 `controller_root` 的驱动 |

### 3.3 配置参数默认值

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `bEnableRealtimeSync` | `false` | 是否启用实时同步 |
| `InstrumentType` | `FINGER_STYLE_GUITAR` | 吉他类型（指弹/电吉他/贝斯） |
| `StringNumber` | `6` | 弦数 |

### 3.4 重要提示

⚠️ **不要修改硬编码名称**

以上所有名称（组件名、控制器名）都是硬编码在源码中的，如果要修改：
1. 必须同时修改源代码中的所有引用
2. 必须确保 Control Rig Blueprint 中的名称与代码一致
3. 推荐保持默认名称不变，避免不必要的兼容性问题

⚠️ **组件名称大小写敏感**

所有名称都是大小写敏感的，例如：
- `Performer` 不能写成 `performer` 或 `PERFORMER`
- `guitar_root` 不能写成 `Guitar_Root` 或 `GUITAR_ROOT`

### 3.5 代码使用示例

#### 示例 1：在蓝图中获取 Control Rig

```cpp
// 获取人物的 ControlRig 实例（用于访问控制器）
UControlRig* PerformerControlRig = FretDanceActor->GetCachedControlRig(TEXT("Performer"));
```

#### 示例 2：初始化吉他同步

```cpp
// 在场景设置完成后调用
if (FretDanceActor && FretDanceActor->Guitar) {
    UFretDanceTransformSyncProcessor::InitializeGuitarSync(FretDanceActor);
    
    // 然后启用实时同步
    FretDanceActor->bEnableRealtimeSync = true;
}
```

#### 示例 3：在 Sequencer 中使用

在 Sequencer 中创建 Transform 轨道时：

1. 展开 `AFretDanceUnreal` Actor
2. 找到 `Performer` 组件 → 添加 `controller_root` 的动画轨道
3. **不需要**为吉他手动添加动画轨道（它会自动跟随）

**注意**：启用实时同步后，吉他会自动跟随 `controller_root`，通常不需要手动设置动画。

### 3.6 常见问题排查

#### 问题 1：Error - 'guitar_root' not found in Guitar's ControlRig Blueprint

**原因**: 吉他 Control Rig 中没有名为 `guitar_root` 的控制器

**解决方法**:
1. 打开吉他的 Control Rig Blueprint
2. 检查是否存在 `guitar_root` 控制器
3. 如果不存在，手动创建或让 `InitializeGuitarSync` 自动创建

**自动创建**：
```cpp
// InitializeGuitarSync 会自动创建缺失的 guitar_root
UFretDanceTransformSyncProcessor::InitializeGuitarSync(FretDanceActor);
```

#### 问题 2：Error - Failed to get ControlRig for component Performer

**原因**: CacheSubsystem 未正确初始化或 LevelSequence 未加载

**解决方法**:
1. 确保场景中已放置了 LevelSequenceActor
2. 确保 LevelSequence 正在播放或已加载
3. 检查 GEngine 和 UControlRigCacheSubsystem 是否正常工作

#### 问题 3：吉他位置不正确

**可能原因**:
- 初始化时吉他位置不合适
- `CachedGuitarRelativeTransform` 计算错误

**解决方法**:
1. 禁用实时同步 (`bEnableRealtimeSync = false`)
2. 手动调整吉他到正确位置
3. 重新调用 `InitializeGuitarSync()`
4. 重新启用实时同步

#### 问题 4：吉他不跟随人物运动

**可能原因**:
- 实时同步未启用
- Performer Control Rig 未正确加载
- `controller_root` 控制器不存在

**解决方法**:
1. 确认 `bEnableRealtimeSync = true`
2. 检查场景中是否有 LevelSequence
3. 在 Sequencer 中检查是否存在 `controller_root` 控制器
4. 如果是电吉他，检查 Control Rig 的层级结构是否正确

---

## 技术细节补充

### 实时同步的数学原理

实时同步的核心是矩阵变换的链式计算：

#### 初始化阶段
```
Step 1: 获取 controller_root 的初始化变换（从蓝图）
  parent_init_global = GetControlRigControlGlobalInitTransform(Performer, "controller_root")

Step 2: 获取 guitar_root 的初始化变换（从蓝图）
  child_init_global = GetControlRigControlGlobalInitTransform(Guitar, "guitar_root")

Step 3: 获取 Actor 世界变换
  parent_actor_world = Performer->GetActorTransform()
  child_actor_world = Guitar->GetActorTransform()

Step 4: 计算世界空间下的初始变换
  parent_init_world = parent_init_global × parent_actor_world
  child_init_world = child_init_global × child_actor_world

Step 5: 计算相对变换
  CachedGuitarRelativeTransform = child_init_world.GetRelativeTransform(parent_init_world)

Step 6: 缓存
  CachedGuitarRelativeTransform 在整个生命周期中保持不变
```

#### 每帧更新阶段
```
Step 1: 获取 controller_root 的当前世界变换
  parent_current_world = GetControlRigControlWorldTransform(PerformerControlRig, "controller_root")

Step 2: 计算 guitar_root 的新世界变换
  child_new_world = CachedGuitarRelativeTransform × parent_current_world

Step 3: 应用变换
  SetControlRigWorldTransform(Guitar, "guitar_root", child_new_world)
```

这确保了：
- 如果 `controller_root` 在原地不动，`child_new_world` = `child_init_world`，吉他保持初始位置
- 如果 `controller_root` 移动，`child_new_world` 相应移动，吉他跟随
- 所有变换都是相对的，完全避免了坐标系转换的复杂性

### 与 StringFlow 的一致性

FretDance 现在完全遵循与 StringFlow 相同的同步原则：

| 特性 | StringFlow（小提琴） | FretDance（吉他） |
|------|---------------------|------------------|
| 同步对象 | Control Rig → Control Rig | Control Rig → Control Rig |
| 乐器根控制器 | `violin_root` | `guitar_root` |
| 初始化方法 | `InitializeControlRelationship` | `InitializeControlRelationship` |
| 更新方法 | `UpdateChildControlFromParent` | `UpdateChildControlFromParent` |
| 缓存变量 | `CachedStringInstrumentRelativeTransform` | `CachedGuitarRelativeTransform` |

**为什么现在保持一致？**
- 使用控制器而不是骨骼更稳定、更灵活
- 统一的工具函数减少了代码重复
- 更容易维护和扩展
- 符合 Control Rig 的最佳实践

---

## 相关代码位置

- **核心实现**: `Plugins/MusicDoll/Source/FretDanceUnreal/Private/FretDanceTransformSyncProcessor.cpp`
- **配置类**: `Plugins/MusicDoll/Source/FretDanceUnreal/Public/FretDanceUnreal.h`
- **通用工具类**: `Plugins/MusicDoll/Source/MusicDollCommon/Private/InstrumentControlRigUtility.cpp`
- **控制器创建**: `Plugins/MusicDoll/Source/Common/Private/ControlRigCreationUtility.cpp`

---

## 修订历史

- **v2.0** - 重构版本，与 StringFlow 保持一致的设计原则
  - 修改：从操作骨骼改为操作控制器（`guitar_root`）
  - 修改：使用通用的 `InitializeControlRelationship` 方法
  - 修改：使用通用的 `UpdateChildControlFromParent` 方法
  - 新增：自动创建缺失的 `guitar_root` 控制器
  
- **v1.0** - 初始版本
  - 基于骨骼的同步机制（已废弃）
