# 01 - 主窗口模块功能规格（frmALTRun）

> 逆向对象：ALTRun V1.46.7 主窗口模块
> 源文件（只读）：
> - `Form\frmALTRun.pas`（2802 行，主逻辑）
> - `Form\frmALTRun.dfm`（8994 行，控件布局/属性，其中约 2300 行为内嵌图片/图标二进制）
> - `ALTRun.dpr`（57 行，程序入口）
> - 关联：`Unit\untShortCutMan.pas`、`Unit\untALTRunOption.pas`、`Unit\untUtilities.pas`、`Unit\untClipboard.pas`、`Unit\untLogger.pas`、`3rdUnit\HotKeyManager\HotKeyManager.pas`、`3rdUnit\CoolTrayIcon\CoolTrayIcon.pas`
>
> 行号基准：原始文件按 ANSI/GBK 转码为 UTF-8 后行号不变，本文所有 `文件:行号` 均指原始文件行号。
>
> 编译环境：Delphi 2007（DCC32，`.dproj` 中 Project Version=7.0，RAD Studio 5.0/BDS 环境），`string = AnsiString`（系统 ACP，中文环境即 GBK/CP936）。
> `DEBUG_MODE = False`、`DEBUG_SORT = False`（`untALTRunOption.pas:16-17`），因此文中标注 DEBUG 的分支均为死代码，但为“1:1 对齐”在文中保留说明。

---

## 0. 模块角色与常量标识

| 项 | 值 | 位置 |
|---|---|---|
| 窗口标题/程序名 `TITLE` | `'ALTRun'` | `untALTRunOption.pas:21` |
| 版本 `ALTRUN_VERSION` | `'V1.46.7'` | `untALTRunOption.pas:22` |
| 升级网址 `UPGRADE_URL` | `http://journeyboy.blog.sohu.com/112820379.html` | `untALTRunOption.pas:25` |
| 配置文件名 | `<ExeDir>\ALTRun.ini` | `untALTRunOption.pas:743,830`；`frmALTRun.pas:42`（首次运行判定） |
| 快捷项列表文件 | `<ExeDir>\ShortCutList.txt` | `untShortCutMan.pas:22,613` |
| 收藏文件 | `<ExeDir>\FavoriteList.txt` | `untShortCutMan.pas:23,628` |
| 语言文件 | `<ExeDir>\<Lang>.lang`（INI 格式） | `untALTRunOption.pas:901` |
| 自定义消息 | `WM_ALTRUN_ADD_SHORTCUT = WM_USER+2000`、`WM_ALTRUN_SHOW_WINDOW = WM_USER+2001` | `frmALTRun.pas:179-180` |
| 托盘音乐 | `<ExeDir>\Popup.wav`（缺失时从资源 `WAVE/PopupWav` 释放） | `frmALTRun.pas:1142-1151` |

---

## 1. 程序启动流程

### 1.1 `dpr` 入口顺序（严格按序）

| 步 | 代码 | 位置 |
|---|---|---|
| 1 | `Application.Initialize` | `ALTRun.dpr:40` |
| 2 | `IsRunFirstTime := not FileExists(<ExeDir> + TITLE + '.ini')`（INI 是否存在＝是否首次运行） | `ALTRun.dpr:42` |
| 3 | `LoadSettings`（读 INI；LoadSettings 内部用 `TIniFile.Create`，文件不存在时不会立刻创建，真正写盘在 SaveSettings） | `ALTRun.dpr:43`、`untALTRunOption.pas:735-820` |
| 4 | `Application.Title := TITLE` | `ALTRun.dpr:46` |
| 5 | `Application.CreateForm(TParamForm, ParamForm)`——**参数输入窗体先于主窗体创建**（全局单例，后续执行带参数项时复用） | `ALTRun.dpr:47` |
| 6 | `Application.CreateForm(TALTRunForm, ALTRunForm)`——主窗体 `FormCreate` 在此执行 | `ALTRun.dpr:48` |
| 7 | `Application.ShowMainForm := False`——禁止 `Application.Run` 自动显示主窗体 | `ALTRun.dpr:49` |
| 8 | `Application.OnMinimize := ALTRunForm.evtMainMinimize`——最小化即隐藏到托盘 | `ALTRun.dpr:50` |
| 9 | `if not ALTRunForm.IsExited then Application.Run`（用于“右键发送到/添加文件”模式下 `FormCreate` 内已 `Application.Terminate` 的情况，避免进入消息循环） | `ALTRun.dpr:53` |
| 10 | `Application.Run` 返回后 `SaveSettings`（再写一次 INI） | `ALTRun.dpr:55` |

> 注：`IsRunFirstTime`、`LoadSettings`、`SaveSettings`、`SetActiveLanguage` 均为 `untALTRunOption` 的全局过程/函数，无参数。

### 1.2 `FormCreate` 初始化顺序（`frmALTRun.pas:1855-2156`）

严格按代码顺序：

1. `Self.Caption := TITLE`（1864）。
2. `ntfMain.IconVisible := False`——先不显示托盘图标（1867）。
3. 双缓冲防闪：`Self`、`lstShortCut`、`edtShortCut`、`edtHint`、`edtCommandLine`、`edtCopy` 的 `DoubleBuffered := True`（1869-1875）。
4. `m_IsExited := False`（1880）。
5. **首次运行：语言选择**（1883-1937）
   - 若 `IsRunFirstTime`：`TLangForm.Create(Self)`，枚举 `<ExeDir>\*.lang` 与固定项 `DEFAULT_LANG='EN'` 填入下拉框，默认选当前 `Lang`；`ShowModal`。
   - 用户取消（非 `mrOk`）：`DeleteFile(<ExeDir>\ALTRun.ini)` 后 `Halt(1)`，进程直接结束（1924-1925）。
   - 用户确认：`Lang := LangForm.cbbLang.Text`，`SetActiveLanguage`。
   - 若没有语言文件（`LangList.Count=0`）则跳过语言窗，直接 `SetActiveLanguage`（1934-1937）。
   - 简体中文特殊标题：`if Lang='简体中文' then LangForm.Caption := '请选择界面语言'`（1912-1913）。
6. 建立 `ShortCutMan := TShortCutMan.Create` 并 `LoadShortCutList`（1940-1941）。构造器内部：设置两种文件名、`WriteDefaultShortCutList=True`、创建列表/正则对象、`LoadFavoriteList`（`untShortCutMan.pas:610-634`）。若 `ShortCutList.txt` 不存在，`LoadShortCutList` 会写入约 40 条内置默认快捷项（`untShortCutMan.pas:1386-1474`）。
7. `m_AgeOfFile := FileAge(ShortCutMan.ShortCutFileName)`（1942，仅被后面已注释的代码使用，实际为死状态）。
8. `m_LastShortCutCmdIndex := -1`、`m_LastKeyIsNumKey := False`（1945-1946）。
9. **命令行参数处理**（1949-2027，见 1.3），可能直接退出或进入“加文件”分支。
10. **单实例检测**（2029-2037，见 1.4）。
11. `InitLogger(DEBUG_MODE, False, False)`（2041；`DEBUG_MODE=False` ⇒ 不写日志文件）。
12. 清理历史 HotRun 遗留：注册表 Run 项 `HotRun`/`HotRun.exe`、Common Startup 的 `HotRun.exe.lnk`、SendTo 的 `HotRun.lnk`，以及把 `HotRun.ini` 重命名为 `ALTRun.ini`（2050-2067）。
13. 同样清理旧名 `ALTRun.exe` 的注册表 Run 与 Startup 链接（2069-2074）。
14. `ApplyHotKey1`、`ApplyHotKey2`（2077-2078）。
15. 注册“最近一次快捷项”热键 `hkmHotkey3.AddHotKey(TextToHotKey(LastItemHotKeyStr, LOCALIZED_KEYNAMES))`（2081；返回值未检查）。
16. 设置 5 个菜单项标题（2084-2088）与 4 个 Hint（2091-2094），语言资源来自 `.lang`。
17. **开机自启项**：先删后加，避免多目录产生重复项：`SetAutoRun(TITLE, ExeName, False)`、`SetAutoRunInStartUp(TITLE, ExeName, False)`（2096-2098）。
18. 首次运行询问是否开机自启（`resAutoRunWhenStart`，MB_YESNO+MB_TOPMOST）→ `AutoRun`；随后 `SetAutoRunInStartUp(TITLE, ExeName, AutoRun)`（2101-2106）。注意实际只用 `SetAutoRunInStartUp`（Common Startup 快捷方式），注册表 Run 的写入被注释（2105）。
19. `AddMeToSendTo(TITLE, False)` 先移除；首次运行询问“是否加入发送到” → `AddToSendTo`；`AddMeToSendTo(TITLE, AddToSendTo)`（2108-2116）。
20. `HandleID := Self.Handle`；`SaveSettings`（2119-2120）。
21. **首次运行额外重启一次**：`RestartMe; Exit`（2123-2127）——保证“发送到”菜单项生效（新进程带 `Restart` 参数，见 1.3）。
22. `m_IsFirstShow := True`、`m_IsFirstDblClickIcon := True`（2130-2133）。
23. `ntfMain.IconVisible := ShowTrayIcon`（2136）；三按钮可见性按配置（2139-2141）。
24. 启动气泡通知：`ShowStartNotification` 时 `ntfMain.ShowBalloonHint(resInfo, Format(resStarted+#13#10+resPressKeyToShowMe, [TITLE, ALTRUN_VERSION, GetHotKeyString]), bitInfo, 5)`（2144-2147，5 秒）。
25. `ntfMain.Hint := Format(resMainHint, [TITLE, ALTRUN_VERSION, #13#10, GetHotKeyString])`（2150）。
26. `m_NeedRefresh := True`（2153）。
27. `if ShowMeWhenStart then actShowExecute(Sender)`（2155，默认 `False`）。

### 1.3 命令行参数（`frmALTRun.pas:1949-2027`）

| `ParamStr(1)` | 行为 | 位置 |
|---|---|---|
| 空字符串 | 正常启动 | 1949 |
| `Restart`（`RESTART_FLAG`） | `Sleep(2000)` 等待旧实例释放互斥体，然后继续正常初始化 | 1953-1956 |
| `Clean`（`CLEAN_FLAG`） | 弹 `resCleanConfirm`（MB_YESNO+MB_TOPMOST），确认后 `SetAutoRun(TITLE,'',False)`、`SetAutoRunInStartUp(TITLE,'',False)`、`AddMeToSendTo(TITLE, False)`；随后 `Application.Terminate` 并退出 | 1958-1970 |
| 其它（文件路径） | 标题改为 `TITLE + ' - Add ShortCut'`；路径含空格则前后加 `"`（1998-1999）；`hALTRun := HandleID`（取自 INI，而不是 FindWindow），若 `IsWindow(hALTRun)` 成立则 `SendMessage(hALTRun, WM_SETTEXT, 1, PChar(FileName))` 让运行中的实例添加；否则本进程 `ShortCutMan.LoadShortCutList; ShortCutMan.AddFileShortCut(FileName)` 直接写入；`m_IsExited := True; Application.Terminate; Exit` | 1974-2026 |
| 说明 | 该分支在单实例检测之前执行，即“带文件参数启动”时不会走互斥体流程 | 1949 vs 2029 |

`WM_SETTEXT` 的接收端在主窗口 `WndProc`：`Msg.WParam = 1` 表示自己程序发来，取 `Msg.LParam` 作为文件名调用 `ShortCutMan.AddFileShortCut`（2767-2777）。

### 1.4 单实例检测（`frmALTRun.pas:2029-2037`）

- `IsRunningInstance('ALTRUN_MUTEX')`（`untUtilities.pas:94-101`：`CreateMutex(nil, True, 'ALTRUN_MUTEX')`，`GetLastError=ERROR_ALREADY_EXISTS` 即已运行）。
- 若已运行：`SendMessage(HandleID, WM_ALTRUN_SHOW_WINDOW, 0, 0)`（`HandleID` 来自 INI 保存的窗口句柄），然后 `Halt(1)` 立即终止本进程。
- 运行中实例收到 `WM_ALTRUN_SHOW_WINDOW` → `actShowExecute(Self)` + `SetForegroundWindow(Application.Handle)`（2759-2765）。
- `HandleID` 在 `FormCreate` 末尾保存（2119-2120），退出时 `actCloseExecute` 置 0 并保存（244-245）。
- 注意：`HandleID` 跨进程重启后可能已失效（句柄不能跨进程复用），只有“带文件参数”分支做了 `IsWindow` 校验（2007），`WM_ALTRUN_SHOW_WINDOW` 分支未校验。

### 1.5 热键注册（`frmALTRun.pas:1186-1231, 2081`）

