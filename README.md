# MusicDoll

MusicDoll是一个系列软件的统称，旨在连接 MIDI 文件与虚拟乐手，实现根据 MIDI 单轨自动生成演奏动画。本仓库提供 **MusicDoll 系列软件在 Unreal Engine 中的插件源码**，用于加载和驱动由 MusicDoll 工具生成的数据文件，使虚拟乐手可在 UE 中完成自动演奏。

## 🎯 核心价值

- **自动化动画生成**：从 MIDI 文件到高精度演奏动画的一键生成
- **模块化设计**：支持多种乐器类型的插件化扩展
- **精确控制**：集成 Control Rig 实现手指级精确控制
- **可视化操作**：友好的 UI 界面降低使用门槛

## 📦 模块组成

### 核心乐器模块

| 模块名称             | 乐器类型    | 状态      |
| -------------------- | ----------- | --------- |
| **KeyRippleUnreal**  | 钢琴        | ✅ 已发布 |
| **StringFlowUnreal** | 小提琴/弦乐 | ✅ 已发布 |
| **FretDanceUnreal**  | 吉他/贝斯   | ✅ 已发布 |
| **BeatBloomUnreal**  | 打击乐/鼓组 | ✅ 已发布 |
| **ZhengDriftUnreal** | 古筝        | ✅ 已发布 |
| **HarpGlideUnreal**  | 竖琴        | ✅ 已发布 |

### 公共模块

- **MusicDollCommon**：通用工具库（烘焙、IK、材质、形态目标等）
- **MusicDollUI**：统一 UI 框架和主窗口管理

## ✨ 主要特性

### 通用功能

- 支持从 MIDI 文件生成演奏动画
- 集成 Control Rig 实现精确骨骼控制
- 支持 Morph Target 形变动画
- 支持乐器材质动画
- 左右手/肢体独立控制
- 状态保存与加载
- 动画一键生成

### 各模块特色

#### KeyRippleUnreal（钢琴）

- 支持白键/黑键按键逻辑，根据 MIDI 音高自动判定按键类型
- 多位置类型（高/中/低音区）映射手部位置
- 钢琴琴键 Morph Target 形变动画系统
- 左右手独立控制，支持单手指跨度配置
- 基于 .keyripple 配置文件的动画数据驱动

#### StringFlowUnreal（弦乐）

- 支持小提琴/中提琴/大提琴/自定义四弦乐器
- 左手四指（1-4指）按弦品位控制（品格索引 FRET_1/9/12）
- 右手弓法控制（弓位置、触弦点 String_Touch_Point）
- 左手位置类型（NORMAL/INNER/OUTER）和右手位置类型（NEAR/FAR/PIZZICATO）
- 四根弦独立调音配置（各乐器预设标准定弦音高）
- 弦振动 Morph Target 动画 + 实时同步系统

#### FretDanceUnreal（吉他/贝斯）

- 支持指弹吉他/电吉他/贝斯三种乐器类型
- 左手把位系统：5个基础位置（P0-P4）+ 4种手部状态（NORMAL/OUTER/INNER/BARRE）
- 右手拨弦/扫弦：5根手指（拇指 T + 食指 I + 中指 M + 无名指 R + 小指 P）+ 手掌 H
- 电吉他特殊控制器层级（手掌与拇指同级，食指为拇指子级）
- 基于 .fretdance 配置文件的动画数据驱动

#### BeatBloomUnreal（打击乐）

- 四肢协调驱动：双手 + 双脚独立控制
- 鼓组配置由 .drumkit 文件动态加载（非硬编码），支持自定义鼓件
- 每个鼓件可配置驱动肢体、MIDI 音符触发和双面组件
- 特殊动作系统：支持组合肢体动作
- 鼓面/镲片 Morph Target 形变动画
- 身体朝向控制（Middle_Hand/Look_At/Head_Control）

#### ZhengDriftUnreal（古筝）

- 21 根弦（索引 0-20），无品格概念
- 左手动作：普通拨弦 + 按弦（PRESS）
- 右手动作：普通拨弦 + 摇指（TREMOLO）
- 手部位置：FAR/MIDDLE/NEAR 三区映射
- 35 个 Control Rig 控制器（双手 + 脚部 + 朝向 + 双线性辅助）
- 弦振动 Morph Target 动画 + 弦独立材质实例
- 身体朝向动画（Head_Control 位置驱动）

#### HarpGlideUnreal（竖琴）🆕

- 47 根弦（索引 0-46），双手仅拨弦无按弦
- 手部姿势系统：FAR/NEAR/ATTACK/REST 四态切换
- 7 个踏板（D/C/B/E/F/G/A），每个 5 档（Flat → Sharp），控制音高升降
- 竖琴单支点三态倾斜（NEAR/MID/FAR），模拟演奏者身体倾斜
- 94 个弦位置记录器（47弦 × 2端点）+ 弦振动 Morph Target + 踏板 Morph Target
- 基于 .harpglide 配置文件的动画数据驱动

