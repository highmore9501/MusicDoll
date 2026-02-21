# Bone Control Mapping 编辑器使用指南

## 概述

**Bone Control Mapping 编辑器** (SBoneControlMappingEditPanel) 是 MusicDoll 插件中的一个专业编辑工具，用于在 Control Rig 中建立骨骼(Bone)与控制器(Control)之间的映射关系。

这个编辑器主要应用于：
- 将角色的骨骼动画绑定到 Control Rig 的控制器
- 管理复杂的骨骼-控制器映射关系
- 支持导出/导入映射配置为 JSON 文件
- 快速定位和编辑映射对

---

## 界面概览

编辑器界面从上到下分为以下主要部分：

```
┌─────────────────────────────────────────────────────────┐
│      Bone Control Mapping Editor (标题)                 │
├─────────────────────────────────────────────────────────┤
│ [SyncBoneControlPairs]                                   │
│ 同步骨骼与控制器的位置变换                               │
├─────────────────────────────────────────────────────────┤
│ [文件路径输入框]  [Browse]                               │
│ 用于选择导出/导入文件                                    │
├─────────────────────────────────────────────────────────┤
│ [Export]  [Import]                                       │
│ 导出和导入映射配置                                       │
├─────────────────────────────────────────────────────────┤
│ ┌─ 映射列表 ────────────────────────────────────────┐  │
│ │ Bone         │ Control       │ Action            │  │
│ ├──────────────┼───────────────┼───────────────────┤  │
│ │ [选择Bone]   │ [选择Control] │ [X 删除]          │  │
│ │ arm_l        │ Arm_L_Control │ [X 删除]          │  │
│ │ hand_l       │ Hand_L_Ctrl   │ [X 删除]          │  │
│ │ ...          │ ...           │ ...               │  │
│ └────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────┤
│ [Add]  [Save]              [警告信息]                   │
│ 添加新行    保存所有映射     显示重复项                  │
└─────────────────────────────────────────────────────────┘
```

---

## 功能详解

### 1. SyncBoneControlPairs 按钮

**功能**：同步骨骼与控制器的位置变换

**使用场景**：
- 当 Control Rig 中的控制器位置与骨骼位置不匹配时
- 需要快速对齐所有已映射的控制器位置

**工作原理**：
- 遍历所有已建立的骨骼-控制器映射
- 将每个控制器的位置、旋转、缩放与对应的骨骼同步
- 输出同步成功/失败的统计信息

**日志输出**：
```
OnSyncBoneControlPairsClicked: Successfully synced 15 pairs (Failed: 0)
```

---

### 2. 文件路径管理区

#### 文件路径输入框
- **用途**：显示和编辑导出/导入的目标文件路径
- **支持的格式**：JSON 文件 (*.json)
- **手动输入**：可直接在输入框中输入文件路径，按 Enter 或点击其他地方确认
- **默认位置**：项目目录 (FPaths::ProjectDir)

#### Browse 按钮
**功能**：打开文件浏览对话框

**使用步骤**：
1. 点击 **Browse** 按钮
2. 在文件浏览窗口中选择 JSON 文件
3. 点击打开，文件路径将自动填充到输入框

**支持的操作**：
- 选择已存在的 JSON 文件（导入）
- 选择保存位置（导出）

---

### 3. Export 和 Import 按钮

#### Export 导出

**功能**：将当前的骨骼-控制器映射保存为 JSON 文件

**使用步骤**：
1. 在编辑器中完成映射配置
2. 点击 **Browse** 选择保存位置（或手动输入路径）
3. 点击 **Export** 按钮

**导出文件格式**：
```json
{
  "BoneControlMappings": [
    {
      "BoneName": "armature_L",
      "ControlName": "Arm_L_Control"
    },
    {
      "BoneName": "hand_L",
      "ControlName": "Hand_L_Control"
    }
  ]
}
```

**导出前检查清单**：
- ✓ 所有映射对都已正确配置
- ✓ 没有空白的骨骼或控制器项
- ✓ 确认保存路径是否可写

**日志反馈**：
```
OnExportClicked: Successfully exported 12 mappings to D:\project\bone_control_mapping.json
```

#### Import 导入

**功能**：从 JSON 文件加载骨骼-控制器映射

**使用步骤**：
1. 点击 **Browse** 选择要导入的 JSON 文件
2. 点击 **Import** 按钮
3. 编辑器会清空现有映射并加载文件中的数据

**导入后的行为**：
- 当前编辑器中的所有映射将被替换
- UI 列表自动刷新显示导入的数据
- 映射自动保存到 Control Rig Blueprint
- 重复项检测自动运行

