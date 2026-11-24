# ALTRun C++ 重写 —— 架构设计

> 目标：用 **C++20 + 纯 Win32 + Direct2D/DirectWrite** 重写 Delphi 2007 的 ALTRun，
> 保持功能 1:1 对齐，零第三方运行时依赖，绿色便携（配置与数据全部在 exe 同目录）。

原始实现的行为细节由四份逆向规格文档固定下来，本文件只描述**重写后的结构**以及
与原文的差异点：

| 文档 | 内容 |
|---|---|
| [spec/01-main-window.md](spec/01-main-window.md) | 主窗口、搜索匹配、键盘交互、执行流程 |
| [spec/02-config-options.md](spec/02-config-options.md) | 50 个 INI 配置键、设置界面、语言文件 |
| [spec/03-shortcut-model.md](spec/03-shortcut-model.md) | 快捷项数据模型、存储格式、打分公式、默认列表 |
| [spec/04-aux-features.md](spec/04-aux-features.md) | 剪贴板、日志、参数窗口、托盘、自动隐藏 |

---

## 1. 技术选型

| 维度 | 选择 | 理由 |
|---|---|---|
| 语言 | C++20 | 用户指定；零依赖、接近原 VCL 的原生行为 |
| UI | Win32（无框架）+ Direct2D / DirectWrite | 原版是自绘皮肤窗口；D2D 提供圆角、半透明、抗锯齿文字 |
| 构建 | CMake 3.28+ / MSVC（`Visual Studio 18 2026` 或 Ninja） | 本机既有 VS 2026 + CMake + Windows SDK 26100 |
| 数据格式 | 与旧版**逐字节兼容**的 `ShortCutList.txt` / `FavoriteList.txt` / `ALTRun.ini` | 用户可无缝迁移，旧 exe 仍能读新 exe 写的文件 |
| 文本 | 内部 UTF-8，Win32 边界 UTF-16，旧文件按 ANSI/GBK(936) 读写 | 中文正确显示，同时兼容 GBK 老数据 |

## 2. 分层与模块

```
src/
├─ main.cpp                 wWinMain：COM/DPI 初始化，进入 App
├─ app/                     进程级编排
│  ├─ App.{h,cpp}           单实例、消息循环、托盘、热键分发、自启动
│  ├─ Hotkey.{h,cpp}        解析 "Alt + R" 并 RegisterHotKey
│  └─ Settings.{h,cpp}      ALTRun.ini 读写（键名与旧版一致）
├─ core/                    无 UI 依赖的可测试逻辑
│  ├─ Encoding.{h,cpp}      GBK ⇄ UTF-8、UTF-8 严格校验
│  ├─ Text.{h,cpp}          UTF-8 ⇄ UTF-16、ASCII 大小写折叠
│  ├─ Shortcut.{h,cpp}      快捷项模型 + ParamType 映射
│  ├─ ShortcutStore.{h,cpp} ShortCutList.txt / FavoriteList.txt / LatestList
│  ├─ ParamHistory.{h,cpp}  ParamHistory.txt 参数历史（rank 淘汰算法）
│  ├─ Matcher.{h,cpp}       正则/通配匹配、打分、Sort() 排序键
│  ├─ Formatter.{h,cpp}     ListFormat 格式化、数字前缀、显示宽度
│  ├─ Launcher.{h,cpp}      命令行解析、占位符替换、ShellExecute/WinExec
│  └─ Logger.{h,cpp}        altRun.log（可开关，带目录回退）
└─ ui/
   ├─ Window.{h,cpp}        无边框窗口基类 + 消息分发
   ├─ D2DContext.{h,cpp}    D2D/DWrite 资源封装（设备丢失重建）
   ├─ MainWindow.{h,cpp}    启动器窗口：输入、结果列表、绘制、键盘/IME
   └─ ParamWindow.{h,cpp}   参数提示窗：单行编辑 + 历史下拉 + 空闲取消
```