- 主/副热键默认值：`HotKey1='Alt + R'`、`HotKey2='Pause'`（`untALTRunOption.pas:746-747`）；“最近项”热键 `LastItemHotKey='ALT+L'`（748）。
- 用 `TextToHotKey(HotKeyStr, LOCALIZED_KEYNAMES=True)`（`HotKeyManager.pas:101`）把文本转成 `Cardinal`（低字 VK + 修饰键位）。
- `hkmHotkey1/hkmHotkey2` 各是一个 `THotKeyManager` 组件（DFM `8651-8656`、`8977-8982`），其 `AddHotKey` 内部 `GlobalAddAtom` 生成 id + `RegisterHotKey(FHandle,...)`，`FHandle` 由 `Classes.AllocateHWnd(HookProc)` 创建的隐藏窗口（`HotKeyManager.pas:455-512, 614-655`）。
- 注册失败：`HotKey1` 一定弹 `resHotKeyError`（“HotKey can not be \"%s\"”，MB_OK+MB_ICONWARNING）；`HotKey2` 仅在其值非空且不等于 `resVoidHotKey`（默认 `'NONE'`）时弹框，并把 `HotKeyStr2` 清空（1196-1230）。
- 两个热键共用同一处理过程 `hkmHotkeyHotKeyPressed`：先抓取剪贴板文本和前台窗口信息（见 6/7 节），然后 `if m_IsShow then actHideExecute else actShowExecute`，即**热键是显隐切换**（2411-2440）。
- 第三个 `THotKeyManager`（DFM `8988-8993`）只注册 `LastItemHotKeyStr`，处理过程 `hkmHotkey3HotKeyPressed`：抓取剪贴板/前台窗口信息后，取 `ShortCutMan.GetLatestShortCutItemList` 的**第 0 项直接执行**，不显示主窗口（2380-2409）。该执行路径不会更新频率/最近列表（调用的是 `Execute(ShortCutItem)` 重载）。
- 打开配置窗口前会 `hkmHotkey1.ClearHotKeys; hkmHotkey2.ClearHotKeys` 避免热键冲突，配置取消/结束时重新 `ApplyHotKey1/2`（265-266, 444-445）。

### 1.6 托盘图标（DFM `3484-8553`；代码 `1867, 2136, 472, 2144-2150, 2543-2573`）

- 组件：`TCoolTrayIcon`（CoolTrayIcon 第三方库），`MinimizeToTray=True`、`PopupMenu=pmMain`、`IconIndex=0`、内嵌多尺寸图标。
- 启动时先 `IconVisible=False`，之后按 `ShowTrayIcon`（默认 True，`untALTRunOption.pas:766`）显示（1867、2136；配置确认后 472）。
- `ntfMain.Hint`＝`resMainHint` 格式化串（2150）。
- 启动气泡 5 秒（2145-2147）。
- 单击与双击绑定同一处理过程 `ntfMainDblClick`（DFM `8549-8550`）：
  - 第一次触发时 `m_IsFirstDblClickIcon` 为 True：创建 `TAutoHideForm` 提示“最好用热键 %s 呼出我”（`resShowMeByHotKey`），模态显示后自动淡出（2549-2567；frmAutoHide 见 2.5）。
  - 之后：`if m_IsShow then actHideExecute else actShowExecute`（2569-2572）。
- `Application.OnMinimize = evtMainMinimize`（`ALTRun.dpr:50`）⇒ 应用最小化时隐藏窗口（到托盘）。

### 1.7 开机自启与“发送到”（`frmALTRun.pas:2096-2116`；`untUtilities.pas:115-167, 335`）

- 开关：INI `[Config] AutoRun`（默认 True）、`AddToSendTo`（默认 True）。
- 启动自启实现：`SetAutoRunInStartUp(TITLE, ExeName, AutoRun)` ⇒ 在 `CSIDL_COMMON_STARTUP`（**所有用户**启动目录）创建/删除名为 `ALTRun.lnk` 的快捷方式（`untUtilities.pas:148-167`，`CreateLink` 走 `IShellLink`）。
- 另有 `SetAutoRun`（注册表 Run 项）仍被用于**清理**旧项：非 Vista 写 `HKLM\...\Run`，Vista+ 写 `HKCU`（`untUtilities.pas:115-146`）；但正常写入启动项已改为只写 Startup 快捷方式（2105-2106，`SetAutoRun` 行被注释）。
- 每次启动先“删除再添加”，避免程序被复制到其他目录后残留多个自启项（2096-2098）。
- 首次运行会各弹一次“是否随系统启动/是否加入发送到”（2101-2113），并因“发送到”需重启生效而 `RestartMe`（2123-2127，见 1.3）。

### 1.8 启动时是否显示主窗口

- 默认**不显示**：`ShowMeWhenStart` 默认 False（`untALTRunOption.pas:765`），且 `Application.ShowMainForm := False`（`ALTRun.dpr:49`）；只有 `ShowMeWhenStart=True` 时在 `FormCreate` 末尾调用 `actShowExecute` 显示（2155）。
- 例外：首次运行若用户取消语言选择 → `Halt(1)` 结束；单实例冲突 → `Halt(1)`；`Clean` → 弹框后退出；`Restart` → 延迟 2 秒后继续。

### 1.9 配置项与默认值（INI `[Config]`/`[Window]`/`[GUI]`/`[DEBUG]`，`untALTRunOption.pas:735-820`）

配置窗口 `chklstConfig.Checked[0..19]` 的索引与变量一一对应（`frmALTRun.pas:279-317, 374-393`）：

| # | 配置变量 | INI 键 | 默认值 | 影响 |
|---|---|---|---|---|
| 0 | `AutoRun` | `AutoRun` | True | 开机自启 |
| 1 | `AddToSendTo` | `AddToSendTo` | True | 加入“发送到”菜单 |
| 2 | `EnableRegex` | `Regex` | True | 正则/通配符匹配模式 |
| 3 | `MatchAnywhere` | `MatchAnywhere` | True | 任意位置匹配 |
| 4 | `EnableNumberKey` | `NumberKey` | True | 列表数字前缀与数字键 |
| 5 | `IndexFrom0to9` | `IndexFrom0to9` | False | 编号 0-9（否则 1-9,0） |
| 6 | `RememberFavouratMatch` | `RememberFavouratMatch` | False | 收藏项置顶 |
| 7 | `ShowOperationHint` | `ShowOperationHint` | True | 提示行 |
| 8 | `ShowCommandLine` | `ShowCommandLine` | True | 命令栏 + 窗口高度 |
| 9 | `ShowStartNotification` | `ShowStartNotification` | True | 启动气泡 |
| 10 | `ShowTopTen` | `ShowTopTen` | True | 只显示前 10 条 |
| 11 | `PlayPopupNotify` | `PlayPopupNotify` | True | 显示时播放 Popup.wav |
| 12 | `ExitWhenExecute` | `ExitWhenExecute` | False | 执行后退出 |
| 13 | `ShowSkin` | `ShowSkin` | True | 背景图 |
| 14 | `ShowMeWhenStart` | `ShowMeWhenStart` | False | 启动即显示主窗口 |
| 15 | `ShowTrayIcon` | `ShowTrayIcon` | True | 托盘图标 |
| 16 | `ShowShortCutButton` | `ShowShortCutButton` | True | 快捷项管理按钮 |
| 17 | `ShowConfigButton` | `ShowConfigButton` | False | 配置按钮 |
| 18 | `ShowCloseButton` | `ShowCloseButton` | True | 关闭（隐藏）按钮 |
| 19 | `ExecuteIfOnlyOne` | `ExecuteIfOnlyOne` | False | 唯一匹配立即执行 |

其它配置：

| 项 | INI 段/键 | 默认值 | 位置 |
|---|---|---|---|
| 主热键 | `[Config] HotKey1` | `'Alt + R'` | `untALTRunOption.pas:746` |
| 副热键 | `[Config] HotKey2` | `'Pause'` | 747 |
| 最近项热键 | `[Config] LastItemHotKey` | `'ALT+L'` | 748 |
| 自动隐藏延时（秒） | `[Config] HideDelay` | 15 | 752 |
| 参数历史上限 | `[Config] ParamHistoryLimit` | 50 | 757 |
| 窗口位置 | `[Window] WinTop/WinLeft` | 0/0 | 772-773 |
| 管理窗位置/大小/列宽 | `[Window] Man*` | 0,0,450,240；列宽 100 | 775-781 |
| 背景图文件名 | `[GUI] BGFileName` | `'BG.jpg'` | 104,788 |
| 三处字体串 | `[GUI] TitleFont/KeywordFont/ListFont` | `'宋体|[]|12|65535|0'` 等 | 96-98,791-796 |
| 列表格式 | `[GUI] ListFormat` | `'%-25s| %s'` | 99,798 |
| 透明色键 | `[GUI] AlphaColor` | `clBlack` | 100,800 |
| 不透明度 | `[GUI] Alpha` | 240 | 101,801 |
| 圆角半径 | `[GUI] RoundBorderRadius` | 12 | 102,802 |
| 窗口宽度 | `[GUI] FormWidth` | 420 | 103,803 |
| 语言 | `[GUI] Lang` | 系统默认语言为 2052 时 `'简体中文'`，否则 `'EN'` | 105-106,805-809 |
| 跟踪日志 | `[DEBUG] TraceEnable` | False（仅供手工修改，不写回） | 811,884 |
| 执行次数 | `[DEBUG] ShortCutRunCount` | 0 | 813 |
| 窗口句柄 | `[DEBUG] HandleID` | 0 | 814 |
| 最近列表索引 | `[DEBUG] LatestList` | `''` | 817 |

### 1.10 退出与重启

| 路径 | 行为 | 位置 |
|---|---|---|
| 菜单/托盘“Close”、`WM_QUERYENDSESSION`/`WM_ENDSESSION`（关机） | `actCloseExecute`：保存 `WinTop/WinLeft`（仅当 `m_IsShow`）、`LatestList := ShortCutMan.GetLatestShortCutIndexList`、`HandleID := 0`、`SaveSettings`、`ShortCutMan.SaveShortCutList`、`Application.Terminate` | 230-251, 1808-1814 |
| 标题栏关闭（`WM_SYSCOMMAND SC_CLOSE`，非 DEBUG） | 仅隐藏窗口 + 清空输入，不退出 | 1794-1804 |
| `ExitWhenExecute` 执行后 | `tmrExit.Enabled := True` → 默认 1000 ms 后 `actCloseExecute` | 745, 8957-8962, 2701-2707 |
| `RestartMe` | `ShellExecute(0,nil,ExeName,'Restart',nil,SW_SHOWNORMAL)` 启动新进程，然后 `actCloseExecute` 退出自身 | 2620-2626 |
| 进程退出收尾 | `FormDestroy` 释放 `ShortCutMan`（其析构会备份 `ShortCutList.txt.bak` 并写 `FavoriteList.txt`）；`dpr` 再 `SaveSettings` | 2158-2167；`untShortCutMan.pas:656-671`；`ALTRun.dpr:55` |

---

## 2. 主窗口形态

### 2.1 DFM 关键属性（`frmALTRun.dfm:1-16, 720-731`）

| 属性 | 值 | 位置 | 说明 |
|---|---|---|---|
| `BorderStyle` | `bsNone` | 1-4 | 无边框 |
| `Caption` | `'ALTRun'` | 5 | 运行时 `actShowExecute` 再设一次（983） |
| `ClientWidth/ClientHeight` | 420 / 250 | 6-7 | 像素（`PixelsPerInch=96`） |
| `Color` | `clMaroon` | 8 | 配合 `TransparentColor=True`（9） |
| `TransparentColor` | `True` | 9 | VCL 透明色（颜色键）开关 |
| `Font` | 宋体（`#23435#20307`），`Height=-16`（≈12pt），`ANSI_CHARSET` | 10-14 | 默认字体 |
| `FormStyle` | `fsStayOnTop` | 15 | 置顶 |
| `KeyPreview` | `True` | 720 | 窗体先于控件收到 KeyDown/KeyPress/KeyUp（**键盘交互的关键前提**） |
| `Position` | `poScreenCenter` | 722 | 首次显示且无有效保存坐标时使用 |
| `Events` | `OnActivate=FormActivate`、`OnCreate=FormCreate`、`OnDestroy=FormDestroy`、`OnKeyDown=FormKeyDown`、`OnKeyPress=FormKeyPress` | 723-727 | 无 OnShow/OnHide/OnClose，也无 AlphaBlend |
| `DesignSize` | 420×250 | 728-730 | — |

子控件（`frmALTRun.dfm:3068-3473`；坐标/尺寸为设计值）：

