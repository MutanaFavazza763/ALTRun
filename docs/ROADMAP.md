# 重写路线图

状态图例：✅ 完成　🔨 进行中　⏳ 未开始

## M0 — 逆向规格（✅）

- [x] 主窗口 / 匹配引擎 / 执行流程规格：`docs/spec/01-main-window.md`
- [x] 50 个 INI 配置键、设置界面、语言机制：`docs/spec/02-config-options.md`
- [x] 快捷项模型、存储格式、打分公式、默认列表：`docs/spec/03-shortcut-model.md`
- [x] 剪贴板 / 日志 / 参数窗口 / 托盘 / 自动隐藏：`docs/spec/04-aux-features.md`

验收：四份文档覆盖原项目 ~10k 行 Pascal 的全部用户可见行为，结论均带源码行号。

## M1 — 可运行骨架（✅）

- [x] CMake + MSVC 构建，零第三方依赖
- [x] 无边框 D2D 窗口：圆角、半透明、置顶、按光标显示器居中
- [x] 全局热键（HotKey1/HotKey2）+ 单实例 + 托盘图标
- [x] `ShortCutList.txt` 兼容读写（GBK、`F%-8d|%-20s|%-30s|%-30s|%s`、空行分隔）
- [x] 首次运行生成与原版一致的默认快捷项列表（原版 34 项；2.0.6 起精简为 32 项）
- [x] `FavoriteList.txt` 读写与 `RememberFavouratMatch` 置顶
- [x] 正则/通配匹配 + 原版打分公式 + 空输入的 `Sort()` 排序键
- [x] `ListFormat` 格式化 + 数字/参数前缀（`1|` / `*2|` / `  |`）
- [x] 执行流水线：环境变量、占位符、`@` 前缀、引号拆分、ShellExecute + WinExec 降级
- [x] `ALTRun.ini` 读写（键名与旧版一致）
- [x] 键盘/鼠标交互、IME 输入、空闲与失焦隐藏、日志

验收：编译通过；`--show` 启动后可见 32 项默认列表；输入 `note` 命中 notepad 并回车启动；
`ShortCutList.txt` 被旧版程序可读（GBK 格式一致）。

## M2 — 参数与最近列表（✅）

- [x] `ParamWindow` 参数输入窗（顶部快捷项名、单行编辑、确定按钮、历史下拉）
- [x] `ParamHistory.txt` 参数历史（`rank|param`，上限 `ParamHistoryLimit`，淘汰最早 rank）
- [x] `{%p}` / `{%c}` / `{%wd}` / `{%wt}` / `{%wc}` 完整占位符行为（执行链已接入）
- [x] `LatestList`（最近执行 10 项，行索引）持久化到 ini `[DEBUG]`
- [x] `LastItemHotKey`（ALT+L）直接执行最近项，且不增加 Freq
- [x] `Ctrl+L` 显示最近列表
- [x] `ExecuteIfOnlyOne`：唯一结果自动执行
- [x] 输入末尾空格 / `;` / `'` 触发执行的原版语义
- [x] 参数窗 Esc 两段式、空闲 `HideDelay` 秒自动取消、`WM_CLOSE` 优雅退出并落盘

验收：`cmd /k %p` 弹出参数窗，输入参数后执行；参数写入 `ParamHistory.txt` 并出现在下次历史中；
ALT+L 直接执行最近项；退出后 ini 含 `LatestList`。

## M3 — 设置界面（✅）

- [x] 原生控件分页窗口 `ConfigWindow`（`SysTabControl32`，720×540，置顶、光标显示器居中）
- [x] 四个页签：**常规** / **热键** / **外观** / **调试·关于**
      （原版 5 个可见 Tab + 1 个隐藏 Tab，重写将列表/皮肤并入"外观"、调试并入"调试·关于"）
- [x] 常规：13 个布尔项（Regex/MatchAnywhere/NumberKey/IndexFrom0to9/RememberFavouratMatch/
      ShowTopTen/ExecuteIfOnlyOne/ExitWhenExecute/ShowCommandLine/ShowOperationHint/
      AutoRun/AddToSendTo/ShowMeWhenStart）+ HideDelay、ParamHistoryLimit