依赖方向严格单向：`ui → app → core`，`core` 不包含 `<windows.h>` 之外的 UI 头文件，
便于日后为 `Matcher`/`ShortcutStore`/`Formatter` 添加单元测试。

## 3. 数据模型（与原版一致）

原版的数据模型非常扁平，重写没有擅自扩展字段（`Shortcut.h`）：

| 字段 | 持久化 | 说明 |
|---|---|---|
| `param_type` | ✅ 第 2 字段 | `None` / `No_Encoding` / `URL_Query` / `UTF8_Query` |
| `keyword` | ✅ 第 3 字段 | **唯一**参与匹配的字符串 |
| `name` | ✅ 第 4 字段 | 仅用于显示 |
| `command_line` | ✅ 第 5 字段（到行尾） | 目标+参数+占位符，执行时再拆分 |
| `freq` | ✅ 第 1 字段 | 从搜索结果执行时 +1 |

不使用 `bool favourite` 之类的字段：原版的"收藏"是独立的
`FavoriteList.txt`（`关键字小写 → Name` 映射），用于 `RememberFavouratMatch` 的置顶。

`ShortcutEntry` 额外记录**空白行**，因为原版用空行作为管理器里的分隔行。

## 4. 存储格式

`ShortCutList.txt`（exe 同目录，ANSI/GBK，CRLF）：

```
F%-8d|%-20s|%-30s|%-30s|%s
Freq   ParamType  Keyword  Name   CommandLine
```

- 解析：先按 `|` 切分（≥4 个分隔符），否则按旧版逗号格式切分；字段 `Trim`；
  全角逗号 `，` 先归一化为 `,`；未知行按 `scOther` 丢弃。
- 写入：字段按 **GBK 字节宽度**右补空格，不截断；临时文件 + `MoveFileEx` 原子替换，
  并保留 `ShortCutList.txt.bak`（原版是非原子写，这里刻意修正）。
- 编码探测：BOM → UTF-8/UTF-16；否则严格 UTF-8 校验成功则按 UTF-8；否则按 GBK 解码。
- `FavoriteList.txt`：`关键字小写|Name`，每行一条，按 30 字节宽度补空格。

`ParamHistory.txt`：`rank|参数`，每行一条，最多 `ParamHistoryLimit`（默认 50）条。
淘汰规则与原文一致：满额时从尾部向前找 rank 最小项删除；已存在项 rank+1；
新项插到第 0 位、rank=0。原版格式串 `'%-10d|%-30s%'` 末尾多了一个孤立 `%`，
很可能导致 `EConvertError` 从而历史从未保存成功（规格 §3.6）；重写改为纯
`rank|param`，读取时容忍旧文件的末尾 `%` 与空白（差异 D10）。

`ALTRun.ini`：`<exe目录>\ALTRun.ini`，键名与旧版完全一致（`HotKey1`、`HideDelay`…），
由 `GetPrivateProfileStringW`/`WritePrivateProfileStringW` 读写，因此保持 ANSI 编码
与 `0/1` 布尔约定。`[DEBUG] LatestList` 保存最近列表的**行索引**逗号串（最多 10 项）。

## 5. 匹配与排序（复刻原公式）

搜索模式（`Regex=True` 默认）：
1. 对输入做通配符转换：`* → .*`、`? → .`（已有的 `.*` 先保护、后还原）。
2. 用 ECMAScript 正则对 **小写化** 后的 `keyword` 搜索；`MatchAnywhere=False` 时要求命中位置为 1。
3. 打分：`1024 + freq*4 - match_pos*128 - (len(keyword)-len(query))*16`，`>0` 才收录。

非正则模式：`Pos(小写输入, 小写keyword)`（1 基）+ 同样的打分公式。

空输入时的默认列表按原版 `Sort()` 键排序（在 GBK 字节上计算）：

```
Rank = freq*1000 - 4*(S[1]-'A') - 2*(S[2]-'A') - 1*(S[3]-'A') - Length(S)
```