## 🏗️ 技术架构

```
MusicDoll Plugin
├── 乐器特定模块 (KeyRipple, StringFlow, FretDance, BeatBloom, ZhengDrift, HarpGlide)
│   ├── Actor 类（场景入口）
│   ├── 处理器类（Animation, ControlRig, Instrument）
│   └── UI 面板（Properties, Operations）
├── 公共模块 (MusicDollCommon)
│   ├── 烘焙系统 (AnimationBaker, BakeTaskManager)
│   ├── IK 系统 (ArcDistributedIK, PoleTargetIK)
│   ├── 工具类 (BoneControlMapping, InstrumentUtility)
│   └── UI 基类 (PanelBase)
└── UI 模块 (MusicDollUI)
    ├── 主窗口管理
    └── 样式系统
```

## 🔧 依赖项

### 必需引擎模块

- Core, CoreUObject, Engine
- InputCore, EnhancedInput
- AnimationCore, AnimGraphRuntime
- **ControlRig**, ControlRigEditor, ControlRigDeveloper
- MovieScene, MovieSceneTracks
- LevelSequence, LevelSequenceEditor
- Sequencer, MovieSceneTools
- Slate, SlateCore
- Json, JsonUtilities
- AssetTools, AssetRegistry

### 编辑器模块

- LevelEditor, UnrealEd
- MovieSceneTools
- EditorStyle, PropertyEditor

## 📋 系统要求

- **Unreal Engine**: 5.7.1（已验证）
- **平台**: Windows
- **开发工具**: Visual Studio 或兼容 IDE

## 🚀 快速开始

### 安装步骤

1. 将插件目录复制到 UE 项目的 `Plugins` 文件夹
2. 重新生成项目解决方案
3. 启动项目并启用插件（编辑 → 插件 → 搜索对应模块名）

### 基本工作流程

以 KeyRippleUnreal（钢琴）为例：

1. **场景设置**
   - 将钢琴模型和角色骨骼拖入场景
   - 添加对应的 Actor（如 KeyRippleUnreal Actor）

2. **参数配置**
   - 选中 Actor，在 Details 面板配置参数
   - 设置 IO 文件路径（.avatar 格式）
   - 绑定骨骼网格体和静态网格体

3. **初始化**
   - 点击 "Setup All Objects" 完成初始化
   - 检查对象状态

4. **动画生成**
   - 配置手部类型（白键/黑键）和位置（高/低/中）
   - 点击 "Generate All Animation" 一键生成
   - 或分别生成表演者动画和乐器动画

5. **预览与调整**
   - 在 Level Sequencer 中播放动画
   - 使用 Save/Load State 保存和恢复状态

## 📚 文档索引

详细文档请参阅 [Documentation](Documentation/) 目录：

### 用户指南

- [MusicDoll 动画生成指南](Documentation/MusicDollAnimationGuide.md) - 动画生成原理详解
- [钢琴用户指南](Documentation/BakeUserGuide.md) - KeyRipple 使用教程
- [Control Rig 缓存子系统指南](Documentation/ControlRigCacheSubsystemGuide.md)
- [弦乐实时同步系统](Documentation/StringFlowRealtimeSyncSystem.md)

### 开发者资源

- [动画生成原则](Documentation/AnimationGenerationPrinciples.md) - 底层实现原理

## 📝 数据结构

### 典型数据流

```
MIDI 文件
    ↓
MusicDoll 工具分析
    ↓
JSON 动画数据 (.avatar)
    ↓
UE 插件解析
    ↓
Level Sequencer 轨道
    ↓
Control Rig 控制器
    ↓
骨骼动画 + Morph Target + 材质
```

### 支持的动画类型

1. **人物演奏动画** - Level Sequencer 驱动身体动作
2. **乐器 Morph Target 动画** - Control Rig 驱动形变
3. **乐器材质动画** - Sequencer 材质轨道驱动

## ⚠️ 免责声明

本项目中的文档均由 AI 编写，可能存在不准确之处。如有问题，请联系作者或参考实际代码实现。

## 🤝 贡献与反馈

欢迎提交 Issue 和 Pull Request！对于功能建议或 Bug 报告，请提供详细描述。

## 📄 许可证

本项目采用 [MIT 许可证](LICENSE)。允许任何人免费使用、复制、修改、合并、发布、分发本软件及其副本，前提是保留本版权声明。

---

**最后更新**: 2026-04-09  
**适用版本**: Unreal Engine 5.7.3  
**维护状态**: 活跃开发中
