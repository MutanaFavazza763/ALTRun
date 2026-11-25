# ALTRun C++ 重写 —— 架构设计

> 目标：用 **C++20 + 纯 Win32 + Direct2D/DirectWrite** 重写 Delphi 2007 的 ALTRun，
> 保持功能 1:1 对齐；主程序零第三方运行时依赖、绿色便携（配置与数据全部在 exe 同目录），
> 设置与关于单独使用 WinUI 3（需系统 Windows App Runtime）。

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
│  ├─ Settings.{h,cpp}      ALTRun.ini 读写（键名与旧版一致；UTF-8 自动转 UTF-16）
│  ├─ DesktopSpace.{h,cpp}  虚拟桌面归属检测（IVirtualDesktopManager）
│  └─ ShellIntegration.{h,cpp}  SendTo 快捷方式、Popup.wav 提示音
├─ core/                    无 UI 依赖的可测试逻辑
│  ├─ Encoding.{h,cpp}      GBK ⇄ UTF-8、UTF-8 严格校验
│  ├─ Text.{h,cpp}          UTF-8 ⇄ UTF-16、ASCII 大小写折叠
│  ├─ FontSpec.{h,cpp}      旧版字体串 `字体名|[]|12|65535|0` 解析/序列化
│  ├─ Localization.{h,cpp}  三种内置语言（zh_cn / zh_hk / en_us）
│  ├─ LocalizationBuiltin.inc  原版 224 键三语言表（由 08-tools 脚本生成，勿手改）
│  ├─ Shortcut.{h,cpp}      快捷项模型 + ParamType 映射
│  ├─ ShortcutSource.{h,cpp} 路径/URL/命令行 → 关键字推导、.lnk 解析、相对路径
│  ├─ ShortcutStore.{h,cpp} ShortCutList.txt / FavoriteList.txt / LatestList
│  ├─ ParamHistory.{h,cpp}  ParamHistory.txt 参数历史（rank 淘汰算法）
│  ├─ Matcher.{h,cpp}       正则/通配匹配、打分、Sort() 排序键
│  ├─ Formatter.{h,cpp}     ListFormat 格式化、数字前缀、显示宽度
│  ├─ Launcher.{h,cpp}      命令行解析、占位符替换、ShellExecute/WinExec
│  └─ Logger.{h,cpp}        altRun.log（可开关，带目录回退）
└─ ui/
   ├─ Window.{h,cpp}        无边框窗口基类 + 消息分发
   ├─ UiText.h              `Loc(key)` / `LocArg` / `LocArgInt`：全 UI 统一取词入口
   ├─ D2DContext.{h,cpp}    D2D/DWrite 资源封装（设备丢失重建）
   ├─ SkinImage.{h,cpp}     WIC 解码皮肤图（JPG/PNG/BMP）→ ID2D1Bitmap
   ├─ AboutWindow.{h,cpp}   关于窗口（版本、内置三语言正文）
   ├─ MainWindow.{h,cpp}    启动器窗口：输入、结果列表、绘制、键盘/IME
   ├─ ParamWindow.{h,cpp}   参数提示窗：单行编辑 + 历史下拉 + 空闲取消
   ├─ ConfigWindow.{h,cpp}  旧的原生设置窗口（仅在 WinUI 程序缺失时回退）
   ├─ ShortcutManagerWindow.{h,cpp}  快捷项管理器：四列 ListView + 拖拽排序 + 导入导出
   └─ ShortcutEditWindow.{h,cpp}     单条编辑对话框（参数模式、文件/目录选择、测试）
└─ settings/                独立 WinUI 3 进程（C++/WinRT）
   ├─ App.xaml/.cpp         单实例互斥量；第二实例激活已有窗口
   ├─ MainWindow.xaml/.cpp  NavigationView 四页：常规/热键/外观/版本与路径
   ├─ Project.idl           MainWindow runtimeclass，供 XAML 生成基类
   └─ settings.rc           星形图标 + VERSIONINFO