**导入前检查清单**：
- ✓ JSON 文件格式正确
- ✓ BoneName 和 ControlName 字段存在
- ✓ 对应的骨骼和控制器在当前 Control Rig 中存在

**日志反馈**：
```
OnImportClicked: Imported 12 mappings from D:\project\bone_control_mapping.json
OnImportClicked: Successfully saved imported mappings
```

---

### 4. 映射列表区

#### 三列布局

| 列名 | 宽度 | 功能 |
|------|------|------|
| Bone | 40% | 显示/选择骨骼名称 |
| Control | 40% | 显示/选择控制器名称 |
| Action | 20% | 删除映射对 |

#### Bone 列操作

**默认状态**：
```
"Select Bone" (灰色文本)
```

**选择骨骼**：
1. 点击列中的骨骼显示区域打开下拉菜单
2. 在顶部搜索框输入骨骼名称进行筛选
3. 从下拉列表中选择目标骨骼
4. 列表自动刷新显示已选骨骼

**搜索功能**：
- 支持模糊搜索（子字符串匹配）
- 不区分大小写
- 实时筛选显示结果

**示例**：
```
输入: "arm"
结果: armature_L, armature_R, Arm_L_Control, arm_IK...
```

#### Control 列操作

**同 Bone 列操作**，但用于选择 Control Rig 中的控制器

**特殊情况**：
- 不显示骨骼相关的控制器
- 仅显示 Control Rig Blueprint 中定义的有效控制器

#### Action 列（删除操作）

**X 按钮**：删除该行的映射对

**操作流程**：
1. 点击对应行的 **X** 按钮
2. 映射对立即被删除
3. 列表刷新
4. **注意**：此操作需要点击 **Save** 才能永久保存

---

### 5. Add 和 Save 按钮

#### Add 按钮

**功能**：添加新的空白映射行

**使用场景**：
- 需要创建新的骨骼-控制器映射关系
- 批量添加多个映射前

**操作**：
1. 点击 **Add** 按钮
2. 在列表底部出现新的空白行
3. 配置该行的骨骼和控制器
4. 点击 **Save** 保存

**初始状态**：
```
新行显示：Select Bone | Select Control | X
```

#### Save 按钮

**功能**：将所有映射配置保存到 Control Rig Blueprint

**重要**：
- 所有编辑操作都临时存储在编辑器中
- 必须点击 **Save** 才能永久保存到 Control Rig Blueprint
- Import 操作会自动调用 Save

**保存前检查**：
- 检测重复的骨骼映射
- 检测重复的控制器映射
- 警告信息会在界面显示

**日志反馈**：
```
OnSaveClicked: Saving 15 mappings
OnSaveClicked: Successfully saved 15 mappings
```

---

### 6. 重复项警告

**位置**：Save 按钮右侧

**显示规则**：
- 若存在重复的骨骼或控制器映射，显示警告文本
- 背景为红色半透明(1.0, 0.5, 0.5)
- 没有重复项时隐藏

**示例警告信息**：
```
检测到重复项：bone - [arm_L, hand_L] control - [Arm_Control]
```

**如何解决重复项**：
1. 查看警告中列出的重复骨骼/控制器名称
2. 在列表中找到相应的行
3. 删除不需要的重复映射
4. 点击 **Save** 保存

---

## 常见工作流程

### 工作流 1：从头创建映射

```
1. 在关卡中选择乐器 Actor（AInstrumentBase）
2. 编辑器自动加载 Control Rig Blueprint
3. 点击 Add 按钮，为每个需要映射的骨骼添加一行
4. 依次为每行选择对应的骨骼和控制器
5. 检查警告信息，确保没有重复项
6. 点击 Save 保存所有配置
7. 点击 Browse，选择导出位置
8. 点击 Export 保存配置到 JSON 文件
```

### 工作流 2：导入已有配置

```
1. 点击 Browse，选择之前导出的 JSON 文件
2. 点击 Import，所有映射将被加载
3. 根据需要修改或删除某些映射
4. 点击 Save 保存修改
5. 点击 SyncBoneControlPairs 同步控制器位置
```

### 工作流 3：快速检查和调整

```
1. 打开编辑器查看当前的映射配置
2. 在映射列表中搜索特定的骨骼或控制器
3. 根据需要修改映射
4. 删除不需要的映射对
5. 点击 Save 保存
6. 观察 Output Log 中的日志信息确认操作
```

### 工作流 4：在多个项目间共享映射

