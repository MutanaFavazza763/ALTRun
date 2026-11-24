# 04 周边辅助模块功能规格（剪贴板 / 日志 / 参数窗 / 无效项 / 自动隐藏 / 关于帮助语言 / 托盘 / 无边框窗口）

> 分析对象：`ALTRun-original`（只读源码，未做任何修改）
> 分析基线：工作副本为 GBK/ANSI 编码，先转成 UTF-8 临时副本后逐行精读；**下文所有行号均指原始源文件行号**（转码不改变行号）。
> 结论均标注 `源文件:行号`；无法从源码确定的写"不确定"。

## 0. 文件清单与规模核对

| 文件 | 实际行数 | 任务描述 | 备注 |
|---|---|---|---|
| `Unit\untClipboard.pas` | 589 | 约 538 | 实为 Delphi VCL `Clipbrd` 单元整份拷贝 + Unicode 补丁 |
| `Unit\untLogger.pas` | 130 | 约 107 | |
| `Form\frmParam.pas` / `.dfm` | 391 / 63 | 约 331 | |
| `Form\frmInvalid.pas` / `.dfm` | 114 / 127 | — | |
| `Form\frmAutoHide.pas` / `.dfm` | 66 / 59 | — | |
| `Form\frmAbout.pas` / `.dfm` | 72 / 1093 | — | dfm 大部分是内嵌图标/图片数据 |
| `Form\frmHelp.pas` / `.dfm` | 26 / 50 | — | 无任何代码 |
| `Form\frmLang.pas` / `.dfm` | 26 / 42 | — | 无任何代码 |
| `Bin\About.EN` / `Bin\About.简体中文` | 36 / 36 | — | 纯文本 |
| `Bin\Clean.bat` | 1 | — | `@ALTRun.exe Clean` |

工程编译特征：同时存在 `ALTRun.bdsproj` 与 `ALTRun.dproj`，且代码普遍按 ANSI `string` 编写（如 `Form\frmParam.pas:131` 注释"不设成WideString就有Bug"、`Unit\untClipboard.pas:302` 的 Unicode 补丁注释），判断发布版为 Delphi 2007 或更早的 ANSI 编译（**不确定**具体版本）。因此本文涉及的文本文件（日志、`ParamHistory.txt`、`About.<Lang>`、`*.lang`）均为系统 ANSI 代码页（简体中文环境 = GBK/CP936），换行 CRLF。

---

## 1. 剪贴板功能（Unit\untClipboard.pas）

### 1.1 这个单元是什么

- 它**不是**一个"剪贴板扩展业务模块"，而是 Delphi / CodeGear VCL `Clipbrd` 单元的**整份拷贝**后改名：保留 CodeGear 1995-2007 版权头（`Unit\untClipboard.pas:1-7`）、类 `TClipboard`（`:61-104`）、全局单例函数 `Clipboard` 与 `SetClipboard`（`:106-107`、`:546-560`），甚至保留 LINUX 分支（`:15-20`、`:143-145`、`:160-162`）。
- 工程不引用 VCL 的 `Clipbrd`，改用本单元顶替（`ALTRun.dpr:23` 的 `untClipboard in 'Unit\untClipboard.pas'`），因此程序里的 `Clipboard` 指本单元实现。
- 相对原版唯一实质改动是**新增 Unicode 文本读写**：`GetAsUnicodeText` / `SetAsUnicodeText`（声明 `:78-79`，实现 `:297-318` / `:332-363`），作者注释"ET Worker: 支持Unicode"（`:302`）。这是全文最重要的行为差异。

### 1.2 支持的数据格式

| 格式 | 读 | 写 | 源码位置 |
|---|---|---|---|
| `CF_TEXT`（ANSI 文本） | `AsText` / `GetAsText` / `GetTextBuf` | `AsText` / `SetAsText` / `SetTextBuf` | `:100`、`:280-295`、`:327-330`、`:261-278` |
| `CF_UNICODETEXT`（Unicode 文本） | `AsUnicodeText` | `AsUnicodeText` | `:101`、`:297-318`、`:332-363` |
| `CF_BITMAP` / `CF_METAFILEPICT` / `CF_PALETTE` | `Assign(TPicture/TBitmap/TMetafile)` | `Assign(TPicture/TGraphic)` | `:365-466`、`:477-496` |
| `RegisterClipboardFormat('Delphi Picture')` → `CF_PICTURE` | `HasFormat(CF_PICTURE)` 时枚举位图类格式 | 同图片写入 | 注册 `:583`；特判 `:518-543` |
| `RegisterClipboardFormat('Delphi Component')` → `CF_COMPONENT` | `GetComponent`（TReader 反序列化组件流） | `SetComponent` | 注册 `:584`；实现 `:178-220`、`:248-259` |

**不支持**：`CF_HTML`（HTML 粘贴格式）、`CF_HDROP`/文件列表、RTF、PNG 自定义格式等。全仓库检索 `CF_HTML`、`HDROP`、`RegisterClipboardFormat('HTML Format')` 均无命中。

### 1.3 底层实现机制

- **打开/关闭引用计数**：`FOpenRefCount`（`:63`），`Open`（`:150-170`）/ `Close`（`:132-148`）；计数归零才真正 `CloseClipboard`。
- `Open` 时优先取 `Application.Handle` 作为剪贴板属主窗口，若为 0 才 `AllocateHWnd` 建隐藏窗口（`:154-164`）；`OpenClipboard` 失败**直接抛异常** `SCannotOpenClipboard`（`:165-166`）。
- 首次写入前会 `Adding` 自动清空剪贴板（`:123-130`）；写入统一走 `SetBuffer`：`GlobalAlloc(GMEM_MOVEABLE + GMEM_DDESHARE)` → `GlobalLock` → `Move` → `SetClipboardData`（`:222-246`）。
- 无消息钩子、无窗口过程处理剪贴板通知：`WndProc` 只是 `DefWindowProc`（`:172-176`）。
- 启动时注册两个自定义格式（`initialization`，`:581-585`）。

### 1.4 主程序怎么用（关键：没有剪贴板监控）

**读取剪贴板**只有 2 个入口，都在热键处理里、且发生在主窗显示之前：

1. 主热键 `hkmHotkeyHotKeyPressed`：`ShortCutMan.Param[0] := Clipboard.AsUnicodeText;`（`Form\frmALTRun.pas:2424`）。
2. "最近一次快捷项"热键（ALT+L）`hkmHotkey3HotKeyPressed`：同样赋值（`Form\frmALTRun.pas:2388`）。

采集顺序是"先剪贴板/前台窗口、后显示窗体"（`Form\frmALTRun.pas:2424-2439`），因为主窗激活会改变 `GetForegroundWindow`。

**写入剪贴板**只有 1 条业务路径：`Ctrl+C` 复制当前选中项的完整命令行：

- 快捷键分支：`edtShortCutKeyDown` 中 `67`（`'C'`）+ `ssCtrl` → `actCopyCommandLineExecute`（`Form\frmALTRun.pas:1626-1636`）。
- 实现：`Clipboard.AsUnicodeText := TShortCutItem(...).CommandLine;`（`Form\frmALTRun.pas:548-557`），随后显示"已复制"星标（见 1.5）。
- 历史上曾用 `SetClipboardText`（ANSI 版本，中文乱码），已注释掉（`Form\frmALTRun.pas:552`、`:1632`）。
- `untUtilities.pas:1255-1287` 的 `SetClipboardText` 是 `SetAsUnicodeText` 的**重复实现**，且全仓库无调用。

**明确没有做的事**（重写时不要照抄错误预期）：

- ❌ 没有剪贴板变化监控（无 `WM_CLIPBOARDUPDATE`、`AddClipboardFormatListener`、`SetClipboardViewer`；检索无命中）。
- ❌ 没有模拟粘贴（`SendKeys` / `PostKeyEx32` 只存在于 `untUtilities.pas:33/347/416`，主流程调用处全被注释：`Form\frmALTRun.pas:741-742`、`:690-715`）。
- ❌ 没有"把选中结果写入剪贴板"的自动流程；写剪贴板仅 `Ctrl+C` 一条显式路径。
- ✅ 唯一自动读取剪贴板的用途：把文本作为 `{%c}` 参数替换进命令行（见第 3 节），例如默认快捷项"百度搜索剪贴板 / Google 搜索剪贴板"（`Bin\ALTRun.ini` 对应的默认项，提示文案见 `Unit\untALTRunOption.pas:984-985` 的 Hint 默认值）。

### 1.5 复制成功的视觉反馈（易漏细节）

复制后主窗右下角一个 16×16 的隐藏 `TEdit edtCopy` 显示字符 `★`（U+2605）：

- 控件定义：`Form\frmALTRun.dfm:3394-3419`，`Text = #9733`（`:3416`）、`Visible = False`（`:3417`）、`TabStop = False`、无边框、`Color = clMoneyGreen`。
- 显示：`edtCopy.Show; tmrCopy.Enabled := True;`（`Form\frmALTRun.pas:554-555`）。
- 隐藏：`tmrCopy`（`Interval = 250`，`Form\frmALTRun.dfm:8963-8967`）到点后 `edtCopy.Hide`（`Form\frmALTRun.pas:2695-2699`）。
- `edtCopy` 同时挂了 `OnMouseDown = MiddleMouseDown`（`Form\frmALTRun.dfm:3418`），即中键也可执行当前项（`Form\frmALTRun.pas:1356-1363`）。

### 1.6 缺陷与风险（重写时必须修）

1. `SetAsUnicodeText` 缓冲区计算错误：`Size := Length(Value) * 4`（`:339`），实际只需 `(Length+1)*2`；随后 `Move(WStr^, DataPtr^, Size)`（`:350`）把整块 4 倍大小写入剪贴板句柄，字符串 NUL 之后是 `AllocMem` 的零字节垃圾。读取方按 NUL 截断一般无害，但句柄内容与大小都不规范。
2. `SetAsUnicodeText` 直接 `OpenClipboard(0)` / `EmptyClipboard` / `CloseClipboard`（`:344-345`、`:360`），**绕过** `TClipboard` 的引用计数与 `FEmptied` 状态；与其他 `AsText`/图片写入混用时引用计数可能错乱。
3. 未检查 `OpenClipboard` / `SetClipboardData` 返回值，失败时静默或抛未捕获异常（`:344`、`:351`）。
4. 读路径 `GetAsUnicodeText` 的 `Open` 在剪贴板被其他程序占用时抛异常（`:305` → `:165-166`）；热键处理处没有 `try/except` 包裹（`Form\frmALTRun.pas:2424`），理论上会导致热键处理抛异常。
5. 只读一种文本格式，不保留源格式（RTF/HTML/图片）；用户从 Word/浏览器复制的富文本会退化成纯文本。

---

## 2. 日志系统（Unit\untLogger.pas）

### 2.1 开关与初始化时序（最容易误解的一点）

