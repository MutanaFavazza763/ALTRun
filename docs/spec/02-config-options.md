# 02 配置系统功能规格（ALTRun.ini / 设置窗口 / 热键 / 外观 / 多语言 / 参数历史）

> 分析对象：`ALTRun-original`（只读源码，未做任何修改）
> 分析基线：源码目录内文本文件为 ANSI/GBK 编码；先转成 UTF-8 临时副本逐行精读，**转码不改变行号，下文所有行号均指原始源文件行号**；`.pas/.dfm/.ini/.lang` 均用 CP936 解码验证成功。
> 工程版本：`TITLE='ALTRun'`、`ALTRUN_VERSION='V1.46.7'`（`untALTRunOption.pas:21-22`）；`README.md` 声明 "It is written in Delphi 2007"，工程同时含 `.bdsproj`/`.dproj`，判断为 Delphi 2007（ANSI 版 RTL）。
> 引用格式：`文件名:行号`，多个行号用 `/` 分隔（读/写）。无法仅凭源码确定的地方写"不确定"。
> 相关文档：`04-aux-features.md`（`frmParam`/`frmInvalid`/`untClipboard`/日志等周边模块的细节可交叉参考）。

---

## 0. 文件清单与结论速览

### 0.1 涉及文件与规模核对

| 文件 | 实际行数 | 任务书估计 | 说明 |
|---|---|---|---|
| `Unit\untALTRunOption.pas` | 1159 | 约 1031 | **配置数据模型 + INI 读写 + 语言资源读取的全部实现**；行 1-1157 为常量/变量声明与 3 个例程 |
| `Form\frmConfig.pas` | 689 | 约 605 | 设置窗口**只负责控件装配/交互/预览**，不直接读写 INI；值搬运在 `frmALTRun.pas:273-433` |
| `Form\frmConfig.dfm` | 3381 | — | 6 个 TabSheet，其中 `tsStyle` 运行时隐藏；大量内嵌位图/图标数据 |
| `Bin\ALTRun.ini` | 54 | — | 随软件分发的样例配置（实为作者机器上的配置，见 §1.8） |
| `Bin\EN.lang` / `Bin\简体中文.lang` | 250 / 250 | — | 语言文件（标准 INI 结构） |
| `Bin\PT-BR.lang` / `Bin\繁体中文.lang` | 232 / 252（非空行） | — | 同结构语言文件（未逐行核对内容，节与键结构一致） |

### 0.2 结论速览（最重要的 12 条）

1. 配置全部存放在**可执行文件同目录的 `ALTRun.ini`**，标准 Windows INI，4 个节：`[Config]`、`[Window]`、`[GUI]`、`[DEBUG]`；无注册表、无 AppData（`untALTRunOption.pas:743/830`）。
2. 读取时机共 3 处：进程启动（`ALTRun.dpr:43`）、设置窗口"重置"后（`frmALTRun.pas:439`）、**每次打开快捷项管理窗口**（`frmShortCutMan.pas:595`）。写入时机共 5 处：主窗 FormCreate 末尾、设置窗 OK/重置后、关闭主窗、进程正常退出、关闭快捷项管理窗口（§2）。
3. 无原子写、无备份、无临时文件；直接走 Win32 Profile API 原地改单键；**编码为系统 ANSI 代码页（简体中文=GBK/CP936），无 BOM**（实测 `Bin\ALTRun.ini` 首字节 `5B 43 6F 6E 66 69 67 5D`）。
4. 设置窗口共 **6 个 TabSheet，运行时显示 5 个**（Config / HotKey / Font / Form / Language），第 6 个 `tsStyle`（设计期预览面板）在 `FormCreate` 中置 `TabVisible=False`（`frmConfig.pas:441`）。
5. 设置窗口**没有即时预览**：打开时从全局变量灌入，点 OK 才写回全局变量并立即应用；点 Cancel 只恢复热键注册。Alpha/圆角/宽度/语言 4 类改动**若与打开时不同，会弹提示并自动重启进程**（`frmALTRun.pas:401-414、535-540`）。
6. 热键以"文本"形式存 INI（如 `Alt + R`），文本中的键名是**本地化**的（`LOCALIZED_KEYNAMES=True`，`untALTRunOption.pas:40`）；冲突检测只依赖 `RegisterHotKey` 是否成功，无应用内查重（`HotKeyManager.pas:489-512`）。
7. 默认热键：主 `Alt + R`、辅 `Pause`、"最近一次项" `ALT+L`（`untALTRunOption.pas:746-748`）；辅助热键可用 `DELETE` 键清除（`frmConfig.pas:537-551`）。
8. 字体配置为 5 段竖线格式 `字体名|样式集合|字号|颜色|字符集`，如 `宋体|[]|12|65535|0`；颜色是 Delphi `TColor` 十进制（`$00BBGGRR`），65535=黄、255=红、8388608=深蓝（`untALTRunOption.pas:96-98、703-733`）。
9. `AlphaColor` 是**透明色键（color key）**，`Alpha` 是整体不透明度，两者同时通过 `SetLayeredWindowAttributes(..., LWA_ALPHA or LWA_COLORKEY)` 生效（`frmALTRun.pas:512-516、1086-1093`）。
10. 语言文件 = 标准 INI，8 个节、共 224 个键；缺文件→回退硬编码英文；缺单键→回退源码内英文默认值；切换语言实质需要重启（代码强制重启）。**已知缺陷：`BtnFAKECloseHint` 键名与两份语言文件里的 `BtnCloseHint` 不匹配，中文界面该提示恒为英文**（§6.6）。
11. `ParamHistoryLimit`（默认 50）只被参数窗口 `frmParam` 使用：参数历史存 `ParamHistory.txt`，行格式 `rank|参数`，满额时淘汰 rank 最小项（`frmParam.pas:85-108、252-377`）。该文件的保存格式串有一个多余的 `%`（`frmParam.pas:366-367`），**是否真的写出内容不确定**（§8.6）。
12. `[DEBUG].TraceEnable` 是**死配置**：读入后从未影响日志（`InitLogger(DEBUG_MODE, ...)` 用编译期常量覆盖），且 `SaveSettings` 也不写回（`untALTRunOption.pas:811、884`）。

---

## 1. 配置文件位置、格式与全部键

### 1.1 文件位置与相关文件

| 文件 | 路径 | 生成/读取者 | 说明 |
|---|---|---|---|
| 主配置 | `<exe目录>\ALTRun.ini` | `untALTRunOption.pas:743/830` | 文件名 = `TITLE + '.ini'`（`TITLE='ALTRun'`） |
| 语言 | `<exe目录>\<Lang>.lang` | `untALTRunOption.pas:901` | `<Lang>` 即 `[GUI].Lang` 的值，如 `简体中文.lang` |
| 关于页文本 | `<exe目录>\About.<Lang>` | `frmAbout.pas:51-52` | 自由文本（不是 INI），`TStringList.LoadFromFile` 直接显示 |
| 皮肤图 | `<exe目录>\<BGFileName>` | `frmALTRun.pas:1040-1046` | 默认 `BG.jpg` |
| 提示音 | `<exe目录>\Popup.wav` | `frmALTRun.pas:1142-1146` | 缺失时从资源释放 |
| 快捷项列表 | `<exe目录>\ShortCutList.txt`（`.bak` 备份） | `untShortCutMan.pas:22、613、656-662` | 非本文件范围，但 `ListFormat/Regex` 等配置影响其展示 |
| 收藏映射 | `<exe目录>\FavoriteList.txt` | `untShortCutMan.pas:23、628-630、1758-1787` | 供 `RememberFavouratMatch` 使用 |
| 参数历史 | `<exe目录>\ParamHistory.txt` | `frmParam.pas:26、190` | 见 §8 |

结论：**绿色便携**设计，配置与可执行文件同目录；程序不写注册表保存配置（注册表仅用于自启动/发送到菜单等系统集成，`untUtilities.pas:115-345`）。

### 1.2 格式与编码

- 标准 Windows INI：`[Section]` + `Key=Value`；节名/键名**大小写不敏感**；同一键重复出现的取舍未验证（不确定，按 Win32 Profile API 通常后者生效）。
- 读写实现：Delphi `TIniFile`（`untALTRunOption.pas:737-745、824-832、896-902`）。Delphi 2007 的 `TIniFile` 底层是 `GetPrivateProfileString/WritePrivateProfileString`，因此：
  - 布尔写为 `1`/`0`（`WriteBool` → `WriteInteger(Ord(Value))`），读为"非 0 即真"；
  - 整数读写为字符串 + `StrToIntDef`，**非法整数静默回退默认值**（RTL 行为，非本仓库源码）；
  - 值两端空白会被 Profile API 截掉（语言文件里大量用于对齐的空格不会进入值）；
  - 不支持转义、不支持多行值；`;` 开头的行是注释（本仓库样例中无注释）。
- 编码：**系统 ANSI 代码页**，简体中文环境为 CP936/GBK，**无 BOM**、CRLF 换行。实测：`Bin\ALTRun.ini`、`Bin\简体中文.lang` 首字节即 `[Config]`/`[Common]` 的 ASCII，用 CP936 可完整还原中文 `宋体`、`简体中文`。
- 若文件被人为改成 UTF-16/BOM，旧程序行为**不确定**（未测试）；C++ 重写时必须显式处理。
- 未知键/未知节：旧程序只按名读已知键，故**不会报错**；`SaveSettings` 只写已知键，通过 Profile API 原地更新，**不会删除未知键**（样例中遗留的 `HotKey` 键即证据，见 §1.8）。

### 1.3 `[Config]` 节（25 个键，全部穷举）

类型约定：`Bool` 存储为 `0/1`；`Int` 为十进制整数；`Str` 为任意文本。**所有键读取时都不做范围校验**。

| 键名 | 类型 | 默认值 | 取值范围 / 边界 | 语义与生效位置 | 源码行号（读/写） |
|---|---|---|---|---|---|
| `HotKey1` | Str | `Alt + R` | 修饰键+主键文本；空/非法 → 注册失败告警 | 主全局热键：唤起/隐藏主窗。`DisplayHotKey1` 显示、`GetHotKey1` 回写 | `untALTRunOption.pas:746` / `:833` |
| `HotKey2` | Str | `Pause` | 同上；**空串或等于 `resVoidHotKey` 表示禁用** | 辅全局热键，作用同主热键 | `untALTRunOption.pas:747` / `:834` |
| `LastItemHotKey` | Str | `ALT+L` | 同上；空/非法 → 静默不注册（无告警） | 执行"最近一次快捷项"的热键（第三个 HotKeyManager） | `untALTRunOption.pas:748` / `:835`；`frmALTRun.pas:2081、2380-2409` |
| `AutoRun` | Bool | `True` | 0/1 | 是否随 Windows 启动；实际动作是"所有用户启动文件夹"内建 `.lnk`（`untUtilities.pas:148-167`）。首次运行会弹窗询问并覆盖（`frmALTRun.pas:2101-2106`） | `untALTRunOption.pas:749` / `:836`；应用 `frmALTRun.pas:454、2106` |
| `AddToSendTo` | Bool | `True` | 0/1 | 是否把程序加入"发送到"菜单（`untUtilities.pas:335-345`）。首次运行弹窗询问（`frmALTRun.pas:2110-2116`） | `untALTRunOption.pas:750` / `:837`；应用 `frmALTRun.pas:455、2116` |
| `Regex` | Bool | `True` | 0/1 | 全局变量名 `EnableRegex`：关键字按正则匹配，并把 `*`→`.*`、`?`→`.`（`untShortCutMan.pas:958-1010`） | `untALTRunOption.pas:751` / `:838` |
| `HideDelay` | Int | `15` | 秒；未校验，`0` → 定时器 0ms（约等于立即隐藏） | 主窗自动隐藏延时（`frmALTRun.pas:2628-2637`），**同时**是参数窗口自动取消延时（`frmParam.pas:328-338`） | `untALTRunOption.pas:752` / `:839` |
| `MatchAnywhere` | Bool | `True` | 0/1 | 为 False 时只允许从首字符开始匹配（`untShortCutMan.pas:1006/1016`；界面文案"如果未选中，从第一个字母匹配关键字"） | `untALTRunOption.pas:753` / `:840` |
| `NumberKey` | Bool | `True` | 0/1 | 全局变量名 `EnableNumberKey`：列表项前加 `0-9`/`*0-9` 编号前缀，并允许按数字键执行对应项（`untShortCutMan.pas:1058-1081、1140-1162`；`frmALTRun.pas:1419-1450`） | `untALTRunOption.pas:754` / `:841` |
| `IndexFrom0to9` | Bool | `False` | 0/1 | 编号规则：True = `0,1,…,9`；False = `1,2,…,9,0`（`untShortCutMan.pas:1064-1077、1145-1158`） | `untALTRunOption.pas:755` / `:842` |
| `RememberFavouratMatch` | Bool | `False` | 0/1 | 把"关键字→上次选中项名"的映射记忆并优先置顶（`untShortCutMan.pas:1046-1054`）；映射始终被写入 `FavoriteList.txt`（`:2071-2077`、`:1758-1787`） | `untALTRunOption.pas:756` / `:843` |
| `ParamHistoryLimit` | Int | `50` | 整数；`0`/负数会关闭参数历史并会清空 `ParamHistory.txt`（推导，见 §8.5） | 参数历史条数上限，仅 `frmParam` 使用 | `untALTRunOption.pas:757` / `:844`；`frmParam.pas:85、305、359` |
| `ShowOperationHint` | Bool | `True` | 0/1 | 是否在输入框内显示提示（随机提示 / "按回车添加"等）（`frmALTRun.pas:2581-2618、1658-1664`） | `untALTRunOption.pas:758` / `:845` |
| `ShowCommandLine` | Bool | `True` | 0/1 | 是否露出底部命令行：仅通过主窗高度 `250/230` 切换实现（`frmALTRun.pas:1104-1107、527-531`） | `untALTRunOption.pas:759` / `:846` |
| `ShowStartNotification` | Bool | `True` | 0/1 | 启动时托盘气泡提示（`frmALTRun.pas:2144-2147`） | `untALTRunOption.pas:760` / `:847` |
| `ShowTopTen` | Bool | `True` | 0/1 | 匹配结果只显示前 10 项（`frmALTRun.pas:1394-1403`；设置窗演示列表也用它，`frmConfig.pas:391`） | `untALTRunOption.pas:761` / `:848` |
| `PlayPopupNotify` | Bool | `True` | 0/1 | 主窗弹出时播放 `Popup.wav`（`frmALTRun.pas:1140-1152`） | `untALTRunOption.pas:762` / `:849` |
| `ExitWhenExecute` | Bool | `False` | 0/1 | 执行快捷项后退出程序（`tmrExit` 250ms 后关闭，`frmALTRun.pas:745、1467、2701-2706`；DFM `frmALTRun.dfm` tmrExit Interval=250） | `untALTRunOption.pas:763` / `:850` |
| `ShowSkin` | Bool | `True` | 0/1 | 是否加载皮肤图；关闭时 `Picture := nil`（省内存）（`frmALTRun.pas:1043-1046、466-469`） | `untALTRunOption.pas:764` / `:851` |
| `ShowMeWhenStart` | Bool | `False` | 0/1 | 启动后立即显示主窗（`frmALTRun.pas:2155`） | `untALTRunOption.pas:765` / `:852` |
| `ShowTrayIcon` | Bool | `True` | 0/1 | 托盘图标可见性（`frmALTRun.pas:2136、472`） | `untALTRunOption.pas:766` / `:853` |
| `ShowShortCutButton` | Bool | `True` | 0/1 | 主窗"快捷项管理"按钮显隐，并影响标题标签布局（`frmALTRun.pas:1049-1084、474-510`） | `untALTRunOption.pas:767` / `:854` |
| `ShowConfigButton` | Bool | `False` | 0/1 | 主窗"配置"按钮显隐（同上） | `untALTRunOption.pas:768` / `:855` |
| `ShowCloseButton` | Bool | `True` | 0/1 | 主窗"关闭"按钮显隐（同上） | `untALTRunOption.pas:769` / `:856` |
| `ExecuteIfOnlyOne` | Bool | `False` | 0/1 | 只有 1 个匹配项时直接执行（`frmALTRun.pas:1480-1483`） | `untALTRunOption.pas:770` / `:857` |