```

依赖方向严格单向：`ui → app → core`，`core` 不包含 `<windows.h>` 之外的 UI 头文件，
便于日后为 `Matcher`/`ShortcutStore`/`Formatter` 添加单元测试。`settings/` 是独立进程，
只通过 `ALTRun.ini` 与窗口消息 `WM_APP+30` 和主程序协作，不链接主程序的 `app/ui` 静态库。

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

## 8. 设置窗口（WinUI 3）

2.1.0 起设置与关于由独立进程 `ALTRun.Settings.exe` 承载（`src/settings/`，
C++/WinRT + XAML + NavigationView），而不是主程序内的原生分页控件。主程序在
`App::OpenSettingsWindow()` 中先调用 `LaunchSettingsApp("general")`：

- 同目录查找 `ALTRun.Settings.exe`，`ShellExecuteW(..., L"--page general", ...)`；
- 返回 ≤32 或文件不存在时，回退到懒创建的原生 `ConfigWindow`；
- `--about` 同理进入 `--page about`，失败才用原生 `AboutWindow`；
- `LaunchSettingsApp` 不等待设置进程，主程序消息循环保持响应。

设置进程保存后直接 `Settings::Save()`，再 `FindWindowW(L"ALTRun.MainWindow.v2")` 并
`PostMessageW(main, WM_APP+30, 0, 0)`；主程序 `HandleExternalMessage` 收到后
`settings_.Load(); ApplySettings();`，重注册热键、刷新外观并写日志。`WM_APP+30` 不能
用 `WM_APP+20`——后者是 `ExecuteIfOnlyOne` 的自动执行消息，会被启动器截获。

WinUI 侧四个页面：**常规**（13 个 ToggleSwitch + HideDelay/ParamHistoryLimit）、
**热键**（三个只读捕获框）、**外观**（宽度/透明度/圆角/ListFormat/语言/背景图/开关）、
**版本与路径**（版本、ini 路径、执行次数、TraceEnable、数据文件）。窗口 1000×720，
Mica 背景 + 自定义 48 px 标题栏；`Project.idl` 声明 `MainWindow` runtimeclass，
否则 XAML 生成的 `MainWindow_base` 找不到类型。

热键捕获沿用主程序的 `Hotkey` 解析/格式化，保存时写回字符串；数值输入在 XAML 的
`NumberBox` 上夹紧范围（Alpha 0–255、圆角 0–200、FormWidth 200–800、HideDelay 0–3600、
ParamHistoryLimit 0–1000）。

## 9. 快捷项管理器

`ShortcutManagerWindow` + `ShortcutEditWindow` 复刻原版管理界面：

- 管理器：`SysListView32` 四列（关键字/显示名/参数类型/命令行），空白分隔行显示为 `—`；
  新增（`Insert`/右键/按钮）、编辑（双击/`F2`）、删除（`Delete`，二次确认）、上移/下移、
  **拖拽排序**（`LVN_BEGINDRAG` + `ListView_HitTest` 插入到目标行前）、导入/导出
  （`IFileOpenDialog`/`IFileSaveDialog`，导入二次确认并自动识别 UTF-8 BOM/UTF-16/GBK）。
- 每次变更立即 `store_.Save()`（原版同样即时落盘），随后触发 `changed_handler_`，
  由 `App` 清空最近列表（行索引已失效）并刷新启动器缓存。
- 编辑对话框：关键字/显示名/命令行、参数模式 4 选 1、`选择文件…`/`选择目录…`
  （`IFileDialog`/`SHBrowseForFolder`，按 `ShortcutSource` 规则自动推导关键字与显示名）、
  `测试`（直接走 `LaunchShortcut`）、`确定`/`取消`。
- 校验：两者都空 → 生成空白分隔行；关键字空而命令行非空 → 拒绝；显示名/关键字含 `,`/`|`
  → 拒绝；重复检测按四列全等（空白行不参与）。
- `core/ShortcutSource`：路径/URL/命令行 → `Shortcut` 的推导规则（原版 §8.1）、`.lnk`
  解析（`IShellLinkW`，图标桩目标保留 `.lnk` 本身）、相对/绝对路径转换（相对 exe 目录）。

## 10. 外观、多语言与系统集成

- **皮肤**：`[GUI] BGFileName`（默认 `BG.jpg`）指向 exe 同目录的图片，`SkinImage` 用 WIC
  解码为 D2D 位图；`DrawFrame` 先按 cover 方式铺满窗口，再叠加 0.74 透明度的圆角面板，
  最后绘制标题/输入/列表。图片缺失、解码失败或 `ShowSkin=0` 时静默回退纯色背景。
  设备丢失重建 render target 后会重新加载位图。
- **字体**：`TitleFont/KeywordFont/ListFont` 沿用旧版五段格式（`Family|StyleSet|Size|TColor|Charset`），
  `FontSpec` 容错解析后用于创建 `IDWriteTextFormat`（磅值 → DIP 直接可用；`[fsBold]` → 粗体）。
- **三种内置语言**：`[GUI] Lang` 只接受 `zh_cn`、`zh_hk`、`en_us`，不再读取任意
  `<Lang>.lang` 或 `About.<Lang>` 外置文件。`NormalizeLanguage()` 会把旧配置的
  `EN` / `简体中文` / `繁体中文` 以及 `zh-HK` 等写法归一化；未知值回退 `en_us`，保存时写回规范值。
  键查找顺序为 **重写独有 `res*` 表 → 原版 224 键内置表 →（裸键时）按 `Section.Key`
  后缀回退 → 键名本身**。224 条来自 `05-original-delphi-source/Bin` 的
  `EN.lang` / `简体中文.lang` / `繁体中文.lang`（8 个 section、键集完全一致），由
  `08-tools/generate_builtin_table.py` 生成到 `LocalizationBuiltin.inc`；原版繁体表中
  残留的简体字会在运行时统一转繁。71 个 `res*` 键的繁体由 Windows
  `LCMAP_TRADITIONAL_CHINESE` + 常见桌面术语替换生成。
  UI 侧统一走 `ui/UiText.h` 的 `Loc(key)`（`LocArg`/`LocArgInt` 处理带一个 `%s`/`%d`
  的旧模板）。`ShortcutStore` 首次生成的默认列表名取自 `[DefaultShortCutName]`，因此
  `zh_cn` 写"我的电脑"，`zh_hk` 写"我的電腦"，`en_us` 写 `My Computer`（GBK 落盘，
  三种语言均已实测无丢字）。修改 `Lang` 并确定后立即重新加载，无需重启。
- **提示音**：`ShowLauncher()` 成功后按 `PlayPopupNotify` 播放 `<exe目录>\Popup.wav`
  （`PlaySoundW`，`SND_ASYNC|SND_NODEFAULT`，缺失静默）。
- **发送到菜单**：`AddToSendTo=1` 时用 `IShellLinkW` 在
  `%APPDATA%\Microsoft\Windows\SendTo\ALTRun.lnk` 创建/重建快捷方式，关闭时删除；设置里改完立即生效。
- **关于窗口**：版本、原版仓库链接、`ShortCutRunCount`、ini/数据目录、当前语言的内置正文
  （原生只读多行 EDIT + 滚动）。托盘菜单"关于(&A)"打开，`--about` 便于开发查看。

## 11. 窗口与交互

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

## 12. 与原文的刻意差异

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
| D13 | 设置与关于改为独立 WinUI 3 进程（原版与 2.0.x 都在主程序内） | 原生 WinUI 控件/主题/无障碍比自绘更完整；主程序保留原生核心与托盘，设置进程通过 ini + `WM_APP+30` 协作，缺失时回退原生窗口 |
| D14 | 确定后**立即**应用热键/外观（原版部分项重启生效） | 更好的体验；`TraceEnable` 仍按原版提示"重启后生效" |
| D15 | 管理器的空白分隔行可编辑（原版禁止） | 统一入口更方便；仍保留"两者都空=空白行"的语义 |
| D16 | 重复检测跳过空白行；关键字也禁止 `\|` | 否则无法添加第二个分隔行；`\|` 会破坏存储格式 |
| D17 | 管理器窗口固定 900×560、列宽固定 | `Settings` 目前没有原版的 `ManWinTop/Width/ManColWidth*` 字段，M5 再补 |
| D18 | 管理器改动后清空最近列表 | 行索引随增删/重排失效（原版同样错位），清空比显示错误条目更安全 |
| D19 | 字体串第 2 段按"样式集"、第 5 段按"字符集"解析 | 与规格 §1.7/§5.1 一致；若按相反映射，典型串 `SimSun\|[]\|12\|65535\|0` 结果相同，只有非空且冲突的串才有区别 |
| D20 | 皮肤之上的面板固定 0.74 透明度，未与 `Alpha` 联动 | `Alpha` 仍作用于整窗分层透明度，保持原版"整窗半透明"语义；面板透明度用于保证文字可读 |
| D21 | 不内嵌默认 `BG.jpg`/`Popup.wav` 资源 | 重写版无资源段；缺失时静默回退纯色/不播放（原版会从资源释放） |
| D22 | 本地化按 `.lang` 键重排，但**布局不变的控件沿用原版措辞** | 原版 VCL 表单的标签文案在 DFM 里、不在 `.lang`；这些控件用 `res*` 键写同样的意思，原版有键的（复选框、菜单、按钮）则连措辞一起对齐，长句（`ParamType_2/3`）只在管理器的宽列使用，编辑框单选改用短文案以免裁切 |
| D23 | 关于窗 420×360（原版约 420×320） | 为容纳 7 行信息 + 可滚动正文 |
| D24 | 唤出前检测虚拟桌面，必要时**重建**主窗（`DesktopSpace` + `App::RecreateMainWindow`） | Windows 无法把窗口搬到别的虚拟桌面；原版是 VCL `Show` 同样受此限制（表现为"热键没反应"）。重建后热键与托盘迁移到新 hwnd，查询失败时 fail-open 不重建 |
| D25 | `ALTRun.ini` 若为 UTF-8 则启动时原地转 UTF-16 | Windows profile API 只认 ANSI 或带 BOM 的 UTF-16；Win11 记事本默认存 UTF-8，会让中文 `Lang`/字体名全变乱码。老 ANSI/GBK 文件不动，写回后仍由 profile API 维护 UTF-16 格式 |
| D26 | 内置原版全部 224 键，并新增 74 个 `res*` 键（共 298 键）；默认列表名走 `[DefaultShortCutName]` | 原版语言文件的键集一致，内置后无需任何语言文件；重写独有的控件文案用 `res*` 补齐。默认列表的显示名按当前内置语言生成（GBK），与原版“按当前语言写默认名”的行为一致 |
| D27 | 只支持 `zh_cn` / `zh_hk` / `en_us` 三种内置语言，不再加载 `.lang` 或 `About.<Lang>` | 用户明确要求收敛语言范围；旧 `Lang=EN/简体中文/繁体中文` 自动迁移，未知值回退 `en_us`，避免外置语言包造成 UI 不一致 |
| D28 | 结果列表按**像素列**绘制，不依赖 `%-25s` 的空格对齐；超长文本以省略号截断 | 比例字体（Segoe UI 等）的空格宽度远小于字母，按字符数填充必然错列，12pt/14pt 下旧截图还会把第二列顶出右边界。`FormatListParts()` 只对两个占位符且中间是竖线的格式启用分列（`%s (%s)` 等仍按整行绘制），列宽取可见行"前缀+关键字"的最宽值并限制在 58% 内 |
| D29 | 首次启动（无 ini）弹欢迎向导：语言、随 Windows 启动、发送到菜单 + 四条用法；默认语言取系统 UI 语言 | 原版首启只弹语言选择与两个系统集成询问，没有使用说明，用户不知道热键和参数星号；向导在选语言后才生成默认快捷项名，避免英文界面配中文默认名。接受后写 ini，因此只出现一次 |
| D30 | 便携包里的样例配置命名 `ALTRun.ini.sample`，不随包提供可用的 `ALTRun.ini` | `Settings::Load()` 以 exe 同目录或 roaming 是否存在 `ALTRun.ini` 判断首次启动；包里放真实 ini 会让首启向导永远不弹。样例只供查键名，用户双击 exe 仍是干净首启 |
| D31 | 托盘图标按 Windows 11 的通知区规则注册：启动重试 3 次、处理 `TaskbarCreated` 重建、首次把本 exe 的 `IsPromoted` 设为 1；无法注册时自动显示启动器 | Win11 把新图标默认塞进 `^` 溢出区，用户会以为程序没图标；`NotifyIconSettings\<hash>` 的 `IsPromoted=1` 才能常驻可见。只在没有显式值（或首次注册）时设一次，用户后来在“任务栏设置”里关掉（`IsPromoted=0`）不覆盖。若目录带 Low 完整性标签，低 IL 进程无法与 Explorer 通信（`Shell_NotifyIcon` 返回错误 5），此时显示主窗口避免“后台有进程但没有任何入口” |
| D32 | `ALTRun.Settings.exe` 是 framework-dependent WinUI 3，不把 180 MB 的 Windows App SDK 自包含进包 | 自包含包实测 294 文件/180 MB（pdb 72 MB、onnxruntime 21 MB、DirectML 18.7 MB），对便携启动器不可接受；改为依赖已安装的 Windows App Runtime，缺失时主程序回退原生设置。干净机器需要先装运行时，写在 README/USAGE |
| D33 | 首启向导的语言下拉改为自绘 Fluent 菜单按钮 | Win32 `COMBOBOX` 即使 `SetWindowTheme(Explorer)` 仍是经典 3D 边框，与卡片/开关不一致；改为 `fluent::CreateButton` + `fluent::ShowPopupMenu`，同时保留键盘操作 |

## 13. 构建、测试与打包

```powershell
# 需要 Visual Studio（含 MSVC + Windows SDK）与 CMake ≥ 3.28
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --target ALTRun
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
  src/settings/ALTRun.Settings.vcxproj /p:Configuration=Release /p:Platform=x64