- 全局变量：`TraceEnable`（`:14`）、`CnDebugEnable`（`:15`）、`LogFileName`（`:16`）、临界区 `CriticalSection`（`:17`），另有未使用的 `MutexHandle`（`:13`）。
- 唯一初始化入口：`InitLogger(IsTraceEnable = False; IsCnDebugEnable = False; IsAppendMode = False)`（`:20`、`:43-67`）。
- 唯一实际调用点：`Form\frmALTRun.pas:2041` → `InitLogger(DEBUG_MODE, False, False);`
  - `DEBUG_MODE` 是**编译期常量** `False`（`Unit\untALTRunOption.pas:16`）。
  - `ALTRun.dpr:37` 的调用被注释；`untUtilities.pas:1154-1212` 是旧版实现，整段注释。
- 因此：**发布版 `TraceEnable` 恒为 `False`，日志一条都不写**。打开日志的唯一办法是用 `DEBUG_MODE = True` 重新编译。
- ini 中的 `[DEBUG] TraceEnable`（`Bin\ALTRun.ini:51`；读取 `Unit\untALTRunOption.pas:811`）**实际无效**：`LoadSettings` 读入后，`FormCreate` 里的 `InitLogger` 会立刻把 `TraceEnable` 覆盖为 `False`；`SaveSettings` 也故意不写该键（`Unit\untALTRunOption.pas:883-884` 注释"TraceEnable仅供手工修改"）。

### 2.2 日志文件路径与命名

```pascal
LogFileName := StringReplace(Application.ExeName, '.exe', '.' + GetCurrentUserName + '.log', [rfReplaceAll]);
```

（`Unit\untLogger.pas:52`）

- 位置：**exe 同目录**；名字：`<exe 基名>.<登录用户名>.log`，如 `ALTRun.MFOrange.log`。
- 用户名：`GetUserName`，缓冲区常量 `cnMaxUserNameLen = 254`（`:29-41`，常量在 `:31`）。
- 注意 `rfReplaceAll` 会替换路径中所有 `.exe` 子串（通常只出现在末尾，风险低）。

### 2.3 文件打开方式与文件头

- `InitLogger` 内：文件存在且 `IsAppendMode = True` 则 `Append`，否则 `Rewrite`（**截断**）（`:57-60`）。
- 唯一调用点传 `IsAppendMode = False`（`Form\frmALTRun.pas:2041`）→ **每次启动覆盖旧日志**。
- 初始化时写一行：`[<DateTimeToStr(Now)>] Logon user = <user>`（`:62`）。

### 2.4 每条日志的写入方式与格式

`AddLog`（`:69-83`）：

1. `EnterCriticalSection`（`:73`）
2. 每次调用都 `AssignFile` + `Append` + `WriteLn` + `CloseFile`（`:75-82`）——**每条日志开关一次文件**，无缓冲、性能差。
3. 文本行格式（由调用方拼好）：`[<DateTimeToStr(Now)>] <内容>`，错误类为 `[<时间>] ERROR: <内容>`（`:89`、`:98`、`:108`、`:118`）。

"级别"只有两种：普通（`TraceMsg`，`:104-122`）与错误（`TraceErr`，`:85-102`）。**没有** DEBUG/INFO/WARN 枚举、没有级别过滤阈值。

`DateTimeToStr` 使用**系统区域设置**（中文系统形如 `2009-01-01 9:40:02`），不是固定格式（对比 `untUtilities.pas:198` 另有一处显式 `'yyyy-mm-dd hh:mm:ss'` 的 `GetFileModifyTime`）。

### 2.5 轮转策略

**没有**。无大小上限、无按天/按大小切分、无过期清理、无压缩；只有"每次启动截断"这一种隐式回收。

### 2.6 CnDebug 通道

`CnDebugEnable = True` 时，同一份内容额外发到 CnPack 的 `CnDebugger`（OutputDebugString/调试器窗口）：

- `TraceErr(Msg)` → `CnDebugger.TraceMsg('ERROR: ' + Msg)`（`:91-92`）
- `TraceErr(fmt,args)` → `TraceMsg(Format('ERROR: %s', [...]))`（`:100-101`）
- `TraceMsg(Msg)` → `TraceMsg`；`TraceMsg(fmt,args)` → `TraceFmt`（`:110-111`、`:120-121`）

调用点传 `False`（`Form\frmALTRun.pas:2041`），即发布版该通道也关闭。

### 2.7 记录的事件类型（示例，全部经 `TraceMsg`/`TraceErr`）

- 生命周期：`FormCreate()`（`Form\frmALTRun.pas:2044`）、`FormActivate`（`:1850`）、`evtMainMinimize`（`:1835`）、`evtMainDeactivate`（`:1702`）、`System shutdown`（`:1810`）。
- 功能动作：`actShowExecute()`（`:981`）、`actAboutExecute()`（`:198`）、`actConfigExecute()`（`:261`）、`actShortCutExecute()`（`:872`）、`actCloseExecute()`（`:232`）、各 `act*`（含 `actUp/actDown`、`:586`、`:1157`）。
- 输入：`FormKeyDown`/`FormKeyPress`/`edtShortCutKeyDown`/`lstShortCutKeyDown` 等每次按键（`:2171`、`:2293`、`:1501`、`:2482`；`frmParam.pas:228`）。
- 热键：`hkmHotkeyHotKeyPressed`、`hkmHotkey3HotKeyPressed`（`:2415`、`:2385`），并打印抓到的前台窗口信息（`:2433`、`:2397`）。
- 执行失败：`ShellExecute(...) Failed`、`WinExec(...) Failed`（`Unit\untShortCutMan.pas:344-353`）。
- UI 细节：`SetFocus failed`、`SetForegroundWindow failed` 等（`Form\frmALTRun.pas:961`、`:1728`）。
- 检索可见主窗体内 `TraceMsg` 调用极密集（数十处），基本是"函数进入点打点"风格。

### 2.8 缺陷

- 临界区只包住 `AddLog` 主体，`CloseFile` 写在 `LeaveCriticalSection` 之后（`:79-82`）；若 `AssignFile/Append` 抛异常，`finally` 里对未打开文件 `CloseFile` 可能二次异常。
- 每条日志开关文件，Windows 上频繁 IO；高 DPI 无关但会拖慢热键响应（DEBUG 版专用）。
- 文本编码为 ANSI（系统代码页），中文日志在 UTF-8 编辑器里乱码；无 BOM、无结构（纯人工 grep 用）。
- 时间戳依赖区域设置，不可机器解析。

---

## 3. 参数窗口（Form\frmParam.pas / .dfm）

### 3.1 何时弹出（调用链）

```
actExecuteExecute                      Form\frmALTRun.pas:662
  └─ ShortCutMan.Execute(Item, keyword) Unit\untShortCutMan.pas:2071-2077
       └─ Execute(ShortCutItem)          Unit\untShortCutMan.pas:673-797
            ├─ ParamType = ptNone → 不弹窗，直接执行（:737-748）
            └─ ParamType <> ptNone 时按命令行占位符分流（:750-796）
                 {%c}  → 用剪贴板文本（:751-754）
                 {%wd} → 用前台窗口 HWND 字符串（:756-759）
                 {%wt} → 用前台窗口标题（:761-764）
                 {%wc} → 用前台窗口类名（:766-769）
                 否则  → ParamForm.ShowModal 让用户输入（:771-781）
```

- 弹窗本身：先 `ParamForm.Caption := ShortCutItem.Name`（`Unit\untShortCutMan.pas:772`），再 `ShowModal`（`:774`）；返回 `mrCancel` 则**放弃执行**（`:774-778`）；否则取 `cmdobj.Param := ParamForm.cbbParam.Text`（`:780`），继续按 `ParamType` 做 URL 编码（`:785-793`）。
- 弹窗前主窗已隐藏：`actExecuteExecute` 先 `evtMainMinimize`（`Form\frmALTRun.pas:677`）。
- 其他也会弹出参数窗的入口：快捷项编辑窗口的"测试"（`Form\frmShortCut.pas:189-218`）、无效项列表右键 Test（`Form\frmInvalid.pas:62-93`）。
- 参数窗是**启动即创建、常驻复用**的：`Application.CreateForm(TParamForm, ParamForm)`（`ALTRun.dpr:47`），从不释放（`Application` 持有），每次 `ShowModal` 复用同一实例。
- 参数窗显示期间再按主热键：`actShowExecute` 会置 `ParamForm.ModalResult := mrCancel` 把它关掉（`Form\frmALTRun.pas:985`）。

### 3.2 窗口形态（.dfm）

| 属性 | 值 | 位置 |
|---|---|---|
| `BorderStyle` | `bsDialog` | `Form\frmParam.dfm:4` |
| `Caption` | `'ng'`（设计期占位，运行期被替换为快捷项名称） | `:5`；替换处 `Unit\untShortCutMan.pas:772` |
| 尺寸 | 394 × 22（单行输入框 + 右侧 OK） | `:6-7` |
| `Position` | `poScreenCenter` | `:16` |
| `KeyPreview` | `True`（窗体层拦截 Esc/Enter） | `:14` |
| 输入控件 | `TComboBoxEx cbbParam`（弹下拉+自动完成） | `:27-42` |
| 自动完成 | `AutoCompleteOptions = [acoAutoSuggest, acoAutoAppend]` | `:33` |
| OK 按钮 | `TRzButton btnOK`，`Default = True`，`ModalResult = 1`，宽 50，`Align = alRight` | `:43-55` |
| 隐藏计时器 | `tmrHide`，`Enabled = False`，设计期 `Interval = 10000` | `:56-62` |

- 选 `TComboBoxEx` 而非 `TComboBox` 的原因写在单元头注释：`TComboBox` 对中文支持有 Bug（`Form\frmParam.pas:1-2`）。
- `TParamForm.Createparams` 覆写：追加 `WS_POPUP`、`WS_EX_TOPMOST`，并把 `WndParent` 设为 `GetDesktopWindow`（`Form\frmParam.pas:169-179`）。效果：无父窗、置顶、不随主窗最小化；`WS_EX_NOACTIVATE` 被注释掉（`:176`），所以能正常拿焦点。

### 3.3 历史记录：存储位置、格式、上限、淘汰

- 文件：`<exe目录>\ParamHistory.txt`，常量 `PARAM_HISTORY_FILE = 'ParamHistory.txt'`（`Form\frmParam.pas:26`），路径在 `FormCreate` 组装（`:190`）。
- 文件不存在时先创建空文件（`:264-276`）。
- **行格式**：`WriteLn(MyFile, Format('%-10d|%-30s%', [rank, param]))`（`:366-367`）
  - 第 1 列：使用次数（整数，左对齐宽 10）
  - 分隔符 `|`
  - 第 2 列：参数文本（左对齐宽 30）
  - 行尾还有一个字面 `%`（见 3.7 风险）