> 注意：`HideDelay`、`ParamHistoryLimit`、`LastItemHotKey` 在设置界面**没有对应控件**，只能手工改 INI（§3.10）。

### 1.4 `[Window]` 节（10 个键）

| 键名 | 类型 | 默认值 | 取值范围 / 边界 | 语义与生效位置 | 源码行号（读/写） |
|---|---|---|---|---|---|
| `WinTop` | Int | `0` | 屏幕像素；`≤0` 视为未设置 | 主窗 Top。**仅当 `WinTop>0 且 WinLeft>0` 时应用**，否则窗体居中（`frmALTRun.pas:1022-1032`）。主窗每次显示都会把当前位置写回内存（`:1131-1132`），关闭时也写（`:237-238`） | `untALTRunOption.pas:772` / `:859` |
| `WinLeft` | Int | `0` | 同上 | 主窗 Left | `untALTRunOption.pas:773` / `:860` |
| `ManWinTop` | Int | `0` | 像素 | 快捷项管理窗口 Top；只有 `ManWinTop=0 且 ManWinLeft=0` 才判为"未设置"并居中（`frmShortCutMan.pas:597-611`） | `untALTRunOption.pas:775` / `:862` |
| `ManWinLeft` | Int | `0` | 像素 | 快捷项管理窗口 Left | `untALTRunOption.pas:776` / `:863` |
| `ManWinWidth` | Int | `450` | 像素 | 快捷项管理窗口 Width | `untALTRunOption.pas:777` / `:864` |
| `ManWinHeight` | Int | `240` | 像素 | 快捷项管理窗口 Height | `untALTRunOption.pas:778` / `:865` |
| `ManColWidth0` | Int | `100` | 像素 | 快捷项管理 ListView 第 1 列（快捷项）宽 | 读 `untALTRunOption.pas:780-781`（循环 `KEY_MANCOLWIDTH+IntToStr(i)`，i=0..3）/ 写 `:867-868` |
| `ManColWidth1` | Int | `100` | 像素 | 第 2 列（名称）宽 | 同上 |
| `ManColWidth2` | Int | `100` | 像素 | 第 3 列（参数类型）宽 | 同上 |
| `ManColWidth3` | Int | `100` | 像素 | 第 4 列（命令行）宽 | 同上 |

> 这 10 个键全部由程序自动维护（打开/关闭管理窗口、显示/关闭主窗时），设置界面不提供修改。
> `ManColWidth0..3` 的键名是拼接生成的，源码中**不存在** `KEY_MANCOLWIDTH0` 之类的独立常量。

### 1.5 `[GUI]` 节（10 个键）+ 默认常量

| 键名 | 类型 | 默认值 | 取值范围 / 边界 | 语义与生效位置 | 源码行号（读/写） |
|---|---|---|---|---|---|
| `BGFileName` | Str | `BG.jpg` | 相对 exe 目录的文件名；空串 → 回退 `BG.jpg` | 皮肤背景图。文件缺失时程序会把 DFM 内嵌图片**写出**为该文件（`frmALTRun.pas:1040-1041`）。设置界面**不可修改**（文件名编辑框隐藏，代码注释掉，`frmConfig.dfm:3238-3250`、`frmALTRun.pas:416-427`） | `untALTRunOption.pas:788-789` / `:870` |
| `TitleFont` | Str | `宋体\|[]\|12\|65535\|0` | 5 段竖线格式，见 §5.1；空串→默认 | 标题标签 `lblShortCut` 字体（`frmALTRun.pas:457/1035`） | `untALTRunOption.pas:791-792` / `:871` |
| `KeywordFont` | Str | `宋体\|[]\|12\|255\|0` | 同上 | 关键字输入框 `edtShortCut` 字体（`:458/1036`） | `untALTRunOption.pas:793-794` / `:872` |
| `ListFont` | Str | `宋体\|[]\|12\|8388608\|0` | 同上 | 结果列表 `lstShortCut` 字体（`:459/1037`） | `untALTRunOption.pas:795-796` / `:873` |
| `ListFormat` | Str | `%-25s\| %s` | Delphi `Format` 模板，必须且只能消费 **2 个 `%s`**（关键字、名称）；非法 → 运行时异常 | 渲染列表项文本：`Format(ListFormat,[Item.ShortCut,Item.Name])`（`untShortCutMan.pas:1030、1133、1923`）。界面下拉框只给 3 个预设，见 §5.5 | `untALTRunOption.pas:798` / `:875` |
| `AlphaColor` | Int(TColor) | `clBlack` = `0` | 0..16777215（TColor）；界面用 `TColorBox` 任选 | **透明色键**：该颜色的像素全透明（`SetLayeredWindowAttributes(..., crKey, ..., LWA_COLORKEY)`，`frmALTRun.pas:516、1093`）。样例值 `128` 恰为窗体本色 `clMaroon`（`frmALTRun.dfm` `Color=clMaroon`） | `untALTRunOption.pas:800` / `:877` |
| `Alpha` | Int | `240` | 界面限制 0..255；运行时按 byte 传入 | 整体不透明度（`LWA_ALPHA`），`0`=全透明（`frmALTRun.pas:516、1093`） | `untALTRunOption.pas:801` / `:878` |
| `RoundBorderRadius` | Int | `12` | 界面限制 0..200 | 圆角半径，像素：`CreateRoundRectRgn(0,0,W,H,r,r)`（`frmALTRun.pas:519、1096`）；`0`=直角 | `untALTRunOption.pas:802` / `:879` |
| `FormWidth` | Int | `420` | 界面限制 200..800 | 主窗宽度（像素）。**仅在 `WinTop/WinLeft` 均 >0 的分支里应用**（`frmALTRun.pas:1031`）；否则不生效（见 §7.5 缺陷） | `untALTRunOption.pas:803` / `:880` |
| `Lang` | Str | 系统默认语言=2052 时 `简体中文`，否则 `EN` | 语言文件基名（不含 `.lang`）；任意字符串（不做存在性校验） | 决定加载哪个 `.lang`；设置界面下拉框选值，见 §6 | `untALTRunOption.pas:805-809` / `:881` |

默认常量（`untALTRunOption.pas:96-105`）：

| 常量 | 值 | 行号 |
|---|---|---|
| `DEFAULT_TITLE_FONT_STR` | `宋体\|[]\|12\|65535\|0` | `:96` |
| `DEFAULT_KEYWORD_FONT_STR` | `宋体\|[]\|12\|255\|0` | `:97` |
| `DEFAULT_LIST_FONT_STR` | `宋体\|[]\|12\|8388608\|0` | `:98` |
| `DEFAULT_LIST_FORMAT` | `%-25s\| %s` | `:99` |
| `DEFAULT_ALPHACOLOR` | `clBlack`（=0） | `:100` |
| `DEFAULT_ALPHA` | `240` | `:101` |
| `DEFAULT_ROUNDBORDERRADIUS` | `12` | `:102` |
| `DEFAULT_FORMWIDTH` | `420` | `:103` |
| `DEFAULT_BGFILENAME` | `BG.jpg` | `:104` |
| `DEFAULT_LANG` | `EN` | `:105` |

### 1.6 `[DEBUG]` 节（5 个键）

| 键名 | 类型 | 默认值 | 取值范围 / 边界 | 语义与生效位置 | 源码行号（读/写） |
|---|---|---|---|---|---|
| `TraceEnable` | Bool | `False` | 0/1 | **不生效**：`LoadSettings` 读入 `untALTRunOption.TraceEnable`，但真正控制日志的是 `untLogger.TraceEnable`，后者被 `InitLogger(DEBUG_MODE,...)` 以编译期 `DEBUG_MODE=False` 覆盖（`untALTRunOption.pas:811`；`untLogger.pas:47`；`frmALTRun.pas:2041`；`DEBUG_MODE` 定义 `untALTRunOption.pas:16`）。且 `SaveSettings` 不写该键（`:884` 注释） | 读 `:811`；**不写** |
| `ShortCutRunCount` | Int | `0` | 非负整数（程序自增） | 累计执行快捷项次数；每执行一次 +1（`untShortCutMan.pas:2092`），满 10000 的整数倍弹恭喜框（`:2094-2097`），关于页显示（`frmAbout.pas:49`） | `untALTRunOption.pas:813` / `:886` |
| `HandleID` | Int | `0` | 十进制 HWND | 主窗句柄：用于"第二实例把文件转发给已运行实例"与"再次启动时唤出主窗"（`frmALTRun.pas:2006-2012、2029-2037`）；启动时写入自身句柄（`:2119-2120`），正常关闭时置 0（`:244`） | `untALTRunOption.pas:814` / `:887` |
| `ModifyTime` | Str | `''`（无默认读取） | — | **已废弃**：读写代码均被注释（`:816`、`:888`）。常量 `KEY_MODIFYTIME` 仍在（`:110`） | 不读不写 |
| `LatestList` | Str | `''` | 逗号分隔的快捷项索引，最多 10 项；容忍空段/尾逗号 | 最近使用的快捷项索引列表（`Ctrl+L`/ALT+L 功能）。关闭主窗时由 `GetLatestShortCutIndexList` 生成并保存（`frmALTRun.pas:242`）；首次显示主窗时解析回填（`:1099`；解析见 `untShortCutMan.pas:1839-1867`，上限 `MAX_LATEST_NUM=10` `:27`） | `untALTRunOption.pas:817` / `:889` |

### 1.7 历史遗留 / 已废弃键

| 键名 | 曾经的节 | 现状 | 证据 |
|---|---|---|---|
| `TitleColor` | `[GUI]` | 不再读取；`LoadSettings` 每次都会**删除**该键 | `untALTRunOption.pas:82、784` |
| `KeywordColor` | `[GUI]` | 同上 | `:83、785` |
| `ListColor` | `[GUI]` | 同上 | `:84、786` |
| `HotKey`（无数字后缀） | `[Config]` | **源码中已无对应常量**，不读不写；样例 INI 里仍存在（增量写盘导致残留） | `Bin\ALTRun.ini:2`；源码无 `KEY_HOTKEY`，只有 `KEY_HOTKEY1/2`（`untALTRunOption.pas:47-48`） |
| `ModifyTime` | `[DEBUG]` | 读写代码注释掉，残留键保留 | `untALTRunOption.pas:110、816、888` |
| `BGFileName` 的界面编辑入口 | `[GUI]` | 键仍有效，但设置界面的 `TFilenameEdit` 被隐藏、赋值代码注释掉 | `frmConfig.dfm:3238-3250`；`frmALTRun.pas:416-427` |

> 注意：`LoadSettings` 的"删除 3 个旧颜色键"是**读过程中的写操作**（`untALTRunOption.pas:784-786`）——即打开程序会改 INI（即使不点任何按钮）。

### 1.8 随附样例 `Bin\ALTRun.ini` 的逐键观测

样例（下文注释为本文所加，原文件无注释）：