```
项目 A（导出）：
1. 配置好骨骼-控制器映射
2. 点击 Export 导出为 JSON

项目 B（导入）：
1. 打开相同的乐器 Actor
2. 点击 Browse，选择 A 项目导出的 JSON
3. 点击 Import 加载配置
4. 根据项目 B 的具体情况调整
5. 点击 Save 和 Export 保存
```

---

## JSON 文件格式详解

### 文件结构

```json
{
  "BoneControlMappings": [
    {
      "BoneName": "骨骼名称",
      "ControlName": "控制器名称"
    },
    ...
  ]
}
```

### 字段说明

| 字段 | 类型 | 必需 | 说明 |
|------|------|------|------|
| BoneControlMappings | Array | ✓ | 映射对的数组 |
| BoneName | String | ✓ | 骨骼的 FName，必须与 Skeleton 中的骨骼名称相同 |
| ControlName | String | ✓ | 控制器的 FName，必须与 Control Rig 中的控制器名称相同 |

### 实际例子

```json
{
  "BoneControlMappings": [
    {
      "BoneName": "spine_01",
      "ControlName": "Spine_Control"
    },
    {
      "BoneName": "spine_02",
      "ControlName": "Chest_Control"
    },
    {
      "BoneName": "neck_01",
      "ControlName": "Neck_Control"
    },
    {
      "BoneName": "head",
      "ControlName": "Head_Control"
    },
    {
      "BoneName": "clavicle_l",
      "ControlName": "Clavicle_L_Control"
    },
    {
      "BoneName": "upperarm_l",
      "ControlName": "UpperArm_L_Control"
    },
    {
      "BoneName": "lowerarm_l",
      "ControlName": "LowerArm_L_Control"
    },
    {
      "BoneName": "hand_l",
      "ControlName": "Hand_L_Control"
    }
  ]
}
```

---

## 故障排查

### 问题 1：编辑器无法加载

**症状**：
```
RefreshMappingList: InstrumentActor is not valid
RefreshMappingList: Instrument or SkeletalMeshActor is null
```

**原因**：
- 选中的 Actor 不是 AInstrumentBase 类型
- Actor 没有关联的 SkeletalMeshActor

**解决方案**：
- 在关卡中选择正确的乐器 Actor
- 检查 Actor 的 SkeletalMeshActor 属性是否设置

---

### 问题 2：骨骼/控制器列表为空

**症状**：
```
GetAllBoneNames: Found 0 bones from hierarchy
GetAllControlNames: Found 0 controls from hierarchy
```

**原因**：
- Control Rig Blueprint 未能正确加载
- Control Rig 中没有定义控制器

**解决方案**：
- 检查 SkeletalMeshActor 是否正确配置了 Control Rig
- 确认 Control Rig Blueprint 中定义了控制器层级
- 查看 Output Log 中的详细错误信息

---

### 问题 3：导入失败

**症状**：
```
OnImportClicked: Failed to parse JSON from file
OnImportClicked: File does not exist
```

**原因和解决方案**：

| 错误信息 | 原因 | 解决方案 |
|---------|------|---------|
| File does not exist | 文件路径不正确或文件被删除 | 重新选择文件路径 |
| Failed to parse JSON | JSON 格式不正确 | 用文本编辑器检查 JSON 文件格式 |
| Failed to save imported mappings | Control Rig Blueprint 无效 | 检查 Actor 和 Control Rig 是否正确 |

---

### 问题 4：重复项警告

**症状**：
```
检测到重复项：bone - [arm_L] control - [Arm_Control]
```

**原因**：
- 同一个骨骼或控制器被映射了多次

**解决方案**：
1. 在列表中搜索警告中的骨骼/控制器名称
2. 删除重复的行
3. 点击 Save

---

### 问题 5：导出文件无法读取

**症状**：
```
OnExportClicked: Failed to write to file
```

**原因**：
- 目标路径不可写（权限不足）
- 磁盘空间不足
- 路径格式不正确

**解决方案**：
- 检查文件夹权限
- 选择不同的保存位置
- 确保路径是有效的绝对路径

---

## 日志输出参考

编辑器在 **Output Log** (Verbose 级别)中输出详细信息，用于调试和验证操作：

### 正常流程日志

```
SBoneControlMappingEditPanel::Construct() - Initializing UI
SBoneControlMappingEditPanel::SetActor() - Setting actor to: Cello_Actor
RefreshMappingList() - Starting refresh
RefreshMappingList: Instrument found: Cello_Actor
RefreshMappingList: Successfully obtained ControlRigBlueprint
RefreshMappingList: Found 32 bones and 18 controls
RefreshMappingList: Successfully loaded 8 pairs
RefreshMappingList: RequestListRefresh called
RefreshMappingList: Refresh completed with 8 pairs
```