- 读入：`SplitString(line, '|')`（`:302`，实现 `untUtilities.pas:261-278`），`rank := StrToInt(Trim(ParamItem[0]))`（`:308`），`Param := Trim(ParamItem[1])`（`:311`），重复参数跳过（`:314`），条数达到 `ParamHistoryLimit` 即停止读（`:305`）。
- 刷新策略：先比较文件修改时间字符串（`GetFileModifyTime`，`:279-285`），未变则跳过；但 `LoadParamHistory` 只在 `FormCreate` 调一次（`:191`），`m_FileModifyTime` 缓存逻辑实际是死代码。
- 保存时机：`FormDestroy`（`:196`）→ 即程序退出时一次性写回；`SaveParamHistory` 只写前 `ParamHistoryLimit` 条（`:357-364`）。
- 上限：`ParamHistoryLimit`，来自 ini `[Config]`，默认 **50**（`Unit\untALTRunOption.pas:757`；样例 `Bin\ALTRun.ini:11`）。
- **淘汰算法**（`btnOKClick` 内，`:68-112`）：
  1. 暂存输入文本（`:77`），在 `Items` 中找下标（`:78`）。
  2. 文本非空且不存在：若 `Items.Count >= ParamHistoryLimit`，**从尾部向前扫描找 rank 最小项**并删除（`:85-99`）；然后把新项 `InsertObject(0, Text, Pointer(0))` 插到**第 0 位，rank = 0**（`:103`）。
  3. 文本已存在：该项 rank + 1（`:107`）。
  4. 最后把文本写回 `cbbParam.Text`（`:110`），`ModalResult := mrOk`（`:111`）。
- 语义注意：淘汰的是"使用次数最少"而不是"最久未用"；新项 rank=0 天然是最小值，所以频繁尝试的新参数会先把旧的低频项挤掉（`:87-99` 与 `:103` 配合的副作用）。
- 历史是**全局共享**的：不区分快捷项，任何需要参数的快捷项看到同一份下拉历史。

### 3.4 交互：回车确认 / Esc 取消 / 超时自动取消

- **回车**：`cbbParamKeyPress` 中 `Key = #13 → btnOKClick`（`Form\frmParam.pas:129-134`）；窗体级 `FormKeyPress` 也处理 `#13 → btnOKClick`（`:226-238`）。两条路径都可能命中（前者在输入框内，后者在别处获得焦点时）。
- **Esc**：`FormKeyDown` 中 `VK_ESCAPE`（`:204-224`）：
  - 文本**非空** → 只清空文本、保持窗口打开并聚焦输入框（`:216-220`）；
  - 文本**为空** → `ModalResult := mrCancel` 关闭 = 取消执行（`:214-215`）；
  - `Key := VK_NONAME` 吞掉按键（`:211`）。
- **自动消失**：`tmrHide` 间隔 = `HideDelay * 1000`（`:328-338`，赋值在 `:335`），到时 `tmrHideTimer` 置 `mrCancel`（`:384-388`）。`HideDelay` 默认 15 秒（`Unit\untALTRunOption.pas:752`）。
- **计时器重置点**：`OnActivate`（`:181-184`）、`OnMouseDown`（`:240-244`）、`OnKeyDown`（`:206`）；即"激活/任意点击/任意按键"都重新计时。`DEBUG_MODE` 下不启动计时器（`:330`）。
- **停止点**：点 OK 时先 `StopTimer`（`:74`）；`OnHide` 停止（`:199-202`）。
- 显示时 `cbbParam.SetFocus`（`FormShow`，`:246-250`），保证可直接键入。
- OK 按钮文字每次 `FormCreate`（`:188`）与 `FormShow`（`:248`）都设一次 `resBtnOK` —— 因为本窗在语言资源加载前就被 `ALTRun.dpr:47` 创建，`FormCreate` 那次拿到的是空串，只有 `FormShow` 那次有效。

### 3.5 参数占位符 / 替换语法（完整表）

常量定义（`Unit\untALTRunOption.pas:27-35`）：

| 占位符 | 含义 | 取值来源 | 定义 |
|---|---|---|---|
| `%p` | 参数（旧写法） | 用户输入 或 剪贴板/前台窗口数据 | `:27` |
| `{%p}` | 参数（新写法，优先匹配） | 同上 | `:28` |
| `{%c}` | 剪贴板文本 | `Clipboard.AsUnicodeText`（热键按下瞬间采集） | `:29` |
| `{%wd}` | 前台窗口句柄（十进制字符串） | `IntToStr(GetForegroundWindow)` | `:30` |
| `{%wt}` | 前台窗口标题 | `GetWindowText` | `:31` |
| `{%wc}` | 前台窗口类名 | `GetClassName` | `:32` |
| `@+` / `@-` / `@` | 命令行前缀：最大化 / 最小化 / 隐藏启动 | `ExecuteCmd` 解析 | `:33-35` |

替换与执行规则（`Unit\untShortCutMan.pas`）：

1. `ParamType = ptNone`：不替换（`Execute(cmdobj)` 里 `ptNone` 分支，`:191-197`）。
2. `ptNoEncoding`（不编码）：
   - 命令行含 `{%p}` → 用正则 `TRegExpr.Replace` 全部替换（`:202-214`）；
   - 否则含 `%p` → 替换（`:215-227`）；
   - 否则含 `{%c}` → 替换（`:228-240`）；
   - 否则含 `{%wd}` / `{%wt}` / `{%wc}` → 替换（`:241-266`、`:267-279`）；
   - 都没有 → 参数作为 `ShellExecute` 的 `lpParameters` 传入（`:280-284`）。
   - **是 if/else-if 链，一条命令行只走第一种命中的占位符**；同一种占位符出现多次会被 `Regex.Replace` 全部替换。混用两种占位符（如 `{%p}`+`{%c}`）不支持，后者原样留在命令行里。
3. `ptURLQuery` / `ptUTF8Query`：命中占位符则替换（`:289-327`）；都没有则把参数**直接拼在命令行末尾**（`:330`）。`ptURLQuery` / `ptUTF8Query` 编码规则：
   - 不编码字符集 `NoConversion = ['A'..'Z','a'..'z','*','@','.','_','-']`，空格 → `+`，其余 → `%XX`（`IntToHex(...,2)` 大写十六进制）（`Unit\untUtilities.pas:1136-1152`）；
   - UTF8 版先 `Utf8Encode`（GBK→UTF-8 字节）再逐字节 `%XX`（`:1117-1134`）。