```ini
[Config]
HotKey=Alt + R          ; 遗留键：当前源码不读（§1.7）
AutoRun=1
AddToSendTo=1
Regex=1
HideDelay=15
MatchAnywhere=1
NumberKey=1
IndexFrom0to9=0
RememberFavouratMatch=0
ParamHistoryLimit=50
ShowOperationHint=1
ShowCommandLine=1
ShowStartNotification=1
ShowTopTen=1
PlayPopupNotify=1
ExitWhenExecute=0
ShowSkin=1
ShowMeWhenStart=0
ShowTrayIcon=1
ShowShortCutButton=0    ; 与代码默认 True 不同（作者机器配置）
ShowConfigButton=0
ShowCloseButton=0       ; 与代码默认 True 不同
HotKey1=Pause           ; 覆盖代码默认 Alt + R
HotKey2=     [5个空格]   ; 非法热键文本：运行时会告警并清空（推导，见下）
ExecuteIfOnlyOne=0
LastItemHotKey=ALT+L
[Window]
WinTop=236
WinLeft=679
ManWinTop=-2
ManWinLeft=-4
ManWinWidth=1230        ; 作者调整过，默认 450
ManWinHeight=808        ; 默认 240
ManColWidth0=100
ManColWidth1=257
ManColWidth2=100
ManColWidth3=543
[GUI]
BGFileName=BG.jpg
TitleFont=宋体|[]|12|65535|0
KeywordFont=宋体|[]|12|255|0
ListFont=宋体|[]|12|8388608|0
ListFormat=%-25s| %s
AlphaColor=128          ; = clMaroon，与窗体本色一致
Alpha=240
Lang=简体中文
RoundBorderRadius=12
FormWidth=420
[DEBUG]
TraceEnable=1           ; 死配置（§1.6）
ShortCutRunCount=11562
HandleID=0
LatestList=41,          ; 尾部有空格，解析时容错
```

观测结论：
1. **样例不是代码默认配置**，而是作者运行后的配置。做 1:1 重写时默认值必须以代码常量为准，样例只用于兼容性回归。
2. `HotKey2=     `（5 个空格）：`DisplayHotKey2` 会 `Trim` 成空并显示 `resVoidHotKey`（`frmConfig.pas:332-335`），`GetHotKey2` 得到空串；但**启动时** `ApplyHotKey2` 会把 `'     '` 解析为 0，因非空且不等于 `resVoidHotKey`(`''`) 而弹出"热键不可为"告警，随后把 `HotKeyStr2` 清空（`frmALTRun.pas:1215-1225`）——这是由代码路径推导的结论（不确定是否真的每次弹，因为第一次运行后会写回空串）。
3. `[Config]` 内键的排列顺序是"旧版本写入的键在前、新版本追加的键在后"（`HotKey` → … → `HotKey1`/`HotKey2`/`ExecuteIfOnlyOne`/`LastItemHotKey`），证明 `SaveSettings` 是**增量原地更新**而非整文件重写，旧键不会被清理。

---

## 2. 配置的持久化时机与机制

### 2.1 何时读

| 时机 | 调用点 | 说明 |
|---|---|---|
| 进程启动，**创建任何窗体之前** | `ALTRun.dpr:42-43` | 先 `IsRunFirstTime := not FileExists(INI)`（`:42`），再 `LoadSettings`（`:43`），然后才 `Application.CreateForm`（`:47-48`）。因此"INI 是否存在"决定是否走首次运行向导 |
| 设置窗口点"重置"并确认后 | `frmALTRun.pas:436-441` | 先 `DeleteFile(ALTRun.ini)`，再 `LoadSettings`（全部落到默认值），随后走"应用并保存"分支 |
| **每次打开快捷项管理窗口** | `frmShortCutMan.pas:590-595` | `FormShow` 里 `LoadSettings`，会覆盖运行期内存中的全局配置（例如用户在外部手改 INI 后打开管理器即可生效） |

读取流程（`untALTRunOption.pas:735-820`）：
1. 拼路径 `<exe目录>\ALTRun.ini`；
2. `TIniFile.Create`（文件不存在也不报错，`ReadString` 直接返回默认值）；
3. 按固定顺序读 4 个节（顺序：Config → Window → GUI → DEBUG）；
4. 读 GUI 前先 `DeleteKey` 删除 3 个旧颜色键；
5. 对 `BGFileName`/`TitleFont`/`KeywordFont`/`ListFont` 做"空串→默认值"兜底；`Lang` 按系统语言选择默认；
6. `IniFile.Destroy`。

### 2.2 何时写

| 时机 | 调用点 | 说明 |
|---|---|---|
| 主窗 `FormCreate` 末尾 | `frmALTRun.pas:2119-2120` | 已写入 `HandleID` 与首启向导结果，**每次启动都写全量键** |
| 设置窗口 OK / 重置后 | `frmALTRun.pas:533` | 应用所有修改后写盘；若需重启，随后的 `RestartMe` 会再触发关闭流程再写一次（`:539`、`:245`） |
| 关闭主窗（菜单退出） | `frmALTRun.pas:230-251`（`SaveSettings` 在 `:245`） | 关闭前 `LatestList := GetLatestShortCutIndexList`、`HandleID := 0` |
| 进程正常退出（事件循环结束后） | `ALTRun.dpr:53-55` | `Application.Run` 返回后再 `SaveSettings` 一次（幂等） |
| 关闭快捷项管理窗口 | `frmShortCutMan.pas:575-588` | `FormDestroy` 中写回 `ManWinTop/Left/Width/Height`、`ManColWidth0..3`，然后 `SaveSettings` |

推论与影响：
- 正常使用一次程序，INI **至少被写 2 次**（启动一次、退出一次）；
- 进程被强杀（任务管理器 / 崩溃）时，本次运行内的 `WinTop/WinLeft/ShortCutRunCount/LatestList` 等**不会落盘**；
- 设置窗口 Cancel 不写盘（只重新注册热键，`frmALTRun.pas:443-447`）。

### 2.3 写入内容集合（`SaveSettings`，`untALTRunOption.pas:822-892`）

写 `[Config]` 25 键、`[Window]` 10 键、`[GUI]` 10 键、`[DEBUG]` 3 键 = **48 个键**；**不写** `TraceEnable`（`:883-884` 注释"仅供手工修改"）和 `ModifyTime`（`:888` 注释）。

即：**INI 中不存在 `TraceEnable` 时，程序运行后也不会自动生成它**（只能手工加）；反之手工加的值也会被保留在文件里（Profile API 不删除未知/未写键）。

### 2.4 原子性 / 备份 / 编码 / 失败处理

| 项 | 结论 | 证据 |
|---|---|---|
| 原子写 | **无**。每条 `Write*` 独立调用 Win32 Profile API 原地改一个键；没有"写临时文件+重命名"，崩溃/断电可能留下半写文件 | `untALTRunOption.pas:833-889`（无任何 Temp/Move 逻辑） |
| 备份 | **INI 无备份**。整个工程只有快捷项列表有备份：`ShortCutList.txt` 在析构时复制为 `.bak`，且读取时若主文件为 0 字节会从 `.bak` 恢复 | `untShortCutMan.pas:656-662、1476-1482` |
| 编码 | 系统 ANSI（CP936/GBK），无 BOM；`TIniFile` 不做 UTF-8 转换 | §1.2 实测字节；`untALTRunOption.pas:745` |
| 写失败 | 完全忽略返回值；目录只读（如装在 `Program Files`）时设置静默丢失，无提示 | `untALTRunOption.pas:833-889` 无 try/except、无返回值检查 |
| 读失败 | 文件损坏/被占用时 `Read*` 返回默认值（RTL 行为）；不会弹错误框 | `untALTRunOption.pas:735-820` |
| 未知键 | 保留（原地更新） | `Bin\ALTRun.ini:2` 的遗留 `HotKey` |
| 首次运行 | INI 不存在 → `IsRunFirstTime=True` → 语言选择 + 自启动/发送到询问 → 写盘 → **主动重启一次**，使第二次启动时 INI 已存在 | `ALTRun.dpr:42`；`frmALTRun.pas:1883-1937、2101-2127` |
| 兼容迁移 | 只有两处主动迁移：<br>1) 读完删 3 个旧颜色键（`untALTRunOption.pas:784-786`）；<br>2) 启动时把旧 `HotRun.ini` 重命名为 `ALTRun.ini`、清理 `HotRun` 的自启动/发送到项（`frmALTRun.pas:2049-2074`） | 同左 |

### 2.5 时序图（文字版）

```
启动:
  dpr:42  IsRunFirstTime = !exists(ALTRun.ini)
  dpr:43  LoadSettings()                  ← 读
  CreateForm(ParamForm)                   → 加载 ParamHistory.txt
  CreateForm(ALTRunForm)                  → 首次运行向导/SetActiveLanguage/ApplyHotKey/...
  frmALTRun:2120 SaveSettings()           ← 写（含 HandleID）
  [首次运行] RestartMe → 退出重来
运行中:
  打开设置窗 → hkm1/hkm2.ClearHotKeys() → 从全局变量灌 UI → ShowModal
    OK     → 回写全局 → 应用 → 需要时提示+RestartMe → SaveSettings ← 写
    重置   → DeleteFile(INI) → LoadSettings ← 读 → 应用 → SaveSettings ← 写 → RestartMe
    Cancel → 仅恢复热键注册
  打开快捷项管理窗 → LoadSettings ← 读 ... 关闭时 SaveSettings ← 写
退出:
  菜单关闭 → LatestList/HandleID → SaveSettings ← 写 → SaveShortCutList → Terminate
  事件循环结束 → dpr:55 SaveSettings ← 写（第二次）
```

---

## 3. 设置窗口结构（`Form\frmConfig.pas` + `frmConfig.dfm`）

### 3.1 窗体属性

| 属性 | 值 | 证据 |
|---|---|---|
| 类 | `TConfigForm`，非自动创建，每次打开 `Create`/`Free` | `frmALTRun.pas:268、544` |
| 尺寸 | Client 554×368；PageControl 540×321 | `frmConfig.dfm:6-7、60-64` |
| 边框 | `bsDialog`；`poScreenCenter` | `frmConfig.dfm:4、17` |
| 字体 | 宋体 9pt（`Font.Height=-13`，ANSI_CHARSET） | `frmConfig.dfm:9-14` |
| 双缓冲 | 运行时 `DoubleBuffered := True` | `frmConfig.pas:438` |
| 标题 | `resConfigFormCaption`（语言文件 `ConfigFormCaption`） | `frmConfig.pas:439` |
| 底部按钮 | `btnOK`(390,335) / `btnCancel`(471,335) / `btnReset`(8,335) | `frmConfig.dfm:22-59、3324-3351` |

### 3.2 Tab 总览：DFM 里 6 个，运行时可见 5 个

| # | TabSheet | DFM Caption | 运行时 Caption（语言键） | ImageIndex | 可见性 |
|---|---|---|---|---|---|
| 0 | `tsConfig` | `&Config` | `PageConfig`（配置） | 0（默认） | 可见 |
| 1 | `tsHotKey` | `&HotKey` | `PageHotKey`（热键） | 1 | 可见 |
| 2 | `tsFont` | `&Font` | `PageFont`（字体） | 2 | 可见 |
| 3 | `tsStyle` | `Style` | 无（不翻译） | 5 | **运行时隐藏**（`frmConfig.pas:441`） |
| 4 | `tsForm` | `F&orm` | `PageForm`（窗体） | 3 | 可见 |
| 5 | `tsLang` | `Language` | `PageLang`（语言） | 4 | 可见 |

Tab 标题在 `FormCreate` 中用语言资源重设（`frmConfig.pas:447-451`），会丢失 DFM 中 `&` 助记符。
`ActivePage = tsConfig`（`frmConfig.dfm:65`）。
`tsStyle` 是**设计期演示面板**（`pnlDemo` 420×250，内含 `imgBackground/lblShortCut/edtShortCut/lstShortCut/edtHint/edtCommandLine/edtCopy/btnShortCut/btnConfig/btnClose`，`frmConfig.dfm:438-3170`）；对应刷新过程 `RefreshDemo`（`frmConfig.pas:553-622`）与 `RefreshOperationHint`（`:624-659`）**调用处被注释**（`:504`），因此运行时完全无用——重写时可不实现该 Tab，但需注意不要把它误当业务界面。

### 3.3 Tab 0 `tsConfig`（配置勾选项）

控件：

| 控件 | 类型 | 关键属性 | 作用 | 证据 |
|---|---|---|---|---|
| `chklstConfig` | `TCheckListBox` | 532×209，宋体 9pt，`OnClick=chklstConfigClick` | 20 个复选项，直接对应 20 个布尔配置 | `frmConfig.dfm:80-97` |
| `spl1` | `TSplitter` | `Align=alTop` | 分隔列表与描述框 | `frmConfig.dfm:70-79` |
| `mmoDesc` | `TMemo` | 只读、绿字、竖向滚动 | 显示当前选中项的描述（`ConfigDescList[i]`） | `frmConfig.dfm:98-115`；`frmConfig.pas:233-236` |

20 个复选项的**顺序与绑定**（索引即 `chklstConfig.Checked[i]`，`frmALTRun.pas:279-317` 读、`:374-393` 写）：

| 索引 | 语言键（ConfigList_i） | 绑定配置键 | 描述语言键（ConfigDescList_i） |
|---|---|---|---|
| 0 | `ConfigList_0` 随系统自动启动 | `AutoRun` | `ConfigDescList_0` |
| 1 | `ConfigList_1` 添加到"发送到"菜单 | `AddToSendTo` | `ConfigDescList_1` |
| 2 | `ConfigList_2` 允许正则表达式(通配符 = *, ?) | `Regex` | `ConfigDescList_2` |
| 3 | `ConfigList_3` 从任意位置匹配关键字 | `MatchAnywhere` | `ConfigDescList_3` |
| 4 | `ConfigList_4` 允许数字快捷键 | `NumberKey` | `ConfigDescList_4` |
| 5 | `ConfigList_5` 数字序列顺序 = 0, 1, …, 8, 9 | `IndexFrom0to9` | `ConfigDescList_5` |
| 6 | `ConfigList_6` 保存最近一次匹配 | `RememberFavouratMatch` | `ConfigDescList_6` |
| 7 | `ConfigList_7` 显示提示 | `ShowOperationHint` | `ConfigDescList_7` |
| 8 | `ConfigList_8` 显示命令行 | `ShowCommandLine` | `ConfigDescList_8` |
| 9 | `ConfigList_9` 启动时显示气泡提示 | `ShowStartNotification` | `ConfigDescList_9` |
| 10 | `ConfigList_10` 仅显示前 10 项 | `ShowTopTen` | `ConfigDescList_10` |
| 11 | `ConfigList_11` 主窗体弹出时播放声音 | `PlayPopupNotify` | `ConfigDescList_11` |
| 12 | `ConfigList_12` 执行快捷项后退出 | `ExitWhenExecute` | `ConfigDescList_12` |
| 13 | `ConfigList_13` 显示皮肤 | `ShowSkin` | `ConfigDescList_13` |
| 14 | `ConfigList_14` 启动时显示窗体 | `ShowMeWhenStart` | `ConfigDescList_14` |
| 15 | `ConfigList_15` 显示托盘图标 | `ShowTrayIcon` | `ConfigDescList_15` |
| 16 | `ConfigList_16` 显示快捷项管理按钮 | `ShowShortCutButton` | `ConfigDescList_16` |
| 17 | `ConfigList_17` 显示配置按钮 | `ShowConfigButton` | `ConfigDescList_17` |
| 18 | `ConfigList_18` 显示关闭按钮 | `ShowCloseButton` | `ConfigDescList_18` |
| 19 | `ConfigList_19` 仅剩一项时立即执行 | `ExecuteIfOnlyOne` | `ConfigDescList_19` |

