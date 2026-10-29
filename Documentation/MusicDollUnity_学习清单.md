# MusicDoll Unity 学习清单（面向插件开发）

> 目标：用最短路径补齐 Unity 关键能力，支撑 MusicDoll 的多演奏者控制、状态保存/恢复、动画生成与导出。
>
> 建议节奏：7 天，每天 1~2 小时。
>
> 学习原则：不追求“全会 Unity”，只学习对当前插件落地有直接价值的内容。

---

## 一、先明确你要学到什么程度

你当前的目标不是做完整游戏，而是做“DCC 风格工具插件”。
因此完成标准应是：

1. 能给一个演奏者创建控制节点和约束器。
2. 能保存/恢复该演奏者控制器状态（recorderTransform）。
3. 能在 Timeline 中驱动动作、BlendShape、材质参数。
4. 能让多个演奏者同场景互不干扰。
5. 能把流程做成工具按钮（Setup / Save / Load / Generate）。

---

## 二、7 天学习路径（每天都有可验证结果）

## Day 1：动画系统全景（Animator / Timeline / Playables）

学习目标：看懂 Unity 动画系统分工，避免后续架构走偏。

建议检索词：

1. Unity Animator vs Timeline vs Playables explained
2. Unity Timeline beginner to advanced
3. Unity PlayableGraph fundamentals

当日产出：

1. 画一张你自己的系统图（谁负责“编辑”、谁负责“运行时驱动”）。
2. 列出你准备用 Timeline 还是 Animator 作为主驱动（建议 Timeline 主驱动）。

验收标准：

1. 你能口头解释：为什么 MusicDoll 更适合 Timeline + Playables，而不是纯 Animator Controller。

---

## Day 2：Timeline 轨道与绑定（多演奏者隔离核心）

学习目标：掌握 TrackBinding、ExposedReference、PlayableDirector 的使用。

建议检索词：

1. Unity Timeline custom track tutorial
2. Unity ExposedReference Timeline binding
3. Unity PlayableDirector multiple characters

当日产出：

1. 做一个双角色 Timeline Demo（A/B 两个角色）。
2. 每个角色各自一条轨道，确认轨道绑定不串对象。

验收标准：

1. 你能稳定复现“同名节点不冲突”的绑定策略。

---

## Day 3：Animation Rigging（空物体 + 约束器控制链）

学习目标：复刻 Blender/Unreal 的“控制器驱动骨骼”思路。

建议检索词：

1. Unity Animation Rigging package tutorial
2. Unity Two Bone IK Constraint tutorial
3. Unity Multi Aim Constraint tutorial

当日产出：

1. 用空物体驱动手臂 IK。
2. 做一个最小手部控制链（手掌 + 一根手指）。

验收标准：

1. 控制器移动后，骨骼按预期跟随。
2. 能解释约束目标、权重、评估顺序。

---

## Day 4：BlendShape 动画（对应 Morph Target）

学习目标：掌握乐器/人物变形动画的可程序化驱动。

建议检索词：

1. Unity BlendShape animation Timeline
2. Unity SetBlendShapeWeight script tutorial
3. Unity custom playable blendshape

当日产出：

1. 在 Timeline 中驱动 1~2 个 BlendShape。
2. 写一个脚本按时间设置 BlendShapeWeight。

验收标准：

1. 你能在播放时实时控制变形。
2. 可切换不同对象而不改逻辑代码（通过绑定切换）。

---

## Day 5：材质参数动画（对应乐器材质动画）

学习目标：掌握材质参数曲线驱动，并避免多角色串值。

建议检索词：

1. Unity material parameter animation Timeline
2. Unity MaterialPropertyBlock tutorial
3. Unity sharedMaterial vs material difference

当日产出：

1. 驱动一个 Float 参数（如 EmissionIntensity）。
2. 驱动一个 Color 参数。
3. 用 MaterialPropertyBlock 让两个角色显示不同参数值。

验收标准：

1. 双角色播放时不会互相覆盖材质参数。

---

## Day 6：数据层（ScriptableObject + JSON + 状态快照）

学习目标：实现 recorderTransform 的保存/恢复与导入导出。

建议检索词：

1. Unity ScriptableObject data architecture
2. Unity JSON serialization best practices
3. Unity save load transform data

当日产出：

1. 定义 PerformerStateLibrary（ScriptableObject）。
2. 实现 SaveState / LoadState。
3. 导出 JSON，再导入回场景。

验收标准：

1. 状态可重复恢复。
2. SaveState 采用 add-or-update，不清空历史状态。

---

## Day 7：编辑器工具化（把流程做成插件）

学习目标：把前 6 天能力串成“可操作面板”。

建议检索词：

1. Unity EditorWindow custom tools tutorial
2. Unity custom inspector buttons
3. Unity asset pipeline editor scripting

当日产出：

1. 一个 MusicDollUnity 工具窗口。
2. 至少 4 个按钮：Setup / Save / Load / Generate。
3. 支持两个演奏者切换与独立操作。

验收标准：

1. 不改代码，只点按钮即可跑完整最小流程。

---

## 三、你可以优先看的频道类型

如果你不想被“偏游戏玩法”的内容干扰，优先筛选：

1. 关键词包含 Editor Tooling / Timeline / Playables / Animation Rigging。
2. 视频里有代码和 Inspector 实操，不是纯概念。
3. 版本标注为 Unity 2022 LTS 或 Unity 6。

不建议优先看：

1. 只讲 Mecanim 状态机切换的教程（对你当前目标帮助有限）。
2. 只做角色动作混合、无工具化部分的内容。

---

## 四、学习中最容易踩的坑（提前规避）

1. 先做单角色成功，再上多角色，容易导致后续全局查找污染。
2. 用 sharedMaterial 改参数会串角色，优先 MaterialPropertyBlock。
3. 把 SaveState 写成先清空再写入，会丢历史状态。
4. Timeline 轨道如果不做明确绑定，会出现“看似同名控件随机串绑”。
5. 过早优化缓存层会增加复杂度；先做可用链路，再看性能数据。

---

## 五、两周后你应达到的能力基线

1. 能独立实现一个乐器模块最小版本（建议先 KeyRipple 风格）。
2. 能解释并实现多演奏者隔离机制。
3. 能把动作、变形、材质三种动画统一到 Timeline 工作流。
4. 能实现基础导入导出和状态管理面板。

如果达到这四点，Unity 版 MusicDoll 就进入“可持续扩展”阶段。
