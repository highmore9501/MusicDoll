# MusicDoll Unity 版本迁移规划（基于现有 MusicDollUnreal 架构复盘）

> 日期: 2026-09-06  
> 目标: 在 Unity 中复刻 MusicDoll 系列在 Unreal/Blender 的核心机制，包括数据采集、状态保存/恢复、动画生成与多演奏者隔离。

---

## 0. 先说明：本规划已对齐的 MusicDollUnreal 真实结构

本次规划不是泛化方案，而是按当前插件代码结构对位设计。核心观察如下：

1. 插件是多模块结构，不是单模块。
   - 模块包括 MusicDollCommon、MusicDollUI，以及各乐器模块（KeyRippleUnreal、FretDanceUnreal、StringFlowUnreal、BeatBloomUnreal、ZhengDriftUnreal、HarpGlideUnreal、WindRiseUnreal、SingerUnreal）。

2. 每个乐器模块普遍采用 Actor + Processor 三件套。
   - Actor 持有配置、当前状态、RecorderTransforms 等数据。
   - Processor 通常拆分为 ControlRigProcessor、AnimationProcessor、InstrumentProcessor（命名在不同乐器略有差异）。

3. 公共能力在 MusicDollCommon。
   - ControlRig 缓存与查询: ControlRigCacheSubsystem。
   - Sequencer 关键帧与材质轨道处理: InstrumentAnimationUtility。
   - Morph Target、材质、坐标转换等通用工具: InstrumentMorphTargetUtility、InstrumentMaterialUtility、CoordinateTransformUtility。

4. UI 是工具入口，且偏手动触发工作流。
   - MusicDollUI 注册窗口和面板。
   - 各模块 OperationsPanel 提供 Save/Load、生成动画、初始化 Control Rig 等按钮。

5. 多演奏者可并存，隔离点实际发生在“对象绑定 + 轨道绑定”（缓存是可选优化）。
   - Unity 端优先保证绑定隔离与上下文隔离，不强制实现 Unreal 同款缓存子系统。

---

## 1. Unity 端目标架构（对位 Unreal 的模块化）

建议 Unity 版本也保持“三层 + 三件套”思想，避免后续难以对齐已有流程。

补充原则：

1. 不强行一比一复刻 ControlRigCacheSubsystem。
2. 如果 Unity 项目里查询开销不高，直接用演奏者上下文内引用即可。
3. 只有在出现明确性能瓶颈时，再增加轻量缓存层。

### 1.1 模块分层

1. MusicDollUnityCommon
   - 通用数据结构、Timeline/Playable 工具、坐标转换、导入导出工具。

2. MusicDollUnityEditor
   - EditorWindow、Inspector 工具、操作面板（Save/Load/Generate/Setup）。

3. 乐器模块（可先做 KeyRippleUnity 模板）
   - 每个乐器维持独立 runtime 组件与 processor。

### 1.2 运行时三件套（建议）

1. PerformerActor（MonoBehaviour）
   - 对应 Unreal 的 AInstrumentBase + 各乐器 Actor。
   - 持有演奏者对象引用、状态枚举/键、RecorderTransforms、IO 路径。

2. ControlRigProcessor（Unity 等价）
   - 负责控制器对象创建、层级校验、SaveState/LoadState。
   - 若使用 Animation Rigging，负责约束目标初始化。

3. AnimationProcessor / InstrumentProcessor
   - 从导入数据生成 Timeline 轨道关键帧。
   - 处理 BlendShape、材质参数、乐器专有动画。

---

## 2. 问题 1：每个演奏者的 recorderTransform 如何保存

你们现有思路是“Actor 内存态 + 文件导出”。Unity 推荐保持同构，而不是换成纯数据库式方案。

### 2.1 推荐保存策略（对位 Unreal）

采用“双态保存”：

1. 运行态（快速读写）
   - 在 PerformerActor 内维护:
   - Dictionary<string, RecorderTransform> RecorderTransforms

2. 资产态（可编辑与版本管理）
   - ScriptableObject: PerformerStateLibraryAsset
   - 保存状态条目、schemaVersion、元数据。

3. 导出态（跨工具链）
   - JSON 文件，用于和 Rust/Blender/Unreal 兼容。

### 2.2 关键字段建议

RecorderTransform 建议至少包含：

1. localPosition
2. localRotation（Quaternion）
3. localScale（可选）
4. controlName（逻辑名）

说明：和 Unreal 一样，优先保存逻辑名，不绑定实例 ID，避免场景重开后失效。

### 2.3 状态写入策略

为了延续现有行为，建议明确两条规则：

1. SaveState 只做 add-or-update，不整体清空库。
2. Setup/Initialize 只补缺失键，不覆盖已保存值。

这与当前 MusicDoll 的稳定性规则一致，能避免历史状态被误清空。

### 2.4 最小接口

```csharp
public interface IRecorderStateStore
{
    void SaveState(string performerId, string stateKey, PerformerSnapshot snapshot);
    bool TryLoadState(string performerId, string stateKey, out PerformerSnapshot snapshot);
    void ExportToJson(string performerId, string path);
    void ImportFromJson(string performerId, string path);
}
```

---

## 3. 问题 2：如何避免多演奏者同名控件冲突

你们在 Unreal 已经证明“同名控件可并存”，Unity 也应使用分层隔离，而不是只靠重命名。

### 3.1 三重隔离机制（必须同时启用）

