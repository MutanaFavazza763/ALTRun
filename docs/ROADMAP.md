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

## M2 — 参数与最近列表（⏳）

- [ ] `frmParam` 参数输入窗口（`ParamType != None` 且命令行无占位符时弹出）
- [ ] `ParamHistory.txt` 参数历史（上限 `ParamHistoryLimit`，默认 50）
- [ ] `{%p}` / `{%c}` / `{%wd}` / `{%wt}` / `{%wc}` 完整占位符行为
- [ ] `LatestList`（最近执行 10 项）持久化到 ini，`LastItemHotKey` 执行最近项
- [ ] `ExecuteIfOnlyOne`：唯一结果自动执行
- [ ] 输入末尾空格 / 数字 / `;` / `'` 触发执行的原版语义

验收：`cmd /k %p` 输入参数后正确执行；ALT+L 启动上一次执行的条目；参数历史可回选。

## M3 — 设置界面（⏳）

- [ ] 5 个可见 Tab（常规 / 外观 / 热键 / 列表 / 关于）+ 1 个隐藏 Tab
- [ ] 50 个配置项全部可编辑并与 ini 双向同步
- [ ] 热键捕获控件、冲突检测、即时生效
- [ ] 托盘菜单与配置按钮入口

验收：修改任一配置项后重启生效，ini 值与界面对应；旧 ini 载入无错。

## M4 — 快捷项管理（⏳）

- [ ] 管理器窗口（ShortCut / Name / ParamType / CommandLine 四列，拖拽排序）
- [ ] 新增/编辑/删除/复制，重复检测（四列全等）
- [ ] 文件/目录选择、`.lnk` 解析、拖放添加、URL 关键字推导
- [ ] 相对/绝对路径批量转换
- [ ] 导入/导出（文本格式）

验收：管理器增删改后 `ShortCutList.txt` 立即落盘且旧版可读。

## M5 — 外观与系统集成（⏳）

- [ ] 皮肤背景图（`BGFileName`）+ 字体字符串解析（`字体名|[]|12|65535|0`）
- [ ] 语言文件（`<Lang>.lang`，224 个键）+ 切换语言
- [ ] 提示音 `Popup.wav`、`PlayPopupNotify`
- [ ] `AddToSendTo`、`AutoRun` 启动文件夹 `.lnk`（原版实现方式）
- [ ] 关于/帮助窗口

验收：切换语言后 UI 文案变化；自定义背景图与字体生效。

## M6 — 质量与发布（⏳）

- [ ] `Matcher` / `ShortcutStore` / `Formatter` 单元测试（含 GBK 中文用例、缺陷回归）
- [ ] 与原版 `ShortCutList.txt` 真实样本的兼容性测试
- [ ] Release 打包（zip：exe + res + 默认 ini + 默认列表）
- [ ] 使用说明文档与截图

验收：CI 可一键构建；兼容性测试通过；发布包在干净 Windows 11 上可直接运行。