| 控件 | 类 | Left/Top/W/H | 关键属性 | 位置 |
|---|---|---|---|---|
| `imgBackground` | TImage | 0/0/420/250 | `Align=alClient`、`Stretch=True`、内嵌 JPEG 图片、`PopupMenu=pmMain`、`OnMouseDown=imgBackgroundMouseDown` | 733-3067 |
| `lblShortCut` | TLabel | 39/0/321/33 | 黄色宋体 -16，`Alignment=taCenter`、`Layout=tlCenter`、`EllipsisPosition=epEndEllipsis`、`Transparent=True`、`PopupMenu=pmMain`、`OnDblClick=actExecuteExecute`、`OnMouseDown=lblShortCutMouseDown`、`OnMouseActivate=lblShortCutMouseActivate` | 3068-3096 |
| `btnShortCut` | TSpeedButton | 8/2/25/25 | `Flat`、`Layout=blGlyphTop`、Hint、`OnClick=actShortCutExecute`、`OnMouseDown=MiddleMouseDown` | 3097-3191 |
| `btnClose` | TSpeedButton | 392/4/22/22 | 锚定右上，`OnClick=actHideExecute`、`OnMouseDown=MiddleMouseDown` | 3192-3269 |
| `btnConfig` | TSpeedButton | 359/4/25/25 | 锚定右上，`OnClick=actConfigExecute`、`OnMouseDown=MiddleMouseDown` | 3270-3338 |
| `lstShortCut` | TListBox | 8/56/404/164 | `BorderStyle=bsNone`、`Ctl3D=False`、`ItemHeight=16`、`Font=clNavy 宋体 -16`、`ImeMode=imClose`、`TabStop=False`、`ParentShowHint=False`、`PopupMenu=pmList`、`OnClick=actSelectChangeExecute`、`OnDblClick=actExecuteExecute`、`OnKeyDown=lstShortCutKeyDown`、`OnMouseDown=lstShortCutMouseDown`、`OnMouseActivate=lstShortCutMouseActivate` | 3339-3366 |
| `edtCommandLine` | TEdit | 8/226/404/16 | 只读、无边框、`Color=clMoneyGreen`、灰字宋体 -13、`TabStop=False`、`PopupMenu=pmCommandLine`、`OnKeyDown=edtCommandLineKeyDown`、`OnMouseDown=MiddleMouseDown` | 3367-3393 |
| `edtCopy` | TEdit | 400/226/16/16 | 隐藏（`Visible=False`），`Text='★'`（`#9733`），青绿字 | 3394-3419 |
| `edtShortCut` | TEdit | 8/30/404/22 | 无边框、`Color=clMoneyGreen`、红字宋体 -16、`ImeMode=imClose`、`TabStop` 默认、设计期 `Text='01234567890'`（会被首次显示清空）、`OnChange=edtShortCutChange`、`OnKeyDown=edtShortCutKeyDown`、`OnMouseDown=MiddleMouseDown`、`OnMouseActivate=edtShortCutMouseActivate` | 3420-3446 |
| `edtHint` | TEdit | 82/35/328/14 | 只读、`Enabled=False`、无边框、`BiDiMode=bdRightToLeft`、黑字宋体 -13、`TabStop=False`、`OnMouseDown=MiddleMouseDown` | 3447-3473 |
| `evtMain` | TApplicationEvents | — | `OnActivate/OnDeactivate/OnIdle/OnMessage/OnMinimize/OnShortCut` | 3474-3483 |
| `ntfMain` | TCoolTrayIcon | — | 见 1.6 | 3484-8553 |
| `pmMain` | TPopupMenu | — | Show/ShortCut/Config/About/Close | 8554-8573 |
| `actlstMain` | TActionList | — | 15 个动作，见第 4 节 | 8574-8650 |
| `hkmHotkey1/2/3` | THotKeyManager | — | Tag=1/2/3，前两者共用 `hkmHotkeyHotKeyPressed`，第三者 `hkmHotkey3HotKeyPressed` | 8651-8656, 8977-8993 |
| `pmList` | TPopupMenu | — | Add/Edit/Delete/`-`/OpenDir | 8657-8676 |
| `pmCommandLine` | TPopupMenu | — | **无任何菜单项**，`OnPopup=pmListPopup` | 8983-8987 |
| `tmrHide` | TTimer | — | 默认 Interval=0、Enabled=False；自动隐藏用 | 8677-8682 |
| `tmrExit` | TTimer | — | 无 Interval 设置（VCL 默认 **1000 ms**） | 8957-8962 |
| `tmrCopy` | TTimer | — | Interval=**250**，用于隐藏 ★ | 8963-8969 |
| `tmrFocus` | TTimer | — | Interval=**500**，处理函数立即 `Exit`（死代码） | 8970-8976 |
| `ilHotRun` | TImageList | 5 幅图标（16×16） | 菜单/动作图标 | 8683-8955 |

> 关键：**结果列表没有图标列、没有 OwnerDraw**（`lstShortCut` 无 `Style=lbOwnerDraw*`、无 `OnDrawItem`），即主窗口不显示 exe 图标，也不做自定义绘制/高亮。

### 2.2 分层透明、颜色键、圆角、背景图

在**首次显示**（`actShowExecute` 的 `m_IsFirstShow` 块，1086-1096）和**配置确认后**（512-519）执行：

```pascal
lg := GetWindowLong(Handle, GWL_EXSTYLE);
lg := lg or WS_EX_LAYERED;
SetWindowLong(Handle, GWL_EXSTYLE, lg);
SetLayeredWindowAttributes(Handle, AlphaColor, Alpha, LWA_ALPHA or LWA_COLORKEY);
SetWindowRgn(Handle, CreateRoundRectRgn(0,0,Width,Height,RoundBorderRadius,RoundBorderRadius), True);
```

- `AlphaColor` 默认 `clBlack`（0=黑色）作为**颜色键**（`untALTRunOption.pas:100`）；`Alpha` 默认 **240**（101）；`RoundBorderRadius` 默认 **12**（102）。
- 窗体自身 `Color=clMaroon` + `TransparentColor=True`（DFM 8-9），VCL 透明色行为与 `LWA_COLORKEY` 是否叠加需运行验证（见“不确定”节）。
- 背景图：`imgBackground` 从 `<ExeDir>\<BGFileName>` 加载（`BGFileName` 默认 `BG.jpg`，`untALTRunOption.pas:104,788`），`ShowSkin=False` 时 `Picture := nil`（1043-1046）。DFM 中内嵌了一张 JPEG 作为设计期图片；若磁盘上 `BG.jpg` 不存在，首次显示会把内嵌图片**保存**为磁盘 `BG.jpg`（1040-1041）。
- 圆角区域只在上述两处设置；窗口尺寸变化（如 `ShowCommandLine` 改变 Height）**没有**重新 `SetWindowRgn`（源码中无其他调用）。

### 2.3 尺寸与位置恢复

- `WinTop`/`WinLeft` 来自 INI `[Window]`，默认 0（`untALTRunOption.pas:772-773`）。
- 首次显示（1022-1032）：
  - `if (WinTop <= 0) or (WinLeft <= 0)` → `Position := poScreenCenter`（注意：**负坐标（副屏/上方）也会被丢弃**）。
  - 否则 `Top := WinTop; Left := WinLeft; Width := FormWidth`（`FormWidth` 默认 420，`untALTRunOption.pas:103`）。
- 高度固定：`ShowCommandLine=True` → `Height := 250`，否则 `Height := 230`（1103-1107；配置确认后同样 528-531）。DFM 设计高度 250。
- 列表高度首次显示时改为 `lstShortCut.Height := 10 * lstShortCut.ItemHeight`（1101）＝160 px。
- 每次显示都会在末尾把当前 `Top/Left` 写回 `WinTop/WinLeft`（1130-1132）；退出时再次保存（235-239）。
- 宽度变化（`FormWidth`）与 Alpha/圆角/语言的修改被标记为“需要重启”（401-414），配置确认后会弹 `resRestartMeInfo` 并 `RestartMe`（535-540）。

### 2.4 显示流程时序（`actShowExecute`，973-1153）

1. `Caption := TITLE`；`if ParamForm <> nil then ParamForm.ModalResult := mrCancel`（防止参数窗残留，983-985）。
2. `ShortCutMan.LoadShortCutList`（内部按文件修改时间决定是否真正重载，987）。
3. 刷新列表：
   - 若输入框非空：`edtShortCut.Text := ''`（触发 `OnChange` 全量刷新，990-993）。
   - 否则若 `m_IsFirstShow or m_NeedRefresh or ShortCutMan.NeedRefresh`：清标志并 `edtShortCutChange`（996-1001）。
   - 否则：只 `RefreshOperationHint`，并把 `ItemIndex` 置 0、更新标题标签（含目录方括号），1004-1011。
4. `Self.Show`（1016，无动画）。
5. 首次显示初始化（1018-1119）：位置、三处字体（`StrToFont`）、背景图、三按钮可见性、标题栏布局计算、`WS_EX_LAYERED`+`SetLayeredWindowAttributes`、`SetWindowRgn`、`ShortCutMan.SetLatestShortCutIndexList(LatestList)`、列表高度、窗口高度。
6. `Application.Restore; SetForegroundWindow(Application.Handle)`（1122-1123，强制抢前台）。
7. `m_IsShow := True`；`edtShortCut.SetFocus`（1124-1125）。
8. `GetLastCmdList`（缓存最近一次结果列表指针，1126）。
9. `RestartHideTimer(HideDelay)`（1127）；`tmrFocus.Enabled := True`（1128，随后定时器第一跳即退出，无实际效果）。
10. 回写 `WinTop/WinLeft`；`m_LastActiveTime := GetTickCount`；`m_IsTop := True`（1130-1137）。
11. `PlayPopupNotify` 时播放 `Popup.wav`：文件不存在则 `ExtractRes('WAVE','PopupWav','Popup.wav')`；存在则 `PlaySound(..., snd_ASYNC)`，否则以资源方式 `PlaySound('PopupWav', HInstance, snd_ASYNC or SND_RESOURCE)`（1139-1152）。

> 没有任何淡入/滑动动画；`Show`/`Hide` 都是 SW_SHOW/SW_HIDE 级瞬时切换。

### 2.5 隐藏与自动隐藏（AutoHide）

- 隐藏基元 `evtMainMinimize`（1831-1841）：
  ```pascal
  inherited;
  edtCopy.Visible := False;
  m_IsShow := False;
  Self.Hide;
  StopTimer;            // 关闭 tmrHide、tmrFocus
  ```
  （`inherited;` 的具体语义存疑，见 10 节；`Application.OnMinimize` 也指向它。）
- **自动隐藏定时器**：`tmrHide`。`RestartHideTimer(Delay)` 仅在 `m_IsShow` 时生效：`Enabled:=False; Interval:=Delay*1000; Enabled:=True`（2628-2638）。`HideDelay` 默认 **15 秒**（`untALTRunOption.pas:752`）。超时 → `evtMainMinimize` + `edtShortCut.Text := ''`（2737-2743）。
- 重置自动隐藏计时的触发点：
  - `FormActivate`（1848-1853）
  - `evtMainActivate`（应用激活，1691-1696）
  - `FormKeyDown`（每次按键，2180）
  - `edtShortCutMouseActivate` / `lstShortCutMouseActivate` / `lblShortCutMouseActivate`（1682-1689, 2516-2523, 2456-2463）
  - `actShortCutExecute` 结束时（966）
- **失去焦点立即隐藏**：`evtMainDeactivate`（1698-1754）注释掉“超过 500ms 才隐藏”的旧逻辑，现行实现为无条件 `evtMainMinimize(Self); edtShortCut.Text := ''`（1704-1706）。
- 其它隐藏触发：热键切换（2436-2439）、托盘单击/双击切换（2569-2572）、`btnClose` 点击→`actHideExecute`（清空文本，755-762）、`ESC`（文本为空时）、`WM_SYSCOMMAND SC_CLOSE`、`tmrHide` 超时、执行前（`actExecuteExecute` 先 `evtMainMinimize`，677）。
- 另有独立的“自动消失提示框” `TAutoHideForm`（非主窗口）：`tmrHide` 1500 ms 后启动 `tmrAlpha`，每 50 ms 把 `AlphaBlendValue` 减 10，减到 <10 时 `ModalResult := mrOk` 关闭（`frmAutoHide.dfm:46-58`；`frmAutoHide.pas:43-64`）。

### 2.6 标题栏/按钮布局计算（首次显示 1048-1084；配置确认 474-510，两处逻辑相同）

- 三个按钮可见性由配置决定：`ShowShortCutButton`（默认 True）、`ShowConfigButton`（默认 False）、`ShowCloseButton`（默认 True）（`untALTRunOption.pas:767-769`）。
- 快捷项管理按钮可见时 `lblShortCut.Left := btnShortCut.Left + btnShortCut.Width`（=33），否则 `lblShortCut.Left := 0`。
- 配置/关闭按钮宽度分配：
  - 关闭按钮可见且配置按钮可见：`btnConfig.Left := btnClose.Left - btnConfig.Width - 10`；`lblShortCut.Width := btnConfig.Left - lblShortCut.Left`。
  - 关闭可见、配置不可见：`lblShortCut.Width := btnClose.Left - lblShortCut.Left`。
  - 关闭不可见、配置可见：`btnConfig.Left := Self.Width - btnConfig.Width - 10`；`lblShortCut.Width := btnConfig.Left - lblShortCut.Left`。
  - 两者都不可见：`lblShortCut.Width := Self.Width - lblShortCut.Left`。
- 该布局只在“首次显示”和“配置确认”时重算；窗口宽度变化不会自动重排（需重启才应用 FormWidth）。
- 按钮/标签标题字体：`TitleFontStr`→`lblShortCut.Font`，`KeywordFontStr`→`edtShortCut.Font`，`ListFontStr`→`lstShortCut.Font`（1034-1037；默认值 `untALTRunOption.pas:96-98`：宋体 12 号，颜色 65535(黄)/255(红)/8388608(藏青)）。

---

## 3. 输入框与搜索匹配引擎

### 3.1 输入文本的语义（重要）

- 主窗口**不做“命令 + 参数”拆分**。`edtShortCut.Text` 整体作为**关键字**传给 `TShortCutMan.FilterKeyWord`（`frmALTRun.pas:1392`）。
- 执行时把整段文本作为 `KeyWord` 传给 `ShortCutMan.Execute(item, edtShortCut.Text)`，该参数**只用于收藏映射**（`untShortCutMan.pas:2071-2077`：`m_FavoriteList.Values[LowerCase(KeyWord)] := Name`），不会成为命令行参数。
- 真正的参数来自：
  1. `ParamForm`（当项 `ParamType <> ptNone` 且命令行不含剪贴板/前台窗口标志时弹出输入框）；
  2. 剪贴板 / 前台窗口信息（`{%c}`/`{%wd}`/`{%wt}`/`{%wc}`，在按热键时已抓取，见 7.3）；
  3. 命令内 `%p` / `{%p}` 占位符（`untALTRunOption.pas:27-28`）。
- `edtShortCut` 的 `ImeMode=imClose`（DFM 3435）⇒ 关键字输入时**关闭输入法**，实际只能输入 ASCII（中文可通过粘贴进入，但匹配仍是字节/字符子串）。
- 设计期 `Text='01234567890'`（DFM 3441）在首次 `actShowExecute` 时因“文本非空”被清空（990-993）。

### 3.2 触发链路

`edtShortCut.OnChange` → `edtShortCutChange`（DFM 3442；实现 1365-1495）：