4. 替换完成后 `ExecuteCmd` 再执行一次 `ReplaceEnvStr`（`Unit\untShortCutMan.pas:339`；`untUtilities.pas:995-1020`），展开 `%VAR%`。
5. `@+`/`@-`/`@` 前缀解析为 `SW_SHOWMAXIMIZED` / `SW_SHOWMINIMIZED` / `SW_HIDE`，从命令行剥掉（`Unit\untShortCutMan.pas:170-185`）。
6. 命令行中出现 `.\` 或 `..\` 时把工作目录设为 exe 目录（`:187-189`）。
7. 真正启动：`CreateThread(@ExecuteCmd)` 在**子线程**执行（`:2089`）→ `ShellExecute(GetDesktopWindow, ...)`（`:341`）失败（返回值 < 33）再 `WinExec`（`:347`），再失败弹 `resCanNotExecute`（`:352-353`）。

### 3.6 缺陷 / 不确定处

- `LoadParamHistory` 若某行没有 `|`，`ParamItem[1]` 越界抛异常 → `except Exit`（`:311-318`），已读入的部分保留、后半截静默丢弃；`:305` 的 `Exit` 会绕过 `Result := True`。
- `SaveParamHistory` 从不置 `Result := True`（`:349`），永远返回 False（返回值无人使用）。
- **不确定**：`Format('%-10d|%-30s%', ...)`（`:366-367`）末尾的孤立 `%` 在 Delphi `Format` 中很可能抛 `EConvertError`；若如此，`SaveParamHistory` 的 `except Exit`（`:371-373`）会让**历史记录永远保存失败**。仓库 `Bin\` 中确实不存在 `ParamHistory.txt`，与该推测相容，但无法在此环境用 Delphi 编译器验证。重写时不要复制该格式串。
- 正则替换把用户输入当 `TRegExpr.Replace` 的替换串（`:207`、`:220` 等），参数中含 `$` 时可能被当作反向引用（**不确定** `TRegExpr` 默认是否启用替换符，取决于 `UseSubstitute` 默认值）。
- 参数历史与 `HideDelay` 都只在 ini 中手改，配置界面未暴露（`Form\frmConfig.pas` 无 `HideDelay`/`ParamHistoryLimit` 引用）。

---

## 4. 无效项提示（Form\frmInvalid.pas / .dfm + 校验逻辑）

### 4.1 什么情况才会出现这个窗口

**它不是启动自检，也不是后台监控**：只有用户在"快捷项管理器"里**主动点工具栏的 Validate 按钮**才触发。

- 入口按钮：`btnValidate: TToolButton`，`Hint = 'Validate all ShortCuts and remove invalid ones'`（`Form\frmShortCutMan.dfm:162-166`），Action `actValidate`（`:236-239`）。
- 实现：`TShortCutManForm.actValidateExecute`（`Form\frmShortCutMan.pas:327-394`）。
- 前置确认框：`resValidateConfirm`（`:338-342`），默认文案（`Bin\EN.lang:178`）：
  `Validate will remove some invalid ShortCut (NOT ALL invalid ones will be checked out), do you still want go?` —— 作者自己承认"查不全"。
- 遍历管理器列表：`if not IsValidCommandLine(CommandLine) then` 逐条收集（`:349-368`）。
- 全部有效 → 弹 `resNoInvalidShortCut`（默认 "No Invalid ShortCut found."，`Bin\EN.lang:175`）并**不显示窗口**（`:373-378`）。

### 4.2 "无效"的判定规则（`IsValidCommandLine`，`Form\frmShortCutMan.pas:396-509`）

按源码顺序（**注意大量"提前放行"分支**）：

1. 去掉命令行前缀 `@+` / `@-` / `@`（`:406-412`）。
2. 含 `\\` → 视为网络路径，**直接判有效**（`:414-415`）。
3. 不含 `\` → 不是文件路径（可能是内部命令/URL），**直接判有效**（`:417-418`）。
4. `ReplaceEnvStr` 展开环境变量（`:420-421`）。
5. 若被 `"` 包围则去掉引号（`:423-425`）。
6. 以 `.\` 开头 → 换成 exe 目录（`:427-430`）。
7. `FileExists` → 有效（`:432-433`）；`DirectoryExists` → 有效（`:435-436`）。
8. 再去一次引号并 `FileExists`/`DirectoryExists`，都不行 → **无效**并返回（`:438-448`）。
9. 否则取**最后一个 `\` 之前**的目录判断 `DirectoryExists`（`:450-466`）。
10. 还不行则从字符串开头找引号内第一段 / 整个第一段判断 `DirectoryExists or FileExists`（`:468-493`）；否则无效。

结果：只有"看起来像本地绝对/相对路径、但文件与目录都不存在"的项才会被判无效；纯命令名、URL、UNC 路径、无 `\` 的项一律放过（这也是 4.1 中确认框措辞的由来）。

### 4.3 窗口内容

- 标题：`resInvalidFormCaption`，默认 `'Invalid ShortCut List'`（`Form\frmInvalid.pas:52`；`Unit\untALTRunOption.pas:1073`；`Bin\EN.lang:170`）。
- 四个按钮/列文字全部从语言资源取：OK、Cancel、`ShortCut`/`Name`/`Param Type`/`Command Line`（`Form\frmInvalid.pas:53-59`）。
- 列表 `lvShortCut: TListView`：`vsReport`、`Checkboxes = True`、`RowSelect`、`GridLines`、`HideSelection = False`、`ReadOnly`、`DragMode = dmAutomatic`、`FullDrag`（`Form\frmInvalid.dfm:68-107`）；列宽 100 / 100 / 100 / 400（`:77-93`）。
- 每行填入 4 个字段，`ImageIndex := Ord(siItem)`，`Checked := True`（默认全选），`Data := Pointer(原索引)` 用于回删（`Form\frmShortCutMan.pas:356-367`）。
- 列表 Hint：`Checked ShortCut will be removed when "OK" button clicked`（`Form\frmInvalid.dfm:73`）。
- 窗口尺寸 472 × 326，`poScreenCenter`（`Form\frmInvalid.dfm:5-6`、`:14`）。

### 4.4 用户可选动作

| 动作 | 行为 | 位置 |
|---|---|---|
| **OK**（默认按钮，`ModalResult = 1`） | 把所有**勾选**项从管理器的列表视图里删除（`ListView.Items.Delete(Data)`，倒序遍历） | `Form\frmShortCutMan.pas:385-390`；按钮 `Form\frmInvalid.dfm:21-53` |
| **Cancel**（`Kind = bkCancel`） | 什么都不改（`if ModalResult = mrCancel then Exit`） | `Form\frmShortCutMan.pas:382`；按钮 `Form\frmInvalid.dfm:54-67` |
| 右键 → **Select All** | 全选（所有项 `Checked := True`） | `Form\frmInvalid.pas:104-111`；菜单 `dfm:111-114` |
| 右键 → **Unselect All** | 全不选 | `Form\frmInvalid.pas:95-102`；`dfm:115-118` |
| 右键 → **-** | 分隔线 | `dfm:119-121` |
| 右键 → **Test** | 用该行 4 个字段重建 `TShortCutItem` 并 `ShortCutMan.Execute`（可能再弹参数窗） | `Form\frmInvalid.pas:62-93`；`dfm:122-125`；命令行为空时提示 `resCommandLineEempty`（`:69-74`） |
| 直接点复选框 / Ctrl+A 等 | ListView 原生多选与勾选 | `dfm:76`、`:100-101` |

**没有"编辑"动作**（任务描述里提到的"编辑"在源码中不存在，用户要编辑只能关掉本窗去管理器里改）；**也没有"忽略并记住"的持久化机制**——"忽略"= 取消勾选后按 OK。

### 4.5 结果如何落盘

本窗只操作**内存中的 ListView**（删除行）。是否写回 `ShortCutList.txt` 取决于快捷项管理器自身的保存流程（`ShortCutMan.LoadFromListView` + `SaveShortCutList`）；无效项窗口不负责保存。若管理器未保存就关闭，删除丢失。

---

## 5. 自动隐藏（Form\frmAutoHide.pas / .dfm）与主窗隐藏机制

### 5.1 结论先行：`frmAutoHide` 不是主窗口的自动隐藏

`TAutoHideForm` 是一个**带淡出动画的临时信息框**，唯一用途：**用户第一次双击托盘图标时**，提示"用热键唤出我"，然后自己淡出消失。

- 唯一创建处：`ntfMainDblClick`（`Form\frmALTRun.pas:2543-2573`），且受 `m_IsFirstDblClickIcon` 控制只弹一次（`:2549-2551`；标志初始化 `:2133`）。
- 文案：`AutoHideForm.Info := Format(resShowMeByHotKey, [GetHotKeyString])`（`:2562`；`:2561` 那次用 `HotKeyStr1` 被下一行覆盖）→ 默认"用热键 %s 唤出我会更好"（`Unit\untALTRunOption.pas:940`）。
- 标题：`resInfo`（`:2560`）。

### 5.2 时序与动画（全部硬编码）

1. `ShowModal` 显示，`FormShow` 把 `Info` 写进 `lblInfo`（`Form\frmAutoHide.pas:38-41`）。
2. `tmrHide`（`Interval = 1500` ms，`Form\frmAutoHide.dfm:46-51`）到点 → 启用 `tmrAlpha`（`Form\frmAutoHide.pas:60-64`）。
3. `tmrAlpha`（`Interval = 50` ms，`dfm:52-58`）每 tick 把 `AlphaBlendValue` 减 `AlphaStep = 10`（`pas:44-45`、`:49-53`）。
4. `AlphaBlendValue < 10` 时 `ModalResult := mrOk` 关闭（`:54-57`）。
5. `AlphaBlend = True`（`dfm:3`），初始 `AlphaBlendValue` 为 VCL 默认 255 → 约 26 tick ≈ **1.3 秒**淡出。
6. 总时长 ≈ 1.5 + 1.3 ≈ **2.8 秒**（点 OK 按钮可提前关闭，`dfm:37-45` 为 `Kind = bkOK`）。
7. 窗体 `bsDialog`、`poScreenCenter`、394 × 208、字号 -16（`dfm:5-8`、`:16`、`dfm:12` 附近）；`lblInfo` 居中、`AutoSize = False`、`Layout = tlCenter`（`dfm:24-36`）。

### 5.3 与 `HideDelay` 配置的关系

**无直接关系**：`frmAutoHide` 的 1500 / 50 / 10 全是常量（`dfm:47`、`dfm:54`、`pas:45`），不读 `HideDelay`。
`HideDelay` 只作用于两处：主窗空闲自动隐藏（`Form\frmALTRun.pas:2628-2638`）与参数窗空闲自动取消（`Form\frmParam.pas:328-338`）。

### 5.4 主窗真正的"自动隐藏"逻辑（补全，重写重点）

| 机制 | 行为 | 位置 |
|---|---|---|
| **失焦立即隐藏** | `Application.OnDeactivate` → `evtMainDeactivate` → 直接 `evtMainMinimize`（Hide + 清空输入） | `Form\frmALTRun.pas:1698-1706`；事件挂接 `Form\frmALTRun.dfm`（`evtMain` 的 `OnDeactivate`），`ALTRun.dpr:50` 另设 `OnMinimize` |
| **空闲超时隐藏** | 显示后每次 Activate/按键/鼠标进入控件都 `RestartHideTimer(HideDelay)`；`tmrHide.Interval := HideDelay*1000`；到点 `evtMainMinimize` + 清空输入 | `:2628-2638`（`:2635` 乘 1000）、`:2737-2743`；重置点 `:1127`、`:1688`、`:1695`、`:1852`、`:2180`、`:2462`、`:2522` |
| Esc | 输入为空 → 隐藏；非空 → 清空 | `:2255-2264`、`:1648-1655` |
| 热键切换 | 已显示 → `actHideExecute` | `:2436-2439` |
| 执行项之前 | `evtMainMinimize` | `:677` |
| 关闭按钮 | 拦截 `WM_SYSCOMMAND/SC_CLOSE`，只隐藏（DEBUG 模式才真退出） | `:1793-1804` |
| 最小化 | `MinimizeToTray = True` + `OnMinimize → evtMainMinimize` | `Form\frmALTRun.dfm:8548`、`ALTRun.dpr:50`、`:1831-1841` |
| 鼠标移出 | **不存在**：无 `OnMouseLeave`、无 `TrackMouseEvent`、无 `WM_MOUSELEAVE` 处理（全仓库检索无命中） | — |

`HideDelay` 默认 **15** 秒（`Unit\untALTRunOption.pas:752`；样例 `Bin\ALTRun.ini:6`），只在 ini `[Config]` 中手改，配置窗口未暴露。

---

## 6. 关于 / 帮助 / 语言窗口

### 6.1 关于窗口（Form\frmAbout.pas / .dfm）

- 打开方式：托盘菜单 About 或 `F1`（`Form\frmALTRun.pas:2228-2232`）→ `actAboutExecute`（`:193-218`）**每次新建** `TAboutForm`（`:212`），`ShowModal` 后 `finally` 释放（`:214-216`）。
- 标题：`Format('%s %s %s', [resAbout, TITLE, ALTRUN_VERSION])`（`:213`）→ 形如 `About ALTRun V1.46.7`。
- 版本号来源：编译期常量 `ALTRUN_VERSION = 'V1.46.7'`（`Unit\untALTRunOption.pas:22`），`TITLE = 'ALTRun'`（`:21`）；不读文件、不读资源版本信息。
- 正文：若 `<exe目录>\About.<Lang>` 存在，则 `mmoAbout.Lines.LoadFromFile(...)` 覆盖（`Form\frmAbout.pas:51-52`，路径内联未抽常量）；否则用 dfm 内置英文文本（`Form\frmAbout.dfm:1048-1074`）。`Lang` 来自 ini `[GUI] Lang`（未配置时：系统语言 ID = 2052 用 `'简体中文'`，否则 `DEFAULT_LANG='EN'`；`Unit\untALTRunOption.pas:805-809`、`:105`）。
- 统计行：`lblStatistics.Caption := Format(resStatistics, [ShortCutRunCount])`（`Form\frmAbout.pas:49`）；`resStatistics` 默认 `'You have run shortcut %d times by now'`（`Unit\untALTRunOption.pas:1114`；`Bin\EN.lang:210`）。
  - `ShortCutRunCount` 存 ini `[DEBUG]`（读 `Unit\untALTRunOption.pas:813`，写 `:886`；样例 `Bin\ALTRun.ini:52` 为 11562）；
  - 每次执行快捷项 `Inc(ShortCutRunCount)`（`Unit\untShortCutMan.pas:2092`），且**每满 10000 次弹一次祝贺框**（`:2094-2097`，文案 `resCongratulations`，`Bin\EN.lang:52`）。
- 横幅图 `imgIcon: TImage`：dfm 内嵌位图，185 × 56，`Cursor = crHandPoint`，`Transparent = True`（`Form\frmAbout.dfm:33-38`、`:1033`）。
- **链接行为**：点击横幅图 → `ShellExecute(0, nil, UPGRADE_URL, nil, nil, SW_SHOWNORMAL)`（`Form\frmAbout.pas:66-69`）；`UPGRADE_URL = 'http://journeyboy.blog.sohu.com/112820379.html'`（`Unit\untALTRunOption.pas:25`）。无返回值检查、无失败提示。
  - Memo 内正文里的 URL **不可点击**（`TMemo` 不做自动链接），用户只能复制。
- 键盘：`Esc` 或 `Enter` → `ModalResult := mrCancel`（`Form\frmAbout.pas:60-64`，`KeyPreview = True`，`dfm:14`）。
- 关闭按钮：`btnOK`，`Kind = bkOK`（`dfm:1084-1092`），文字运行期置 `resBtnOK`（`pas:48`）。
- 布局：`bsDialog`、`poScreenCenter`、454 × 328；正文 `ReadOnly`、`ScrollBars = ssBoth`、`WordWrap = False`（`dfm:4-16`、`:1036-1083`）。

### 6.2 `Bin\About.EN` 与 `Bin\About.简体中文`

- 纯文本（ANSI/GBK，CRLF），36 行，四个段落：
  - `[Usage]` / `[使用方法]`：1. 按热键唤出（缺省 Alt+R） 2. 输入关键字回车执行（`About.EN:1-3`、`About.简体中文:1-3`）
  - `[About Author]` / `[关于作者]`：ET Worker / ET民工[源自火星] + 邮箱（`:5-7`）
  - `[Web Site]` / `[软件主页]`：最新版博客地址 + Google Code 开源地址（`:9-14`）
  - `[Acknowledgements]` / `[致谢列表]`：约 20 位贡献者名单及链接（`:16-36`）
- 两份文件结构、人数一致，仅语言不同；`Lang` 决定加载哪一个（`Form\frmAbout.pas:51-52`）。

### 6.3 帮助窗口（Form\frmHelp.pas / .dfm）

- `frmHelp.pas` **没有任何事件处理代码**（全文 `:1-26` 只有类声明与空 implementation）。
- `frmHelp.dfm` 的 `mmoAbout: TMemo` **没有 `Lines.Strings` 内容**（`dfm:20-40`）→ 运行期是一个**空窗口**。
- 属性：标题 `'Help'`、442 × 326、`poScreenCenter`；Memo `ReadOnly`、`ScrollBars = ssBoth`、`WordWrap = False`；`btnOK` 为 `Kind = bkOK`（`dfm:4-6`、`:20-49`）。
- 打开方式：快捷项编辑窗口的 Help 按钮 → `btnHelpClick`（`Form\frmShortCut.pas:143-153`），每次新建、`finally` 释放。
- 判断：这是**未完成/占位功能**（内容从未被填充，也没有加载文件的逻辑）。

### 6.4 语言选择窗口（Form\frmLang.pas / .dfm）

- `frmLang.pas` 同样**无代码**（`:1-26`）；`.dfm` 只有 `cbbLang: TComboBox`（`csDropDownList`）与 `btnOK: TBitBtn`（`Kind = bkOK`）；`bsDialog` + `FormStyle = fsStayOnTop`、294 × 38、标题 `'Please choose language'`（`dfm:4-7`、`:14`、`:22-41`）。
- 全部逻辑在主窗体 `FormCreate` 里（`Form\frmALTRun.pas:1882-1937`）：
  1. 仅"首次运行"触发：`IsRunFirstTime := not FileExists(<exe目录>\ALTRun.ini)`（`ALTRun.dpr:42`）。
  2. 下拉项：先加 `DEFAULT_LANG = 'EN'`（`Unit\untALTRunOption.pas:105`；`Form\frmALTRun.pas:1888-1889`），再由 `GetLangList` 扫描 exe 目录下 `*.lang`（`:2322-2348`；底层 `GetFileListInDir(..., 'lang', False)`，`untUtilities.pas:910-936` 用**子串包含**过滤扩展名，并去掉文件名末尾 5 个字符 `.lang` 得语言名）。
  3. 选中当前 `Lang`；若 `Lang = '简体中文'` 则把标题改成中文 `'请选择界面语言'`（`:1900-1913`，注释"专门为了简体中文"）。
  4. 确认 → `Lang := LangForm.cbbLang.Text; SetActiveLanguage;`（`:1917-1921`）。
  5. **取消 → `DeleteFile(<exe目录>\ALTRun.ini)` 然后 `Halt(1)`**（`:1922-1926`）：删配置并立即退出。重写时这是必须重新评估的破坏性行为。
  6. 若一个 `.lang` 都扫不到（`LangList.Count = 0` 或 `GetLangList` 失败），**跳过**该窗口（`:1909`、`:1929-1932`）。
- 潜在 Bug：`GetFileListInDir` 用 `FindFirst` 后直接 `while FindNext`（`untUtilities.pas:922-924`），**目录枚举的第一项被跳过**，可能漏掉一个语言文件。
- 本地化机制：`SetActiveLanguage`（`Unit\untALTRunOption.pas:894` 起）以 `TIniFile` 读 `<exe目录>\<Lang>.lang`，段结构固定：`[Common]`、`[ALTRunForm]`、`[ConfigForm]`、`[InvalidForm]`、`[ShortCutManForm]`、`[ShortCutForm]`、`[AboutForm]`、`[DefaultShortCutName]`（`Bin\EN.lang:1-209`、`Bin\简体中文.lang` 同构）。所有界面字符串都是"读取 ini 键 + 内置英文默认值"。
- 注意 `About.<Lang>` 与 `<Lang>.lang` 是**两套独立文件**：前者是关于窗正文纯文本，后者是界面字符串 INI。

---

## 7. 托盘图标交互

### 7.1 组件与图标

- `ntfMain: TCoolTrayIcon`（第三方 `3rdUnit\CoolTrayIcon`，`Form\frmALTRun.dfm:3484` 起），属性：`IconIndex = 0`（`:8546`）、`PopupMenu = pmMain`（`:8547`）、`MinimizeToTray = True`（`:8548`）、`OnClick = ntfMainDblClick`、`OnDblClick = ntfMainDblClick`（`:8549-8550`）。
- 可见性：启动时先 `IconVisible := False`（`Form\frmALTRun.pas:1867`），配置加载后 `ntfMain.IconVisible := ShowTrayIcon`（`:2136`；配置修改时 `:472`；`ShowTrayIcon` 默认 True：`Unit\untALTRunOption.pas:766`、`Bin\ALTRun.ini:20`）。
- 悬浮提示：`ntfMain.Hint := Format(resMainHint, [TITLE, ALTRUN_VERSION, #13#10, GetHotKeyString])`（`Form\frmALTRun.pas:2150`；默认模板 `'%s %s%s[HotKey = %s]'`，`Unit\untALTRunOption.pas:939`）。

### 7.2 单击 / 双击 / 右键

| 操作 | 底层消息 | 结果 |
|---|---|---|
| **左键单击** | `WM_LBUTTONDOWN` 启动 `ClickTimer`（`GetDoubleClickTime`）→ 超时未再点击才 `Click` | 因绑定了 `OnDblClick`，单击被**延迟一个双击间隔**才触发 `ntfMainDblClick`（`CoolTrayIcon.pas:440-462`、`:485-504`、`:1505-1509`） |
| **左键双击** | `WM_LBUTTONDBLCLK` → `DblClick` | 同一处理函数 `ntfMainDblClick`（`CoolTrayIcon.pas:534-548`、`:1512-1516`）；单击/双击**行为完全一致** |
| **右键** | `WM_RBUTTONDOWN` 且 `PopupMenu.AutoPopup` → `SetForegroundWindow(TrayIconHandler.FHandle)` + `PopupAtCursor` | 在光标处弹出 `pmMain`（`CoolTrayIcon.pas:464-483`） |
| 中键 | `WM_MBUTTONDOWN`/`WM_MBUTTONUP` | 未接线（`CoolTrayIcon.pas:477-484`、`:526-533`，工程未设 `OnMouseDown`） |

`ntfMainDblClick` 的动作（`Form\frmALTRun.pas:2543-2573`）：

1. 若 `m_IsFirstDblClickIcon`：弹 `frmAutoHide` 提示热键（见第 5 节），然后置标志为 False（`:2549-2567`）。
2. `if m_IsShow then actHideExecute else actShowExecute`（`:2569-2572`）→ 显示/隐藏主窗切换。

### 7.3 右键菜单清单（`pmMain`，`Form\frmALTRun.dfm:8554-8573`）

| 顺序 | 菜单项 | Action | 快捷键 | 动作 | 源码 |
|---|---|---|---|---|---|
| 1 | Show | `actShow` | — | 显示主窗（`actShowExecute`） | `dfm:8558-8560`、`:8582-8586`；`Form\frmALTRun.pas:973-1153` |
| 2 | ShortCut(&S) | `actShortCut` | `Alt+S` | 打开快捷项管理器（`TShortCutManForm`） | `dfm:8561-8563`、`:8587-8593`；`pas:857-971` |
| 3 | Config(&C) | `actConfig` | `Alt+C` | 打开配置窗口 | `dfm:8564-8566`、`:8594-8600`；`pas:253-546` |
| 4 | About(&A) | `actAbout` | `F1`（主窗内） | 打开关于窗口 | `dfm:8567-8569`、`:8601-8605`；`pas:193-218`、`:2228-2232` |
| 5 | Close(&X) | `actClose` | — | 保存设置/最近列表/快捷项，`Application.Terminate` | `dfm:8570-8572`、`:8606-8610`；`pas:230-251` |

- 菜单文字运行期被语言资源覆盖：`actShow.Caption := resMenuShow; ... actClose.Caption := resMenuClose;`（`Form\frmALTRun.pas:2084-2088`；默认值 `Bin\EN.lang:9-13`，中文如"显示(&W)/快捷项(&S)/配置(&C)/关于(&A)/关闭(&X)"）。
- 菜单图标：`Images = ilHotRun`（`dfm:8555`），各 Action `ImageIndex = 0..4`（`dfm:8578-8610`）。
- 主窗输入框/列表右键另有 `pmList`（Add/Edit/Delete/-/Open Dir，`dfm:8657-8676`）与 `pmCommandLine`（**空菜单，只有 `OnPopup`**，`dfm:8983-8987`；`OnPopup = pmListPopup` 只控制 `mniOpenDir` 可见性，`pas:2575-2579`）。所以"复制"没有右键入口，只能 `Ctrl+C`。

### 7.4 启动通知与首双击提示

- 启动气泡（`ShowStartNotification` 默认 True，`Unit\untALTRunOption.pas:760`）：
  `ntfMain.ShowBalloonHint(resInfo, Format(resStarted + #13#10 + resPressKeyToShowMe, [TITLE, ALTRUN_VERSION, GetHotKeyString]), bitInfo, 5)`（`Form\frmALTRun.pas:2144-2147`）——标题 `resInfo`、图标 `bitInfo`、超时 **5 秒**（`CoolTrayIcon` 允许 5..60，`CoolTrayIcon.pas:102`）。
- 首次双击托盘的提示窗：`frmAutoHide`（第 5 节）。

### 7.5 关闭 / 最小化拦截

- `WM_SYSCOMMAND + SC_CLOSE`：DEBUG 模式真退出，否则仅隐藏并清空输入（`Form\frmALTRun.pas:1793-1804`）——**用户点 X 不会退出程序**。
- `WM_QUERYENDSESSION / WM_ENDSESSION`（系统关机）：`actCloseExecute` 保存后退出（`:1808-1814`）。
- 最小化：`MinimizeToTray = True`（`dfm:8548`）+ `Application.OnMinimize := ALTRunForm.evtMainMinimize`（`ALTRun.dpr:50`）→ `Hide` + `StopTimer`（`Form\frmALTRun.pas:1831-1841`）。
- `ExitWhenExecute = 1` 时，执行完快捷项后启动 `tmrExit`（未设 Interval → VCL 默认 **1000 ms**，`dfm:8957-8960`），到点 `actCloseExecute`（`Form\frmALTRun.pas:745`、`:2701-2707`）。

---

## 8. 无边框拖动、窗口置顶与焦点获取

### 8.1 主窗口样式

| 属性 | 值 | 位置 |
|---|---|---|
| `BorderStyle` | `bsNone`（无标题栏/无边框） | `Form\frmALTRun.dfm:4` |
| `FormStyle` | `fsStayOnTop`（置顶） | `:15` |
| `Color` / `TransparentColor` | `clMaroon` / `True`（颜色键透明底色） | `:8-9` |
| 初始位置 | `Left = -400`、`Top = -300`（先藏在屏幕外） | `:2-3` |
| 客户区 | 420 × 250 | `:7-8` |
| 双缓冲 | 窗体与 5 个子控件 `DoubleBuffered := True`（防闪） | `Form\frmALTRun.pas:1870-1875` |

### 8.2 无边框拖动实现（核心技巧）

没有 `WM_NCHITTEST` 处理；靠"控件 `OnMouseDown` → 释放捕获 → 发系统命令"实现：

```pascal
if Button = mbLeft then
begin
  ReleaseCapture;
  SendMessage(Handle, WM_SYSCOMMAND, SC_DRAGMOVE, 0);
end
else if Button = mbMiddle then actExecuteExecute(Sender);
```

- 常量：`SC_DRAGMOVE: LongInt = $F012`（`Unit\untUtilities.pas:53`）。
- 拖拽热区 1：背景图 `imgBackground.OnMouseDown`（`Form\frmALTRun.pas:2442-2454`）。
- 拖拽热区 2：标题标签 `lblShortCut.OnMouseDown`（`:2465-2477`）。
- 中键在以上两处及输入框/提示框/命令行框（`MiddleMouseDown`，`:1356-1363`）+ `edtCopy`（`dfm:3418`）上都会执行当前项。
- **限制**：`$F012 = SC_DRAGMOVE` 是 Vista 起才支持的系统命令；XP 上行为不确定（源码未做版本分支，工程资源里带 `Res\ALTRunForVista.manifest` 暗示只保证 Vista+）。

### 8.3 分层透明与圆角

首次显示时一次性设置（`Form\frmALTRun.pas:1086-1096`）：

```pascal
lg := GetWindowLong(Handle, GWL_EXSTYLE) or WS_EX_LAYERED;
SetWindowLong(Handle, GWL_EXSTYLE, lg);
SetLayeredWindowAttributes(Handle, AlphaColor, Alpha, LWA_ALPHA or LWA_COLORKEY);
SetWindowRgn(Handle, CreateRoundRectRgn(0, 0, Width, Height, RoundBorderRadius, RoundBorderRadius), True);
```

- `AlphaColor` 默认 `clBlack`（`Unit\untALTRunOption.pas:100`；样例 ini 设 128）、`Alpha` 默认 240（`:101`）、`RoundBorderRadius` 默认 12（`:102`）、`FormWidth` 默认 420（`:103`）；读取处 `:800-803`。
- 配置变化时会重设圆角：`:519`。
- 高度随 `ShowCommandLine` 切换为 250 / 230（`:1104-1107`）。

### 8.4 窗口置顶

- 主窗：`FormStyle = fsStayOnTop`（`dfm:15`）→ VCL 自动加 `WS_EX_TOPMOST`。
- 参数窗 / 快捷项编辑窗：覆写 `CreateParams` 显式追加 `WS_EX_TOPMOST`，并且 `WndParent := GetDesktopWindow`（`Form\frmParam.pas:169-179`；`Form\frmShortCut.pas:220-230`）。
- 语言窗：`FormStyle = fsStayOnTop`（`Form\frmLang.dfm:14`）。
- 全仓库**没有**直接 `SetWindowPos(HWND_TOPMOST)` 调用。

### 8.5 焦点获取相关技巧与失败史

- 显示前先取环境信息（剪贴板、前台窗口），避免被自己激活后取错（`Form\frmALTRun.pas:2424-2431`；ALT+L 版 `:2388-2395`）。
- 显示：`Application.Restore; SetForegroundWindow(Application.Handle); ... edtShortCut.SetFocus;`（`Form\frmALTRun.pas:1122-1128`），并把 `tmrFocus.Enabled := True`（`:1128`）。
- 第二实例唤起：`SendMessage(HandleID, WM_ALTRUN_SHOW_WINDOW, 0, 0)`（`:2032`；消息常量 `WM_USER + 2001`，`:179-180`；`HandleID` 存 ini `[DEBUG]`，`Unit\untALTRunOption.pas:814`），处理端 `actShowExecute` 后再补一次 `SetForegroundWindow`（`Form\frmALTRun.pas:2759-2765`）。
- 编辑框/参数窗显示后也 `SetForegroundWindow(Application.Handle)` + `SetFocus`（`Form\frmShortCut.pas:258-262`；`Form\frmParam.pas:246-250`）。
- **已放弃的 hack**：
  - `tmrFocus`（500 ms 定时重试 `SetForegroundWindow`）整体逻辑被 `Exit` 掉，注释写明"因为将窗体重新置于前台经常失败，故放弃这个Timer"（`Form\frmALTRun.pas:2709-2735`，`Exit` 在 `:2718`）。
  - `evtMainDeactivate` 中"失焦后循环 `SetForegroundWindow` 抢回前台"的代码整段注释（`:1708-1752`，含 `:1729` 的循环）。
  - 现在的策略是"失焦就隐藏"，不再抢焦点（`:1704-1706`）。
- 未使用 `AttachThreadInput`、`AllowSetForegroundWindow`、`keybd_event` 等前台锁技巧（检索无命中）。

### 8.6 各窗口的无边框/置顶汇总

| 窗口 | 边框 | 置顶方式 | 父窗 | 位置 |
|---|---|---|---|---|
| 主窗 `TALTRunForm` | `bsNone` + 圆角 Region + 分层 | `fsStayOnTop` | 默认 | `dfm:4`、`:15`；`pas:1086-1096` |
| 参数窗 `TParamForm` | `bsDialog` | `WS_EX_TOPMOST` + `WS_POPUP` | `GetDesktopWindow` | `frmParam.pas:169-179` |
| 快捷项编辑 `TShortCutForm` | 标准 | 同上 | 同上 | `frmShortCut.pas:220-230` |
| 关于 `TAboutForm` | `bsDialog` | — | Owner=主窗 | `frmAbout.dfm:4` |
| 无效项 `TInvalidForm` | 默认 | — | Owner=管理器 | `frmInvalid.dfm:1-15` |
| 自动隐藏 `TAutoHideForm` | `bsDialog` + `AlphaBlend` | — | Owner=主窗 | `frmAutoHide.dfm:3-5` |
| 帮助 `THelpForm` | 默认 | — | Owner=编辑窗 | `frmHelp.dfm:1-14` |
| 语言 `TLangForm` | `bsDialog` | `fsStayOnTop` | Owner=主窗 | `frmLang.dfm:4`、`:14` |

---

## 9. 关键硬编码常量与默认值（含行号）

### 9.1 产品/协议常量（`Unit\untALTRunOption.pas`）

| 常量 | 值 | 行号 |
|---|---|---|
| `DEBUG_MODE` | `False`（发布版） | `:16` |
| `DEBUG_SORT` | `False` | `:17` |
| `TITLE` | `'ALTRun'` | `:21` |
| `ALTRUN_VERSION` | `'V1.46.7'` | `:22` |
| `UPGRADE_URL` | `http://journeyboy.blog.sohu.com/112820379.html` | `:25` |
| `PARAM_FLAG` | `'%p'` | `:27` |
| `NEW_PARAM_FLAG` | `'{%p}'` | `:28` |
| `CLIPBOARD_FLAG` | `'{%c}'` | `:29` |
| `FOREGROUND_WINDOW_ID_FLAG` | `'{%wd}'` | `:30` |
| `FOREGROUND_WINDOW_TEXT_FLAG` | `'{%wt}'` | `:31` |
| `FOREGROUND_WINDOW_CLASS_FLAG` | `'{%wc}'` | `:32` |
| `SHOW_MAX_FLAG` / `SHOW_MIN_FLAG` / `SHOW_HIDE_FLAG` | `'@+'` / `'@-'` / `'@'` | `:33-35` |
| `RESTART_FLAG` / `CLEAN_FLAG` | `'Restart'` / `'Clean'` | `:37-38` |
| `LOCALIZED_KEYNAMES` | `True` | `:40` |
| `DEFAULT_ALPHACOLOR` / `DEFAULT_ALPHA` | `clBlack` / `240` | `:100-101` |
| `DEFAULT_ROUNDBORDERRADIUS` / `DEFAULT_FORMWIDTH` | `12` / `420` | `:102-103` |
| `DEFAULT_BGFILENAME` | `'BG.jpg'` | `:104` |
| `DEFAULT_LANG` | `'EN'` | `:105` |
| `HideDelay` 默认 | `15`（秒） | `:752` |
| `ParamHistoryLimit` 默认 | `50` | `:757` |
| `TraceEnable` 默认（ini，实际被覆盖） | `False` | `:811` |
| `ShortCutRunCount` 默认 | `0` | `:813` |
| `resShowMeByHotKey` 默认 | `'It''s better to show me by press hotkey %s'` | `:940` |
| `resMainHint` 默认 | `'%s %s%s[HotKey = %s]'` | `:939` |
| `resCleanConfirm` 默认 | `'Confirm to disable autorun when system started, and remove me from "SendTo" menu?'` | `:951` |
| `resInvalidFormCaption` 默认 | `'Invalid ShortCut List'` | `:1073` |
| `resStatistics` 默认 | `'You have run shortcut %d times by now'` | `:1114` |
| `resCongratulations` 默认 | `'Congratulations! You have executed %d times!'` | `Bin\EN.lang:52` |
| 提示文案（剪贴板相关默认） | `'Press CTRL+C to copy CMD line to Clipboard'`、`'Type "cb" to search Clipboard text by Baidu'`、`'Type "cg" to search Clipboard text by Google'` | `:977`、`:984-985` |

### 9.2 各窗口/模块常量

| 项 | 值 | 行号 |
|---|---|---|
| `PARAM_HISTORY_FILE` | `'ParamHistory.txt'` | `Unit\untClipboard.pas` 无关；`Form\frmParam.pas:26` |
| 参数窗尺寸 | 394 × 22；OK 宽 50 | `Form\frmParam.dfm:6-7`、`:44-47` |
| 参数窗设计期 tmrHide | `Interval = 10000`（运行期被 `HideDelay*1000` 覆盖） | `Form\frmParam.dfm:58`；`Form\frmParam.pas:335` |
| 参数历史行格式 | `'%-10d|%-30s%'` | `Form\frmParam.pas:366-367` |
| 自动隐藏窗：等待 | `1500 ms` | `Form\frmAutoHide.dfm:47` |
| 自动隐藏窗：淡出步长/间隔 | `AlphaStep = 10`、`50 ms` | `Form\frmAutoHide.pas:45`；`dfm:54` |
| 自动隐藏窗尺寸 | 394 × 208 | `Form\frmAutoHide.dfm:7-8` |
| 无效项窗尺寸 | 472 × 326；列宽 100/100/100/400 | `Form\frmInvalid.dfm:5-6`、`:77-93` |
| 关于窗尺寸 / 横幅图 | 454 × 328 / 185 × 56 | `Form\frmAbout.dfm:6-7`、`:36-37` |
| 帮助窗尺寸 | 442 × 326 | `Form\frmHelp.dfm:5-6` |
| 语言窗尺寸 | 294 × 38；下拉 197 × 21 | `Form\frmLang.dfm:6-7`、`:34-35` |
| 主窗复制星标计时器 | `250 ms` | `Form\frmALTRun.dfm:8965` |
| 主窗焦点重试计时器 | `500 ms`（已停用） | `Form\frmALTRun.dfm:8972`；`Form\frmALTRun.pas:2718` |
| 主窗退出计时器 | 未设 Interval → 1000 ms 默认 | `Form\frmALTRun.dfm:8957-8960` |
| 主窗显示高度 | 250（显示命令行）/ 230 | `Form\frmALTRun.pas:1104-1107` |
| 应用自定义消息 | `WM_USER + 2000` / `WM_USER + 2001` | `Form\frmALTRun.pas:179-180` |
| 拖拽系统命令 | `SC_DRAGMOVE = $F012` | `Unit\untUtilities.pas:53` |
| 剪贴板自定义格式名 | `'Delphi Picture'` / `'Delphi Component'` | `Unit\untClipboard.pas:583-584` |
| 日志用户名缓冲 | `cnMaxUserNameLen = 254` | `Unit\untLogger.pas:31` |
| 启动气泡超时 | `5` 秒 | `Form\frmALTRun.pas:2147` |
| 通关祝贺阈值 | 每 `10000` 次 | `Unit\untShortCutMan.pas:2094` |
| 重启前等待 | `2000 ms` | `Form\frmALTRun.pas:1955` |
| `Clean` 流程 | `@ALTRun.exe Clean` | `Bin\Clean.bat:1`；`Form\frmALTRun.pas:1958-1970` |

### 9.3 `Bin\Clean.bat` 的实际作用

- 内容仅一行 `@ALTRun.exe Clean`（`Bin\Clean.bat:1`）。
- 主程序收到 `ParamStr(1) = 'Clean'`（`Form\frmALTRun.pas:1958`）→ 弹 `resCleanConfirm`（`:1960-1961`）→ 确认后 `SetAutoRun(TITLE,'',False)`、`SetAutoRunInStartUp(TITLE,'',False)`、`AddMeToSendTo(TITLE,False)` 取消开机启动与"发送到"菜单项（`:1963-1965`），然后 `Application.Terminate`。
- 它**不删文件**（不删 ini/日志/快捷项/历史），只是"注销系统集成"的入口。

---

## 10. 重写注意点（C++ / 现代 Windows）

### 10.1 剪贴板

- 原实现大量代码是 Delphi VCL 的"古董"（`OpenClipboard` 引用计数、`GMEM_DDESHARE`、`AllocMem`）。C++ 下只需封装 3 个原子操作：
  1. **读 Unicode 文本**（热键处理时）：`IsClipboardFormatAvailable(CF_UNICODETEXT)` → `OpenClipboard(nullptr)` → `GetClipboardData(CF_UNICODETEXT)` → 复制到 `std::wstring` → `CloseClipboard()`。**必须**在 `try/catch` 或 RAII 下调用，原版在剪贴板被占用时会抛异常（`untClipboard.pas:165-166`）。
  2. **写 Unicode 文本**（Ctrl+C 复制命令行）：`GlobalAlloc(GMEM_MOVEABLE, (len+1)*sizeof(wchar_t))` + `SetClipboardData(CF_UNICODETEXT, ...)`。**不要**照抄原版 `Length*4` 的大小计算（`untClipboard.pas:339`）。
  3. 需要兼容旧程序时再额外写一份 `CF_TEXT`（系统 ANSI 代码页），原版只写 Unicode。
- 如果产品以后要"复制富文本/HTML/文件列表"，需要 `RegisterClipboardFormat(L"HTML Format")` 自行拼 CF_HTML 头，或写 `CF_HDROP`（`DROPFILES` 结构 + 双 NUL 结尾的路径列表）——**原版完全没有这些**，属于新增需求，不是 1:1 对齐。
- 若要监听剪贴板变化：用 `AddClipboardFormatListener(hwnd)` + `WM_CLIPBOARDUPDATE`（Vista+），替代老式 `SetClipboardViewer` 链。原版没有需求，建议**不做**。
- `Clipboard` 单例、`TClipboard` 类、`CF_PICTURE/CF_COMPONENT` 自定义格式、图片/图元/组件流读写（`untClipboard.pas:178-220`、`:365-466`）在 ALTRun 中**零使用**，重写时直接丢弃，可省 ~400 行。
- `untUtilities.SetClipboardText`（`untUtilities.pas:1255-1287`）是死代码，丢弃。

### 10.2 日志

- 原版日志在发布版**永不写出**（`DEBUG_MODE=False` 覆盖 ini 开关），却占用了每处 `TraceMsg` 调用。建议重写为：
  - 保留 `TraceMsg/TraceErr` 调用点为 `DSH_TRACE(...)` 宏，编译期可整体关闭（等价于 `DEBUG_MODE`）；
  - 用 **spdlog**（或 `std::format` + 简单 `ofstream`）落地：命名按 `<exe名>.<用户名>.log`（沿用原路径规则，便于用户对照），**时间戳用 ISO 8601 固定格式**而不是 `DateTimeToStr` 的本地化格式（`untLogger.pas:62`、`:89`）；
  - 打开 `std::ios::app`，开一次文件常驻（原版每条日志开关文件，`untLogger.pas:75-82`）；
  - 加**大小轮转**（spdlog `rotating_file_sink`，如 2 MB × 3 份），原版没有任何轮转；
  - 用 `std::mutex` 替代 `TRTLCriticalSection`，并且把 `CloseFile` 放进 `finally` 正确位置（原版 `:79-82` 顺序有误）；
  - 若要保留 ini 的 `TraceEnable`，必须在读配置**之后**再初始化 logger，不要像原版那样用编译期常量覆盖用户设置（`untALTRunOption.pas:811` vs `frmALTRun.pas:2041`）。
- 记录内容建议沿用原版覆盖面（生命周期、每个动作入口、热键、执行失败、SetFocus 失败），因为排障时这些打点确实有用。

### 10.3 参数窗口

- 窗口本身用普通 `WS_POPUP | WS_EX_TOPMOST` 窗口（原版 `frmParam.pas:169-179` 的 `CreateParams` 等价于：`CreateWindowEx(WS_EX_TOPMOST, ...)`，不要用 `WS_EX_NOACTIVATE`）。
- 输入控件用 `ComboBox`（`CBS_DROPDOWN` + `AutoComplete`）或自绘下拉；原版选 `TComboBoxEx` 是因为 Delphi `TComboBox` 的中文 Bug（`frmParam.pas:1-2`），C++ 无此问题。
- 历史模型建议**保留语义但换存储**：
  - 原版"全局历史 + 使用次数 rank + 满额淘汰最小 rank + 新项 rank=0"（`frmParam.pas:85-108`）逻辑简单但语义有坑（新项永远最先被淘汰）。1:1 对齐时可照抄；若追求更好体验，改为 `unordered_map<wstring,int>` + LRU 或"次数+最近时间"双键，但要同步改 `ParamHistory.txt` 格式（原格式 `'%-10d|%-30s%'` 有末尾 `%` 风险，见 3.6，**不要照抄**）。
  - 历史加载的 mtime 缓存是死代码（只在启动读一次），可删。
- `HideDelay*1000` 的空闲自动取消（`frmParam.pas:335`）要保留：用 `SetTimer(hwnd, TIMER_IDLE_CANCEL, HideDelay*1000, nullptr)`，并在 `WM_ACTIVATE`/`WM_KEYDOWN`/`WM_LBUTTONDOWN` 时 `SetTimer` 重置（原版重置点见 3.4）。
- Esc 两段式行为（非空→清空；空→取消）容易被 C++ 重写漏掉，必须保留（`frmParam.pas:204-224`）。
- 参数窗与主窗解耦：主窗隐藏后再弹参数窗（`frmALTRun.pas:677`）；参数窗被主热键打断时以"取消"关闭（`frmALTRun.pas:985`）——C++ 下建议改为参数窗知道自己被取消，而不是由外部改 `ModalResult` 这种 Delphi 特有机制。

### 10.4 占位符替换

- 原版用正则替换（`TRegExpr`）实现，且**只处理第一个命中的占位符类型**（if/else-if 链，`untShortCutMan.pas:202-284`）。C++ 下建议：
  - 用一个统一的替换函数扫描全部占位符（`%p`、`{%p}`、`{%c}`、`{%wd}`、`{%wt}`、`{%wc}`），一次遍历全部替换——这会让"一条命令行同时用多个占位符"从**不支持**变为**支持**，属于行为增强（需与产品确认是否允许偏离原版）；
  - 若严格 1:1，保留"只替换第一类"的优先级链和"`{%p}` 优先于 `%p`"的顺序；
  - 替换时**不要**把用户输入当正则替换串（避免 `$` 被解释成反向引用，见 3.6 不确定项），用字面量替换即可。
- URL 编码表要照抄：`[A-Za-z0-9*@._-]` 不编码、空格→`+`、其余 `%XX` 大写（`untUtilities.pas:1136-1152`）；`ptUTF8Query` 先 UTF-8 编码再 `%XX`（`:1117-1134`）。C++ 用自带 `WideCharToMultiByte(CP_UTF8)` + 手写百分号编码。
- 执行仍建议后台线程 + `ShellExecuteW`/`ShellExecuteExW`；失败回退 `CreateProcessW`（比原版 `WinExec` 现代）。`@+`/`@-`/`@` 前缀映射到 `nShowCmd`（`untShortCutMan.pas:170-185`）。
- `ReplaceEnvStr` 展开 `%VAR%` 可直接用 `ExpandEnvironmentStringsW`（原版手写，`untUtilities.pas:995-1020`）。

### 10.5 无效项校验

- 原版的"校验"非常保守（大量提前放行，见 4.2），`resValidateConfirm` 自己也承认查不全。C++ 重写建议保留**同样的判定顺序**以保证 1:1，但可以：
  - 把判定抽成纯函数 `bool IsValidCommandLine(std::wstring)`，返回"有效/无效/无法判定"三态，UI 上对"无法判定"给灰色标记；
  - 用 `std::filesystem::exists` 替代 `FileExists` + `DirectoryExists` 两次调用（注意 `\\` 网络路径判定要保留）；
  - 校验放在后台线程 + 进度条（原版在 UI 线程同步遍历，`frmShortCutMan.pas:349-368` 期间 `Screen.Cursor := crHourGlass`）。
- 无效项窗口的交互（勾选删除 / 全选 / 全不选 / 测试）用 `ListView`（`LVS_REPORT | LVS_EX_CHECKBOXES`）直接对应；原版 `Data` 存原始索引的做法在 C++ 里换成"稳定 ID"更安全。
- **不要**新增后台自动校验（原版没有，且会误删用户暂时失效的路径）。

### 10.6 淡出提示 / 自动隐藏

- `frmAutoHide` 等价实现：无边框小窗 + `WS_EX_LAYERED` + `SetLayeredWindowAttributes(..., alpha, LWA_ALPHA)`，`SetTimer` 1500 ms 后每 50 ms 把 alpha 减 10，减到 <10 关闭（`frmAutoHide.pas:43-58`）。C++ 可以用 `Sleep` 死在 UI 线程不行，必须定时器或 `std::thread` + 消息。
- 现代系统可用 `DwmSetWindowAttribute`/动画 API（`AnimateWindow`）替代手写 alpha 循环，但**时长与视觉要和原版对齐**（约 2.8 s）才好验收。
- 主窗隐藏逻辑是重点：**失焦立即隐藏** + **HideDelay 空闲隐藏** 两条并存（见 5.4）。C++ 用 `WM_ACTIVATE`/`WM_KILLFOCUS` + `SetTimer`。注意原版"失焦即隐藏"意味着点任务栏/别处就消失，这是 ALTRun 的核心交互，**不要**改成"移出鼠标才隐藏"。

### 10.7 关于 / 帮助 / 语言

- 关于窗：内容来源必须保留"外部文件优先"（`About.<Lang>` 覆盖内置文本）；版本号从编译期常量改为**统一版本头文件/CMake 变量**填充，同时写入 exe 的 `VERSIONINFO` 资源，`About` 文本仍展示同一字符串。
- 横幅图点击打开网址：用 `ShellExecuteW(L"open", url, ...)`；建议加失败提示（原版无，`frmAbout.pas:68`）。
- Memo 里 URL 不可点击是原版行为；若想提升，可换 RichEdit + `ENM_LINK`，但属于增强。
- 帮助窗**当前是空窗口**（`frmHelp.dfm` 无内容）。1:1 重写意味着继续空着；建议与产品确认后改为加载 `Help.<Lang>.txt` 或内嵌帮助文本——这是原版明显的未完成项。
- 语言选择窗：保留 `.lang`（INI 段结构）与 `About.<Lang>` 两套文件格式以兼容现存语言包；`GetLangList` 的"第一项被跳过"Bug（`untUtilities.pas:922-924`）必须修。首次运行取消时**删除 ini 并退出**（`frmALTRun.pas:1922-1926`）是危险行为，建议改为"默认 EN 继续启动"。
- 语言字符串表建议改为 `std::unordered_map<std::wstring,std::wstring>` 一次加载整个 INI，避免原版"每个字符串一次 `TIniFile.ReadString`"的重复 IO。

### 10.8 托盘图标

- 用 `Shell_NotifyIconW` + `NOTIFYICONDATAW`：`NIF_ICON|NIF_MESSAGE|NIF_TIP`，回调 `WM_APP+1`，`uCallbackMessage` 里区分 `WM_LBUTTONUP`/`WM_LBUTTONDBLCLK`/`WM_RBUTTONUP`。
- **单击延迟要复刻**：原版因为同时绑定 Click 与 DblClick，单击必须等 `GetDoubleClickTime()`（`CoolTrayIcon.pas:440-462`）。C++ 下两个选择：a) 完全复刻（单击延迟约 500 ms 才响应，用户体验差但 1:1）；b) 只处理双击 + 右键（很多现代启动器这样做）。建议与产品确认；若保留单击，可用 `GetDoubleClickTime()` 定时器手动模拟。
- 右键菜单用 `TrackPopupMenu`，弹出前**必须** `SetForegroundWindow(hwnd)`（否则点别处菜单不消失），再 `PostMessage(hwnd, WM_NULL, 0, 0)`（现代等效于原版 `CoolTrayIcon.pas:464-483` 的 `SetForegroundWindow(TrayIconHandler.FHandle)`）。
- 启动气泡用 `NIF_INFO` + `NOTIFYICONDATAW`（原版 `ShowBalloonHint(..., 5)`，`frmALTRun.pas:2144-2147`）；现代 Windows 10/11 会转成通知中心，超时参数语义已变，需实测。
- 关闭按钮拦截：用 `WM_SYSCOMMAND`/`SC_CLOSE` 直接 `return 0` + 隐藏（原版 `frmALTRun.pas:1793-1804`），或更规范地用 `WM_CLOSE`；退出只走托盘菜单与系统关机消息（`WM_QUERYENDSESSION`/`WM_ENDSESSION`）。
- `MinimizeToTray`（原版靠 CoolTrayIcon 组件，`dfm:8548`）在 C++ 里要自己处理 `WM_SYSCOMMAND/SC_MINIMIZE` 或 `WM_SIZE`。