### 3.4 Tab 1 `tsHotKey`（热键）

| 控件 | 类型 | 关键属性 | 作用 | 证据 |
|---|---|---|---|---|
| `lbledtHotKey1` | `TLabeledEdit` | **ReadOnly**、`LabelPosition=lpLeft`、`OnKeyDown` | 主热键的"按键名"显示/捕获；标签文本 = `LblHotKey1`（主热键） | `frmConfig.dfm:206-224`；`frmConfig.pas:476` |
| `grpKeys1` | `TGroupBox` | 167×56 | 主热键修饰键组 | `frmConfig.dfm:166-205` |
| `chkWindows1` / `chkAlt1` / `chkCtrl1` / `chkShift1` | `TCheckBox` | 文案固定英文 `Windows`/`Alt`/`Ctrl`/`Shift` | 主热键修饰键（Win/Alt/Ctrl/Shift） | `frmConfig.dfm:173-204` |
| `lblPlus` | `TLabel` | `+` | 修饰键组与按键之间的加号（纯装饰） | `frmConfig.dfm:123-137` |
| `lbledtHotKey2` | `TLabeledEdit` | ReadOnly、`OnKeyDown` | 辅热键；标签 = `LblHotKey2`（辅热键） | `frmConfig.dfm:225-243`；`frmConfig.pas:477` |
| `grpKeys2` / `chkWindows2` / `chkAlt2` / `chkCtrl2` / `chkShift2` | 同上 | 辅热键修饰键组 | 同主热键 | `frmConfig.dfm:244-283` |
| `lbl1` | `TLabel` | `+` | 装饰 | `frmConfig.dfm:138-152` |
| `lblHotkeyHint` | `TLabel` | 红黑字 | 文案 = `LblHotKeyHint`（"可以选择另一个热键作为辅热键"） | `frmConfig.dfm:153-165`；`frmConfig.pas:475` |
| 控件提示 | — | `lbledtHotKey1.Hint=LblHotKeyHint1`，`lbledtHotKey2.Hint=LblHotKeyHint2` | 悬停提示说明"敲键盘选择主/辅热键，DELETE 可去除辅热键" | `frmConfig.pas:478-479` |

### 3.5 Tab 2 `tsFont`（字体）

| 控件 | 类型 | 作用 | 证据 |
|---|---|---|---|
| `lblTitleFont` / `lblKeywordFont` / `lblListFont` | `TLabel` | 三个标签（语言键 `LblTitleFont` 等） | `frmConfig.dfm:291-311` |
| `lblTitleSample` / `lblKeywordSample` / `lblListSample` | `TLabel` | 三个字体示例标签；打开设置窗时用 `StrToFont(...)` 把当前字体套上去（`frmConfig.pas:320-322`）；修改字体只改这三个示例标签，改完在 OK 时由 `FontToStr` 回写字符串 | `frmConfig.dfm:312-335`；`frmConfig.pas:320-322` |
| `btnModifyTitleFont` | `TButton` | 打开 `TFontDialog` 改标题字体；选完强制 `Pitch := fpFixed`（等宽）**但 Pitch 不进入字符串格式，等于丢失** | `frmConfig.dfm:382-390`；`frmConfig.pas:204-211` |
| `btnModifyKeywordFont` | `TButton` | 改关键字字体 | `frmConfig.pas:166-171` |
| `btnModifyListFont` | `TButton` | 改列表字体，并同步设置"列表格式示例"标签字体 | `frmConfig.pas:173-182` |
| `btnResetFont` | `TButton` | 确认后把三个示例标签重置为三个 `DEFAULT_*_FONT_STR` | `frmConfig.pas:193-202`；`frmConfig.dfm:409-423` |
| `lblResetFont` | `TLabel` | "重置所有字体"标题 | `frmConfig.dfm:336-348` |
| `shp1` | `TShape` | 分隔线 | `frmConfig.dfm:364-374` |
| `lblListFormat` | `TLabel` | "列表格式" | `frmConfig.dfm:349-355` |
| `cbbListFormat` | `TComboBox` | `Style=csDropDownList`（**只能选，不能输入**）；运行时加入 3 个预设 + 当前自定义值 | `frmConfig.dfm:424-436`；`frmALTRun.pas:324-331` |
| `lblFormatSample` / `lblListFormatSample` | `TLabel` | "格式示例"/"列表格式示例"；`cbbListFormatChange` 用 `Format(cbbListFormat.Text, ['calc','计算器'])` 刷新示例 | `frmConfig.dfm:356-381`；`frmConfig.pas:223-231` |

### 3.6 Tab 3 `tsForm`（窗体外观）

| 控件 | 类型 | 关键属性 | 绑定配置 | 证据 |
|---|---|---|---|---|
| `lstAlphaColor` | `TColorBox` | 颜色下拉框 | `AlphaColor` | `frmConfig.dfm:3226-3237`；`frmALTRun.pas:333、402` |
| `seAlpha` | `TRxSpinEdit` | `Decimal=0`，`MaxValue=255`（未设 `MinValue`） | `Alpha` | `frmConfig.dfm:3251-3259`；`frmALTRun.pas:334、405` |
| `seRoundBorderRadius` | `TRxSpinEdit` | `Decimal=0`，`MaxValue=200`（未设 `MinValue`） | `RoundBorderRadius` | `frmConfig.dfm:3260-3269`；`frmALTRun.pas:335、408` |
| `seFormWidth` | `TRxSpinEdit` | `Decimal=0`，`MinValue=200`，`MaxValue=800`，设计期 `Value=400` | `FormWidth` | `frmConfig.dfm:3270-3281`；`frmALTRun.pas:336、411` |
| `lblFormAlphaColor` / `lblFormAlpha` / `lblRoundBorderRadius` / `lblFormWidth` | `TLabel` | 各字段标题（语言键 `LblFormAlphaColor`/`LblFormAlpha`/`LblRoundBorderRadius`/`LblFormWidth`） | — | `frmConfig.dfm:3177-3225` |
| `lblAlphaHint` | `TLabel` | 红字："修改主界面设置需要重新启动本软件以生效"（语言键 `LblAlphaHint`） | — | `frmConfig.dfm:3199-3211`；`frmConfig.pas:495` |
| `lblBackGroundImage` + `edtClientDBFileBGFileName` | `TLabel` + `TFilenameEdit` | **两者 `Visible=False`**（皮肤图片文件名不可编辑） | （原拟绑定 `BGFileName`，已注释） | `frmConfig.dfm:3191-3198、3238-3250`；`frmALTRun.pas:416-427` |

三个 SpinEdit 都注册了 `OnKeyDown`：回车=`mrOk`、ESC=`mrCancel`（`frmConfig.pas:661-686`）。

### 3.7 Tab 4 `tsLang`（语言）

| 控件 | 类型 | 关键属性 | 绑定配置 | 证据 |
|---|---|---|---|---|
| `cbbLang` | `TComboBox` | `Style=csDropDownList` | `Lang` | `frmConfig.dfm:3309-3321`；`frmALTRun.pas:340-361、413-414` |
| `lblLanguage` | `TLabel` | "语言"（`LblLanguage`） | — | `frmConfig.dfm:3289-3295` |
| `lblLanguageHint` | `TLabel` | 红字："修改语言需要重新启动本软件以生效"（`LblLanguageHint`） | — | `frmConfig.dfm:3296-3308` |

下拉项来源（打开设置窗时构建，`frmALTRun.pas:338-361`）：
1. 先加入 `DEFAULT_LANG`（`EN`）并选中第 0 项；
2. `GetLangList` 扫描 `<exe目录>` 下文件名含 `lang` 的文件，去掉末尾 5 个字符（`.lang`）作为显示名，逐个加入（去重）；
3. 在列表中找到与 `Lang` 相等的项并选中。
注意：`cbbLang.OnChange` 在 DFM 里错误地指向 `cbbListFormatChange`（`frmConfig.dfm:3320`），切换语言会顺带刷新列表格式示例（无副作用但属缺陷）。

### 3.8 隐藏 Tab `tsStyle`

- 运行时 `tsStyle.TabVisible := False`（`frmConfig.pas:441`），用户永远看不到；
- 内容是一个 420×250 的"假主窗"面板 `pnlDemo`（含背景图、标题标签、输入框、结果列表、提示、命令行、三个模拟按钮）；
- 本来由 `RefreshDemo` 把当前配置套到演示控件上（字体/背景/按钮显隐/命令行高度，`frmConfig.pas:553-622`），但**唯一调用点被注释**（`:504`）；
- `RefreshOperationHint`（`:624-659`）也只在 `edtShortCutChange`（`:423`）里被调用，而 `edtShortCutChange` 只在 `RefreshDemo` 中被调（`:621`）→ 整条链是死代码。

### 3.9 按钮与模态结果流程

| 按钮 | 结果 | 代码 | 说明 |
|---|---|---|---|
| `btnOK` | `mrOk` | `frmConfig.dfm:22-50`（`ModalResult=1`、`Default=True`） | 回写全部值 → 应用 → 保存 → 必要时重启 |
| `btnCancel` | `mrCancel` | `frmConfig.dfm:51-59`（`Kind=bkCancel`） | 不保存；重新注册旧热键后退出 |
| `btnReset` | `mrRetry` | `frmConfig.pas:184-191` | 先弹确认框（`ResetAllConfig`），确认后把 `ModalResult` 设为 `mrRetry`（Delphi 的 `mrRetry` 值=4，被调用方特判） |

调用方 `actConfigExecute` 的分支（`frmALTRun.pas:253-546`）：

```
hkm1.ClearHotKeys(); hkm2.ClearHotKeys()          // 编辑期间注销热键，避免冲突
ConfigForm := TConfigForm.Create(Self)
从全局变量把 20 个复选框/热键/字体/列表格式/颜色/Alpha/圆角/宽度/语言灌入 UI
ShowModal
case ModalResult of
  mrOk:    回写全局变量（HotKeyStr1/2、20 个布尔、3 个字体串、ListFormat、AlphaColor/Alpha/圆角/宽度/Lang）
           IsNeedRestart := 任一(AlphaColor/Alpha/圆角/宽度/Lang) 变化
  mrRetry: DeleteFile(ALTRun.ini); LoadSettings; IsNeedRestart := True
  else:    ApplyHotKey1; ApplyHotKey2; Exit        // Cancel：只恢复热键，不保存
end
SetAutoRunInStartUp(...); AddMeToSendTo(...)        // 系统级集成
把字体套到主窗三个控件；ApplyHotKey1/2            // 立即生效
ShowSkin→加载/清空背景图；ShowTrayIcon→托盘图标；三按钮显隐与标题栏布局重排
SetWindowLong(WS_EX_LAYERED) + SetLayeredWindowAttributes(AlphaColor, Alpha, ...)
SetWindowRgn(CreateRoundRectRgn(..., RoundBorderRadius, ...))
SetActiveLanguage                                   // 立即重读语言资源（已建控件不重译）
edtShortCutChange                                   // 刷新列表（数字编号顺序等）
按 ShowCommandLine 设主窗高度 250/230
SaveSettings                                        // 写盘
if IsNeedRestart then 提示(resRestartMeInfo) + RestartMe   // 自动重启
```

### 3.10 控件联动、只读、隐藏、启用/禁用

| 现象 | 结论 | 证据 |
|---|---|---|
| 复选框联动描述框 | 点击任一项 → `mmoDesc.Text := ConfigDescList[ItemIndex]` | `frmConfig.pas:233-236` |
| 复选框之间启用/禁用 | **完全没有**。例如"数字序列顺序"（索引 5）在语义上依赖"允许数字快捷键"（索引 4），但 UI 不置灰 | `frmConfig.pas:434-505` 无 Enabled 逻辑 |
| 热键编辑框 | ReadOnly（只能按键捕获，不能手动输入） | `frmConfig.dfm:220、239` |
| 热键修饰键 | 4 个复选框，无互斥限制 | `frmConfig.dfm:166-283` |
| 皮肤图片文件名 | 标签+编辑框隐藏，不可修改 | `frmConfig.dfm:3197、3249` |
| 列表格式下拉框 | DropDownList（不可输入）；只含 3 个预设 + INI 里既有的自定义值 | `frmConfig.dfm:429`；`frmALTRun.pas:324-331` |
| 语言下拉框 | DropDownList，内容 = `EN` + exe 目录下 `*.lang` | `frmALTRun.pas:338-361` |
| 三个 SpinEdit 回车/ESC | 直接确定/取消对话框 | `frmConfig.pas:661-686` |
| 重置按钮 | 会二次确认 | `frmConfig.pas:186-188` |
| 重置字体按钮 | 会二次确认 | `frmConfig.pas:195-197` |

### 3.11 即时生效 vs 保存生效（逐项）