1. 清空 `lblShortCut.Caption/Hint`、`lstShortCut.Hint`、`edtCommandLine.Text`，`lstShortCut.Clear`（1379-1384）。
2. `ShortCutMan.FilterKeyWord(edtShortCut.Text, StringList)`（1392）。
3. 有结果时：
   - `ShowTopTen=True`：**只取前 10 条**逐条 `AddObject`（1394-1401）；
   - `ShowTopTen=False`：`lstShortCut.Items.Assign(StringList)`（全部，1403）。
4. 无结果时：清空标签/命令栏；然后执行“数字键 / 空格键选择”特例（1411-1471，见 3.7）。
5. 有结果时：`ItemIndex := 0`，标签=第 0 项名称（153 目录则加 `[]`，1474-1477, 1488）。
6. `ExecuteIfOnlyOne and (Count=1)` → 立即 `actExecuteExecute`（1480-1484）。
7. `GetLastCmdList`（1491）刷新“上一次列表指针缓存”；`RefreshOperationHint`（1494）刷新提示行。

### 3.3 非正则 / 正则两种匹配算法（`untShortCutMan.pas:925-1089`）

#### 3.3.1 公共前置

```
结果列表清空
if m_SortedShortCutList.Count = 0 -> 直接返回 False
if KeyWord = ' ' -> 直接返回 False            // 单个空格不匹配任何项
if KeyWord = ''  -> 结果 := m_SortedShortCutList 的全量副本（已排序、已格式化的 scItem）
                    跳过匹配排序，直接进入“收藏置顶/数字前缀”步骤
否则 kw := LowerCase(KeyWord)
```

`m_SortedShortCutList` 由 `Sort` 在 `LoadShortCutList` 末尾构建（见 3.4），元素字符串为 `Format(ListFormat,[ShortCut,Name])`，对象为 `TShortCutItem`。

#### 3.3.2 正则模式（`EnableRegex=True`，默认值 True，`untALTRunOption.pas:751`）

1. **通配符预处理**（957-981）：
   - `.*` → 临时串 `~!@#`；再把所有 `*` → `.*`；再把 `~!@#` 还原为 `.*`（即“单独的 `*` 视为 `.*`，已存在的 `.*` 保留”）；
   - 所有 `?` → `.`（即“单字符通配”，**不是** `.?`，后者实现被注释）。
2. 遍历 `m_SortedShortCutList`（985-1035）：
   - 非 `scItem` 跳过（990）；
   - `if Length(kw) > Length(item.ShortCut) then Continue`（993，**在正则执行前按“模式串长度 > 目标长度”剪枝**，因此包含 `.*` 的模式可能被过早丢弃，属必须复刻的怪癖）；
   - `m_Regex.Expression := kw`；`if not m_Regex.Exec(LowerCase(item.ShortCut)) then Continue`（997-999）；异常时 `Result := False` 但**并不退出**（1000-1003，状态可疑，见 10 节）；
   - `if (not MatchAnywhere) and (m_Regex.MatchPos[0] > 1) then Continue`（1005-1007）——`MatchAnywhere` 默认 True（`untALTRunOption.pas:753`），即允许从任意位置开始匹配；
   - `Item.Rank := m_Regex.MatchPos[0]`（1-based 匹配起始位置，1009）。
3. 命中（`Item.Rank > 0`）后计算排序分（1026，常数见 8 节）：
   ```
   Item.Rank := 1024 + Item.Freq*4 - Item.Rank*128 - (Length(Item.ShortCut) - Length(kw))*16
   StringList.AddObject(Format(ListFormat,[Item.ShortCut, Item.Name]), Item)
   ```
   含义（隐式）：频率越高越靠前；匹配位置越靠前越靠前；快捷名越短越靠前。
4. 对 StringList 执行 `QuickSort(...,0,Count-1)`（1040）——**降序**（Rank 大在前）。

#### 3.3.3 非正则模式（`EnableRegex=False`）

- `Item.Rank := Pos(LowerCase(kw), LowerCase(item.ShortCut))`（1-based；0 表示不含，1013）；
- `MatchAnywhere=False` 时要求 `Rank=1`（必须前缀匹配，1016）；
- 命中后同样用 3.3.2 的 rank 公式并排序。
- 即“包含匹配/前缀匹配”，大小写不敏感（通过 `LowerCase` 实现，AnsiString 语义）。

#### 3.3.4 匹配后的通用后处理

1. **收藏置顶**（1045-1054）：`RememberFavouratMatch=True`（默认 False，`untALTRunOption.pas:756`）且收藏表 `FavoriteList` 中存在键 `LowerCase(kw)` 时，在结果中找到 `Name` 等于收藏值的项并 `Move(i,0)` 移到第一项（只移动第一命中项）。
2. **数字前缀**（1058-1081）：`EnableNumberKey=True`（默认 True）时给前 10 项加显示前缀（见 5.1）。
3. 返回 `StringList.Count > 0`。

#### 3.3.5 MatchAnywhere 语义小结

| 模式 | `MatchAnywhere=True` | `MatchAnywhere=False` |
|---|---|---|
| 非正则 | `Pos(kw, shortcut) > 0` 任意位置 | 必须 `Pos=1`（前缀） |
| 正则 | `TRegExpr.Exec` 任意位置 | 必须 `MatchPos[0]=1` |

### 3.4 初始排序（`Sort`，`untShortCutMan.pas:1894-1929`）

`LoadShortCutList` 解析完文件后调用 `Sort` 重建 `m_SortedShortCutList`：

```
for 每个 scItem:
    Rank := Freq * 1000
    k := 8
    for j := 1 .. Length(ShortCut):
        k := k div 2                     // j=1→4, j=2→2, j=3→1
        if j > 3 then Break
        Rank := Rank - k * (Ord(UpCase(ShortCut[j])) - Ord('A'))
    Rank := Rank - Length(ShortCut)
    m_SortedShortCutList.AddObject(Format(ListFormat,[ShortCut,Name]), Item)
QuickSort(m_SortedShortCutList, 0, Count-1)      // Rank 降序
```

效果：频率主导；前 3 个字符按字符码参与微调（大写化后与 `'A'` 的差值），数字/符号会因差值为负而获得加分；最后按长度微调（短者优先）。`Sort` 只在加载/重载列表时执行（1532），后续搜索用 3.3 的公式重算 `Rank`。

### 3.5 排序实现细节（`QuickSort`/`BubbleSort`/`Partition`，`untShortCutMan.pas:1636-1744, 1230-1259`）

- `QuickSort`：`Q_MIN = 12`；`if r-p <= 12 then BubbleSort else` 随机支点 `SelectPivot`（`Randomize; p + Random(r-p+1)`，1830-1837）后递归。
- `Partition`：降序划分（右指针找 `Rank >= Pivot`，左指针找 `Rank <= Pivot`）。
- `BubbleSort`：仅当 `左.Rank < 右.Rank` 时交换（即降序冒泡）。
- 结论：**排序是不稳定排序且支点随机**，同分项的相对次序不可保证（小列表 ≤13 项走冒泡，同分项基本保持原序；大列表随机）。1:1 重写时若要求完全一致，需接受“同分不定序”。

### 3.6 匹配高亮

**源码中不存在任何匹配高亮实现**：`lstShortCut` 是普通 `TListBox`（非 OwnerDraw，DFM 3339-3366 无 `Style`/`OnDrawItem`），`frmALTRun.pas` 中也没有 `Canvas.TextOut`/`DrawItem`/`Font.Color` 着色代码。历史版本可能用 `TitleColor/KeywordColor/ListColor`，但当前 `LoadSettings` 主动删除这三个配置键（`untALTRunOption.pas:784-786`），且变量不再存在。→ **C++ 重写时不需要复刻高亮；如产品要求高亮，属于新增功能。**

### 3.7 “数字键/空格选择”特例（`frmALTRun.pas:1411-1471`）

只有在**本次过滤结果为 0 条**时进入；使用的是**上一次非空列表**的缓存：

- 缓存结构：`m_LastShortCutPointerList: array[0..9] of Pointer`（最多 10 个 `TShortCutItem*`）、`m_LastShortCutListCount`、`m_LastShortCutCmdIndex`（148-150）；由 `GetLastCmdList` 填充（2350-2378）：仅当当前 `lstShortCut.Count>0` 时复制前 min(10,Count) 个对象指针，并记录 `ItemIndex`。
- **数字键**（1418-1450）：条件 `EnableNumberKey and m_LastKeyIsNumKey and 文本末字符 ∈ '0'..'9'`：
  - `k := 末字符值`；
  - `IndexFrom0to9=True`：`if k <= m_LastShortCutListCount-1` → 执行 `m_LastShortCutPointerList[k]`，`KeyWord = Copy(文本,1,Len-1)`（去掉刚输入的数字）；即显示编号 `0..9` 与下标一致。
  - `IndexFrom0to9=False`（默认）：`if k=0 then k:=10`；`if k <= m_LastShortCutListCount` → 执行 `m_LastShortCutPointerList[k-1]`；即显示编号 `1..9,0`。
  - 执行前 `evtMainMinimize`，执行后清空输入框；**此分支不处理 `ExitWhenExecute`**（与下面空格分支不同，属怪癖）。
  - `m_LastKeyIsNumKey` 在每次 `edtShortCutKeyDown` 开头置 False、在数字键分支置 True（1503, 1564）。
- **空格键**（1452-1470）：文本末字符为空格，且 `m_LastShortCutListCount>0` 且 `0 <= m_LastShortCutCmdIndex < Count` → 执行 `m_LastShortCutPointerList[m_LastShortCutCmdIndex]`（即上一次高亮项），`KeyWord = Copy(文本,1,Len-1)`；清空输入框；**`ExitWhenExecute` 时启用 `tmrExit`**（1467）。
- 说明：空格/数字特例只在“文本变化后结果为空”的那一次变化内有效（随后 `GetLastCmdList` 会把缓存清零/置 -1，1491）。因此正常使用方式为“关键字有匹配 → 输入空格或数字使结果为空 → 触发执行”。

### 3.8 `ExecuteIfOnlyOne`

`ExecuteIfOnlyOne=True`（默认 False）且过滤结果恰好 1 条时，输入变化会立即执行该项（1480-1484），**不等待回车**。

---

## 4. 键盘交互完整表

### 4.1 分发顺序（决定优先级）

1. 主窗体 `KeyPreview=True`（DFM 720）⇒ 窗体的 `OnKeyDown`（`FormKeyDown`）先于焦点控件收到按键；`OnKeyPress`（`FormKeyPress`）同理，先于控件。
2. `TApplicationEvents`（`evtMain`）的 `OnMessage` 在消息循环中观察所有消息（滚轮等，`evtMainMessage`）；`OnShortCut` 处理过程为空（1843-1846），不改变 VCL 的快捷键分发。
3. 焦点控件各自的事件：`edtShortCutKeyDown`（输入框）、`lstShortCutKeyDown`（列表）、`edtCommandLineKeyDown`（命令栏）。
4. `TActionList` 的 `SecondaryShortCuts`（Tab/Shift+Tab/Alt+S/Alt+C，DFM 8590-8644）由 VCL 快捷键机制处理。
5. 全局热键（RegisterHotKey）独立于焦点，由隐藏窗口接收 `WM_HOTKEY`。

### 4.2 按键行为表

> 焦点默认在 `edtShortCut`；`ShowOperationHint` 为真且列表项显示串第 3 个字符（`Items[i][2]`）是数字时，绝大多数按键/上下移动后会更新提示行为 `resRunNum`（`Format(resRunNum,[digit,digit])`，1658-1664、605-611）。

