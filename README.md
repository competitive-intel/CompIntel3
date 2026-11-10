# CompIntel3

使用 **C++20 / C++/WinRT / WinUI 3** 编写的 Windows 桌面对战工作台，重写参考项目 CompIntel 的本地编程对战流程。包含五子棋和路墙棋规则、C++ Player 编写与编译、本地人机对战、程序互战和对局回放。

这是离线桌面应用。原 Web 版的账号、邮箱、管理员、服务器评测队列和多人云平台不包含在本项目中。

## 构建与启动

需要 Windows 10 2004（19041）及以上、x64、Visual Studio 2022/2026 或 Build Tools 的“使用 C++ 的桌面开发”、Windows SDK **10.0.26100.0**。首次还原依赖需要网络。运行规则测试另需 CMake 3.24 及以上；Visual Studio 2026 请使用支持其生成器的 CMake。

在仓库根目录运行 PowerShell：

```powershell
.\scripts\build.ps1 -Configuration Release -Run
```

脚本自动查找 MSBuild，下载 NuGet CLI，并按照 `packages.config` 还原固定版本依赖。默认构建 Debug；发布构建使用上面的 `-Configuration Release`。也可先运行 `-RestoreOnly`，再使用 Visual Studio 打开 `CompIntel3.sln`。应用输出位于：

```text
artifacts\x64\Release\CompIntel3.exe
```

WinUI 3 运行时使用自包含部署，启动应用时保留输出目录内的全部文件。C++ Release 运行库仍需安装 Microsoft Visual C++ 2015–2026 x64 Redistributable。Debug 版本供安装生成工具的开发环境使用。

```powershell
.\scripts\build.ps1 -Tests
```

`-Tests` 会在应用构建成功后通过 CMake 运行规则、程序运行器、示例程序完整对局和存储恢复测试。测试工程独立于 WinUI；也可通过 `cmake -S . -B build\tests`、`cmake --build build\tests --config Debug` 和 `ctest --test-dir build\tests -C Debug --output-on-failure` 单独运行。

详细操作见 [使用指南](docs/usage.md)，开发 Player 见 [游戏协议](docs/protocol.md)，进程、存储与限制见 [架构说明](docs/architecture.md)。

通过 `-UiTests` 可另外构建和运行真实 WinUI 应用内集成检查：

```powershell
.\scripts\build.ps1 -UiTests
```

该检查在独立测试数据目录中创建真实窗口与控件、调用产品流程，检查日志位于 `artifacts\ui-smoke.log`。测试程序不会注入鼠标键盘输入。它还会将真实 WinUI 控件的界面截图导出到 `artifacts/ui-review/`，覆盖五个页面的深浅主题、紧凑导航及有数据的状态；截图范围与设计说明见 [界面设计](docs/ui-design.md)。

集成检查覆盖页面与主题加载、五子棋与路墙棋对战、编辑器真实编译、Player 两种席位与程序互战、立即取消、非法输出判负、回放与数据重载。

## 程序协议

自定义 Player 是在整局中持续运行的 C++20 控制台程序。使用标准输入读取初始化和对手上一步，向标准输出写一行合法动作并立即刷新；调试文字写入标准错误。

五子棋初始化是三行 `1`、`15 15`、`seat`，落子输出 `X Y`。沿用原协议：席位 0 执白先手，席位 1 执黑后手，至少五子连线获胜，无禁手。

路墙棋初始化为 `1 seat`，走棋输出 `0 X Y`，放墙输出 `1 X Y O`；`O=0` 竖墙，`O=1` 横墙。棋格为 9×9，墙中心坐标为 1–8，每方十面墙。必须为双方保留通路；相邻棋子按规则直跳或斜跳。详细说明见 `docs/`，可运行示例见 `examples/`。

先手读取初始化后立即行动；后手先读取对手第一步。以后只输入对手最近一步，不重复初始化。对局结束时运行器终止进程，不发送结束标记。程序也应在读取失败时结束。

## 项目结构

| 路径 | 内容 |
| --- | --- |
| `src/main.cpp`、`src/ui/` | WinUI 3 桌面入口、界面与本地存储 |
| `src/core/` | 不依赖 WinUI 的 C++ 游戏规则和内置策略 |
| `src/runtime/` | MSVC 编译、管道通信、超时和进程资源控制 |
| `examples/` | 两种游戏的独立 C++ Player 示例；嵌入程序资源 |
| `tests/` | 规则与进程运行器验证 |
| `.reference/` | 提供的原项目，只读参考，不参与编译 |

应用为本机开发者工具。Job Object 限制进程数和内存并清理子进程，运行器另监测主进程的回合 CPU 与墙钟时间。这些限制用于避免失控程序拖住对局，并不构成用于恶意代码的安全沙箱；Player 使用当前用户权限执行。仅编译和运行自己信任的源码。实现与限制见 `docs/architecture.md`。

固定依赖：Windows App SDK 组件 **2.6.5**、C++/WinRT **3.0.260818.1**、WebView2 SDK **1.0.3719.77**（WinUI 投影依赖）。界面通过 C++ 原生控件构建，不使用 WebView 承载页面。

CompIntel 的参考规则和协议沿用其 MIT 许可，保留声明见 `COMPINTEL_LICENSE.txt`。NuGet 依赖的许可和第三方声明随各自包提供。
