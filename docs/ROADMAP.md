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
- [x] 首次运行生成与原版一致的默认快捷项列表（34 项）
- [x] `FavoriteList.txt` 读写与 `RememberFavouratMatch` 置顶
- [x] 正则/通配匹配 + 原版打分公式 + 空输入的 `Sort()` 排序键
- [x] `ListFormat` 格式化 + 数字/参数前缀（`1|` / `*2|` / `  |`）
- [x] 执行流水线：环境变量、占位符、`@` 前缀、引号拆分、ShellExecute + WinExec 降级
- [x] `ALTRun.ini` 读写（键名与旧版一致）
- [x] 键盘/鼠标交互、IME 输入、空闲与失焦隐藏、日志

验收：编译通过；`--show` 启动后可见 34 项默认列表；输入 `note` 命中 notepad 并回车启动；
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

验收：`--manager` 打开管理器可见 34 项 + 空白分隔行；新增/编辑/删除/拖拽后立即落盘；
重启后列表行数不再递增（连续 3 次启停均为 42 行）；编辑对话框可打开并取消。

## M5 — 外观与系统集成（⏳）

- [ ] 皮肤背景图（`BGFileName`）+ 字体字符串解析（`字体名|[]|12|65535|0`）
- [ ] 语言文件（`<Lang>.lang`，224 个键）+ 切换语言
- [ ] 提示音 `Popup.wav`、`PlayPopupNotify`
- [ ] `AddToSendTo`、`AutoRun` 启动文件夹 `.lnk`（原版实现方式）
- [ ] 关于/帮助窗口

验收：切换语言后 UI 文案变化；自定义背景图与字体生效。

## M6 — 质量与发布（⏳）

- [ ] `Matcher` / `ShortcutStore` / `Formatter` / `ParamHistory` 单元测试
      （含 GBK 中文用例、缺陷回归）
- [ ] 与原版 `ShortCutList.txt` 真实样本的兼容性测试
- [ ] Release 打包（zip：exe + res + 默认 ini + 默认列表）
- [ ] 使用说明文档与截图

验收：CI 可一键构建；兼容性测试通过；发布包在干净 Windows 11 上可直接运行。