| 按键 / 组合 | 生效条件 | 行为 | 源码 |
|---|---|---|---|
| **字符键（含中文/粘贴）** | 输入框 | 默认 TEdit 行为，触发 `OnChange` 重新过滤 | DFM 3442 |
| **数字 0-9 / 小键盘 0-9** | 输入框，无 Ctrl/Alt | 标记 `m_LastKeyIsNumKey=True`，字符进入输入框；若过滤后列表为空则按 3.7 执行上一次列表第 N 项 | 1497-1505, 1562-1586, 1418-1450 |
| **Ctrl+数字 / Alt+数字** | 输入框（任意位置） | 直接执行当前匹配列表的第 N 项（映射见 4.3），`Index` 越界则不执行；先隐藏窗口，执行后清空输入框 | 1566-1585 |
| **; （VK 186，分号键）** | 输入框，无需修饰键 | 执行列表下标 2（即显示编号 No.2 或 No.3，视 `IndexFrom0to9`） | 1589-1608 |
| **' （VK 222，单引号键）** | 输入框，无需修饰键 | 执行列表下标 3 | 1589-1608 |
| **Ctrl+D** | 输入框 | `KillMessage(Handle, WM_CHAR)`（阻止字符输入）；当前项可打开目录（`DirAvailable`）时隐藏窗口并打开所在文件夹；否则不动作 | 1610-1624 |
| **Ctrl+C** | 输入框 | 复制当前项 `CommandLine` 到剪贴板（`Clipboard.AsUnicodeText`），显示 `edtCopy`（★）250 ms | 1626-1636；548-557；8963-8969 |
| **Ctrl+L** | 输入框 | 显示“最近使用列表”（最多 10 项，来自 `m_LatestList`）并 `KillMessage WM_CHAR` | 1638-1646；2640-2685 |
| **PgUp / PgDn** | 输入框 | `Key:=VK_NONAME`，`PostMessage(lstShortCut.Handle, WM_KEYDOWN, ...)` 转发给列表滚动 | 1547-1559 |
| **↑ / ↓** | 主窗体（KeyPreview，`edtCommandLine` 有焦点时除外） | 列表选择循环上/下移（-1→0/末尾、0→末尾、末尾→0）；更新 `lblShortCut`/`edtCommandLine`；`m_LastShortCutCmdIndex := ItemIndex`；更新数字提示 | 2186-2226（另 584-613、1155-1184 的 actUp/actDown 同逻辑） |
| **Enter（#13）** | 主窗体 `KeyPress`，且 `Visible` | `Key:=#0`；`actExecuteExecute` 执行当前选中项 | 2299-2304 |
| **Esc（VK_ESCAPE）** | 主窗体 `KeyDown` | `Key:=VK_NONAME`；输入框为空→隐藏窗口；非空→清空输入框 | 2255-2264 |
| **Esc** | 输入框（经列表/命令栏转发路径） | 同上的清空/隐藏逻辑（与窗体处理重复；正常焦点在输入框时窗体先处理） | 1648-1655 |
| **F1** | 主窗体 | 关于对话框（`actAboutExecute`） | 2228-2232 |
| **F2** | 主窗体 | 编辑当前项（`actEditItemExecute`） | 2234-2238 |
| **Insert** | 主窗体 | 新增快捷项（`actAddItemExecute`） | 2240-2244 |
| **Delete** | 主窗体，且列表有选中项 | 删除当前项（带确认，`actDeleteItemExecute`） | 2246-2253；559-582 |
| **Tab** | ActionList 二级快捷键 | `actDown`：下一项（循环） | DFM 8640-8644；584-613 |
| **Shift+Tab** | ActionList 二级快捷键 | `actUp`：上一项（循环） | DFM 8634-8638；1155-1184 |
| **Alt+S** | ActionList 二级快捷键 | 打开快捷项管理窗口 | DFM 8587-8592；857-971 |
| **Alt+C** | ActionList 二级快捷键 | 打开配置窗口 | DFM 8594-8599；253-546 |
| **Alt+F4 / 关闭按钮** | `WM_SYSCOMMAND SC_CLOSE`（非 DEBUG） | 仅隐藏窗口并清空输入；DEBUG 模式才真正关闭程序 | 1794-1804 |
| **鼠标滚轮** | `m_IsTop=True` 时（应用消息） | 向上滚→`PostMessage(lst, VK_UP)`，否则 `VK_DOWN`（方向判断写法存疑，见 10 节） | 1816-1827 |
| **鼠标中键** | 背景图/标题标签/三个按钮/输入框/提示框/命令栏 | 执行当前选中项 | 1356-1363, 2442-2454, 2465-2477, 3420-3446 等 |
| **鼠标中键** | 结果列表 | 执行当前选中项（不按坐标选中） | 2525-2541 |
| **鼠标右键** | 结果列表 | 先模拟左键按下以选中该项（`WM_LBUTTONDOWN`）并更新标题/命令栏，再弹出 `pmList` | 2525-2535；DFM 8657-8676 |
| **双击** | 结果列表 / 标题标签 | 执行当前选中项 | DFM 3093, 3362；662-753 |
| **单击/双击托盘** | 托盘图标 | 首次：AutoHideForm 提示；之后切换显示/隐藏 | 2543-2573 |
| **列表内其它键** | `lstShortCut` 有焦点 | 除 Enter、PgUp/PgDn 外：`PostMessage(edtShortCut.Handle, WM_KEYDOWN, Key)` 并把焦点交回输入框 | 2479-2514 |
| **命令栏内其它键** | `edtCommandLine` 有焦点 | 除 Enter、PgUp/PgDn 外：转发 `WM_KEYDOWN` 给输入框并聚焦 | 1331-1354 |
| **全局热键 1/2**（默认 `Alt+R` / `Pause`） | 任意前台程序 | 显示↔隐藏切换（同时抓取剪贴板/前台窗口信息） | 2411-2440 |
| **全局热键 3**（默认 `ALT+L`） | 任意前台程序 | 直接执行“最近使用列表”第 0 项（不显示窗口、不更新频率/最近列表） | 2380-2409 |

### 4.3 Ctrl/Alt+数字与分号/引号的索引映射

`Index` 计算：小键盘 `Key>=96 ⇒ Key-96`，主键盘 `Key-48`（1568-1571）。范围检查：
- `IndexFrom0to9=True`：`Index > Count-1` ⇒ 不执行（0-based）；
- `IndexFrom0to9=False`（默认）：`Index > Count` ⇒ 不执行。

执行映射：
- `IndexFrom0to9=True` → `lstShortCut.Items.Objects[Index]`（Ctrl+0 = 第 1 项，Ctrl+1 = 第 2 项……）；
- `IndexFrom0to9=False` → `lstShortCut.Items.Objects[(Index+9) mod 10]`（1→第 1 项，2→第 2 项，…，9→第 9 项，0→第 10 项）。

分号/单引号固定 `Index := 2/3` 走同一映射，即“执行显示编号 2 / 3 的项”（1588-1608）。

### 4.4 其它键盘相关说明

- `edtShortCutKeyPress`（1667-1680）**未在 DFM 绑定且首句 `Exit`**，为死代码（DFM 3420-3446 无 `OnKeyPress`）。
- `edtShortCutKeyDown` 中 `VK_UP/VK_DOWN` 分支被整段注释（1509-1546），上下键统一由 `FormKeyDown` 处理。
- `evtMainShortCut` 为空实现（1843-1846），但 ActionList 的 `SecondaryShortCuts` 仍由 VCL 正常分发。
- 输入框获得焦点时 `FormKeyDown` 对未处理键会再次 `edtShortCut.SetFocus`（2279-2287），属防御性代码。
- 隐藏窗口（`m_IsShow=False`）时 `FormKeyDown` 的转发行为基本不生效（窗口不可见/无焦点）。

---

## 5. 结果列表内容与行为

### 5.1 列表项文本格式

`FilterKeyWord`/`Sort`/`GetLatestShortCutItemList` 生成文本的方式（`untShortCutMan.pas:1030, 1133, 1923`）：

```
文本 = Format(ListFormat, [Item.ShortCut, Item.Name])
```

- `ListFormat` 默认 `'%-25s| %s'`（`untALTRunOption.pas:99`），配置可选：
  `'%-25s| %s'`、`'%s (%s)'`、`'%s [%s]'`（`untALTRunOption.pas:659-663`）。注意 `%-25s` 为 **AnsiString 字节宽度**（中文名按 2 字节计），与 C++ Unicode 的“字符宽度”不同。
- 当 `EnableNumberKey=True` 时再前缀化（`untShortCutMan.pas:1058-1081`）：
  - 前 10 项且 `IndexFrom0to9=True`：`Format(' %d|%s', [i, 文本])`（`ParamType=ptNone`）或 `Format('*%d|%s', [i, 文本])`（有参数类型）；
  - 前 10 项且 `IndexFrom0to9=False`：编号为 `(i+1) mod 10` → 1,2,…,9,0；
  - 第 11 项起：`Format('  |%s', [文本])`。
- `*` 前缀表示该快捷项 `ParamType <> ptNone`（执行时需要参数）。
- 由于前缀固定占 4 字符（空格+数字+`|`），因此提示逻辑用 `Items[ItemIndex][2]` 取显示编号（`frmALTRun.pas:608, 1661`）。

### 5.2 TopTen 与条数上限

- `ShowTopTen` 默认 True（`untALTRunOption.pas:761`），为 True 时最多只把过滤结果的前 **10** 条放进列表（`frmALTRun.pas:1394-1401`）；为 False 时放全部（1403）。
- 最近列表（Ctrl+L / 热键3）最多 10 项（`MAX_LATEST_NUM=10`，`untShortCutMan.pas:27, 489-506`；`GetLastCmdList` 也只缓存 10 个，2364-2367）。

### 5.3 行高、高度、滚动

- 行高 `ItemHeight=16`（DFM 3354），列表字体宋体 -16。
- 首次显示把列表高度设为 `10 * ItemHeight = 160` px（1101），即**一屏 10 行**；DFM 设计高度 164。
- 条目超过可视行数时由 `TListBox` 默认行为出现垂直滚动条，无自定义滚动/分页逻辑。
- 列表无列（非 `TListView`）、单列文本；无图标、无复选框、无自定义绘制。

### 5.4 收藏（Favorite）机制

- 收藏文件 `FavoriteList.txt`，格式由 `SaveFavoriteList` 写入：`Format('%-30s|%-30s%', [LowerCase(名称), 值])`（`untShortCutMan.pas:1778-1779`）；读取时按行 `SplitString(...,'|',...)` 写入 `m_FavoriteList.Values[Trim(ParamItem[0])] := Trim(ParamItem[1])`（1308-1316）。
- 记录时机：**每次通过主窗口执行**（带关键字的重载）→ `m_FavoriteList.Values[LowerCase(KeyWord)] := item.Name`（2071-2074）。热键3 执行、`Execute(item)` 重载不会记录。
- 使用时机：`RememberFavouratMatch=True` 且当前关键字在收藏表中存在同名记录时，把匹配项中 `Name` 等于收藏值的项移到第 1 位（`untShortCutMan.pas:1045-1054`）。
- **没有“收藏标记/星标”图形或文字**：列表里看不到收藏状态；`*` 前缀表示“需要参数”，不是收藏。
- 收藏文件在 `TShortCutMan.Destroy` 时统一保存（`untShortCutMan.pas:669`）。

### 5.5 图标

- 主窗口结果列表**不显示任何图标**（见 3.6/2.1）。`tlHotRun` ImageList 仅用于 `pmMain`/`actlstMain` 的菜单项图标（DFM 8555, 8575）。
- 快捷项管理窗口使用**静态 ImageList**（`ilShortCutMan`）的两种图标：`siItem`（第 0 幅，有效快捷项）/ `siInfo`（第 1 幅，空行），通过 `ImageIndex := Ord(siItem/siInfo)`（`frmShortCutMan.pas:131,139,362,739,747`；`untShortCutMan.pas:906,916`）。**整个项目中未发现“从 exe 提取图标”（ExtractIcon/SHGetFileInfo）的调用**；`.lnk` 解析仅用于取出目标路径（`untUtilities.pas:859` `ResolveLink`）。
- 托盘图标为组件内嵌多尺寸 ICO（DFM 3486-8545）。

### 5.6 选中项与命令栏

- 选中变化（`actSelectChangeExecute`，843-855 及 `DisplayShortCutItem` 1321-1329）：
  - `lblShortCut.Caption := Item.Name`；`lblShortCut.Hint := Item.CommandLine`；
  - `edtCommandLine.Text := resCMDLine + Item.CommandLine`（`resCMDLine` 默认 `'CMD='`，`untALTRunOption.pas:943`）；
  - `DirAvailable` 为真时标题两侧加 `[ ]`（表示可按 Ctrl+D 打开目录）。
- `DirAvailable`（1256-1319）判定：去掉 `@+`/`@-`/`@` 前导标志、去掉成对引号；含 `\\`（UNC）直接 False；`FileExists(命令行)` → True；以 `.\` 开头 → True；否则取最后一个 `\` 前的路径 `DirectoryExists` 判断（命令行以 `"` 开头时从第 2 字符取到 `slashpos-1`）。
- 命令栏只读、不可编辑；其右键菜单为空（见 7.2）。

### 5.7 提示行 `edtHint`（`RefreshOperationHint`，2581-2618）

- `ShowOperationHint=False`（默认 True）→ 隐藏。
- 否则显示：
  - 输入为空：`Randomize` 后随机取一个非空 `HintList[i]`（25 条，`untALTRunOption.pas:641-655`，可被 `.lang` 覆盖，963-987）；
  - 长度 <6：无结果 → `resKeyToAdd`；`DirAvailable` → `resKeyToOpenFolder`；否则 `resKeyToRun`；
  - 长度 ≥6：隐藏。
- 另有数字提示：`ShowOperationHint and ItemIndex>=0 and Length(文本)<10 and 显示串[2] ∈ '0'..'9'` → `edtHint.Text := Format(resRunNum, [digit,digit])`（1658-1664；actUp/actDown 中 605-611、1176-1182）。

### 5.8 最近使用列表（Ctrl+L）

`ShowLatestShortCutList`（2640-2685）：清空界面 → `ShortCutMan.GetLatestShortCutItemList(StringList)`（按 `m_LatestList` 顺序 `Format(ListFormat)`，`untShortCutMan.pas:1113-1163`）→ 赋给列表 → `m_NeedRefresh := True` → 选中第 0 项并更新标签/命令栏/`[]` 标记 → `GetLastCmdList` → `RefreshOperationHint`。该列表**不受关键字过滤**，且下次输入变化会重新过滤。

---

## 6. 执行流程

### 6.1 `actExecuteExecute`（662-753）

1. `if lstShortCut.Count > 0` 才执行（只看 Count，**不检查 ItemIndex**；若 `ItemIndex=-1` 则后续 `Items.Objects[-1]` 有越界风险，见 10 节）。
2. 先 `evtMainMinimize(Self)`：隐藏主窗口、停止计时器（677）。
3. `ShortCutMan.Execute(TShortCutItem(lstShortCut.Items.Objects[lstShortCut.ItemIndex]), edtShortCut.Text)`（731）——带关键字重载（记录收藏、最近、频率）。
4. `edtShortCut.Text := ''`（734）。
5. `if ExitWhenExecute then tmrExit.Enabled := True`（745；约 1 秒后退出程序）。
6. 若列表为空：弹 `resNoItemAndAdd`（“No such item \"%s\", add this shortcut?”，MB_OKCANCEL+MB_ICONQUESTION，注意**没有 MB_TOPMOST**）；确认则 `actAddItemExecute`（749-752）。

