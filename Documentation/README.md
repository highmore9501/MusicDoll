README.md

# MusicDoll

MusicDoll是一个系列软件的统称，它们连接Midi文件与虚拟乐手，让虚拟乐手能根据Midi单轨生成播放动作，从而实现虚拟乐手的自动演奏。
本仓库并不是任何一个MusicDoll软件的源码，而是MusicDoll系列软件的Unreal Engine 插件的源码。
Unreal插件的作用是连接Unreal Engine与MusicDoll软件所生成的数据文件，从而让虚拟乐手能在Unreal Engine中演奏。
MusicDoll将由多个乐器类型组成，目前已上线钢琴版本，也就是KeyRippleUnreal，其它乐器版本还在开发中。

## KeyRippleUnreal

KeyRippleUnreal是KeyRipple软件的Unreal插件，目前可使用于Unreal Engine 5.7.1。

### 功能特性

- 支持从MIDI文件生成钢琴演奏动画
- 提供钢琴键形变动画系统（Morph Target）
- 集成Control Rig系统，支持手指精确控制
- 支持左右手独立控制（左手、右手）
- 支持黑白键识别（白色键、黑色键）
- 支持多种位置类型（高、低、中）

### 安装方法

1. 将此插件复制到您的Unreal Engine项目的Plugins文件夹下
2. 重新生成项目解决方案
3. 启动项目时启用插件

### 使用方法

1. 在内容浏览器中创建一个新场景或打开现有场景
2. 将钢琴模型拖入场景中
3. 添加KeyRippleUnreal Actor到场景中
4. 在视口选中KeyRippleUnreal Actor后，将在Details面板看到KeyRipple属性和操作面板

#### 界面功能

插件提供了一个集成的界面，分为两个标签页：

**Properties（属性）标签页**：

- 显示和编辑KeyRipple的各种数值属性（如OneHandFingerNumber、LeftestPosition、MinKey、MaxKey等）
- 配置文件路径（IOFilePath），用于保存钢琴演奏模型的数据文件，后缀为`.avatar`.
- 配置骨骼网格体和钢琴静态网格体引用
- 初始化操作按钮：Check Objects Status、Setup All Objects、Export Recorder Info、Import Recorder Info

**Operations（操作）标签页**：

- 手部状态设置：可以分别设置左右手对应的是白键还是黑键（WHITE/BLACK）
- 手部位置类型：可以分别设置左右手的位置类型（HIGH/LOW/MIDDLE）
- 主要功能按钮：
  - Save State：保存当前状态
  - Load State：加载之前保存的状态
  - Generate Performer Animation：生成表演者（手指）动画
  - Generate Piano Key Animation：生成钢琴键动画
  - Generate All Animation：生成全部动画
  - Init Piano：初始化钢琴

5. 典型工作流程：
   - 首先在Properties标签页中设置好各种参数，包括钢琴模型、骨骼网格等
   - 点击Setup All Objects完成初始化设置
   - 在Operations标签页中设置各种演奏时的基准状态，包括左右手类型和位置
   - 使用Generate All Animation一键生成全部动画，或分别使用Generate Performer Animation和Generate Piano Key Animation生成相应动画
   - 如需调整，可以使用Save State和Load State功能保存和恢复状态

### 模块构成

- [KeyRippleUnreal]：主要Actor类，包含钢琴和手部动画的基本逻辑
- [KeyRippleAnimationProcessor]：动画处理工具类，负责处理动画生成相关操作
- [KeyRipplePianoProcessor]：钢琴处理工具类，处理钢琴相关的初始化和动画
- [KeyRippleControlRigProcessor]：Control Rig处理工具类，管理Rig对象状态
- [KeyRippleOperationsPanel]：操作面板界面
- [KeyRipplePropertiesPanel]：属性面板界面

### 依赖项

- Core
- CoreUObject
- Engine
- InputCore
- AnimationCore
- AnimGraphRuntime
- Json
- JsonUtilities
- EnhancedInput
- ControlRig
- ControlRigEditor
- ControlRigDeveloper
- LevelEditor
- MovieScene
- MovieSceneTracks
- LevelSequence
- LevelSequenceEditor
- Sequencer
- UnrealEd
- MovieSceneTools
- Slate
- SlateCore
- AssetTools
- AssetRegistry
- Common