| 配置组 | 点 OK 后 | 是否触发重启 |
|---|---|---|
| 20 个复选项（`AutoRun`/`AddToSendTo`/`Regex`/`MatchAnywhere`/`NumberKey`/`IndexFrom0to9`/`RememberFavouratMatch`/`ShowOperationHint`/`ShowCommandLine`/`ShowStartNotification`/`ShowTopTen`/`PlayPopupNotify`/`ExitWhenExecute`/`ShowSkin`/`ShowMeWhenStart`/`ShowTrayIcon`/三个按钮显隐/`ExecuteIfOnlyOne`） | 立即（改全局变量 + 立即重排 UI/托盘/皮肤/高度），并立即写盘 | 否 |
| `AutoRun` / `AddToSendTo` | 立即执行系统集成（建/删启动快捷方式、发送到快捷方式） | 否 |
| 三个字体 | 立即套到主窗控件 | 否 |
| `ListFormat` | 立即刷新列表文本 | 否 |
| `HotKey1` / `HotKey2` | 立即重新注册 | 否 |
| `AlphaColor` / `Alpha` / `RoundBorderRadius` / `FormWidth` | 先立即应用分层/圆角；**只要其中任一项与打开设置窗时不同**，保存后弹 `resRestartMeInfo` 并 `RestartMe` | 任一项变化 → **是** |
| `Lang` | 立即 `SetActiveLanguage` 重读资源，但主窗菜单/按钮文案在 FormCreate 时已固定 → 实际要重启；代码在语言变化时强制重启 | 语言变化 → **是** |
| `BGFileName` | 设置界面不可改；改 INI 后重启生效（启动时加载） | 手动改 INI 后需重启 |

> 注意：只要 4 项外观 + 语言中**任意一项**与打开设置窗时不同，就会重启（`frmALTRun.pas:401-414`）。字体改动不会触发重启。这与界面红字提示"需要重新启动以生效"一致，但实现上不是"下次启动生效"，而是**保存后立刻自动重启**。

---

## 4. 热键设置交互

### 4.1 三个热键的角色

| 热键 | 变量 | 默认值 | 注册对象 | 行为 |
|---|---|---|---|---|
| 主热键 | `HotKeyStr1` | `Alt + R`（`untALTRunOption.pas:746`） | `hkmHotkey1`（`frmALTRun.pas:46`） | 唤起/隐藏主窗（`hkmHotkeyHotKeyPressed`，`:2411-2440`） |
| 辅热键 | `HotKeyStr2` | `Pause`（`:747`） | `hkmHotkey2`（`:80`） | 同主热键 |
| 最近项热键 | `LastItemHotKeyStr` | `ALT+L`（`:748`） | `hkmHotkey3`（`:83`、`:2081`） | 直接执行"最近一次使用"的快捷项（`hkmHotkey3HotKeyPressed`，`:2380-2409`） |

### 4.2 内部数值编码与文本格式

- 热键在内存中是一个 `Cardinal`：低字节=虚拟键码 VK，高位字节为修饰键位（`HotKeyManager.pas:131-181`）：
  `Shift=0x2000`、`Ctrl=0x4000`、`Alt=0x8000`、`Win=0x10000`（来自 `VK2_SHIFT=32` 左移 8 位等）。
- 文本形式：`修饰键 + 修饰键 + ... + 主键`，用 ` + `（加号前后有空格）连接；生成顺序固定为 **Win、Alt、Ctrl、Shift**（`frmConfig.pas:507-529`）。
- 解析（`TextToHotKey`，`HotKeyManager.pas:331-451`）：
  - 按 `+` 切分，`Trim` 每段；**大小写不敏感**；
  - 修饰键同时接受英文 `Shift/Ctrl/Alt/Win` 和本地化名（`LocalModName_*`，由 `GetKeyNameText` 取得）；
  - 主键支持：单字符（`0-9`、`A-Z` 直接取 ASCII 作为 VK）、`Pause`、`NumLock` 特判（`:420-430`）、以及 `$08..$FF` 范围内用 `HotKeyToText` 暴力比对的名称（含 `F1-F24`、方向键、媒体键等，`:368-381`）；
  - 任一修饰键无法识别 → 返回 0；主键无法识别 → 返回 0。
- 显示（`HotKeyToText`，`HotKeyManager.pas:202-329`）：按键名来自 `GetKeyNameText`，因此**是本地化名称**（`LOCALIZED_KEYNAMES=True`，`untALTRunOption.pas:40`）；特判 `Pause`/`NumLock` 返回英文；媒体键在长度 ≤1 时用内部英文名。

### 4.3 用户如何捕获按键

1. `lbledtHotKey1` 是只读 `TLabeledEdit`，`OnKeyDown = lbledtHotKey1KeyDown`（`frmConfig.dfm:223`）。
2. 处理：`lbledtHotKey1.Text := HotKeyToText(Key, LOCALIZED_KEYNAMES)`（`frmConfig.pas:531-535`）——**只取当前按下的这一个键**，不区分/不累积修饰键状态；修饰键由 4 个复选框决定。
3. 辅热键处理多一个分支：`Key=VK_DELETE` 时清空 4 个复选框并把文本设为 `HotKeyToText(0)`（空串），实现"禁用辅热键"（`frmConfig.pas:537-551`）；主热键**没有** DELETE 清除逻辑。
4. 提交时 `GetHotKey1/2` 把"复选框 + 文本"拼成热键字符串（`frmConfig.pas:507-529`），默认顺序固定 Win→Alt→Ctrl→Shift，再交给调用方回写全局变量。
5. 打开设置窗时 `DisplayHotKey1/2` 反向解析：按 `+` 切分，前 n-1 段匹配修饰键并勾选复选框，最后一段填进编辑框（`frmConfig.pas:275-366`）。
   - `DisplayHotKey1` 用 `TStringList.DelimitedText` 切分（`:297-298`），源码注释明确说这种写法对 `Scroll Lock` 这类**含空格的键名**会只显示 `Lock`（`:343-348` 的注释）；`DisplayHotKey2` 已改用自写 `SplitString`（`:341`）。→ 主热键含空格键名时存在显示/回写截断风险（源码注释已证实该行为，属**不确定边界**：未实测具体 Delphi 版本的 `DelimitedText` 规则）。
   - 文本为空（`Trim=''`）时编辑框显示 `resVoidHotKey`（`:288-292、332-336`）；EN/简体中文语言文件中该值为**空串**（`EN.lang:96`、`简体中文.lang:96`），源码兜底默认是 `NONE`（`untALTRunOption.pas:999`）。
6. 设置窗打开期间两个热键被注销（`frmALTRun.pas:265-266`），关闭后：OK 用新值重注册、Cancel 用旧值重注册（`:443-445、462-463`）。

### 4.4 冲突检测与失败处理

| 场景 | 行为 | 证据 |
|---|---|---|
| 与其它程序冲突 / 系统占用 | `RegisterHotKey` 失败 → `AddHotKey` 返回 0 → 调用方报错 | `HotKeyManager.pas:489-512`；`frmALTRun.pas:1186-1231` |
| 主热键非法/冲突 | 弹 `resHotKeyError`（"热键不可为 %s"），**不注册**，但字符串已写入全局变量并会被保存（下次启动继续报错） | `frmALTRun.pas:1194-1202、833` |
| 辅热键非法/冲突 | 仅当 `HotKeyStr2` 非空且不等于 `resVoidHotKey` 时弹告警，然后**清空** `HotKeyStr2`（自动禁用） | `frmALTRun.pas:1215-1228` |
| 主/辅热键完全相同 | 主先注册成功，辅注册失败 → 走辅热键失败路径并清空 | 同上（无应用内查重） |
| `LastItemHotKey` 非法 | `hkmHotkey3.AddHotKey(...)` 的返回值**被忽略**，静默不生效、无提示 | `frmALTRun.pas:2081` |
| 系统级"可用性预检" | `HotKeyAvailable` / `HotKeyValid` 存在于库中（`HotKeyManager.pas:97、183、568`）但**全仓库无调用** | grep 无命中 |
| 注册实现 | 用 `GlobalAddAtom('HotKeyManagerHotKey'+十进制热键值)` 作 id，在隐藏窗口上 `RegisterHotKey`，`WM_HOTKEY` 回到 `HookProc` 再回调 `OnHotKeyPressed` | `HotKeyManager.pas:489-512、614-655` |

### 4.5 默认值与非直观点

- **主热键默认 `Alt + R`**（`untALTRunOption.pas:746`），这是 ALT 组合；**辅热键默认 `Pause`**（`:747`）是**无修饰的单键**，独占性很强，容易与游戏/输入法冲突——重写时是否保留此默认需产品确认（源码事实如此）。
- `LastItemHotKey` 默认写法是 `ALT+L`（大写、无空格），解析大小写不敏感，等价于 `Alt + L`（`:748`、`HotKeyManager.pas:346-357`）。
- 热键字符串持久化后**带本地化键名**（如中文系统下 `Pause` 按键名可能是"暂停"？——但 `Pause` 被特判为英文 `Pause`（`HotKeyManager.pas:319-320`），`NumLock` 同理（`:322-323`）；其它键名由 `GetKeyNameText` 给出，随系统键盘布局变化）。因此**同一份 INI 换到不同语言的 Windows 上可能无法解析热键**（例如 `Alt + R` 通常稳定，但功能键/特殊键名可能变化）。
- `Alt + R` 中的 `R`：`TextToHotKey` 对单字符直接 `Ord('R')=$52`，不依赖键盘布局。
- 辅热键禁用值有两套判定：空串 `''`，或等于 `resVoidHotKey`（语言文件里通常被本地化为空串，源码默认 `NONE`）（`frmALTRun.pas:2316-2319`）。

---

## 5. GUI 外观配置

### 5.1 字体字符串编码格式

存储格式（`FontToStr`，`untALTRunOption.pas:703-708`）：

```
<字体名>|<样式集合>|<字号>|<颜色>|<字符集>
例：宋体|[]|12|65535|0
```

| 段 | 含义 | 类型/取值 | 编码细节 | 证据 |
|---|---|---|---|---|
| 1 | 字体名称（`TFont.Name`） | 字符串 | 本地化字体名，如 `宋体`；ANSI 代码页存储 | `untALTRunOption.pas:706` |
| 2 | 样式集合（`TFont.Style`） | `[...]` 包起来的逗号分隔集合 | 可能含 `fsBold`、`fsItalic`、`fsUnderline`、`fsStrikeOut`，顺序固定为 Bold→Italic→Underline→StrikeOut；空集为 `[]`；例 `[fsBold,fsItalic]` | `untALTRunOption.pas:676-691` |
| 3 | 字号（`TFont.Size`） | 整数（磅 pt） | 直接 `StrToInt`；默认 12 | `:707` |
| 4 | 颜色（`TFont.Color`） | 十进制整数（Delphi `TColor`） | `TColor = $00BBGGRR`：低字节=红……`65535=$00FFFF`=黄、`255=$0000FF`=红、`8388608=$800000`=深蓝（clNavy） | `:707` |
| 5 | 字符集（`TFont.Charset`） | 字节整数 | `0`=ANSI_CHARSET；默认配置均为 0（宋体在简中环境用 ANSI_CHARSET 正常工作） | `:707` |

解析规则（`StrToFont`，`untALTRunOption.pas:710-733`）：
- `SplitString(str,'|',List)`（`untUtilities.pas:261-278`，纯按 `|` 切分、保留空段）；
- **要求段数 ≥5**，否则返回 `False`（不抛异常，字体保持原样）；
- 依次赋 `Name`、`Style`、`Size`、`Color`、`Charset`；
- 数字段用 `StrToInt`，**非法数字会抛 `EConvertError` 且未被捕获**（`StrToFont` 只有 `try/finally` 没有 `except`）→ 形如 `宋体|[]|abc|0|0` 的 INI 会导致应用启动/显示窗体时异常（§7.1）。
- 样式段用 `Pos('fsBold',...)` 之类的**松散包含判断**（`:693-701`），因此 `[fsBoldX]` 也会被识别为加粗。

写入规则：`FontToStr(Font)` 永远输出 5 段；`TitleFont`/`KeywordFont`/`ListFont` 的默认值见 §1.5。
**注意**：界面里修改标题字体后会强制 `Pitch := fpFixed`（`frmConfig.pas:210`），但 `Pitch` 不在字符串格式中，**不会被持久化**，下次打开设置窗即丢失。

### 5.2 三个字体各自作用对象

| 配置键 | 应用对象 | 证据 |
|---|---|---|
| `TitleFont` | 主窗标题标签 `lblShortCut` | `frmALTRun.pas:457、1035`；设置窗示例标签 `lblTitleSample`（`frmConfig.pas:320`） |
| `KeywordFont` | 关键字输入框 `edtShortCut` | `frmALTRun.pas:458、1036`；`lblKeywordSample`（`frmConfig.pas:321`） |
| `ListFont` | 结果列表 `lstShortCut` | `frmALTRun.pas:459、1037`；`lblListSample`（`frmConfig.pas:322`） |

### 5.3 透明度、透明色键、圆角、窗口宽度

| 配置项 | 实现 | 说明 | 证据 |
|---|---|---|---|
| `AlphaColor` | `SetWindowLong(WS_EX_LAYERED)` + `SetLayeredWindowAttributes(Handle, AlphaColor, Alpha, LWA_ALPHA or LWA_COLORKEY)` | `crKey=AlphaColor`：**该颜色像素完全透明**。默认 `clBlack=0`，而主窗 `Color=clMaroon`（`$80`）→ 用默认 INI 时"透明色键"与窗体背景色不一致；样例 INI 用 `128`（clMaroon）才是有效配置 | `frmALTRun.pas:512-516、1086-1093`；`frmALTRun.dfm` 顶层 `Color=clMaroon`、`TransparentColor=True` |
| `Alpha` | 同上 `bAlpha=Alpha` | 整体不透明度 0..255（UI 上限 255）；样例 240；0=完全不可见 | 同上 |
| `RoundBorderRadius` | `SetWindowRgn(Handle, CreateRoundRectRgn(0,0,Width,Height,r,r), True)` | 圆角椭圆宽高均为 r 像素；样例 12；UI 上限 200；0=矩形 | `frmALTRun.pas:518-519、1095-1096` |
| `FormWidth` | `Self.Width := FormWidth` | **只在 `m_IsFirstShow` 且 `WinTop/WinLeft` 均 >0 的分支执行**（`frmALTRun.pas:1022-1032`）；DFM 默认 `ClientWidth=420`，与默认值 420 一致 | `frmALTRun.pas:1031`；`frmALTRun.dfm:7` |