### 6.2 `TShortCutMan.Execute(item)`——命令、工作目录、参数（`untShortCutMan.pas:673-797`）

1. 仅处理 `ShortCutType=scItem`（682）。
2. 建 `TCmdObject`（Command/WorkingDir/Param/ParamType），`Command := item.CommandLine`，再 `ReplaceEnvStr`（694-697）。
3. `str := RemoveQuotationMark(Trim(item.CommandLine), '"')`（700）：仅当首尾都是 `"` 且内部无 `"` 时才去引号（`untUtilities.pas:222-241`）。
4. 工作目录推导（702-735）：
   - 若 `str` 不是以 `\\`（UNC）开头：
     - `FileExists(str)` → `WorkingDir := ExtractFileDir(str)`；
     - 否则若 `str[1]='"'`（原文注释示例 `"C:\...\firefox.exe" -Profile "Profiles"`）：去掉开头引号，找下一个 `"` 前的内容作为 `TrueCmdFile`；若该文件存在 → `Command := TrueCmdFile`、`WorkingDir := ExtractFileDir(TrueCmdFile)`、`Param := Trim(第一个"之后剩余)`。
5. 参数处理（737-796）：
   - `ParamType = ptNone`：
     - 若步骤 4 解析出了内嵌参数（`cmdobj.Param <> ''`）→ `ParamType := ptNoEncoding`；否则保持 `ptNone`；
     - `Execute(cmdobj)`。
   - 其它：按命令行中的标志决定 `Param` 来源：
     - 含 `{%c}` → `Param := m_Param[0]`（热键按下时的剪贴板文本）；
     - 含 `{%wd}` → `m_Param[1]`（前台窗口句柄字符串）；
     - 含 `{%wt}` → `m_Param[2]`（前台窗口标题）；
     - 含 `{%wc}` → `m_Param[3]`（前台窗口类名）；
     - 否则弹 `ParamForm`（Caption=项名称），`mrCancel` 则放弃执行（`cmdobj.Free; Exit`），`Param := ParamForm.cbbParam.Text`；
     - 编码：`ptURLQuery → GetURLQueryString(Param)`；`ptUTF8Query → GetUTF8QueryString(Param)`；`ptNoEncoding` 不编码（`untUtilities.pas:1117,1136`）。
   - `Execute(cmdobj)`。

### 6.3 `ExecuteCmd`——真正启动（`untShortCutMan.pas:159-359`）

1. **显示方式前缀**（170-185）：
   - `@+` → `SW_SHOWMAXIMIZED`；`@-` → `SW_SHOWMINIMIZED`；`@` → `SW_HIDE`；前缀用 `CutLeftString` 去掉后再 `Trim`。
2. **相对路径**（187-189）：命令行含 `.\` 或 `..\` ⇒ `WorkingDir := ExtractFileDir(Application.ExeName)`。
3. **参数替换**（191-336），分三种 `ParamType`：
   - `ptNone`：`PCommandStr := Command`；`PParamStr := nil`；`PWorkingDir := WorkingDir`。命令串原样作为 `lpFile` 传给 ShellExecute（**没有**任何“引号包裹/拆分”逻辑）。
   - `ptNoEncoding`：按 `{%p}` → `%p` → `{%c}` → `{%wd}` → `{%wt}` → `{%wc}` 的 **if/else 顺序**检查，命中第一个存在的标志后，用正则 `TRegExpr.Replace` 把它（该标志的**全部出现**）替换为 `Param`，`PParamStr := nil`；
     - 若上述标志一个都不存在 → 不替换，`PParamStr := Param`（作为**独立参数**传给 ShellExecute）。
   - `ptURLQuery` / `ptUTF8Query`：只检查 `{%p}` → `%p` → `{%c}` 三个标志（**不检查** `{%wd}/{%wt}/{%wc}`）；
     - 命中 → 正则替换，`PParamStr := nil`；
     - 都未命中 → `PCommandStr := Command + Param`（**直接字符串拼接，无分隔符、无编码**），`PParamStr := nil`，`PWorkingDir := nil`。
   - 注：`TRegExpr.Replace` 为**全局替换**（`3rdUnit\regexpr\RegExpr.pas:3780-3797`），第三参 `False` 表示不解释 `$1` 替换符。
4. **环境变量替换**（338-339）：`PCommandStr := ReplaceEnvStr(...)`：正则 `(%)(.*?)(%)` 取**第一个** `%...%`，若该环境变量存在则 `StringReplace` 替换全部同名变量（`untShortCutMan.pas:1746-1756`）。**只处理一种变量名**，多个不同变量不会全部替换。
5. **启动**（341-355）：
   ```pascal
   ret := ShellExecute(GetDesktopWindow, nil, PCommandStr, PParamStr, PWorkingDir, ShowCmd);
   if ret < 33 then
   begin
     ret := WinExec(PCommandStr, ShowCmd);
     if ret < 33 then
       Application.MessageBox(Format(resCanNotExecute, [PCommandStr, PParamStr]),
         resWarning, MB_OK+MB_ICONWARNING);
   end;
   cmdobj.Free;
   ```
   - **不用 `CreateProcess`**：采用 `ShellExecute`（`GetDesktopWindow` 作 owner），失败回退 `WinExec`。
   - 失败提示文案默认 `'Can not execute "%s %s"'`（`untALTRunOption.pas:934`）。
6. 执行发生在**新线程**里（见 6.7），`cmdobj` 在线程内释放。

### 6.4 参数/标志速查

| 标志 | 含义 | 取值来源 | 位置 |
|---|---|---|---|
| `%p` | 位置参数（旧式） | ParamForm 或已有参数 | `untALTRunOption.pas:27` |
| `{%p}` | 位置参数（新式） | 同上 | 28 |
| `{%c}` | 剪贴板文本 | 热键按下瞬间的 `Clipboard.AsUnicodeText` | 29；`frmALTRun.pas:2424` |
| `{%wd}` | 前台窗口句柄（十进制字符串） | `IntToStr(GetForegroundWindow)` | 30；2427 |
| `{%wt}` | 前台窗口标题（`GetWindowText`，最长 255） | 2430 | 31 |
| `{%wc}` | 前台窗口类名（`GetClassName`，最长 255） | 2433 | 32 |
| `@+` / `@-` / `@` | 最大化 / 最小化 / 隐藏启动 | 命令行前缀 | 33-35 |
| `.\` / `..\` | 相对当前 exe 目录 | 命令行内容 | `untShortCutMan.pas:188` |

> 注意：`@` 前缀与 `DirAvailable`/`actOpenDirExecute` 中“去掉前缀”的判断是 `Pos(SHOW_MAX_FLAG, CommandLine)=1` 之类的**前缀判断**，三个标志按 `@+`、`@-`、`@` 顺序判断（`frmALTRun.pas:788-793, 1273-1278`；`untShortCutMan.pas:171-185`）。

### 6.5 参数输入窗 `ParamForm`（`frmParam.pas`；调用 `untShortCutMan.pas:771-781`）

- 全局单例在 `dpr` 中先创建（`ALTRun.dpr:47`），避免每次新建。
- 显示为模态，`Caption := item.Name`；`cbbParam` 为可编辑组合框，历史记录来自 `ParamHistory.txt`，容量由 `ParamHistoryLimit`（默认 50，`untALTRunOption.pas:757`）控制；按回车确认（`frmParam.pas:129-134`）。
- 取消（`mrCancel`）⇒ 整个执行取消。
- 主窗口显示时会把 `ParamForm.ModalResult := mrCancel`（`frmALTRun.pas:985`），防止上次取消状态残留。

### 6.6 `ExitWhenExecute`（默认 False）

- 执行后 `tmrExit.Enabled := True`（745），`tmrExit` 未设 Interval（VCL 默认 1000 ms）→ `tmrExitTimer` 调 `actCloseExecute` 真正退出进程（8957-8962, 2701-2707）。
- 只有主窗口“回车/点击执行”与“空格执行”路径会设置；数字键特例、热键3 路径不设置（怪癖）。

### 6.7 线程模型

- `TShortCutMan.Execute(cmdobj)`：`CreateThread(nil, 0, @ExecuteCmd, Pointer(cmdobj), 0, ThreadID)`（`untShortCutMan.pas:2089`），**每个执行请求一个工作线程**；`ShortCutRunCount` 自增，每 **10000** 次弹 `resCongratulations`（2092-2097）。
- `ExecuteCmd` 内使用 `Application.MessageBox`（VCL 非线程安全、且可能显示在主窗口之后）——重写时应改为向 UI 线程投递提示。
- `cmdobj` 由线程释放；主线程不再持有引用。

### 6.8 关于 `frmInvalid`

- 主窗口**不调用** `TInvalidForm`。`frmInvalid` 仅被快捷项管理窗口使用：`frmShortCutMan.pas:101`（uses）、`:331-345`（创建、显示“无效快捷项”列表）。
- 其功能：列出命令行为空的快捷项（`lvShortCut` 4 列：ShortCut/Name/ParamType/CommandLine），支持全选/取消全选/右键“测试执行”（`frmInvalid.pas:50-113`）。
- 主窗口的“执行失败提示”是 **`ExecuteCmd` 中的 `Application.MessageBox(resCanNotExecute)`**（`untShortCutMan.pas:352-353`），不是 `frmInvalid`。

---

## 7. 拖放、右键菜单、剪贴板

### 7.1 拖放

- **主窗口不支持文件拖放**：`frmALTRun.pas`/`.dfm` 中没有 `DragAcceptFiles`、`WM_DROPFILES` 处理、`OnDropFiles`、`BeginDrag`、`OnDragDrop`。
- 与“拖文件”等价的入口是**命令行参数**（“发送到”菜单/快捷方式把文件路径作为 `ParamStr(1)`，见 1.3）以及 `WM_SETTEXT` 消息转发。
- 项目其它窗体支持拖放：`frmShortCut.pas:255`（`DragAcceptFiles(Handle, True)`，编辑快捷项时拖文件）、`frmShortCutMan.pas:571`（管理窗口列表，`OnDragDrop=lvShortCutDragDrop`，DFM 110）。
- 拖放工具函数在 `untUtilities.pas:54-55, 1040-1058`（`GetDragFileCount` / `GetDragFileName`）。

### 7.2 右键菜单

| 菜单 | 挂载对象 | 菜单项 | 位置 |
|---|---|---|---|
| `pmMain` | `imgBackground`（734）、`lblShortCut`、托盘图标 | Show（`actShow`）/ShortCut（`actShortCut`）/Config（`actConfig`）/About（`actAbout`）/Close（`actClose`），带 `ilHotRun` 图标 | DFM 3062, 3088, 8547, 8554-8573 |
| `pmList` | `lstShortCut` | Add/Edit/Delete/分隔符 `-`/Open Dir；`OnPopup=pmListPopup` 时按 `DirAvailable` 决定 Open Dir 及其分隔符是否可见 | DFM 3358, 8657-8676；2575-2579 |
| `pmCommandLine` | `edtCommandLine` | **没有任何菜单项**；`OnPopup=pmListPopup`（会把 `pmList.mniOpenDir` 的可见性改掉，疑似 bug） | DFM 3387, 8983-8987；2575-2579 |

### 7.3 剪贴板

- 组件：项目自带的修改版 VCL 剪贴板 `Unit\untClipboard.pas`，`TClipboard.AsUnicodeText` 读写 `CF_UNICODETEXT`（`untClipboard.pas:297-351`），普通 `AsText` 用 `CF_TEXT`（266-277）。
- **读**：主/副热键与热键3 被按下时，先 `ShortCutMan.Param[0] := Clipboard.AsUnicodeText`（2388, 2424），供 `{%c}` 参数项使用。**抓取时机是热键按下瞬间**（主窗口显示之前），即“上一个前台程序”的剪贴板内容。
- **写**：`Ctrl+C` → `actCopyCommandLineExecute`：`Clipboard.AsUnicodeText := item.CommandLine`；显示 `edtCopy`（★），250 ms 后由 `tmrCopy` 隐藏（548-557, 2695-2699）。
- `Ctrl+C` 仅复制当前项的**原始 CommandLine**（不做参数替换、不含环境变量展开）。
- 输入框本身是 `TEdit`，标准 Ctrl+V 粘贴未被拦截，可通过粘贴输入关键字（`edtShortCutKeyPress` 未绑定，无自定义粘贴逻辑）。

---

## 8. 关键硬编码常量与“魔法数字”清单

| 常量/数值 | 含义 | 位置 |
|---|---|---|
| `WM_USER+2000` / `WM_USER+2001` | 自定义消息 AddShortCut / ShowWindow | `frmALTRun.pas:179-180` |
| `'ALTRUN_MUTEX'` | 单实例互斥体名 | `frmALTRun.pas:2029` |
| `'Restart'` / `'Clean'` | 命令行特殊参数 | `untALTRunOption.pas:37-38` |
| `Sleep(2000)` | Restart 参数等待旧实例退出 2 秒 | `frmALTRun.pas:1955` |
| `5` | 启动气泡提示秒数 | `frmALTRun.pas:2145-2147` |
| `HideDelay = 15`（秒） | 自动隐藏延时 | `untALTRunOption.pas:752` |
| `Delay * 1000` | 自动隐藏定时器毫秒换算 | `frmALTRun.pas:2635` |
| `250` ms | 复制 ★ 提示显示时长 | DFM 8965 |
| `500` ms | tmrFocus 间隔（死代码） | DFM 8972 |
| 1000 ms（VCL 默认） | tmrExit 退出延时 | DFM 8957-8962 |
| `10 * ItemHeight` | 列表高度 = 10 行 | `frmALTRun.pas:1101` |
| `ItemHeight = 16` | 行高（像素） | DFM 3354 |
| `250` / `230` | 主窗口高度（显示/隐藏命令行区） | `frmALTRun.pas:1104-1107, 528-531` |
| `420` | 默认窗口宽度 `FormWidth` | `untALTRunOption.pas:103`；DFM 6-7 |
| `240` | 默认 `Alpha`（0-255） | `untALTRunOption.pas:101` |
| `clBlack`(0) | 默认透明色键 | `untALTRunOption.pas:100` |
| `12` | 默认圆角半径 | `untALTRunOption.pas:102` |
| `1024`, `4`, `128`, `16` | 搜索 ranking 公式系数 | `untShortCutMan.pas:1026` |
| `1000` | 初始排序频率系数 | `untShortCutMan.pas:1909` |
| `8`, `2`, `3` | 初始排序前三字符的位权（8→4→2→1，循环 3 次） | `untShortCutMan.pas:1910-1919` |
| `Q_MIN = 12` | 快速排序转冒泡阈值 | `untShortCutMan.pas:1702, 1725` |
| `10` | TopTen 条数；最近列表上限；最近指针数组大小 | `frmALTRun.pas:1396, 2364-2367, 148`；`untShortCutMan.pas:27` |
| `10000` | 执行次数达到 1 万弹祝贺框 | `untShortCutMan.pas:2094` |
| `Length(edtShortCut.Text) < 10` | 显示数字提示的长度阈值 | `frmALTRun.pas:1660, 607, 1178` |
| `Length(edtShortCut.Text) < 6` | 提示行切换长度阈值 | `frmALTRun.pas:2604` |
| `'%-25s| %s'` | 默认 ListFormat | `untALTRunOption.pas:99` |
| `'%-30s|%-30s%'` | 收藏文件行格式 | `untShortCutMan.pas:1778` |
| `' %d|'` / `'*%d|'` / `'  |'` | 列表数字前缀格式 | `untShortCutMan.pas:1067-1080` |
| `★`（#9733） | 复制提示字符 | DFM 3416 |
| `1500` / `50` / `10` | AutoHideForm 停留 1500ms、淡出步进 50ms、每步 Alpha-10 | `frmAutoHide.dfm:47,54`；`frmAutoHide.pas:44-58` |
| `ParamHistoryLimit = 50` | 参数历史上限默认值 | `untALTRunOption.pas:757` |
| `ALTRUN_VERSION = 'V1.46.7'` | 版本 | `untALTRunOption.pas:22` |
| `DEBUG_MODE=False` / `DEBUG_SORT=False` | 调试开关（默认关闭） | `untALTRunOption.pas:16-17` |

---

## 9. 外部依赖单元与调用到的函数签名

> 仅列主窗口直接/间接强相关的函数与属性；完整声明见各源文件。

### 9.1 `untShortCutMan`（`TShortCutMan` 全局实例 `ShortCutMan`）

类型（`untShortCutMan.pas:28-59`）：

```pascal
TShortCutType = (scOther, scItem, scBlank, scRemark);          // 30
TParamType    = (ptNone, ptNoEncoding, ptURLQuery, ptUTF8Query); // 33
TCmdObject = class Command, WorkingDir, Param: string; ParamType: TParamType; end; // 38-44
TShortCutItem = class ShortCutType, ParamType, ShortCut, Name, CommandLine: ..., Rank, Freq: Integer; end; // 47-56
```

主窗口用到的方法（声明位置，实现位置）：

```pascal
function  FilterKeyWord(KeyWord: string; var StringList: TStringList): Boolean;   // 126 / 925
function  Execute(ShortCutItem: TShortCutItem): Boolean; overload;               // 140 / 673
function  Execute(cmdobj: TCmdObject): Boolean; overload;                        // 141 / 2079
function  Execute(ShortCutItem: TShortCutItem; KeyWord: string): Boolean; overload; // 142 / 2071
function  LoadShortCutList(FileName: string = ''): Boolean;                      // 88 / 1360
function  SaveShortCutList(FileName: string = ''): Boolean;                      // 89 / 1794
function  AddFileShortCut(FileName: string): Boolean;                            // 144 / 377
function  GetShortCutItemIndex(ShortCutItem: TShortCutItem): Integer;            // 123 / 1189
function  DeleteShortCutItem(Index: Integer): Boolean;                           // 122 / 636
function  GetLatestShortCutItemList(var StringList: TStringList): Boolean;       // 98 / 1113
function  GetLatestShortCutIndexList: string;                                    // 99 / 1091
function  SetLatestShortCutIndexList(IndexList: string): Boolean;                // 100 / 1839
property  NeedRefresh: Boolean;                                                  // 82
property  ShortCutList: TShortCutList;                                           // 81
property  ShortCutFileName: string;                                              // 80
property  Param[Index: Integer]: string;                                         // 83（内部 array[0..5]）
```

### 9.2 `untALTRunOption`（全局配置/资源/常量）

全局变量（主窗口读写）：`HotKeyStr1, HotKeyStr2, LastItemHotKeyStr, IsRunFirstTime, AutoRun, AddToSendTo, EnableRegex, HideDelay, MatchAnywhere, EnableNumberKey, IndexFrom0to9, RememberFavouratMatch, ParamHistoryLimit, ShowOperationHint, ShowCommandLine, ShowStartNotification, ShowTopTen, PlayPopupNotify, ExitWhenExecute, ShowSkin, ShowMeWhenStart, ShowTrayIcon, ShowShortCutButton, ShowConfigButton, ShowCloseButton, ExecuteIfOnlyOne, BGFileName, TitleColor/KeywordColor/ListColor(遗留), TitleFontStr, KeywordFontStr, ListFontStr, ListFormat, AlphaColor, Alpha, RoundBorderRadius, FormWidth, Lang, WinTop, WinLeft, ..., ShortCutRunCount, HandleID, ModifyTime, LatestList` 及全部 `res*` 字符串、`HintList`、`ConfigDescList`、`ListFormatList`（`untALTRunOption.pas:377-663`）。

过程/函数：

```pascal
procedure LoadSettings;                                        // 665 / 735
procedure SaveSettings;                                        // 666 / 822
procedure SetActiveLanguage;                                   // 667 / 894
function  FontStylesToStr(FontStyles: TFontStyles): string;    // 669
function  StrToFontStyles(strStyles: string): TFontStyles;     // 670
function  FontToStr(Font: TFont): string;                      // 671 / 703
function  StrToFont(strFont: string; Font: TFont): Boolean;    // 672 / 710
```

### 9.3 `untUtilities`（`untUtilities.pas`，interface 行号）

```pascal
17 function  CutLeftString(str: string; Num: Integer): string;
18 function  RemoveQuotationMark(str: string; QuotationMark: string): string;
19 procedure SplitString(str: string; Delimiter: string; var strList: TStringList);
21 function  IsRunningInstance(const MutexName: string): Boolean;
22 function  SetAutoRun(ApplicationName, ExecutablePath: string; AutoRun: Boolean = True): Boolean;
23 function  SetAutoRunInStartUp(ApplicationName, ExecutablePath: string; AutoRun: Boolean = True): Boolean;
24 function  GetAutoRunItemPath(ApplicationName: string): string;
25 function  GetFileModifyTime(const FileName: string): string;
26 function  SetFileModifyTime(const FileName: string; FileTime: TDateTime): Boolean;
28 function  GetFileSize(const FileName: string): Integer;
29 function  ReduceWorkingSize: Boolean;
32 function  AddMeToSendTo(Name: string; IsAdd: Boolean): Boolean;
35 function  GetSendToDir: string;
37 function  ResolveLink(ALinkFile: string): string;
38 function  IsNumericStr(const str: string): Boolean;
39 function  GetFileListInDir(var List: TStringList; const Dir: string; ...): Boolean;
41 function  ExtractRes(ResType, ResName, ResNewName: string): Boolean;
42 function  ReplaceEnvStr(str: string): string;
43 function  IsVista: Boolean;
46 function  RefreshEnvironmentVars: Boolean;
54 function  GetDragFileCount(hDrop: Cardinal): Integer;
55 function  GetDragFileName(hDrop: Cardinal; FileIndex: Integer = 1): string;
65 function  GetUTF8QueryString(Keyword: string): string;
66 function  GetURLQueryString(Keyword: string): string;
76 procedure SetClipboardText(AStr: string);
79 function  FindWindowByCaption(Caption: string): HWND;
81 procedure KillMessage(Wnd: HWnd; Msg: Integer);
83 function  SetLayeredWindowAttributes(hwnd: HWND; crKey: Longint; bAlpha: Byte; dwFlags: Longint): Longint;
```

### 9.4 `untClipboard`（VCL TClipboard 的改造版）

```pascal
TClipboard = class(TPersistent)                                // untClipboard.pas:61
  property AsText: string read GetAsText write SetAsText;      // 100
  property AsUnicodeText: string read GetAsUnicodeText write SetAsUnicodeText; // 101（CF_UNICODETEXT）
  procedure Open; procedure Close; function GetAsHandle(...); ...
