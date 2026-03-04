# MusicDoll HTML 文档说明

## 📚 概述

本目录包含 MusicDoll 插件的现代化 HTML 格式技术文档，采用 Unreal Engine 深色主题设计，提供美观、专业的阅读体验。

## 📁 文件结构

```
HTML/
├── index.html                      # 文档导航首页
├── css/
│   └── musicdoll-docs.css          # 统一样式表（UE 深色主题）
├── js/
│   └── musicdoll-docs.js           # 交互脚本（导航、代码复制等）
├── MusicDollAnimationGuide.html    # 动画生成原理指南
├── BakeUserGuide.html              # 烘焙系统用户指南
├── StringFlowRealtimeSyncSystem.html  # 实时同步系统指南
└── ControlRigCacheSubsystemGuide.html   # ControlRig 缓存子系统指南
```

## 🎨 设计特色

### 视觉风格
- **Unreal Engine 深色主题**: 深灰色背景 + 蓝色强调色
- **Slate UI 风格组件**: 模拟编辑器界面，增强熟悉感
- **专业语法高亮**: 代码块采用 VS Code 风格配色
- **响应式布局**: 支持桌面端和移动端自适应

### 交互功能
- ✅ 侧边栏导航（带激活状态指示）
- ✅ 可折叠章节（details/summary）
- ✅ Tab 切换功能
- ✅ 代码块一键复制
- ✅ 平滑滚动定位
- ✅ 悬停效果与过渡动画

### UI 组件模拟
- **Properties Panel**: 模拟属性面板布局
- **Operations Panel**: 模拟操作按钮区域
- **Bake Queue Panel**: 模拟烘焙队列界面
- **Actor Selector**: 模拟 Actor 选择下拉框
- **Flow Diagrams**: CSS 样式化的流程图

## 📖 文档列表

### 1. 动画生成原理 (MusicDollAnimationGuide.html)
**内容**: 
- 三类核心动画详解（人物、Morph Target、材质）
- Control 列表与数据格式
- 完整工作流程与蓝图实现步骤
- 常见问题解答

**适合人群**: 所有 MusicDoll 用户

### 2. 烘焙系统用户指南 (BakeUserGuide.html)
**内容**:
- 全局烘焙队列使用说明
- 多 Actor 批量提交流程
- 烘焙设置与注意事项
- FAQ 与使用建议

**适合人群**: 需要烘焙动画到 Sequencer 的用户

### 3. 实时同步系统 (StringFlowRealtimeSyncSystem.html)
**内容**:
- 三个 Control Rig 的架构关系
- 父子同步机制的数学原理
- 详细的初始化配置步骤
- 琴弓设置与故障排查

**适合人群**: 使用弦乐器模块的进阶用户

### 4. ControlRig 缓存子系统 (ControlRigCacheSubsystemGuide.md)
**内容**:
- 系统架构与设计理念
- 分层缓存策略
- 性能优化与跨场景保持
- 未来扩展方向

**适合人群**: 开发者和技术爱好者

## 🚀 使用方法

### 方法 1：直接打开
直接在浏览器中打开 `index.html` 文件即可浏览所有文档。

### 方法 2：本地服务器（推荐）
使用任意 HTTP 服务器启动本地预览:

```bash
# 使用 Python
cd Documentation/HTML
python -m http.server 8000

# 访问 http://localhost:8000
```

### 方法 3：集成到项目
将 HTML 目录部署到 Web 服务器或集成到文档系统中。

## 🎯 主要特性

### 导航功能
- **侧边栏**: 固定定位，显示文档结构
- **面包屑**: 当前位置指示
- **快速链接**: 常用章节直达
- **搜索友好**: 清晰的标题层级

### 内容展示
- **代码块**: 语法高亮 + 复制按钮
- **表格**: 响应式数据表格
- **流程图**: ASCII 艺术风格图表
- **提示框**: 多种颜色的 Callout（Note/Warning/Important/Tip）
- **折叠章节**: 可展开/收起的详细内容

### 响应式设计
- **桌面端**: 双栏布局（侧边栏 + 主内容）
- **平板端**: 单栏布局，侧边栏可折叠
- **移动端**: 适配小屏幕，优化触摸操作

## 🛠️ 自定义与扩展

### 修改主题颜色
编辑 `css/musicdoll-docs.css` 中的 CSS 变量:

```css
:root {
    --accent-blue: #0078d4;      /* 主强调色 */
    --accent-green: #2ea043;     /* 成功/绿色 */
    --accent-orange: #d29922;    /* 警告/橙色 */
    --bg-primary: #1a1a1a;       /* 主背景 */
    /* ... */
}
```

### 添加新文档
1. 创建新的 HTML 文件，复制现有文档的结构
2. 在侧边栏添加导航链接
3. 在 `index.html` 中添加文档卡片

### 扩展交互功能
编辑 `js/musicdoll-docs.js` 添加新的交互逻辑。

## 📊 浏览器兼容性

- ✅ Chrome / Edge (最新版)
- ✅ Firefox (最新版)
- ✅ Safari (最新版)
- ⚠️ IE11 (部分功能受限)

## 💡 最佳实践

### 阅读建议
1. **新手用户**: 从《动画生成原理》开始，了解基础概念
2. **实际操作**: 参考《烘焙系统用户指南》进行操作
3. **问题排查**: 查看各文档的 FAQ 部分
4. **深入理解**: 阅读《实时同步系统》和《缓存子系统》

### 搜索技巧
- 使用浏览器 Ctrl+F / Cmd+F 搜索关键词
- 利用侧边栏快速定位章节
- 注意文档中的提示框（Callout），包含重要信息

## 🔗 相关资源

- **原始 Markdown 文档**: `Documentation/` 目录
- **源代码**: `Source/` 目录下的各个模块
- **示例项目**: （如有）链接到示例项目地址

## 📝 版本历史

### v2.0 (2026/03/04)
- ✨ 全新 HTML 格式文档
- 🎨 Unreal Engine 深色主题
- 📱 响应式设计
- 🔧 代码复制功能
- 📋 交互式导航

### v1.x (历史版本)
- 📄 Markdown 格式文档

## 🤝 贡献与反馈

如发现文档中的错误或有改进建议，欢迎联系开发团队。

---

**最后更新**: 2026/03/04  
**文档维护**: MusicDoll 开发团队