结果行文本 =（数字前缀，可选）+ `ListFormat`（默认 `%-25s| %s`）：
前 10 行无参数条目为 `" 1|"`、有参数条目为 `"*2|"`，其余为 `"  |"`；
`IndexFrom0to9` 决定 `1..9,0` 还是 `0..9`。

## 6. 执行流水线

`Launcher::BuildLaunchPlan` 严格按原顺序处理：

1. 展开 `%VAR%` 环境变量。
2. 按优先级替换**唯一一个**占位符：`{%p}` → `%p` → `{%c}`（剪贴板）→
   `{%wd}`（前台窗口句柄）→ `{%wt}`（标题）→ `{%wc}`（类名）；参数按模式编码
   （`URL_Query` 用 GBK 字节百分号编码，`UTF8_Query` 用 UTF-8 字节）。
3. 命令行未含占位符时：`URL_Query`/`UTF8_Query` 直接拼接编码结果，其余追加原始参数。
4. 剥离窗口显示前缀：`@+` 最大化、`@-` 最小化、`@` 隐藏。
5. 去掉首尾成对引号；若整串 `FileExists` → 整串为命令（工作目录取其目录）；
   若以 `"` 开头且首个引号内文件存在 → 拆成 命令 + 剩余参数 + 文件目录。
6. 相对路径（`.\`、`..\`）的工作目录设为 exe 目录。
7. 执行：`ShellExecuteExW`（`open`），失败时降级 `WinExec`（与原版一致），
   均失败才提示并计入日志。

## 7. 参数提示、最近列表与快捷执行

**参数提示（原 `frmParam.pas`）**：`Launcher::NeedsParamPrompt` 判定——`ParamType != None`
且命令行不含 `{%c}`/`{%wd}`/`{%wt}`/`{%wc}` 这几个自带取值的占位符时，先弹出
`ParamWindow`：

- 顶部是快捷项名称，单行编辑框（原生 EDIT，支持 IME/剪贴板），右侧"确定"按钮；
- 下方列出全局参数历史（最多 6 行，显示 `次数x` 与参数文本），`↑/↓` 选择或鼠标点击填入；
- `Enter` 确认并执行，`Esc` 两段式（非空先清空、空再取消），空闲 `HideDelay` 秒自动取消；
- 确认后参数写入 `ParamHistory.txt`，随后按 `ParamType` 编码并替换 `{%p}`/`%p`
  （`URL_Query`/`UTF8_Query` 追加编码结果）。

**最近列表（`[DEBUG] LatestList`）**：最近执行的 10 个条目以**行索引**形式持久化。
`Ctrl+L` 在列表区显示最近列表（不过滤，仅展示，可数字键执行）；`LastItemHotKey`
（默认 `ALT+L`）直接执行最近一次条目，且**不增加**该条目的 Freq（与原版一致）。
非搜索路径执行（最近项热键）同样计入最近列表，但不写收藏。

**快捷执行键**（原版 key handling）：

| 输入 | 行为 |
|---|---|
| 末尾空格 | 执行当前选中项 |
| `1`–`9`、`0` | 执行对应编号项（`NumberKey`，`IndexFrom0to9` 决定编号基准） |
| `;` | 执行显示编号 2 的项 |
| `'` | 执行显示编号 3 的项 |
| `ExecuteIfOnlyOne=True` 且仅 1 条结果 | 立即执行（通过 `WM_APP+20` 延迟投递避免重入） |

## 8. 窗口与交互

- 无边框 `WS_POPUP` + `WS_EX_TOOLWINDOW | WS_EX_TOPMOST`；`DwmSetWindowAttribute`
  开启 Win11 圆角与深色模式；`Alpha<255` 时用 `WS_EX_LAYERED` + `SetLayeredWindowAttributes`。
- 位置：光标所在显示器工作区，水平居中、垂直略偏上（原版行为）。
- 输入：窗口自绘单行编辑（支持 IME 组合串），`WM_CHAR` 输入、`Backspace` 删字符、
  `Ctrl+V` 粘贴、`Ctrl+Delete` 清空；数字键直接执行对应行。