### 操作日志

```
OnAddRowClicked: Adding new row
OnBoneSelectionChanged: Bone changed to spine_01
OnControlSelectionChanged: Control changed to Spine_Control
OnSaveClicked: Saving 9 mappings
OnSaveClicked: Successfully saved 9 mappings
OnExportClicked: Successfully exported 9 mappings to D:\project\mapping.json
OnImportClicked: Imported 9 mappings from D:\project\mapping.json
OnImportClicked: Successfully saved imported mappings
```

### 错误日志

```
GetAllBoneNames: ControlRigBlueprint is not valid
GetAllControlNames: ControlRigBlueprint is not valid
RefreshMappingList: Failed to get ControlRigBlueprint from SkeletalMeshActor
OnSaveClicked: Failed to save mappings
OnImportClicked: Failed to parse JSON from file
```

---

## 最佳实践

### ✓ 推荐做法

1. **定期导出配置**
   - 每次完成重要的映射修改后，导出配置
   - 便于备份和版本控制

2. **使用有意义的命名**
   - 骨骼和控制器名称应该一致且易于识别
   - 例：bone_spine_01 ↔ Control_Spine_01

3. **批量操作前验证**
   - 在导入或大量修改前，先检查当前配置
   - 必要时先导出备份

4. **及时检查重复项**
   - 每次保存前检查警告信息
   - 解决重复项再继续

5. **使用搜索功能**
   - 对于大量映射，使用搜索快速定位
   - 提高编辑效率

### ✗ 避免的做法

1. 不要在未保存的情况下关闭编辑器
2. 不要修改导入的 JSON 文件结构
3. 不要在多个编辑器实例间切换（会导致数据不同步）
4. 不要忽视重复项警告

---

## 性能考虑

### 编辑器性能

- **映射数量**：编辑器可以高效处理 100+ 的映射对
- **搜索速度**：实时搜索基于客户端过滤，无网络延迟
- **文件 I/O**：导入/导出操作基于文件大小，通常 < 1 秒

### 内存占用

- 每个映射对约占用 64 字节内存
- 1000 个映射对约占用 64 KB
- 对编辑器性能无明显影响

---

## 高级用法

### 脚本自动化

虽然编辑器主要是手动工具，但也可以通过代码调用相关函数：

```cpp
// 获取编辑器实例
SBoneControlMappingEditPanel* Editor = ...;

// 设置 Actor
Editor->SetActor(MyInstrumentActor);

// 读取映射列表
TArray<FBoneControlPair> Mappings = Editor->GetCurrentMappings();

// 刷新列表
Editor->RefreshMappingList();
```

### 自定义集成

可以扩展编辑器以支持：
- 自定义的 JSON 导入/导出格式
- 与外部动画工具的集成
- 自动化的映射生成算法

---

## 常见问题 (FAQ)

**Q: 如何快速创建大量映射？**
A: 使用 JSON 导入功能。先用文本编辑器或脚本生成 JSON 文件，然后导入即可。

**Q: 可以同时编辑多个 Actor 的映射吗？**
A: 不可以，编辑器同时只能处理一个 Actor。需要切换 Actor 时，新的 Actor 数据会覆盖当前编辑。

**Q: 映射对是否有数量限制？**
A: 理论上无限制，但实际受 Control Rig 中的控制器数量限制。

**Q: 导出的 JSON 文件可以手动编辑吗？**
A: 可以，JSON 格式简单易懂，可以用文本编辑器修改。修改后重新导入即可。

**Q: 删除映射对是否可撤销？**
A: 不可撤销。删除后必须点击 Save 才会永久保存。在点击 Save 前，可以关闭编辑器重新打开来恢复。

---

## 相关资源

- **BoneControlMappingUtility.h** - 映射数据操作的工具类
- **InstrumentBase.h** - 乐器 Actor 的基类
- **BoneControlPair.h** - 映射对的数据结构定义
- **Control Rig 文档** - 关于 Control Rig 的更多信息

---

## 版本历史

### v1.0 (当前版本)
- 初始发布
- 基本的映射编辑功能
- JSON 导入/导出
- 重复项检测
- SyncBoneControlPairs 同步功能

---

## 技术支持

如遇到问题，请：
1. 查看 Output Log 中的错误信息
2. 参考本文档中的"故障排查"部分
3. 检查 Console 变量设置
4. 查看相关的源代码注释

---

**最后更新**：2024
**作者**：MusicDoll 开发团队
