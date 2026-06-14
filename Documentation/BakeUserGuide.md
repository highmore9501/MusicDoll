# 烘焙（Bake）系统 用户指南

本指南面向使用者，介绍 MusicDoll 插件中新烘焙系统的使用方法与常见操作。该系统支持在多个 Actor（例如 StringFlow、KeyRipple 等）之间提交待烘焙的 Control，一次集中执行并写入到当前打开的 Level Sequence 中。

**Bake系统已经弃用！但暂时还保留了界面和功能，实际需要烘焙更建议直接使用sequence自带的烘焙功能，后续会考虑移除这个系统。**

---

## 快速概览

- 烘焙任务由“提交（Add）”和“执行（Bake All）”两步组成。
- 你可以在不同 Actor 的 Bake 面板中扫描并选择要烘焙的 Control，然后将它们提交到全局烘焙队列。
- 全局烘焙队列面板位于主界面底部（Bake Queue），用于查看、编辑烘焙设置、执行或清理队列中的任务。
- 提交的任务在队列中保留，切换 Actor 后仍然存在，直到被执行或手动移除。

---

## 面板说明

### 1. 模块 Bake 面板（例如 StringFlow / KeyRipple 的 Bake Tab）

功能：

- 扫描当前 Actor 相关的 Control Rig，展示可烘焙的 Control 列表。
- 从下拉菜单选择 Control，然后点击 "Add Selected" 将任务提交到全局队列。
- 在面板下方显示「My Submitted Tasks」，仅列出当前 Actor（你所在面板的 Actor）提交的任务，并可逐条移除或一次性移除所有由该 Actor 提交的任务。

注意点：

- 这些面板仅负责选择与提交，不直接执行烘焙。
- 只能删除自己（当前 Actor）提交的任务，无法删除其他 Actor 的任务。

### 2. 全局烘焙队列（Bake Queue）面板

位置：主面板底部，始终可见（当队列为空时可折叠隐藏）。

功能：

- 显示当前队列中所有待烘焙任务（按提交顺序或分组显示）。
- 编辑共享烘焙设置：Start / End / Step / Overwrite（是否覆盖已存在关键帧）。
- 操作按钮：
  - `Bake All`：执行队列中所有任务（一键烘焙）。
  - `Clear All`：清空队列中的所有任务（不会修改 Level Sequence）。
  - `Clear Tracks`：针对队列中任务对应的 Control，在 Level Sequence 中清除对应通道（会修改序列）。
- 任务条目可单独删除（点击条目右侧 × 按钮）。
- 显示烘焙进度与状态信息。

行为说明：

- 一旦点击 `Bake All` 并完成烘焙，队列会被自动清空，所有面板的「My Submitted Tasks」也会刷新为无任务状态。
- `Clear Tracks` 会根据队列中任务定位到对应 Control Rig 的轨道并清理通道，请谨慎使用。

---

## 常用工作流程（示例）

1. 在编辑器中打开要操作的 Level Sequence。确保 Sequence 已加载。
2. 在主面板中从 Actor 列表选择第一个演奏 Actor（例如 StringFlow）。
3. 切换到该模块的 Bake Tab，点击 `Scan Control Rigs`。等待扫描完成。
4. 在 Performer/Instrument/Bow 下拉框中选择需要烘焙的 Control（可多次操作提交多个 Control）。
5. 点击 `Add Selected` 将选中 Control 提交到全局队列。
6. 切换到另一个 Actor（例如 KeyRipple），重复步骤 3–5，继续向队列累计任务。
7. 在主面板底部的 `Bake Queue` 面板调整 Start/End/Step/Overwrite 设置。
8. 点击 `Bake All`。界面将显示烘焙进度与状态信息，完成后队列自动清空。

---

## 重要提示与注意事项

- 提交任务不会马上写入序列，只有执行 `Bake All` 才会真实写关键帧。
- 提交的任务与 Actor 绑定：如果在提交后删除或销毁了某个 Actor 或其 ControlRig，执行烘焙前系统会自动跳过这些失效任务。
- 烘焙过程会移动 Sequencer 播放头并触发当前场景中 Actor 的同步逻辑（与播放头评估一致），所以请确保在烘焙前保存好未保存的编辑状态。
- `Overwrite existing keyframes`：选中后将先清理序列中对应 Control 的已存在关键帧，再写入新的烘焙数据；否则新关键帧将与现有关键帧共存，可能造成结果不确定。
- 烘焙耗时与总帧数、Control 数量和场景复杂度相关。建议先在较小范围（少帧、少 Control）进行测试确认参数正确。

---

## 常见问题（FAQ）

Q：切换 Actor 会丢失我刚刚添加的任务吗？

A：不会。任务提交到全局队列后与面板实例无关，切换 Actor 后依然保留在队列中（除非手动移除或执行烘焙）。

Q：为什么有些 Control 提交后无法烘焙？

A：可能原因包括：

- 对应 Actor 或 ControlRig 在烘焙前已被销毁或未绑定到 Sequence（系统会在执行前尝试清理失效任务）；
- 用户输入了无效的帧范围或步长；
- 该 Control 并非 Transform/EulerTransform 类型（某些自定义通道不参与本次烘焙）。

Q：烘焙后我看不到关键帧，怎么办？

A：请检查：

- 当前编辑的 Level Sequence 是否为你在面板中选择的 Sequence；
- 烘焙设置（Start/End/Step）是否正确；
- 是否启用了 `Overwrite existing keyframes` 导致意外覆盖（可以先关闭测试）；
- 在 Sequencer 中查找对应的 ControlRig Track，并展开 Section 检查通道。

Q：可以只烘焙队列中某一部分任务吗？

A：当前提供的是“全部烘焙（Bake All）”与逐个删除的工作流。要部分烘焙，可在队列中删除不想烘焙的任务，仅保留目标任务后点击 `Bake All`。

---

## 使用建议

- 先用较短的帧范围和少量 Control 做试验，确认结果和参数无误后再进行完整烘焙。
- 在大型烘焙任务前备份或保存当前 Level Sequence，以便回滚。
- 如果需要多次尝试不同参数，可以将任务提交到队列后修改 Bake Settings，然后再次执行（任务会基于当前设置进行写入）。

---

如需进一步帮助或发现问题，请将烘焙任务的简要信息（参与的 Actor 名称、Control 名称、Start/End/Step、是否勾选 Overwrite）和控制台日志一并提交给开发者团队以便排查。祝使用顺利！