- 结果：最多 10 行（`ShowTopTen=False` 时最多 16 行），`↑/↓`/`PgUp`/`PgDn` 选择，
  `Enter` 执行，`Esc` 关闭；鼠标悬停高亮、点击执行、标题区拖动窗口。
- 空闲隐藏：`HideDelay` 秒（规格确认单位为秒），每次按键/移动重置定时器。
- 失焦隐藏：`WM_ACTIVATE` 为非活动时隐藏（原版行为）。
- 托盘：`Shell_NotifyIcon` 左键切换窗口、右键菜单（打开 / 设置 / 退出）。
- 全局热键：`HotKey1`（默认 `Alt + R`）、`HotKey2`（默认 `Pause`，空则禁用）、
  `LastItemHotKey`（默认 `ALT+L`，当前仅唤起窗口，执行"最近项"待 M2）。

## 9. 与原文的刻意差异

| 编号 | 差异 | 原因 |
|---|---|---|
| D1 | 分隔符按"是否存在 3 个以上 `|`"判定，而非"首字符是 F" | 修复原版缺陷 P1（旧逗号格式中 `Firefox,...` 这类条目会被丢弃） |
| D2 | 命令行用"剩余字段重新拼接"还原 | 修复 P2/P3（原版用全局字符串删除定位，会把命令行截断） |
| D3 | 正则非法时视为不匹配 | 原版异常后仍读残留 `MatchPos`，行为未定义 |
| D4 | 结果排序为稳定排序 + 确定性 tie-break（分数、Freq、关键字） | 原版随机快排导致同分顺序不定 |
| D5 | 非正则模式的长度守卫使用**原始**输入长度 | 原版在通配符展开后判断，会误杀合法命中 |
| D6 | 内部 UTF-8、文件边界做 GBK 转换 | 现代 Windows 下正确显示中文，同时兼容旧数据 |
| D7 | 写文件为临时文件 + 原子替换，并保留 `.bak` | 原版非原子写，崩溃会丢列表 |
| D8 | 日志除 `%LOCALAPPDATA%` 外回退到 exe 目录 | 便携/受限环境（沙箱、受限用户）也能记录 |
| D9 | 界面用 D2D 自绘替代 VCL 皮肤引擎 | 现代渲染、无第三方皮肤库；皮肤图/字体解析在 M5 实现 |
| D10 | `ParamHistory.txt` 写 `rank\|param`，读取容忍旧格式的末尾 `%` | 原版格式串 `'%-10d\|%-30s%'` 的孤立 `%` 很可能让保存永远抛异常（规格 U2），直接照抄会让参数历史失效 |
| D11 | 最近项热键走 `WM_HOTKEY` 直接执行，参数项先弹参数窗 | 原版同样先取参数再执行；这里把"唤起窗口"改为"直接执行"，符合热键语义 |
| D12 | `ExecuteIfOnlyOne` 用 `WM_APP+20` 延迟投递执行 | 在结果列表重建过程中同步执行会重入 `RefreshMatches`/`HideLauncher` |

## 10. 构建与运行

```powershell
# 需要 Visual Studio（含 MSVC + Windows SDK）与 CMake ≥ 3.28
cmake -S . -B build/vs -G "Visual Studio 18 2026" -A x64
cmake --build build/vs --config Release

# 产物：build/vs/Release/ALTRun.exe（res/ 会一并复制）
# 开发期直接展示窗口：
.\build\vs\Release\ALTRun.exe --show
```

或使用预设：`cmake --preset msvc-release` / `cmake --build --preset release`（Ninja）。

运行后在 exe 目录生成：`ALTRun.ini`、`ShortCutList.txt`（+`.bak`）、
`FavoriteList.txt`、`altRun.log`。

## 11. 尚未实现（见 [ROADMAP.md](ROADMAP.md)）

设置界面（5 个 Tab / 50 键）、快捷项管理/编辑/导入导出、皮肤背景与字体解析、
语言文件、`AddToSendTo`、单实例唤醒时复用已运行实例的窗口、单元测试与打包。