# 产物：build/Release/ALTRun.exe + build/settings/x64/Release/ALTRun.Settings.exe
.\build\Release\ALTRun.exe --show         # 开发期直接展示启动器
.\build\Release\altRunTests.exe           # 核心单元测试（1099 项断言）
ctest --test-dir build -C Release         # 或通过 CTest 运行

# 便携版打包（含 WinUI 设置程序与 xbf/pri/winmd/bootstrap）
pwsh -NoProfile -File scripts/package.ps1 -Version 2.1.0
```

或使用预设：`cmake --preset msvc-release` / `cmake --build --preset release`（Ninja）。
`-DALTRUN_BUILD_TESTS=OFF` 可跳过测试目标。

运行后在 exe 目录生成：`ALTRun.ini`、`ShortCutList.txt`（+`.bak`）、
`FavoriteList.txt`、`ParamHistory.txt`、`altRun.log`。

单元测试覆盖：GBK ⇄ UTF-8 往返、`ListFormat`/数字前缀/CJK 宽度、通配符→正则、
打分公式与 `Sort()` 排序键、`ShortCutList.txt` 新旧两种格式解析与序列化往返、
参数历史淘汰语义、路径/URL/.lnk 关键字推导、字体串容错解析、列表两列拆分、298 个本地化键
在 `en_us` / `zh_cn` / `zh_hk` 下全部可解析，以及旧语言值归一化。

## 14. 尚未实现（见 [ROADMAP.md](ROADMAP.md)）

管理器窗口尺寸与列宽记忆、单实例唤醒时复用已运行实例的窗口、CI 配置。
语言切换仍需要手改 ini 或在"外观"页输入三个规范值之一。