### 10.9 无边框窗口、置顶与焦点

- **拖动**：原版 `ReleaseCapture()` + `SendMessage(WM_SYSCOMMAND, SC_DRAGMOVE=0xF012)`（`frmALTRun.pas:2442-2454`、`:2465-2477`；常量 `untUtilities.pas:53`）。现代更通用的写法是处理 `WM_NCHITTEST` 返回 `HTCAPTION`（对无边框窗口同样有效，且不依赖 Vista+ 的 `SC_DRAGMOVE`），**推荐用 `WM_NCHITTEST`**；若要 1:1 保留 `SC_DRAGMOVE` 也可以（Vista+ 全支持）。
- **分层透明 + 颜色键**：`SetWindowLong(GWL_EXSTYLE, WS_EX_LAYERED)` + `SetLayeredWindowAttributes(hwnd, colorKey, alpha, LWA_ALPHA|LWA_COLORKEY)`（`frmALTRun.pas:1086-1093`）。C++ 同样 API 即可，注意 `WS_EX_LAYERED` 在 Win8+ 对子窗口可用性变化，但顶层窗口无碍。
- **圆角**：原版 `CreateRoundRectRgn` + `SetWindowRgn`（`frmALTRun.pas:1096`）。Win11 可改用 `DwmSetWindowAttribute(DWMWA_WINDOW_CORNER_PREFERENCE)`，但半径不可精确匹配 12 px；若要求像素级一致，保留 Region 方案（注意 Region 圆角**无抗锯齿**，边缘会有锯齿）。
- **置顶**：`WS_EX_TOPMOST`（主窗靠 `fsStayOnTop` 隐式设置）。C++ 里直接 `CreateWindowEx(WS_EX_TOPMOST|...)` 或 `SetWindowPos(HWND_TOPMOST)`。
- **焦点**：原版承认"抢前台经常失败"并放弃了重试循环（`frmALTRun.pas:2717-2718`）。现代 Windows 前台锁更严格，建议：
  - 热键触发时用 `AllowSetForegroundWindow(ASFW_ANY)` 是错误方向；正确做法是热键注册（`RegisterHotKey`）后系统通常允许前台切换，再 `SetForegroundWindow` + `SetFocus`；
  - 备用方案：`keybd_event(VK_MENU)` 抖动 / `AttachThreadInput` 到前台线程后再 `SetForegroundWindow`（原版没用，慎用）；
  - 更稳的产品化方案是"热键 → 消息 → 主线程 `ShowWindow(SW_SHOWNOACTIVATE)` + 自绘键盘焦点"或改用 `Windows.UI.Input`/无障碍 API 的启动器集成，但这偏离 1:1。
  - 注意**采集顺序**：必须在显示主窗之前抓 `GetForegroundWindow()` 与剪贴板（`frmALTRun.pas:2424-2431`），否则拿到的是自己。