1. 数据命名空间隔离
   - 键结构: performerId/stateKey/controlName
   - 示例: KR_001/Note_060/H_L 与 KR_002/Note_060/H_L

2. 场景层级隔离
   - 每个演奏者一个根节点 PerformerRoot\_{PerformerId}
   - 控件查找只在该根节点子树内，不允许全局查找

3. Timeline 绑定隔离
   - 每个演奏者一套独立 TrackBinding（或独立子 Timeline）
   - 通过 ExposedReference/PlayableBinding 固定到该演奏者实例

### 3.2 运行时上下文设计

建议每个演奏者拥有独立上下文：

1. performerId
2. controlByName: Dictionary<string, Transform>
3. meshByRole: Dictionary<string, SkinnedMeshRenderer>
4. director: PlayableDirector

全局管理器仅维护 performerId -> context，不直接维护控件名 -> 对象。

说明：在这个结构下，通常不需要额外全局缓存子系统。因为 Setup 阶段已经建立了稳定引用，后续按 performerId 直达对象即可。

### 3.3 冲突预防工具

Editor 增加 Validate 按钮，自动检查：

1. performerId 是否重复。
2. 同一演奏者 controlName 是否重复。
3. 轨道绑定是否跨演奏者串绑。
4. 资产中的状态键是否带 performerId 前缀。

---

## 4. 问题 3：Unity 里如何做变形动画与材质动画

目标是与当前 Unreal 的“可批量生成 + 可在时间线上查看 + 可导出”能力对齐。

### 4.1 变形动画（BlendShape）

推荐主方案：自定义 Timeline Track（BlendShapeTrack）

1. Track 绑定 SkinnedMeshRenderer。
2. Clip 内保存 shapeName + keyframe 列表。
3. PlayableBehaviour.ProcessFrame 每帧执行 SetBlendShapeWeight。

优点：

1. 与现有 JSON 驱动逻辑一致。
2. 更适合批量导入与多演奏者并行。
3. 能直接保留“工具生成轨道”的工作流。

补充方案：

1. 对最终交付可选烘焙到 AnimationClip，便于和 Animator 生态协作。

### 4.2 材质动画

推荐主方案：MaterialParamTrack + MaterialPropertyBlock（每演奏者独立）

1. Track 绑定 Renderer。
2. Clip 保存参数名和曲线（Float/Color/Vector）。
3. ProcessFrame 中向该 Renderer 写入 PropertyBlock。

关键点：

1. 多演奏者共用材质时，优先 PropertyBlock，避免 sharedMaterial 串值。
2. 若需要长期固化某角色材质，可切换到实例材质。

### 4.3 与乐器动画联动方式

每个演奏者建立三类并行轨道：

1. ControlTrack（控制器/约束目标）
2. BlendShapeTrack（人物或乐器变形）
3. MaterialParamTrack（乐器材质参数）

三轨共用统一时间基准（帧或秒），保证动作、变形、材质同步。

---

## 5. 对位 Unreal 的实现映射表

| Unreal 现有职责                              | Unity 对位实现                                                |
| -------------------------------------------- | ------------------------------------------------------------- |
| Actor 持有配置与 RecorderTransforms          | PerformerActor MonoBehaviour + ScriptableObject 状态库        |
| ControlRigProcessor 的 Setup/Save/Load       | ControlRigProcessorUnity（控制器层级与状态写回）              |
| InstrumentAnimationUtility 写 Sequencer 轨道 | TimelineUtilityUnity 写 Track/Clip/Curve                      |
| MorphTarget/Material Utility                 | BlendShapeTrack + MaterialParamTrack                          |
| ControlRigCacheSubsystem                     | 可选：轻量 PerformerBindingRegistry（仅在性能瓶颈出现时引入） |
| OperationsPanel 手动触发按钮                 | EditorWindow/Inspector 按钮（Setup/Save/Load/Generate）       |

---

## 6. 分阶段落地计划（按你当前团队习惯）

### 阶段 1：先做一个乐器模板（建议 KeyRipple）

1. 完成 PerformerActor、StateStore、Setup/Save/Load 按钮。
2. 打通 JSON 导入导出。
3. 完成双演奏者同场景隔离验证。

验收：两个演奏者同名控件保存/恢复互不影响。

### 阶段 2：补全时间线动画能力

1. ControlTrack。
2. BlendShapeTrack。
3. MaterialParamTrack。

验收：同段音乐下，多演奏者动作、变形、材质同步播放且不串值。

### 阶段 3：工程化与跨工具链

1. 批量导入 Rust 输出。
2. 批量生成 Timeline 资产。
3. 可选烘焙与导出桥接（给 Unreal/Blender 复用）。

验收：形成可重复生产流程，而非仅 Demo。

---

## 7. 结论（对应你的三个问题）

1. recorderTransform 的保存方式
   - 建议保持你们现有哲学：演奏者上下文内 Dictionary 做运行态，ScriptableObject + JSON 做持久化与交换。
   - SaveState 只增改，Setup 只补齐，不做全量清空。

2. 多演奏者同名控件隔离
   - 用 performerId 命名空间 + 根节点隔离 + Timeline 绑定隔离三重机制。
   - 所有查找都限定在 performer context 内。
   - 不要求实现独立缓存子系统；缓存在 Unity 端属于可选优化项。

3. 变形动画和材质动画
   - 变形: BlendShapeTrack（主）+ AnimationClip 烘焙（辅）。
   - 材质: MaterialParamTrack + MaterialPropertyBlock（主），实例材质（按需）。