- [x] 热键：三个按键捕获框（`Alt + R` 风格文本、Delete 清空、Tab/Shift+Tab 切换焦点）
- [x] 外观：FormWidth/Alpha/RoundBorderRadius、ListFormat 下拉（3 个预置 + 可编辑）、
      Lang、BGFileName、ShowSkin/ShowTrayIcon/PlayPopupNotify/ShowStartNotification
- [x] 调试·关于：版本、ini 路径、ShortCutRunCount、TraceEnable、相关数据文件路径说明
- [x] 确定 = 写回 `Settings` → `settings_.Save()` → 回调 `App::ApplySettings()`
      （重注册三个热键、应用自启动、刷新主窗透明度/尺寸/定时器）；取消/Esc 不写盘
- [x] 托盘菜单"设置"打开；开发用 `--config` 直接打开

验收：`--config` 可打开并切换四个页签；修改后点"确定" ini 落盘且日志出现
`config window: settings applied` / `settings applied`；取消不改变 ini。

## M4 — 快捷项管理（✅）

- [x] 管理器窗口 `ShortcutManagerWindow`（`SysListView32` 四列：关键字/显示名/参数类型/命令行，
      空白分隔行显示为 `—`，900×560 置顶）
- [x] 新增 / 编辑 / 删除（二次确认）/ 上移 / 下移 / 拖拽排序（`LVN_BEGINDRAG` + HitTest 插入）
- [x] 双击行编辑、`Delete` 删除、`Insert` 新增、`F2` 编辑、右键菜单（新增/编辑/删除）
- [x] 编辑对话框 `ShortcutEditWindow`：关键字/显示名/命令行、参数模式 4 选 1、
      选择文件/选择目录（自动推导关键字与显示名）、测试执行、`Enter` 确定 / `Esc` 取消
- [x] 校验：关键字为空且命令行也为空 → 生成空白分隔行；关键字为空但命令行非空 → 拒绝；
      显示名/关键字禁止 `,` 与 `|`（会破坏存储格式）
- [x] 重复检测：四列（关键字+显示名+参数类型+命令行）全等即拒绝（空白行不参与比较）
- [x] 每次增删改/重排立即写回 `ShortCutList.txt`（GBK、原子替换、自动 `.bak`）
- [x] `core/ShortcutSource`：路径/URL/命令行 → 关键字推导、`.lnk` 解析（`IShellLinkW`）、
      相对/绝对路径转换（相对 exe 目录）
- [x] 导入/导出（`IFileOpenDialog`/`IFileSaveDialog`，导入二次确认，自动识别 UTF-8 BOM /
      UTF-16 BOM / 严格 UTF-8 / GBK）
- [x] 托盘菜单"快捷项管理"入口 + 开发用 `--manager`
- [x] 修复既有 bug：`ShortcutStore::Load()` 会因文件末尾 CRLF 多生成一个空白行，
      导致每次启动/保存列表不断增长

验收：`--manager` 打开管理器可见 32 项 + 空白分隔行；新增/编辑/删除/拖拽后立即落盘；
重启后列表行数不再递增（连续 3 次启停均为 42 行）；编辑对话框可打开并取消。

## M5 — 外观与系统集成（✅）

- [x] 皮肤背景图（`BGFileName`，WIC 解码 JPG/PNG/BMP → D2D 位图，cover 铺满 +
      0.74 透明面板保证可读；缺失/`ShowSkin=0` 静默回退纯色）
- [x] 字体串解析（`TitleFont/KeywordFont/ListFont`：`字体名|样式集|磅值|TColor|字符集`，
      容错解析、五段序列化、`fsBold` → 粗体）
- [x] 三种内置语言（`Localization`：`zh_cn` / `zh_hk` / `en_us`，不加载外部语言文件；
      `res*` → 原版 224 键三语言表 → 裸键后缀回退 → 键名；主窗口/关于/设置/
      管理器/编辑/参数窗/托盘/默认列表全部接入，见 M7/M8）