补充：
- 主窗是 `bsNone` 无边框、`fsStayOnTop` 置顶窗（`frmALTRun.dfm`）。
- `ShowCommandLine` 通过窗口高度 250↔230 实现（默认 DFM `ClientHeight=250`）——即"显示/隐藏命令行"本质是改变窗口高度，命令框并未显式 `Visible` 切换（`frmALTRun.pas:1104-1107、527-531`）。
- 背景图不缩放：`imgBackground` 为 `TImage`（`Align=alClient`），DFM 未设 `Stretch`（默认 `False`）、未设 `Center`，图片按原尺寸从左上角绘制（`frmALTRun.dfm`）。

### 5.4 皮肤背景图

| 项 | 结论 | 证据 |
|---|---|---|
| 键 | `[GUI].BGFileName`，默认 `BG.jpg` | `untALTRunOption.pas:788、104` |
| 路径 | 相对 exe 目录（`ExtractFilePath(Application.ExeName) + BGFileName`） | `frmALTRun.pas:1044` |
| 缺文件 | 若文件不存在，把 `imgBackground.Picture`（DFM 内嵌图）**保存**为该文件，再加载 | `frmALTRun.pas:1040-1041` |
| 关闭皮肤 | `ShowSkin=False` 时 `Picture := nil`（释放约 1M 内存，语言文案如此描述） | `frmALTRun.pas:1043-1046`；`ConfigDescList_13` |
| 界面可改 | **不可**（文件名控件隐藏，代码注释） | §3.6 |
| 设置窗演示 | 隐藏 Tab 内还有一份同样的"缺文件则写出"逻辑（死代码） | `frmConfig.pas:561-573` |

### 5.5 列表格式（`ListFormat`）

- 用途：`Format(ListFormat, [ShortCut, Name])` 生成列表项显示文本（`untShortCutMan.pas:1030、1133、1923`）。
- 预设（`ListFormatList`，`untALTRunOption.pas:659-663`）：
  1. `%-25s| %s`（默认；关键字左对齐 25 列 + 竖线 + 名称）
  2. `%s (%s)`
  3. `%s [%s]`
- 设置窗下拉框加入这 3 个预设；若 INI 中的 `ListFormat` 不在预设内，再把它追加为第 4 项并选中（`frmALTRun.pas:324-331`），因此**无法在界面输入全新格式**，只能选。
- 示例实时刷新：`Format(cbbListFormat.Text, ['calc','计算器'])`，格式非法时 `try/except Exit` 静默不更新（`frmConfig.pas:223-231`）。
- 若 INI 里的格式串非法（如 3 个 `%s` 或 `%d`），在主窗过滤列表时 `Format` 抛异常且**无人捕获**（`untShortCutMan.pas:1030` 在 try/finally 内、无 except）→ 可能导致错误对话框/功能失效（§7.5）。

### 5.6 列表列宽（`ManColWidth0..3`）

- 是**快捷项管理窗口**（`frmShortCutMan`）的 4 列宽度，不是主窗结果列表：列依次为 快捷项/名称/参数类型/命令行（`frmShortCutMan.pas:562-565`）。
- 读：加载时若 `ManWinTop/ManWinLeft` 不同时为 0，则应用窗口位置尺寸，并把 4 列宽按 `ManColWidth[i]` 设置（`frmShortCutMan.pas:590-615`）。
- 写：窗口析构时回写 `Top/Left/Width/Height` 和 4 个列宽，然后 `SaveSettings`（`frmShortCutMan.pas:575-588`）。
- 默认全部 100；样例为 100/257/100/543。

---

## 6. 多语言机制

### 6.1 文件发现与命名

- 语言 = `<exe目录>\<Lang>.lang`（`untALTRunOption.pas:901`），`Lang` 来自 `[GUI].Lang`。
- 语言列表 = 目录下**文件名中含 `lang`** 的所有文件，显示名 = 去掉末尾 5 个字符（`.lang`）后的文件名（`frmALTRun.pas:2322-2348`；`untUtilities.pas:910-936` 中 `Ext='lang'` 的包含匹配）。
  - 因此 `EN.lang`→`EN`、`简体中文.lang`→`简体中文`、`PT-BR.lang`→`PT-BR`、`繁体中文.lang`→`繁体中文`。
  - 列表顺序取决于文件系统枚举顺序（未排序），`EN` 永远被强制插到第一项（`frmALTRun.pas:340-341`）。
- 首次运行（INI 不存在）会弹语言选择框；取消则删除 INI 并 `Halt(1)`（`frmALTRun.pas:1883-1932`）。该对话框 `frmLang.pas` 无任何代码，仅一个 ComboBox + 确定按钮。
- 默认值：系统默认语言 ID=2052（简体中文）→ 默认 `简体中文`，否则 `EN`（`untALTRunOption.pas:805-809`）。

### 6.2 文件结构与全部键（8 个节 / 224 个键）

语言文件就是标准 INI（同样用 `TIniFile` 读取），节名由源码常量固定（`untALTRunOption.pas:116-123`），与配置 INI 完全独立。

| 节名 | 键数 | 键名清单（全部） | 源码读取行 |
|---|---|---|---|
| `[Common]` | 27 | `Info`、`About`、`Delete`、`Warning`、`WordOr`、`WordAnd`、`MenuShow`、`MenuShortCut`、`MenuConfig`、`MenuAbout`、`MenuClose`、`BtnOK`、`BtnCancel`、`BtnReset`、`BtnHelp`、`BtnTest`、`BtnAdd`、`BtnEdit`、`BtnDelete`、`BtnPathConvert`、`BtnValidate`、`BtnClose`、`ShortCut`、`Name`、`ParamType`、`CommandLine`、`CanNotExecute` | `:905-934` |
| `[ALTRunForm]` | 49 | `Started`、`PressKeyToShowMe`、`MainHint`、`ShowMeByHotKey`、`NoItemAndAdd`、`HotKeyError`、`CMDLine`、`RunNum`、`KeyToAdd`、`KeyToOpenFolder`、`KeyToRun`、`AutoRunWhenStart`、`AddToSendToMenu`、`RestartMeInfo`、`CleanConfirm`、`RelativePathSuggestion`、`GetFileNameFail`、`ShortCutExisted`、`Congratulations`、`BlankLineAdded`、`BtnShortCutHint`、`BtnConfigHint`、`BtnFAKECloseHint`、`EdtShortCutHint`、`HintList_0`…`HintList_24`（25 个） | `:937-987` |
| `[ConfigForm]` | 76 | `PageConfig`、`PageHotKey`、`PageFont`、`PageForm`、`PageLang`、`ConfigFormCaption`、`ResetAllConfig`、`ResetAllFonts`、`VoidHotKey`、`LblHotKey1`、`LblHotKey2`、`LblTitleFont`、`LblTitleSample`、`LblKeywordFont`、`LblKeywordSample`、`LblListFont`、`LblListSample`、`LblListFormat`、`LblResetFont`、`LblFormatSample`、`LblListFormatSample`、`LblFormAlphaColor`、`LblFormAlpha`、`LblRoundBorderRadius`、`LblFormWidth`、`LblBackGroundImage`、`LblAlphaHint`、`LblLanguage`、`LblLanguageHint`、`LblHotKeyHint`、`LblHotKeyHint1`、`LblHotKeyHint2`、`BtnModifyTitleFont`、`BtnModifyKeywordFont`、`BtnModifyListFont`、`BtnResetAllFonts`、`ConfigList_0`…`ConfigList_19`、`ConfigDescList_0`…`ConfigDescList_19` | `:990-1070` |
| `[InvalidForm]` | 2 | `InvalidFormCaption`、`CommandLineEempty`（源码键名就拼错为 `Eempty`） | `:1073-1074` |
| `[ShortCutManForm]` | 15 | `ShortCutManFormCaption`、`NoInvalidShortCut`、`DeleteBlankLine`、`DeleteConfirm`、`ValidateConfirm`、`PathConvertConfirm`、`ConvertFinished`、`BtnAddHint`、`BtnEditHint`、`BtnDeleteHint`、`BtnPathConvertHint`、`BtnValidateHint`、`BtnHelpHint`、`BtnCloseHint`、`BtnCancelHint` | `:1077-1092` |
| `[ShortCutForm]` | 16 | `ShortCutFormCaption`、`GrpShortCut`、`BtnFile`、`BtnDir`、`GrpParamType`、`LblEdtCommandLineHint`、`ParamType_0`…`ParamType_3`、`SelectDir`、`ParamConfirm`、`ReplaceConfirm`、`ReplaceShortCutConfirm`、`ReplaceNameConfirm`、`NameCanNotInclude` | `:1095-1111` |
| `[AboutForm]` | 1 | `Statistics` | `:1114` |
| `[DefaultShortCutName]` | 38 | `Default_ShortCut_Name_0`…`Default_ShortCut_Name_37`（生成默认 `ShortCutList.txt` 时使用，`untShortCutMan.pas:1401-1466`） | `:1117-1154` |

`EN.lang` / `简体中文.lang` 实测与上表**逐键 1:1**，唯一差异见 §6.6。

### 6.3 解析规则与回退策略

| 规则 | 结论 | 证据 |
|---|---|---|
| 解析器 | 与配置 INI 同一个 `TIniFile`（Win32 Profile API），键名大小写不敏感，值两端空白被裁剪，不支持转义/多行 | `untALTRunOption.pas:896-902` |
| 值中的对齐空格 | 无意义（会被裁掉）；例如 `EN.lang:182` 行末大量空格不会进入值 | `EN.lang:182` |
| 整文件缺失 | `TIniFile` 不报错，所有键返回**源码内硬编码英文默认值**；**不会创建语言文件** | `untALTRunOption.pas:905-1154` |
| 单键缺失 | 回退该 `ReadString` 的第三个参数（英文默认值），不会去英文语言文件里找 | 同上 |
| 值为空 | 合法（如 `VoidHotKey` 就是空串） | `EN.lang:96` |
| 未知键/多余键 | 忽略；不会报错（`EN.lang`/`简体中文.lang` 中 `ALTRunForm.BtnCloseHint` 就是无用键，见 §6.6） | §6.6 |
| 语言文件编码 | ANSI（CP936 简中），无 BOM；UTF-8 中文语言文件会乱码（未做转换） | §1.2 实测 |
| 回退链 | 语言文件 → 源码硬编码默认值；**没有**"中文缺键回退英文语言文件"这一级 | `untALTRunOption.pas:905-1154` |

### 6.4 占位符（`%s` / `%d`）与调用点

值里可含 Delphi `Format` 占位符；**由使用处**做 `Format`，语言文件本身不转义。占位符写错（多/少/类型不符）会在运行时抛异常（未被捕获）。

| 语言键 | 默认值（源码英文兜底） | 占位符语义 | 调用点 |
|---|---|---|---|
| `Common.CanNotExecute` | `Can not execute "%s %s"` | 命令、参数 | `untShortCutMan.pas:352-353` |
| `ALTRunForm.Started` | `%s %s is started,` | 程序名、版本 | `frmALTRun.pas:2146-2147` |
| `ALTRunForm.PressKeyToShowMe` | `You can press %s to show me` | 热键串（主/辅合串） | 同上（与 `Started` 用 `#13#10` 拼接） |
| `ALTRunForm.MainHint` | `%s %s%s[HotKey = %s]` | 程序名、版本、换行、热键串 | `frmALTRun.pas:464、2150` |
| `ALTRunForm.ShowMeByHotKey` | `It's better to show me by press hotkey %s` | 热键串 | `frmALTRun.pas:2562` |
| `ALTRunForm.NoItemAndAdd` | `No such item "%s", add this shortcut?` | 输入的关键字 | `frmALTRun.pas:750` |
| `ALTRunForm.HotKeyError` | `HotKey can not be "%s"` | 非法热键串 | `frmALTRun.pas:1198、1221` |
| `ALTRunForm.RunNum` | `Press ALT+%s or CTRL+%s to run ShortCut` | 数字（两处同一数字） | `frmALTRun.pas:609-610、1180-1181、1662-1663` |
| `ALTRunForm.ShortCutExisted` | `... [%s (%s) = %s]. Replace it?` | 已存在项的 快捷项/名称/命令行 | `untShortCutMan.pas:460-461` |
| `ALTRunForm.Congratulations` | `Congratulations! You have executed %d times!` | 累计执行次数 | `untShortCutMan.pas:2095-2097` |
| `ShortCutManForm.DeleteConfirm` | `Delete %s(%s)?` | 快捷项、名称 | `frmShortCutMan.pas:194` |
| `ShortCutManForm.PathConvertConfirm` | `...?%sYES: ...%sNO: ...` | 两个换行符 `#13#10` | `frmShortCutMan.pas:284` |
| `ShortCutManForm.ConvertFinished` | `Convert finished (Count = %d)` | 转换数量 | `frmShortCutMan.pas:324` |
| `ShortCutForm.ReplaceConfirm` | `Replace "%s" with "%s"?` | 关键字、新值 | `frmShortCut.pas:93、101、127、135` |
| `AboutForm.Statistics` | `You have run shortcut %d times by now` | 累计执行次数 | `frmAbout.pas:49` |

其余 200 余个语言键都不含占位符（纯静态文本）。

### 6.5 切换语言