- **失焦即隐藏**（`frmALTRun.pas:1698-1706`）在 C++ 中用 `WM_ACTIVATE(WA_INACTIVE)` 实现；注意"弹出自己的参数窗/对话框"时也会触发失焦，原版靠 `ShowModal` 的 Application 级处理规避，C++ 需要判断"新前台窗口是否属于本进程"（`GetWindowThreadProcessId` 比对），否则参数窗一弹主窗就藏了（原版顺序是先藏主窗再弹，所以没暴露问题，但重写若调整顺序会踩坑）。

### 10.10 可以整体删除的包袱

| 内容 | 原因 |
|---|---|
| `Unit\untClipboard.pas` 的 VCL 拷贝（图片/图元/组件流/`CF_PICTURE`/`CF_COMPONENT`/`HasFormat`/`Assign`，约 `:22-543` 大部分） | ALTRun 只用 Unicode 文本读写 |
| `untUtilities.SetClipboardText`（`untUtilities.pas:1255-1287`） | 死代码 |
| `untUtilities` 的 `SendKeys`/`PostKeyEx32`/`GetDragFile*`/`CreateLink` 等 | 主流程未用（`frmALTRun.pas:690-742` 全注释） |
| `untLogger` 的 `CnDebug` 通道、`MutexHandle` 变量 | CnPack 专用/未使用 |
| `frmHelp` 的空壳 | 未完成功能，需产品决策 |
| `frmParam` 的历史 mtime 缓存、`SaveParamHistory` 的 `Result` | 死逻辑 |
| `Res\ALTRunForVista.manifest`、`ALTRun.bdsproj`、`*.dcu`、`*.xml`（Delphi 工程文件） | 重写后无意义 |