- [x] 提示音 `Popup.wav`（`PlayPopupNotify`，`PlaySoundW` 异步，缺失静默）
- [x] `AddToSendTo`（`%APPDATA%\Microsoft\Windows\SendTo\ALTRun.lnk`，设置里改完立即生效）
- [x] 关于窗口（版本、原版仓库链接、`ShortCutRunCount`、ini/数据目录、内置三语言正文；
      托盘菜单"关于(&A)"与开发用 `--about`）
- [x] 设置界面外观页新增"背景图片文件名"，改完点确定即重新加载皮肤

验收：设置自定义字体后标题/关键字/列表字号字族变化；放入 `BG.jpg` 后启动器显示皮肤；
三种 `Lang` 值均显示对应内置语言；`--about` 可打开。

## M6 — 质量与发布（✅）

- [x] `altRunTests` 单元测试目标（**1024 项断言**，CTest 集成，`-DALTRUN_BUILD_TESTS=OFF` 可关）：
      Encoding（GBK 中文往返）、Formatter（ListFormat/前缀/CJK 宽度）、Matcher（通配符转换、
      打分公式、`Sort()` 排序键、大小写、空行跳过）、ShortcutStore（新旧格式解析/序列化往返、
      含 `|` 的命令行）、ParamHistory（rank 递增与最小 rank 淘汰）、ShortcutSource（路径/URL
      关键字推导、目录名不截断、缺失 .lnk）、FontSpec、Localization（295 键 × 三语言全覆盖）
- [x] 编码兼容验证：读取 GBK 编码的 `ShortCutList.txt`（日志 `decoded as ANSI/GBK`）、
      修复尾部 CRLF 导致空行累积的缺陷
- [x] Release 便携打包：`scripts/package.ps1`（自动找 CMake → 构建 → 暂存 exe/res/文档/`ALTRun.ini.sample`
      → `dist/ALTRun-<版本>-win64.zip`），已实测 0.21 MB 包在干净目录运行并生成数据文件
- [x] 使用说明 [USAGE.md](USAGE.md) 与 10 张界面截图（启动器/皮肤/多语言/参数/设置/管理器/关于）
- [ ] CI（GitHub Actions）：本地已有 `ctest`/打包一键命令，CI 待后续接入

验收：`ctest -C Release` 全绿；`scripts/package.ps1` 产出 zip；解压后直接运行 exe 正常。

## M7 — 本地化收尾（✅）

- [x] 原版 `EN.lang` / `简体中文.lang` 各 224 键（8 section，键集一致）解析并生成
      `src/core/LocalizationBuiltin.inc`，无 `.lang` 文件也能整段切换中英文
- [x] `Localization::Get()` 四级查找（文件 → `res*` → 224 键表 → 裸键后缀），大小写不敏感
- [x] `ui/UiText.h`（`Loc`/`LocArg`/`LocArgInt`）统一入口；`MainWindow` 的本地 `Loc` 删除
- [x] 5 个窗口 + 托盘菜单/tooltip + `ShortcutStore` 默认列表名接入；`src` 下无中文硬编码
- [x] 72 个 `res*` 键补齐重写独有文案（导入导出、参数窗、调试页、热键捕获提示等）
- [x] 测试：296 键 × 2 语言逐个断言不回退（`tests/LocalizationKeys.inc`，由
      `08-tools/generate_test_loc_keys.py` 生成），断言数 124 → 720

验收：`Lang=EN` / `Lang=简体中文` 两种 ini 下启动，日志分别为 `using built-in en/zh strings`；
首次生成的 `ShortCutList.txt` 显示名随语言变化（中文旧名 GBK / 英文新名）。
证据：`07-evidence/localization/`。

## M8 — 三种内置语言（✅）

- [x] `[GUI] Lang` 收敛为 `zh_cn` / `zh_hk` / `en_us`；旧 `EN` / `简体中文` /
      `繁体中文` 自动迁移，未知值回退 `en_us`