1. 设置窗 `tsLang.cbbLang` 选项来自 §6.1；OK 时 `Lang := cbbLang.Text`（`frmALTRun.pas:413-414`）。
2. 若 `Lang` 变化 → `IsNeedRestart=True` → 保存后弹 `resRestartMeInfo` 并 `RestartMe`（ShellExecute 自身 + `RESTART_FLAG`，重启后 `Sleep(2000)`）（`frmALTRun.pas:535-540、2620-2626、1953-1956`）。
3. 同一对话框里即使语言没变也会调用一次 `SetActiveLanguage`（`frmALTRun.pas:521-522`）。
4. `SetActiveLanguage` 只重读资源变量（`untALTRunOption.pas:894-1157`），不会重建已存在的窗口文字（主窗菜单/按钮文案/提示在 `FormCreate` 中一次性设置，`frmALTRun.pas:2083-2094`）→ **结论：切换语言实质需要重启**，代码用强制重启保证一致。
5. 若 INI 里的 `Lang` 不在列表内（例如被删掉的语言文件），下拉框会落到第 0 项 `EN`；用户点 OK 后 `Lang` 被规范化为 `EN`（`frmALTRun.pas:340-357`）。

### 6.6 已知缺陷：`BtnFAKECloseHint` 键名不匹配

- 源码读取的键是 `BtnFAKECloseHint`（常量 `KEY_BTNFAKECLOSEHINT`，`untALTRunOption.pas:181`；读取 `:960`，默认 `'Press ESC to hide me'`）。
- 两份语言文件在 `[ALTRunForm]` 节里写的却是 `BtnCloseHint`（`EN.lang:57`、`简体中文.lang:57`），与源码键名**不一致**。
- 结果：中文界面下主窗关闭按钮的提示始终是英文默认值 `Press ESC to hide me`（`frmALTRun.pas:2093`）。`[ShortCutManForm].BtnCloseHint` 是另一个键（`:1091`），工作正常。
- 对比脚本验证：除这一处外，两份语言文件与源码 224 个读取键完全 1:1（无缺、无多余）。

### 6.7 关于页文本 `About.<Lang>`

- 文件名 = `About.` + `Lang`（`frmAbout.pas:51-52`），随包提供 `Bin\About.EN`、`Bin\About.简体中文`。
- 内容是**自由文本**（有 `[Usage]`/`[使用方法]` 等仿 INI 节，但**不是** INI），直接 `TStringList.LoadFromFile` 显示在 Memo 中，不做键值解析。
- 文件缺失时 Memo 显示 DFM 设计期内容/空白，不报错。
- 编码同为 ANSI；中文版文件为 GBK。

---

## 7. 校验与容错

### 7.1 加载期（INI → 内存）

| 情形 | 行为 | 证据 |
|---|---|---|
| 文件不存在 | 全部默认值；`IsRunFirstTime=True` 触发首启向导 | `ALTRun.dpr:42`；`untALTRunOption.pas:735-820` |
| 节/键缺失 | 单键回退默认值 | 同上 |
| 整数非法（如 `Alpha=abc`） | RTL 层 `StrToIntDef` 回退默认值（不崩） | Delphi 2007 RTL 行为；`untALTRunOption.pas:800-817` |
| 布尔非 0/1 | 非 0 视为 True（RTL 行为） | `untALTRunOption.pas:749-770` |
| 字符串为空 | `BGFileName`/三个字体串有"空→默认"兜底；`ListFormat`/`Lang` 无兜底 | `:788-809` |
| 字体串段数 <5 | `StrToFont` 返回 False，字体保持默认（安全） | `:721` |
| 字体串数字段非法 | **抛 `EConvertError`，无捕获** → 启动/显示主窗时异常 | `:725-727`（`StrToFont` 只有 try/finally） |
| 字体串样式段乱写 | 松散识别，可能被误判为加粗等 | `:693-701` |
| `ListFormat` 非法 | 图表文本 `Format` 抛异常（过滤列表时） | `untShortCutMan.pas:1030` |
| `Lang` 指向不存在的文件 | 全英文兜底，不报错、不创建文件 | `untALTRunOption.pas:901-905` |
| 热键文本非法 | 主键告警、辅键告警后清空、最近项热键静默失败 | §4.4 |
| `Alpha` 超界 | 无校验；`SetLayeredWindowAttributes` 的 `bAlpha` 是 BYTE，按低 8 位截断 | `frmALTRun.pas:516` |
| `RoundBorderRadius`/`FormWidth` 越界 | 无校验；直接传给 Win32 / 赋给 `Width` | `frmALTRun.pas:519、1031` |
| `WinTop/WinLeft` ≤0 | 视为未设置→窗体居中 | `frmALTRun.pas:1022-1026` |

### 7.2 编辑期（设置窗）

- 数值型只靠 `TRxSpinEdit` 的 `MinValue/MaxValue` 限制（`Alpha`≤255、圆角≤200、宽度 200..800）；**保存后仍可能被手工改 INI 绕过**。
- 字体用 `TFontDialog`，不会产生非法字符串；但能选到系统不存在的字体名（GDI 会自动替换显示字体）。
- 列表格式用 `csDropDownList`，只能选预设/既有值。
- 热键编辑框只读，只能按键捕获。
- 没有任何跨项一致性校验（如"数字快捷键"未开时"0-9 排序"仍可选）。
- 重置全部配置：删除整个 INI（不保留备份）→ 立即以默认值重建（`frmALTRun.pas:436-441、533`）。

### 7.3 文件级异常

| 情形 | 行为 |
|---|---|
| INI 只读/被占用/目录无写权限 | 所有 `Write*` 静默失败，无提示；本次设置不持久（`SaveSettings` 无错误处理，`untALTRunOption.pas:822-892`） |
| INI 被截断/乱码 | 能解析的键生效，不能解析的用默认；不会自动备份/修复 |
| 进程被强杀 | 本次运行中的 `WinTop/WinLeft/ShortCutRunCount/LatestList` 等修改丢失（写盘只在 §2.2 的时机） |
| INI 版本过旧 | 缺的新键用默认值，下一次 `SaveSettings` 自动补写（因为每次启动都会全量写） |
| 用户手工删掉整个 INI | 等价首次运行：语言选择 + 自启动/发送到询问 + 自动重启一次 |

### 7.4 `frmInvalid` 的用途（与配置校验无关）

`frmInvalid.pas` 是**快捷项列表校验结果窗口**，由快捷项管理器的"校验（Validate）"按钮弹出（`frmShortCutMan.pas:327-394`），不是配置项校验窗口：

- 用途：把 `IsValidCommandLine` 判定为无效的快捷项（`frmShortCutMan.pas:396-462`，主要检查命令行引用的本机路径是否存在）列出来，让用户勾选后批量删除；
- 界面：`TListView` 4 列 = 快捷项/名称/参数类型/命令行（`frmInvalid.pas:56-59`），带复选框；
- 右键菜单：全选、全不选、**测试执行**（用临时 `TShortCutItem` 调 `ShortCutMan.Execute`，`frmInvalid.pas:62-93`）；命令行为空时提示 `CommandLineEempty`；
- 只会删除用户勾选的项，并回写到快捷项管理列表（`frmShortCutMan.pas:384-390`）；
- 文案键在语言文件 `[InvalidForm]`。

### 7.5 缺陷与风险清单（重写时建议修）

| # | 缺陷 | 影响 | 证据 |
|---|---|---|---|
| 1 | INI 无原子写/无备份 | 崩溃时配置可能损坏且不可恢复 | `untALTRunOption.pas:822-892` |
| 2 | 字体串数字非法未捕获 | 启动/显示窗体崩溃风险 | `:725-727` |
| 3 | `ListFormat` 非法未捕获 | 过滤列表时异常 | `untShortCutMan.pas:1030` |
| 4 | `TraceEnable` 死配置 | 用户改 INI 无效 | `untALTRunOption.pas:811/884`；`untLogger.pas:47`；`frmALTRun.pas:2041` |
| 5 | `BtnFAKECloseHint` 键名不匹配 | 中文界面混英文 | §6.6 |
| 6 | `FormWidth` 仅在位置已保存时生效 | 用户清空 WinTop/WinLeft 后宽度设置失效 | `frmALTRun.pas:1022-1032` |
| 7 | `DisplayHotKey1` 空格键名截断风险 | 含空格键名（Scroll Lock 等）显示/回写错误 | `frmConfig.pas:297-298` + 注释 `:343-348` |
| 8 | `AlphaColor` 默认 `clBlack` 与窗体色 `clMaroon` 不一致 | 默认配置下透明色键失效（除非设成 128/窗体色） | `untALTRunOption.pas:100`；`frmALTRun.dfm` `Color=clMaroon` |
| 9 | 主热键注册失败仍被写盘 | 每次启动都告警 | `frmALTRun.pas:371-372、1194-1202、533` |
| 10 | `LastItemHotKey` 注册结果不检查 | 配置错误无感知 | `frmALTRun.pas:2081` |
| 11 | 打开快捷项管理窗口会 `LoadSettings` 覆盖内存状态 | 运行期外部改 INI 会"半即时"生效，行为难预期 | `frmShortCutMan.pas:595` |
| 12 | 首启流程自动重启一次 | 启动体验慢（`ShellExecute` + 2 秒等待） | `frmALTRun.pas:2122-2127、1953-1956` |
| 13 | 每次启动/退出都全量写 INI | 增加损坏窗口；也确实用于补齐新键 | §2.2 |
| 14 | 数值越界不裁剪 | `Alpha`/圆角/宽度异常值直接进 Win32 | §7.1 |

---

## 8. 参数历史（`frmParam.pas` + `ParamHistoryLimit`）

### 8.1 它是什么

当某个快捷项的 `ParamType` 不是"无参数"，且命令行里没有 `{%c}`/`{%wd}`/`{%wt}`/`{%wc}` 等自动参数占位符时，执行该快捷项会弹出参数输入窗口 `ParamForm`（`untShortCutMan.pas:749-781`），`cbbParam` 下拉展示**全局共享的**参数历史，回车确认后按参数类型做编码并执行。

### 8.2 存储位置与格式

| 项 | 结论 | 证据 |
|---|---|---|
| 文件名 | `<exe目录>\ParamHistory.txt` | `frmParam.pas:26、190` |
| 编码 | 文本文件（`TextFile` + `WriteLn`），ANSI CP936，CRLF | `frmParam.pas:340-377` |
| 行格式 | `"<rank 左对齐10列>\|<param 左对齐30列>"`（+格式串末尾多余的 `%`，见 §8.6） | `frmParam.pas:366-367` |
| 字段1 | rank：整数（该参数被使用次数，用于淘汰） | `:308、366` |
| 字段2 | 参数原文（ANSI，可含中文） | `:311、367` |
| 生命周期 | `TParamForm` 在 `ALTRun.dpr:47` 随程序启动自动创建 → `FormCreate` 时加载；`FormDestroy` 时保存 | `frmParam.pas:186-197` |

### 8.3 加载流程（`frmParam.pas:252-326`）

1. 文件不存在 → 创建空文件（`ReWrite`），继续；
2. 取文件修改时间，与内存缓存比较，未变化则跳过重读（`:279-285`）；
3. 逐行 `Trim` 后按 `|` 切分（`SplitString`，`untUtilities.pas:261-278`）；
4. **读到第 `ParamHistoryLimit` 条就停止**（`:305`）；
5. `cnt := StrToInt(Trim(ParamItem[0]))`、`Param := Trim(ParamItem[1])`（`:308-311`）；
6. 若列表中无同名参数则 `AddObject(Param, Pointer(cnt))`（去重，`:314-315`）；
7. 任一步异常 → `except Exit`，已加载部分保留。

### 8.4 交互与淘汰（`frmParam.pas:68-112`）

- 用户敲内容回车 → `btnOKClick`：
  - 若文本已存在：`Objects[Index] := Pointer(rank+1)`（计数+1，`:107`）；
  - 若是新参数：若当前条数 ≥ `ParamHistoryLimit`，**从后向前找 rank 最小的一项删掉**（`:85-100`），然后把新参数 `InsertObject(0, Param, Pointer(0))` 插到第一项（`:103`）；
- 因此下拉列表大致是"最近使用在前"的顺序，rank 用于**LFU 淘汰**；
- 参数窗口还有自动关闭：`HideDelay` 秒无操作 → `mrCancel`（`:328-338、384-388`）；这使 `HideDelay` 同时影响主窗与参数窗。

### 8.5 保存与上限

- `FormDestroy` 调 `SaveParamHistory`：`ReWrite` 整文件，按当前 Items 顺序写出，**最多写 `ParamHistoryLimit` 行**（`frmParam.pas:340-377`）。
- 上限语义：`ParamHistoryLimit` 同时是"加载上限"、"内存条数上限"、"写盘行数上限"；**没有任何下限/上限校验**。
- 推导行为：
  - `= 0`：加载时 `Count(0) >= 0` 立即退出（不读任何行）；新增时 `Count >= 0` 立即删一项（空列表无操作）再插 1 项；保存时 `MaxCnt=0` → 写出空文件。**即历史被清空且不再积累**。
  - `< 0`：加载立即退出；新增时 `Count >= 负数` 恒真 → 对空列表 `Delete(0)` 会抛异常（未捕获，`frmParam.pas:99`）→ 可能崩溃；保存时 `MaxCnt` 为负 → 循环不执行 → 清空文件。**建议重写时把该值钳制为 ≥1 或明确 0=禁用**。
- 保存时机：仅在进程正常退出（`FormDestroy`）时；强杀会丢失本次会话新增的历史。

### 8.6 缺陷与不确定项

1. **写入格式串末尾多一个 `%`**：`Format('%-10d|%-30s%', [...])`（`frmParam.pas:366-367`）。Delphi 的 `Format` 对孤立 `%` 的行为**不确定**：若视为非法格式，会抛 `EConvertError` → 被外层 `except Exit` 吞掉 → **`ParamHistory.txt` 永远写不出内容**（只被清空）。同理 `FavoriteList.txt` 的保存格式串也有同样的多余 `%`（`untShortCutMan.pas:1778-1779`），可能导致收藏映射也写不出。→ **重写/迁移时必须先用旧程序实测生成的 `ParamHistory.txt` 字节内容确认**，不要假设文件格式。
2. `LoadParamHistory` 中 `ParamItem` 在 `try` 内、`Reset` 之后才 `Create`，若 `Reset` 抛异常，`finally` 会对未初始化指针 `Free`（`frmParam.pas:288-322`）→ 潜在访问违例（极少数文件被占用场景）。
3. 行解析要求至少 2 段；若某行没有 `|`，`ParamItem[1]` 越界 → 异常 → `except Exit`（部分加载）。
4. 历史是全局共享，不区分快捷项；同一参数在 A 项用过，B 项也能看到。
5. 文件路径与编码与主 INI 一致（exe 目录 + ANSI）。