end;
function Clipboard: TClipboard;                                 // 106
function SetClipboard(NewClipboard: TClipboard): TClipboard;    // 107
```

### 9.5 `HotKeyManager`（`THotKeyManager` 组件）

```pascal
constructor Create(AOwner: TComponent); override;               // 85 / 455
destructor  Destroy; override;                                  // 86 / 471
function  AddHotKey(HotKey: Cardinal): Word;                    // 87 / 489（返回 atom，0=失败）
function  RemoveHotKey(HotKey: Cardinal): Boolean;              // 89
function  RemoveHotKeyByIndex(Index: Word): Boolean;            // 90
procedure ClearHotKeys;                                         // 91 / 592
function  HotKeyValid(HotKey: Cardinal): Boolean;               // 92
property  OnHotKeyPressed: TOnHotKeyPressed;  //(HotKey: Cardinal; Index: Word)
function  TextToHotKey(Text: string; Localized: Boolean): Cardinal; // 101 / 331
function  HotKeyToText(HotKey: Cardinal; Localized: Boolean): string; // 100
procedure HookProc(var Msg: TMessage); virtual;                 // 83 / 614（内部 AllocateHWnd 窗口过程）
```

### 9.6 `CoolTrayIcon`（`ntfMain`）

```pascal
property IconVisible: Boolean;                                  // CoolTrayIcon.pas:245
property Hint: THintString;                                     // 242
property PopupMenu: TPopupMenu;                                 // 248
property MinimizeToTray: Boolean;                               // 253
property OnClick / OnDblClick: TNotifyEvent;                    // 257-258
function ShowBalloonHint(Title, Text: String; IconType: TBalloonHintIcon; Timeout: Integer): Boolean; // 210/1267
```

### 9.7 `untLogger`

```pascal
procedure InitLogger(IsTraceEnable: Boolean = False; IsCnDebugEnable: Boolean = False; IsAppendMode: Boolean = False); // 20
procedure TraceMsg(const Msg: string); overload;                 // 24
procedure TraceMsg(const AFormat: string; Args: array of const); overload; // 25
procedure TraceErr(...); overload;                               // 22-23
procedure AddLog(const str: string);                             // 21
```

### 9.8 其它窗体

| 单元 | 主窗口调用的入口/成员 | 位置 |
|---|---|---|
| `frmConfig` | `TConfigForm`：`DisplayHotKey1/2(HotKeyStr)`、`GetHotKey1/2`、`chklstConfig.Checked[0..19]`、`lblTitleSample/lblKeywordSample/lblListSample.Font`、`cbbListFormat`、`lstAlphaColor.Selected`、`seAlpha/seRoundBorderRadius/seFormWidth.Value`、`cbbLang`；`ShowModal`/`ModalResult`（`mrOk`/`mrRetry`/`mrCancel`） | `frmALTRun.pas:253-546` |
| `frmShortCut` | `TShortCutForm`：`lbledtShortCut`、`lbledtName`、`lbledtCommandLine`、`rgParam.ItemIndex`、`ShowModal` | `frmALTRun.pas:615-660`；`untShortCutMan.pas:377-487` |
| `frmShortCutMan` | `TShortCutManForm`：`lvShortCut`（TListView） | `frmALTRun.pas:938-970` |
| `frmAbout` | `TAboutForm`（`Caption`、`ShowModal`） | `frmALTRun.pas:193-218` |
| `frmLang` | `TLangForm`：`cbbLang` | `frmALTRun.pas:1883-1932` |
| `frmParam` | `TParamForm`：`cbbParam.Text`、`Caption`、`ShowModal`、`ModalResult` | `frmALTRun.pas:985`；`untShortCutMan.pas:771-781` |
| `frmAutoHide` | `TAutoHideForm`：`Caption`、`Info`、`ShowModal` | `frmALTRun.pas:2543-2573` |
| `frmInvalid` | 仅 `frmShortCutMan` 使用（主窗口不用） | `frmShortCutMan.pas:101,331-345` |

> `ALTRun.dpr` 还链接了 `ExceptionLog`（EurekaLog 异常处理单元，`ALTRun.dpr:5`）与 `pngimage`（`ALTRun.dpr:8`）；主窗口逻辑未直接调用。

---

## 10. 不确定项汇总（源码无法静态确认 / 需运行验证）

1. `evtMainMinimize` 中 `inherited;`（`frmALTRun.pas:1833`）的语义：`TALTRunForm` 未声明同名覆盖，按 Delphi 规则通常编译报错；但项目附带可执行文件，说明源码版本与实际编译版本可能略有差异。按可观察行为推断它只是“隐藏”，不影响功能。
2. Alt+数字/Alt+S 等组合键在 `KeyPreview` 下是否总能到达 `OnKeyDown`：代码注释声称“ALT+Num 或 CTRL+Num 都可以执行”（1561），但 `WM_SYSKEYDOWN` 经 VCL 分发可能与菜单加速键竞争；需要实机验证。
3. `MouseWheel` 方向判定（1818-1823）的实际效果（见 11.7-1）需实机确认。
4. `Form.Color=clMaroon` + `TransparentColor=True` 与 `SetLayeredWindowAttributes(AlphaColor=clBlack)` 的叠加效果（哪个颜色真正透明）需运行验证。
5. `actOpenDirExecute` 中 `FileExists(ExtractFileDir(...))` 分支（806-810）的原始意图与可达性。
6. 正则匹配异常后继续执行 `MatchPos` 的行为（`untShortCutMan.pas:999-1009`，`m_Regex` 可能处于失败状态）。
7. 中文/GBK 关键字在 `LowerCase`、`Pos`、`Format('%-25s')` 下的实际字节级行为（本机/旧系统差异），以及旧 `.lang`/INI 文件的确切编码（推测 ANSI/GBK）。
8. `ShowMeWhenStart=True` 时窗口在 `FormCreate` 期间 `Show` 的时机与任务栏/Z 序表现（`actShowExecute` 内部 `SetForegroundWindow` 可能受前台锁限制）。
9. `tmrFocus` 被设计成“重新抢焦点”但代码直接 `Exit`（2709-2735）；不同 Windows 版本下前台窗口行为不同，`SetForegroundWindow` 成功率无法静态确定。
10. `TConfigForm` 内部 `GetHotKey1/2`、`chklstConfig` 索引与显示顺序的对应关系仅从主窗口侧引用可确认索引含义（0..19 与选项的映射见 1.2/`frmALTRun.pas:279-317`），配置窗内的空值/校验逻辑需参见 `frmConfig.pas`（不在本文范围）。

---

*（本文档基于 2026-11-24 对 `ALTRun-original` 的只读分析；只读目录未被修改。）*

---

## 11. 重写注意点（Delphi/VCL 特有行为的 C++ 等价实现建议）

### 11.1 字符串编码

- 原项目是 **Delphi 2007（AnsiString）**：`string` 为系统 ANSI 代码页（中文 Windows = CP936/GBK）。所有 `Pos/Length/Copy/LowerCase/UpCase/Format('%-25s')` 都是**字节级**操作：
  - `Length('中文') = 4`（字节）；
  - `Pos` 可能在多字节字符中间“命中”；
  - `%-25s` 按字节补空格（中文名占 2 字节）；
  - `LowerCase/UpCase` 只可靠处理 ASCII。
- C++ 重写建议：内部统一 UTF-16/UTF-32 或 UTF-8，Win32 边界转换；**为 1:1 对齐“匹配/长度剪枝/格式化对齐”这些可观察行为，应显式定义**：以 UTF-16 码元还是以本地代码页字节做子串与长度比较。若要求与旧版结果完全一致（尤其含中文关键字/名称），需要按“本地代码页字节”模拟 `Pos/`、`Length` 与 `%Ns` 宽度；否则会出现对齐差异和极少数匹配差异。至少要在规格中明确选择。
- 文件格式（`ALTRun.ini`、`ShortCutList.txt`、`FavoriteList.txt`、`*.lang`）目前是 ANSI 文本；重写如需兼容存量数据，读写都要用 ACP（或先做一次性转换）。

### 11.2 搜索匹配引擎等价实现

- 需要复刻的非显然规则（务必逐条实现）：
  1. 关键字为空 → 返回“全量排序列表”；关键字为单个空格 → 返回空。
  2. `Length(关键字) > Length(ShortCut)` 直接跳过（**正则模式也先剪枝**）。
  3. 正则模式下 `*`→`.*`、`?`→`.`（`.*` 保护），对**小写化后的**模式与目标做 `TRegExpr` 搜索；`MatchPos[0]` 为 1-based 起点。
  4. `MatchAnywhere=false` 时要求起点=1（正则）或 `Pos=1`（非正则）。
  5. 排序分公式固定：`1024 + Freq*4 - pos*128 - (len(shortcut)-len(keyword))*16`，降序。
  6. 收藏置顶（`RememberFavouratMatch`）、数字前缀、TopTen 截断、`ExecuteIfOnlyOne`。
- 正则库：`TRegExpr`（`3rdUnit\regexpr\RegExpr.pas`）语法与 PCRE 不完全相同（如替换、`$` 语义、选项默认值）。C++ 用 `std::regex`（ECMAScript）需注意：`\d`/`\w`/字符类/贪婪/回溯大致兼容，但 `.` 默认不匹配换行（TRegExpr 同样默认 `ModifierS=False`，快捷名无换行，风险低）；对用户自写正则的极端语法差异建议记录为已知差异。
- 排序稳定性：Delphi 版 `QuickSort` 随机支点 + 不稳定冒泡；若要求结果顺序完全一致，需要接受“同分不定序”或用稳定排序并声明差异。
- `LowerCase` 对小写化后的中文没有意义；C++ 若用 `towlower` 会改变部分 Unicode 字符而不改变 GBK 字节——应只用 ASCII 大小写折叠来模拟。

### 11.3 焦点、消息循环、热键与键盘

- `KeyPreview=True` 的优先级必须复刻：**窗体级 KeyDown/KeyPress 先于控件**；Enter 是通过 `WM_CHAR`（OnKeyPress）触发的，不是 `WM_KEYDOWN`——重写时若只在 `WM_KEYDOWN` 处理 Enter，会改变与 Alt/IME 组合时的行为。
- `WM_SYSKEYDOWN`（Alt+数字/Alt+S 等）与普通 `WM_KEYDOWN` 的分发路径不同；Delphi 的 `TActionList` 二级快捷键由 VCL 在分发链前段处理。建议 C++ 直接用消息过滤/加速键表统一实现，并保持“Tab/Shift+Tab 切换列表、Alt+S/Alt+C 打开窗口”的既有优先级。
- 全局热键：`RegisterHotKey` + 专用隐藏窗口（`AllocateHWnd` 等价物：`CreateWindowEx` 的 message-only 窗口或 `HWND_MESSAGE` 子窗口）。注意原实现每个热键用 `GlobalAddAtom` 作 id，且 `HookProc` 末尾调用 `DefWindowProc`（不吞消息）；C++ 中热键 id 可用自定义整数，但要注意与其它热键冲突的处理与失败提示文案。
- Delphi 的 `VK_NONAME`（用于“吞键”）在 C++ 中没有直接对应；应改为置 `Handled=true`/返回 0 并 `return`，不要真的把手键改成怪值再传给控件。
- 托盘/主窗口隐藏时的“抢焦点”：原实现 `Application.Restore + SetForegroundWindow(Application.Handle)`，并在注释中承认经常失败（`tmrFocus` 因此废弃）。C++ 等价建议：使用 `AllowSetForegroundWindow`/`AttachThreadInput` 或把窗口的 `WS_EX_NOACTIVATE` 关掉再 `SetForegroundWindow`，并保留失败兜底。
- `TApplicationEvents.OnMessage` 是应用级消息观察器（`WM_MOUSEWHEEL` 依赖它）；C++ 在主消息循环里做 `PeekMessage` 过滤即可，但要注意“只在 `m_IsTop` 时响应滚轮”的既有条件。

### 11.4 窗口外观/DPI

- 无边框 + 置顶 + 分层窗口 + 颜色键 + Alpha + 圆角区域：`CreateWindowEx(WS_EX_TOPMOST|WS_EX_LAYERED, WS_POPUP, ...)`、`SetLayeredWindowAttributes(colorKey, alpha, LWA_ALPHA|LWA_COLORKEY)`、`SetWindowRgn(CreateRoundRectRgn(...))`。
- 原程序以 96 DPI 像素硬编码（420×250、行高 16、按钮 25×25）；VCL 在高 DPI 下会有缩放/字体替换行为，而源码中未做 DPI 适配。C++ 建议：要么固定 96 DPI 感知（`SetProcessDPIAware`）保持像素一致，要么做缩放并声明差异；FormWidth/Alpha/圆角默认值见 8 节。
- 背景图：`TImage.Stretch` 拉伸铺满客户区；GDI+ 或 Direct2D 实现时注意 JPEG 解码差异（原使用 `pngimage`/VCL `jpeg` 单元）。
- 窗口尺寸变化后**没有**重新设置圆角区域（原程序靠“重启才改宽度”规避），重写时若允许动态改宽高，需要补 `SetWindowRgn`，否则圆角会错位——这属于行为增强，需产品确认。

### 11.5 配置文件与状态

- INI 读写用 `TIniFile`（GetPrivateProfileString/WritePrivateProfileString 语义：ANSI、写文件、段/键名大小写不敏感、值原样）。C++ 建议直接调用 Win32 Profile API 以保持对旧 `ALTRun.ini` 的兼容，而不是自写 INI 解析（注意空值/默认值/中文写入）。
- `.lang` 是 INI 文件，键名固定（`untALTRunOption.pas:116-375`）；重写时**必须保留键名和默认英文回退值**，否则旧语言包失效。
- `LatestList` 存的是 ShortCutList 文件的**行索引**逗号串（`GetLatestShortCutIndexList`，`untShortCutMan.pas:1091-1111`），列表文件顺序变化会使最近列表错位——兼容实现需保留该格式与语义。
- 收藏文件键是小写关键字、值是项名称（不是索引），改名会失配。
- `HandleID` 存在 INI `[DEBUG]` 段，是“伪单实例转发”的句柄；重写时建议改为命名管道/消息窗口 + 互斥体，但若要兼容既有 INI 不必保留该键。

### 11.6 执行与线程

- 启动方式：`ShellExecute(GetDesktopWindow, nil, cmd, param, workdir, showCmd)` → 失败 `WinExec` → 失败 MessageBox。C++ 建议保留“两段式回退”和完全相同的 `ret < 33` 判定，否则某些 URL/文档关联的失败行为会不同。
- 工作目录推导与“引号内 exe + 后续参数”的解析逻辑（6.2）必须逐行复刻；否则 `"C:\Program Files\...\x.exe" -arg` 类命令行为会变。
- 环境变量替换只替换“第一个匹配到的 `%NAME%` 变量的全部同名出现”（`untShortCutMan.pas:1746-1756`）；C++ 若实现为“全部变量展开”会改变行为 → 建议先按原逻辑实现，再另立增强项。
- 执行在工作线程中完成，且线程里弹 `Application.MessageBox`（VCL 线程不安全）。C++ 建议：工作线程只做 `ShellExecute`，失败信息通过 `PostMessage` 回主线程弹框（**这是有意的行为差异**：修复了潜在崩溃/提示乱序，需在文档中标注）。
- `TDateTime`/`FileAge`：`GetFileModifyTime` 用 `FileDateToDateTime` + `FormatDateTime('yyyy-mm-dd hh:mm:ss')` 做字符串比较（`untUtilities.pas:192-199`）；C++ 用 `FILETIME` 转本地时间格式化即可，注意原实现只精确到秒，且**依赖文件时间可被外部修改**来触发重载。

### 11.7 需要决策的既有怪癖/Bug（1:1 对齐时逐条确认）

1. **滚轮方向判断**：`if Msg.wParam > 0`（1816-1827）——`wParam` 高位是带符号滚动量，未取 `HIWORD`，向下滚时高位为 `0xFFFF`，作为无符号数仍 > 0，很可能“上下都当向上”。重写时若修正方向，与旧版不一致。
2. **数字键特例不处理 `ExitWhenExecute`**（1428-1449 对比 1453-1470）。
3. **`actExecuteExecute` 只判 `Count>0` 不判 `ItemIndex`**，`ItemIndex=-1` 时会访问 `Objects[-1]`（675-731）。
4. **`pmCommandLine` 无菜单项**且 `OnPopup` 指向 `pmListPopup`（会误改 `pmList` 的菜单项可见性）（DFM 8983-8987；2575-2579）。
5. **`WM_ALTRUN_ADD_SHORTCUT` 处理为空**（只有 Trace），真正的“加文件”走 `WM_SETTEXT`（2749-2777）。
6. **`FileExists(ExtractFileDir(...))` 分支**几乎不可能成立（`ExtractFileDir` 返回目录，`FileExists` 对目录为 False），属死分支（`frmALTRun.pas:806-810`）。
7. **`evtMainMinimize` 里的 `inherited;`**（1833）在 `TForm` 中无同名方法，疑似无实际作用（见 10 节）。
8. `edtShortCut` 设计期文本 `'01234567890'`（DFM 3441）依赖“首次显示时清空”来消除，若某路径未清空会看到脏文本。
9. `m_LastShortCutText`、`m_AgeOfFile`、`tmrFocus`、`ShowLatestShortCutList` 的 `m_NeedRefresh` 等字段存在冗余/半死代码。
10. 配置项 `AlphaColor/Alpha/RoundBorderRadius/FormWidth/Lang` 修改后即使已即时应用，仍会触发一次“需要重启”提示与 `RestartMe`（401-414, 535-540）。
11. `Sort`/`QuickSort` 的随机支点（`Randomize` 每次调用）导致同分项顺序不定。
12. `ReplaceEnvStr` 只展开一个变量名；`ptURLQuery` 无占位符时直接“命令+参数”拼接。
13. 正则模式下的异常分支 `Result := False` 后继续使用 `m_Regex.MatchPos[0]`（999-1009），异常时行为不确定。
14. DEBUG 模式差异：`WM_SYSCOMMAND SC_CLOSE` 在 DEBUG 下会真正关闭程序（1797-1799），发布版仅隐藏——以发布版为准。