- [x] 删除任意 `<Lang>.lang` 与 `About.<Lang>` 外置文件加载，关于正文也改为内置
- [x] 原版 `繁体中文.lang` 一并生成进 224 键表；残留简体字运行时统一转繁
- [x] 71 个 `res*` 键用 `LCMAP_TRADITIONAL_CHINESE` + 桌面术语替换补齐繁体
- [x] 测试：295 键 × 3 语言，断言数 720 → **1024**；隐藏冒烟覆盖三种语言，
      并验证恶意/遗留 `zh_cn.lang` 不会覆盖内置文案

验收：三种语言日志分别为 `using built-in zh_cn/zh_hk/en_us strings`；
默认列表名依次为 `我的电脑` / `我的電腦` / `My Computer`，GBK 落盘无丢字。
证据：`07-evidence/three-locales/`。

## M9 — 列表对齐与首次启动向导（✅，2.0.4）

- [x] 结果列表改为像素列：`FormatListParts()` 拆出前缀/关键字、分隔符、名称，
      列宽由可见行最宽者决定并限制在 58%，`%s (%s)` 等非 `|` 格式仍整行绘制
- [x] 过长关键字/名称用 `IDWriteTextLayout` 的 character trimming 画省略号，
      不再被窗口右边界硬切
- [x] 首次启动（无 ini）弹欢迎向导：语言（简/繁/英）、随 Windows 启动、
      添加到发送到菜单，外加四条基本用法
- [x] 首次默认语言取系统 UI 语言；默认快捷项名在向导确认后才生成，
      避免英文界面配中文默认名
- [x] `--welcome` 开发标志可强制打开向导，便于检查三种语言排版
- [x] 测试：298 键 × 3 语言 + 两列/整行拆分样例，断言数 1024 → **1045**

验收：12pt 与 14pt 比例字体下列表第二列对齐；长名称显示省略号；
空目录首启看到中文向导，选 English 后默认列表名为 `My Computer`。
证据：`07-evidence/first-run/`。

## M10 — 托盘图标可见性与恢复（✅，2.0.5）

- [x] `AddTrayIcon` 启动重试 3 次，返回成功/失败；失败时记录 `GetLastError`
- [x] 处理 Explorer 的 `TaskbarCreated` 广播，重启资源管理器后自动重建图标
- [x] Win11 首次注册后把本 exe 的 `IsPromoted` 设为 1，避免图标只躺在 `^` 溢出区；
      已显式设为 0（用户主动隐藏）时不覆盖
- [x] `IsPromoted=0/1` 在设置页切换“显示托盘图标”时立即生效（删除后重加）
- [x] 识别 Low 完整性进程：Shell 拒绝图标时日志给出目录完整性原因，并自动显示
      启动器，不再出现“有进程、没图标、也没窗口”
- [x] 验证：低 IL 目录得到错误 5 + 启动器回退；改为 Medium 后图标注册成功；
      UI Automation 在任务栏可见区找到 `ALTRun - Alt+R to open`

验收：正常目录启动后任务栏可见区出现 ALTRun 图标，Explorer 重启后仍在；
低完整性目录不会静默驻留。证据：`07-evidence/tray/`。

## M11 — 原版星形图标与默认列表现代化（✅，2.0.6）

- [x] 把原 Delphi 工程 `Res/Carracho.ico` 嵌入 exe（`IDI_ALTRUN`），托盘、窗口类、
      Alt+Tab 统一使用星形图标；托盘不再使用 `IDI_APPLICATION`
- [x] 默认列表删除 `Upgrade` 升级网址与百度知道，`iexplore.exe` 改为默认浏览器
      （`browser` → `https://www.bing.com`）
- [x] 百度搜索改 Bing（含剪贴板版 `cb`）；Sogou/VeryCD/Yahoo 替换为 Bing 图片、
      Bilibili、知乎；`MyDocument` 指向用户文档，`Reg` 用 `regedit`，`myip` 用
      `ipconfig`