### 10.11 重写时必须保留的外部契约（1:1 对齐清单）

1. **文件契约**（与 exe 同目录）：`ALTRun.ini`、`ShortCutList.txt`（主模块规格）、`ParamHistory.txt`、`About.<Lang>`、`<Lang>.lang`、`BG.jpg`、`Popup.wav`、`ALTRun.<用户名>.log`。若格式变更，需提供迁移。
2. **INI 键**：`[Config]` 的 `HideDelay`、`ParamHistoryLimit`、`ShowTrayIcon`、`ShowStartNotification`、`PlayPopupNotify`、`ExitWhenExecute`、`LastItemHotKey`；`[GUI]` 的 `Lang`、`Alpha`、`AlphaColor`、`RoundBorderRadius`、`FormWidth`、`BGFileName`、字体串；`[DEBUG]` 的 `ShortCutRunCount`、`HandleID`（单实例唤醒）、`LatestList`。键名与语义需逐项对应（读取处集中在 `Unit\untALTRunOption.pas:738-819`，写入 `:822-892`）。
3. **热键语义**：主热键显示/隐藏、`Alt+L` 执行最近项（`Form\frmALTRun.pas:2080-2081`、`:2380-2409`）。
4. **参数占位符**语法与 URL 编码字节级一致（`Unit\untShortCutMan.pas:191-336`、`untUtilities.pas:1117-1152`）。
5. **剪贴板采集时机**：热键按下瞬间、主窗显示之前（`frmALTRun.pas:2424`）。
6. **命令行的 `@+`/`@-`/`@` 前缀**与 `.\` 相对路径语义。
7. **段标题**：`[Usage]`/`[About Author]`/`[Web Site]`/`[Acknowledgements]`（`About.EN:1-36`）——若用户自定义了 `About.<Lang>`，重写版应原样显示不解析。
8. **单实例唤醒**：`WM_ALTRUN_SHOW_WINDOW` 的替代方案（命名管道/`RegisterWindowMessage`）需保证第二实例能唤起主窗并退出。

---

## 附：不确定项汇总

1. 发布版 Delphi 编译版本（ANSI/Unicode）——由代码风格推断为 2007 或更早，**不确定**。
2. `Format('%-10d|%-30s%')` 末尾 `%` 是否抛异常 → 参数历史是否从未成功保存，**不确定**（无法在无 Delphi 环境验证；`Bin\` 中无 `ParamHistory.txt` 与之相容）。
3. `TRegExpr.Replace` 默认是否解释 `$` 替换符 → 参数含 `$` 时行为**不确定**。
4. `SC_DRAGMOVE` 在 XP 上的行为**不确定**（代码无版本分支）。
5. `CoolTrayIcon` 在 `OnClick`+`OnDblClick` 同时绑定时单击延迟的确切毫秒数取决于系统 `GetDoubleClickTime()`（通常 500 ms），**具体值随系统设置变化**。
6. 无效项校验对"带空格的残缺路径"的准确率——源码注释自己承认 `D:\Philips\Lenovo\Status.xls` 之类会逃过检查（`Form\frmShortCutMan.pas:497-508` 注释块）。
