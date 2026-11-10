# WinUI 界面设计

应用使用原生 WinUI 3 控件及 Windows App SDK 随附的 XamlControlsResources，不定义应用专用 Style、ControlTemplate 或颜色调色板。

## 结构与任务

- TitleBar 统一显示应用身份并提供原生窗口拖动区域；主窗口使用 MicaBackdrop。
- NavigationView 管理对弈、程序实验室、回放、规则和设置。选择状态、键盘导航、焦点指示、紧凑模式及展开面板使用控件自身的行为。
- 对弈页将游戏与棋盘放在主区，双方选择、开始与结束操作及行棋记录放在右侧。开始对局是主要操作。
- 实验室将游戏和名称、CommandBar 文件操作、源码编辑与草稿状态分组；右侧放置编译操作、版本和诊断。代码继续使用等宽字体。
- 回放使用档案与棋盘的主从布局，图标按钮提供中文辅助功能名称与工具提示。
- 设置按外观、编译与运行、本地数据分组，页面可以纵向滚动。首次运行跟随 Windows 外观；旧资料库保留已有深浅色偏好。

## 资源与可访问性

文字使用官方 Title、Subtitle、BodyStrong、Body 与 Caption 字阶。背景、边框、分隔线、次要文字、按钮强调色和圆角均引用官方 ThemeResource。主要按钮使用 AccentButtonStyle，棋格命中区域使用 SubtleButtonStyle，其悬停、按下、禁用及焦点外观由 WinUI 提供。

棋盘是领域绘图，继续使用 Canvas；其颜色绑定到 XAML ThemeResource，而非在 C++ 中复制 RGB 值。棋子使用 Windows 的 Window / WindowText 系统颜色，其他元素使用 WinUI 文字、控件与强调色资源，随深浅色、系统强调色及对比度主题重新求值。状态提供实时通知语义，表单提供名称或关联标签，空列表提供下一步操作说明。

布局使用 4 的倍数间距。NavigationView 自动适应较窄窗口；棋盘用 Viewbox 保持比例，源码和列表独立滚动。窗口最小尺寸为 960 × 720 逻辑像素。

## 验证与截图

运行 `scripts/build.ps1 -UiTests` 会在独立测试资料库中创建真实 WinUI 窗口、运行产品流程，并通过 RenderTargetBitmap 导出当前 XAML 视觉树到 `artifacts/ui-review/`。截图覆盖五个页面的浅色、深色及紧凑导航布局，以及实际对局、版本和回放状态。

这些 PNG 是运行中的原生控件渲染，不是 HTML 或设计稿。RenderTargetBitmap 不捕获桌面壁纸、系统合成的 Mica 或系统窗口按钮，因此材料及系统窗口区域仍需在桌面截图接口可用时检查。对比度资源依照 WinUI 与 Windows 系统资源绑定，测试不会修改用户的系统对比度设置。

## 官方依据

- https://learn.microsoft.com/windows/apps/develop/ui/windows-app-sdk-app-structure
- https://learn.microsoft.com/windows/apps/design/controls/navigationview
- https://learn.microsoft.com/windows/apps/develop/platform/xaml/xaml-theme-resources
- https://fluent2.microsoft.design/design-principles