---

## 9. 重写注意点（C++）

### 9.1 内部数据模型：强类型 `Config` 结构体 + 独立的编解码层

建议把"INI 文本 ↔ 内存配置"与"设置界面 ↔ 内存配置"彻底解耦，形成：

```
struct FontSpec { std::wstring name; uint32_t styleBits; int sizePt; COLORREF color; BYTE charset; };
struct HotkeySpec { UINT mods; UINT vk; };            // 内部规范形式
struct WindowPlacement { int top, left, width, height; };

struct Config {
  // [Config]
  HotkeySpec hotkey1{ MOD_ALT, 'R' }, hotkey2{ 0, VK_PAUSE }, lastItemHotkey{ MOD_ALT, 'L' };
  bool autoRun=true, addToSendTo=true, regex=true, matchAnywhere=true, numberKey=true;
  bool indexFrom0to9=false, rememberFavouratMatch=false, showOperationHint=true;
  bool showCommandLine=true, showStartNotification=true, showTopTen=true, playPopupNotify=true;
  bool exitWhenExecute=false, showSkin=true, showMeWhenStart=false, showTrayIcon=true;
  bool showShortCutButton=true, showConfigButton=false, showCloseButton=true, executeIfOnlyOne=false;
  int hideDelay=15, paramHistoryLimit=50;
  // [Window]
  int winTop=0, winLeft=0; int manWinTop=0, manWinLeft=0, manWinWidth=450, manWinHeight=240;
  std::array<int,4> manColWidth{100,100,100,100};
  // [GUI]
  std::wstring bgFileName=L"BG.jpg";
  FontSpec titleFont{L"宋体",0,12,0x00FFFF,0}, keywordFont{...0xFF...}, listFont{...0x800000...};
  std::wstring listFormat=L"%-25s| %s";
  COLORREF alphaColor=0; int alpha=240, roundBorderRadius=12, formWidth=420;
  std::wstring lang;                     // 语言文件基名（保持旧文件名语义）
  // [DEBUG]
  int shortCutRunCount=0; unsigned long long handleId=0; std::wstring latestList;
};
```

要点：
- **保留键名与默认值的一一映射**（第 1 节表格可直接当代码注释/单元测试基准）；
- 布尔/整数/字符串的读取容错按 §7.1 复制（非 0 即真、非法整数回退默认、字体段数不足返回默认）；
- 热键内部用 `{mods,vk}`，读写时再与本地化文本互转；**不要**把文本当唯一真相（避免系统语言变化时丢配置）。

### 9.2 持久格式：短期继续 INI，长期 JSON（保留 INI 导入/导出）

| 方案 | 优点 | 风险 | 结论 |
|---|---|---|---|
| 继续 INI（ANSI，Win32 Profile API 或自写解析器） | 与旧版本 100% 互换；用户可直接编辑；迁移零成本 | 无类型、无嵌套、无注释保留保证、ANSI 中文编码脆弱 | **v1 必须支持读写旧 INI** |
| TOML | 可读性好、有类型 | 需引入库；旧键名/大小写/ANSI 映射工作一样要做；C++ 侧库生态不如 JSON | 可选 |
| JSON（UTF-8 无 BOM） | 结构化、易扩展、跨平台、C++ 库成熟 | 用户手改体验差；需版本字段与迁移逻辑 | **推荐作为 v2 原生格式** |

推荐落地方式：
1. **内部只认 `Config` 结构体**；实现两个编解码器：`LegacyIniCodec`（必做）与 `JsonCodec`（v2）。
2. 启动时探测顺序：`ALTRun.json` 存在 → 读 JSON；否则读 `ALTRun.ini` → 内存 → 若允许则把 JSON 写出。**永远不要在没有迁移成功的情况下删除旧 INI**。
3. 若要求"旧版本仍能读新版本配置"，则 v1 阶段继续**只写** `ALTRun.ini`（同时可写 JSON 作镜像），并保留旧键名、旧字体/颜色/热键文本格式。
4. 建议 v1 不做"启动即全量写盘"（旧行为），改为：**只在配置变更或正常退出时写**；写入前做 `.ini.bak` 备份（行为改进，需产品确认是否要 1:1 保留旧行为）。

### 9.3 对旧 INI 的向后兼容规则（逐条）

1. **路径与文件名**：`<exe目录>\ALTRun.ini`、`<Lang>.lang`、`About.<Lang>`、`ParamHistory.txt`、`ShortCutList.txt`、`FavoriteList.txt`、`BG.jpg`、`Popup.wav` 保持同名同目录。
2. **编码**：读取时按字节读入，先尝试 UTF-8 BOM/UTF-8 校验（新文件），失败回退 CP936/CP_ACP（旧文件）；写回旧格式时用 CP936（或 CP_ACP），不带 BOM。语言文件同理；语言文件**基名**就是显示名（GBK 字节），新实现内部转 UTF-16 后仍需能用原文件名拼路径。
3. **键名大小写**：Windows INI 大小写不敏感 → 自写解析器也要大小写不敏感比较，但**保留原文件中的键名大小写**（不要因为重写而改大小写）。
4. **未知键/未知节**：必须原样保留（旧程序靠 Profile API 天然保留；自写解析器要显式保留未知行）。至少不能删除旧版遗留的 `HotKey`、`ModifyTime` 等键，否则降级回旧版会丢配置。
5. **默认值**：完全照抄 §1 表格（尤其 `HotKey1=Alt + R`、`HotKey2=Pause`、`Alpha=240`、`AlphaColor=0`、`Lang` 依系统 2052 判定）。
6. **写入范围**：只写自己负责的键；`TraceEnable` 保持"只读不写"（或按新产品决定），避免把用户的日志开关改掉。
7. **删除/迁移**：旧程序会删 `[GUI]` 的 `TitleColor/KeywordColor/ListColor`；新实现建议**保留**这些旧键（或同样删除，但要写入迁移日志），避免用户降级时读到更旧值。
8. **文本编码格式**：
   - 字体串严格保持 5 段格式（§5.1），解析时用与 `StrToFont` 相同的宽容度；
   - 热键串保持 `修饰键 + 键` 的本地化文本（§4.2）。若新实现改用 VK 数值，则**导出旧格式时必须能生成旧程序可解析的文本**：可用 `GetKeyNameText` 的同一策略，或内置一张 VK→旧英文名/本地化名的映射表；
   - `ListFormat` 保持 Delphi `Format` 语义（`%s`/`%d`/`%%`），或至少能识别 3 个预设与用户自定义格式（若换 C++ 格式化，需实现兼容子集）。
9. **历史键名遗留**：`BtnFAKECloseHint` 这类"源码键名与语言文件键名不一致"的问题，新实现应**两个键都认**（先读规范名，读不到再读旧名），并修语言文件；否则中文界面会继续显示英文。
10. **数值截断**：`Alpha` 按 0..255 钳制、宽度/圆角按 UI 范围钳制（旧程序不钳制，属于可改进项；但若要 1:1 行为，至少记录差异）。
11. **热键文本跨语言**：建议新实现把 `[Config]` 热键写成**带标准英文名的规范文本**（`Alt + R`），并把"本地化显示名"仅放在 UI 层；读旧值时要能解析旧的本地位名（可复用 Win32 `GetKeyNameText` 扫描，或维护别名表）。
12. **参数历史文件**：先实测旧程序生成的 `ParamHistory.txt` 实际字节（§8.6 的第 1 条不确定项），再写导入器；新格式建议 JSON 数组 `[{rank,param}]`，导入后保留淘汰算法（LFU）与上限语义。

### 9.4 行为对齐清单（建议写成回归测试）

| 场景 | 期望行为（旧程序） | 测试方式 |
|---|---|---|
| 空目录首次启动 | 弹语言选择；询问自启动/发送到；写 INI；重启一次 | 清空目录启动，检查 INI 与进程重启 |
| 只有 `[Config]` 节的旧 INI | 其余键取默认；启动后全量补齐 | 用最小 INI 启动后对比键集 |
| `HotKey1` 非法 | 告警，热键不生效，值仍写盘 | 构造非法热键启动 |
| `HotKey2` 为空 | 不告警、不注册 | 检查无告警且热键不生效 |
| 字体串段数不足 | 使用默认字体，不崩 | 构造 `宋体\|[]` |
| `Alpha`/圆角/宽度改动 | 保存后提示重启并自动重启 | 观察进程重启与 INI |
| 语言切换 | 保存 → 提示 → 重启 → 新语言生效 | 切换 简体中文/EN |
| 参数历史新增/复用/满额淘汰 | 见 §8.4 | 构造满额历史后输入新参数 |
| 关闭后复开 | 窗口位置/尺寸/列宽恢复 | 移动主窗与管理窗后重启 |
| 最近使用列表（最多 10 项） | 关闭写 `LatestList=…`，重启后 ALT+L 行为一致 | 执行多次后检查 |

### 9.5 其它实现建议

- **集中默认值表**：用一张 `KeySpec{name, section, type, default, validator, apply}` 表驱动读取/写入/设置窗口生成，避免三处（读、写、UI）漂移。
- **设置窗口生成**：旧窗口 20 个复选框硬编码索引；新实现建议用数据驱动列表（第 3.3 节表格），但保持"索引 0..19 ↔ 键"顺序以兼容语言键 `ConfigList_i/ConfigDescList_i`。
- **热键捕获**：旧实现"只取一个键 + 4 个复选框"；建议改为标准组合键捕获控件（按下即记录 mods+vk），但 UI 上仍需允许单键热键（旧默认 `Pause`）。
- **线程安全**：`ShortCutRunCount` 旧代码在多线程执行路径里 `Inc`（`untShortCutMan.pas:2092`），新实现用原子变量。
- **错误可见性**：旧程序大量 `except Exit` 静默吞错；新实现应至少写日志（但日志默认关闭需与旧行为一致）。
- **不要照抄**：`frmConfig` 隐藏 Style Tab / `RefreshDemo` 死代码、`DisplayHotKey1` 的 DelimitedText 缺陷、`TraceEnable` 死配置、`BtnFAKECloseHint` 键名缺陷、`FormWidth` 应用条件缺陷。这些应在重写时修正并记录为"行为变更"。

---

## 10. 不确定项清单（未实测 / 无法仅凭源码确定）

| # | 不确定点 | 现状与影响 |
|---|---|---|
| 1 | Delphi `Format` 对孤立尾部 `%` 的处理 | 决定 `ParamHistory.txt` / `FavoriteList.txt` 是否真能写出内容（`frmParam.pas:366`、`untShortCutMan.pas:1778`）。需运行旧程序实测生成文件 |
| 2 | `TStringList.DelimitedText` 对含空格键名的切分规则（Delphi 2007 具体实现） | 决定主热键 `Scroll Lock` 等是否被截断（`frmConfig.pas:297-298`；源码注释已说明会发生，但未实测） |
| 3 | Win32 Profile API 写键时是否保留注释与键顺序 | 影响"旧 INI 经旧程序保存后格式变化"的兼容策略；样例 INI 无注释，无法验证 |
| 4 | 首次运行 `LoadSettings` 的 `DeleteKey` 是否会在 INI 不存在时创建文件 | 不影响结果（随后 `SaveSettings` 必然创建），但影响首启时序细节 |
| 5 | `untALTRunOption.TraceEnable` 与 `untLogger.TraceEnable` 同名全局的最终解析 | 结论"配置不生效"是确定的（`InitLogger` 覆盖日志器变量），但若编译器/作用域解析使读取写入的是日志器变量，结论需复核；建议重写时彻底移除该键或改为真正生效 |
| 6 | 语言文件重名键、重复节的行为 | 现有 4 份语言文件均无重复；未验证 Profile API 对重复键"先/后生效" |
| 7 | `BDS`/`DPROJ` 精确编译器版本 | README 声明 Delphi 2007；存在 `.bdsproj` 与 `.dproj` 双工程文件，未能 100% 确认实际编译用的 IDE 版本（不影响 ANSI 结论） |
| 8 | 旧程序在 UTF-16/BOM INI 下的读写行为 | 未测试；C++ 重写需自行定义 |
| 9 | 样例 INI 的 `HotKey2=     ` 是否真会在首次启动弹告警 | 由代码路径推导（`ApplyHotKey2`），未实际运行验证 |

---

## 附录 A：关键调用点速查

| 主题 | 位置 |
|---|---|
| 配置读写实现 | `untALTRunOption.pas:735-820`（Load）、`:822-892`（Save） |
| 键名常量 | `untALTRunOption.pas:42-111` |
| 默认常量 | `untALTRunOption.pas:96-105` |
| 字体编解码 | `untALTRunOption.pas:676-733` |
| 语言资源 224 键 | `untALTRunOption.pas:905-1154` |
| 首次运行/启动流程 | `ALTRun.dpr:40-55`；`frmALTRun.pas:1855-2156` |
| 设置窗口调用与应用 | `frmALTRun.pas:253-546` |
| 设置窗口 UI 装配 | `frmConfig.pas:434-505` |
| 热键显示/捕获/回写 | `frmConfig.pas:275-366、507-551` |
| 热键注册与冲突 | `frmALTRun.pas:1186-1231`；`HotKeyManager.pas:131-181、331-512、614-655` |
| 分层/圆角/宽度 | `frmALTRun.pas:512-519、1086-1096` |
| 参数历史 | `frmParam.pas:68-112、186-197、252-377` |
| 快捷项管理窗口位置/列宽 | `frmShortCutMan.pas:575-615` |
| 无效项校验窗口 | `frmShortCutMan.pas:327-394`；`frmInvalid.pas:50-111` |