- [x] 三语言默认名同步：默认浏览器 / Bing 搜索 / Bing 搜索剪贴板
- [x] 验证：1045 checks / 0 failure，CTest 1/1；三语言种子各 32 项，无百度/IE/
      Upgrade 残留；任务栏真实桌面可见星形图标

验收：干净目录首启列表 32 项；托盘图标为原版黄色星形。证据：
`07-evidence/tray/taskbar-star-206.png`、`07-evidence/defaults-206-runtime/`。

## M12 — WinUI 3 设置与 Fluent 视觉（✅，2.1.0）

- [x] 新建 `src/settings/`：C++/WinRT + WinUI 3 + XAML NavigationView，1000×720、
      Mica、自定义标题栏；四页常规/热键/外观/版本与路径
- [x] `Project.idl` 声明 `MainWindow` runtimeclass，解决 `MainWindow_base` 缺失；
      vcxproj 加 Midl/ResourceCompile，`settings.rc` 提供星形图标与 VERSIONINFO
- [x] 单实例互斥量 `ALTRun.Settings.SingleInstance.v1`；`--page`
      直达页面；保存后 `WM_APP+30` 通知主程序 `Settings::Load + ApplySettings`
- [x] 主程序 `LaunchSettingsApp` / `OpenSettingsWindow` / `OpenAboutWindow`；
      `--config`、`--about`、托盘菜单优先 WinUI，缺 exe 回退原生
- [x] 首启向导语言下拉改为 Fluent 菜单按钮；管理器 ListView 去 3D 边框、
      Explorer 主题、10.5pt；编辑框沿用 Fluent 控件
- [x] 版本统一 2.1.0：CMakeLists、package.ps1、AboutWindow/ConfigWindow、
      WinUI 关于页、两个 exe 的 VERSIONINFO
- [x] 打包脚本复制 `ALTRun.Settings.exe/pri/xbf/winmd/bootstrap`；
      2.1.0 便携包 1.02 MB
- [x] 验证：主程序/设置 0 error；1099 checks / 0 failure；四页真实运行；
      保存后 ini 落盘与主程序重载日志

验收：`--config` 打开 WinUI 设置，`--about` 打开版本页；保存后主程序立即应用。
证据：`07-evidence/fluent-ui/settings-winui3-*.png`、`welcome-fluent-210-v3.png`、
`manager-fluent-210-v2.png`。

## M13 — 全窗口 UI 重构（🔨，2026-11-26）

- [x] 启动器搜索框增加搜索图标，结果数量改为 Fluent 风格徽章，保持可读性与层级
- [x] 参数确认按钮补齐 hover / pressed / capture 状态，鼠标操作反馈与键盘操作一致
- [x] 公共 Fluent 绘制修复 GDI 画刷泄漏，避免设置、欢迎页、管理器长期运行后资源增长
- [x] 首启向导语言选择改为标准下拉控件，减少自绘菜单与整体风格冲突
- [x] 设置外观页增加显式“应用主题”菜单：跟随 Windows / 浅色 / 深色，并持久化到 `[GUI] Theme`
- [x] 设置外观页增加皮肤文件选择器、路径显示和 JPG/PNG/BMP 使用提示
- [ ] 继续统一启动器、参数窗与 WinUI 设置的 DPI 重排、空状态、可访问性名称和完整截图

验收：主程序可启动并显示主窗口；主程序与核心测试构建通过；本轮核心测试为 1134 checks / 0 failure(s)。

## 后续可选增强（⏳）

- [ ] 管理器窗口尺寸/列宽记忆（原版 `ManWin*`/`ManColWidth*`）
- [x] 设置/向导语言下拉（2.1.0 已由 WinUI ComboBox 与 Fluent 菜单实现）
- [ ] 单实例唤醒时复用已运行实例（`HandleID` 消息）；WinUI 设置双开激活待实测
- [ ] 干净机器缺 Windows App Runtime 时的预检/安装引导
- [ ] GitHub Actions CI 与代码签名
