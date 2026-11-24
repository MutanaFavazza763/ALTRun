# 03 · 快捷项数据模型与核心工具库功能规格（ALTRun v1.46.7）

> 目的：为 C++ 重写提供 1:1 对齐依据。本文只描述**原始 Delphi 实现的实际行为**，包括缺陷与历史兼容逻辑；不描述理想设计。
> 分析对象：`ALTRun-original`（只读，未做任何修改）。
> 分析日期：2026-11-24。

## 0. 阅读须知与证据约定

### 0.1 源文件与引用缩写

| 缩写 | 源文件（相对于 `ALTRun-original/`） | 行数 |
|---|---|---|
| `SCM` | `Unit/untShortCutMan.pas` | 2103（任务描述中的“约 1761 行”不准确，以实测为准） |
| `UT` | `Unit/untUtilities.pas` | 1347 |
| `OPT` | `Unit/untALTRunOption.pas` | 1159 |
| `LOG` | `Unit/untLogger.pas` | 130 |
| `FSCM` | `Form/frmShortCutMan.pas` / `.dfm` | 785 / 676 |
| `FSC` | `Form/frmShortCut.pas` / `.dfm` | 304 / 209 |
| `FMAIN` | `Form/frmALTRun.pas` | 2802 |
| `FCFG` | `Form/frmConfig.pas` | — |
| `FINV` | `Form/frmInvalid.pas` | — |
| `FPARAM` | `Form/frmParam.pas` | — |
| `REGEX` | `3rdUnit/regexpr/RegExpr.pas` | — |
| `INI` | `Bin/ALTRun.ini` | 54 |
| `LANG` | `Bin/简体中文.lang` | — |
| `DFM` | 对应窗体 `.dfm` 文件 | — |

引用格式统一为 `文件:行号` 或 `文件:起-止`，行号即原始文件行号。

### 0.2 源文件编码

- `untShortCutMan.pas`、`untUtilities.pas`、`frmShortCutMan.pas`、`frmShortCut.pas`：**GBK/ANSI（代码页 936）**，无 BOM，按 UTF-8 解码会失败。
- `frmShortCutMan.dfm`、`frmShortCut.dfm`：纯 ASCII。
- `frmALTRun.pas`：**UTF-8 无 BOM**（与其它 `.pas` 编码不一致，混用）。
- 因此本文分析采用“GBK 解码后的行号”，与原始字节行号一致。
- 编译版本**不确定**：仓库同时存在 `ALTRun.dproj`（Delphi 2007+）与 `ALTRun.bdsproj`（BDS 2006），无法仅凭工程文件断定 `string` 是 `AnsiString` 还是 `UnicodeString`（影响 §5、§10 中标注的行为）。

### 0.3 已确认“不存在”的能力（先给结论，后文给证据）

- 无“别名（alias）”字段；无“备注（note）”字段；无“图标（icon）”字段；无“最后使用时间”字段；无“是否收藏”布尔字段；无“目标路径/参数/工作目录”分离字段。
- 无开始菜单/桌面扫描；无批量导入/导出；无复制粘贴/克隆条目命令；无按列排序。
- 所谓 TopTen 不是持久化榜单，只是“当前过滤结果取前 10 条显示”。
- 收藏（Favourite）不是条目字段，而是独立的 `FavoriteList.txt` 映射（**搜索关键字 → 显示名**）。

---

## 1. 总体结构

### 1.1 关键类型（SCM:28-150）

| 类型 | 定义 | 说明 |
|---|---|---|
| `TShortCutType` | `(scOther, scItem, scBlank, scRemark)` | 条目种类；`scItem`=真实快捷项，`scBlank`=空白分隔行；`scOther`/`scRemark` 仅在解析中间态出现，**不持久化**（SCM:30） |
| `TParamType` | `(ptNone, ptNoEncoding, ptURLQuery, ptUTF8Query)` | 参数模式（SCM:33） |
| `TShortCutIcon` | `(siItem, siInfo)` | 管理界面图标索引：0=快捷项，1=空白行（SCM:36） |
| `TCmdObject` | `Command / WorkingDir / Param / ParamType` | 执行期临时对象，不持久化（SCM:38-44） |
| `TShortCutItem` | 见 §2.2 | 持久化数据模型（SCM:47-56） |
| `TShortCutList` | `= TObjectList` | `OwnsObjects=True`，自动释放条目（SCM:59、622） |
| `TShortCutMan` | 管理器类 | 全局单例 `ShortCutMan: TShortCutMan`（SCM:152-153），在 `FMAIN:1940` 创建 |

### 1.2 数据流概览

```
ShortCutList.txt  ──LoadShortCutList()──▶ m_ShortCutList(TObjectList)
        ▲                                      │ Sort() → m_SortedShortCutList(带显示串+对象指针)
        │                                      ▼
   SaveShortCutList() ◀── 内存修改(Freq/CRUD)   FilterKeyWord() → 结果 TStringList
                                               │
管理界面(FSCM) ◀── FillListView()/LoadFromListView() ──▶ m_ShortCutList
FavoriteList.txt ◀── SaveFavoriteList()/LoadFavoriteList() ──▶ m_FavoriteList
ALTRun.ini [DEBUG] LatestList ◀── 索引串 ──▶ m_LatestList(TStringList, 内存)
```

### 1.3 持久化文件一览

| 文件 | 位置 | 内容 | 读写时机 | 证据 |
|---|---|---|---|---|
| `ShortCutList.txt` | 程序 exe 同目录 | 快捷项主体，ANSI 文本，CRLF | 启动 Load；关闭/编辑/配置保存时 Save | SCM:22、613、1360-1535、1794-1828 |
| `ShortCutList.txt.bak` | 同上 | 上次正常退出时的备份 | 管理器析构时复制；启动发现主文件 0 字节时恢复 | SCM:656-662、1476-1482 |
| `FavoriteList.txt` | 同上 | 收藏映射 `关键字小写|显示名` | 启动加载；退出时保存 | SCM:23、628-630、1272-1326、1758-1787 |
| `ALTRun.ini` | 同上 | 全部选项 + `[DEBUG] LatestList` + `ShortCutRunCount` | 启动/关闭、配置保存 | OPT:743、817、886-889 |
| `ParamHistory.txt` | 同上 | 参数输入历史（与快捷项模型无直接关系） | 参数窗体创建/销毁 | FPARAM:190-196 |
| `*.lang` | 同上 | 界面文案（含默认条目的 Name） | 语言切换时 | OPT:901-1159 |

**注意**：重写项目当前 `src/core/ShortcutStore.h:18` 的注释（“%APPDATA%\ALTRun\shortcuts.db”）与原始实现不同；原始文件**在 exe 同目录**，这是升级兼容的关键（见 §11.3）。

---

## 2. 快捷项数据结构

### 2.1 条目种类与参数模式

**TShortCutType（SCM:30）**

| 值 | 含义 | 持久化 | 管理界面可见 | 主界面可执行 |
|---|---|---|---|---|
| `scOther` | 解析失败/普通文本行（临时态） | 否（若被写入则为该行 `Name`，SCM:1883-1884） | 否 | 否 |
| `scItem` | 真实快捷项 | 是（`F...` 行） | 是（图标 0） | 是 |
| `scBlank` | 空白分隔行 | 是（空行） | 是（图标 1） | 否（不会进入匹配结果，SCM:990） |
| `scRemark` | 注释行（当前版本**不会产生**，相关代码已注释，SCM:1976-1982） | 否（写出为 Name，但加载时丢弃） | 否 | 否 |

**TParamType（SCM:33、1562-1574、1931-1945）**

| 值 | 持久化字符串 | 界面文案（LANG） | 语义 |
|---|---|---|---|
| `ptNone` | 空串 `''`（字段填 20 空格） | “无参数 / No Param” | 不提示参数；命令行按原样执行（若含引号参数会被拆成参数，见 §8.4） |
| `ptNoEncoding` | `No_Encoding` | “参数无编码 / Param without encoding” | 提示用户输入参数，原样替换占位符 |
| `ptURLQuery` | `URL_Query` | “参数进行 URL 编码（如 Baidu 搜索） / Param with URL encoding” | 参数经 ANSI 百分号编码后拼接/替换 |
| `ptUTF8Query` | `UTF8_Query` | “参数进行 UTF-8 编码（如 Google/Yahoo 搜索） / Param with UTF-8 encoding” | 参数经 UTF-8 字节百分号编码后拼接/替换 |

模式字符串解析大小写不敏感（`LowerCase`，SCM:1937-1942）；空串视为 `ptNone`。

### 2.2 `TShortCutItem` 完整字段清单（SCM:47-56）

| 字段 | Delphi 类型 | 含义 | 持久化 | 默认值 | 说明与证据 |
|---|---|---|---|---|---|
| `ShortCutType` | `TShortCutType` | 条目种类 | **否**（由行格式推导） | `scOther`（对象零初始化） | 加载时先置 `scOther`，解析成功改 `scItem`/`scBlank`（SCM:1959-1974、1995-1997） |
| `ParamType` | `TParamType` | 参数模式 | **是**（第 2 字段字符串） | `ptNone` | SCM:1879-1880、1931-1945 |
| `ShortCut` | `string` | **关键字**，用户输入用来匹配的串 | **是**（第 3 字段） | `''` | 全系统唯一参与匹配的字段（SCM:999、1013）；无独立“别名”字段 |
| `Name` | `string` | 显示名，仅用于 UI 展示/收藏映射/最近列表文字 | **是**（第 4 字段） | `''` | 不参与匹配；管理界面“Name”列；编辑框禁止含 `,`（FSC:157-165） |
| `CommandLine` | `string` | 完整命令行：可执行文件/文档/目录/URL + 参数 + 占位符 | **是**（第 5 字段，到行尾） | `''` | 目标/参数/工作目录都从这一串**派生**，不分开存 |
| `Rank` | `Integer` | 排序得分（瞬态） | **否** | 0 | 被 `Sort()` 与 `FilterKeyWord()` 覆写（SCM:1909-1921、1009-1026） |
| `Freq` | `Integer` | 使用次数（= run count） | **是**（第 1 字段 `F<数字>`） | 0 | 仅搜索执行路径 +1（SCM:2075），详见 §6.2 |

补充：`AppendShortCutItem` 只设置前 5 个字段，`Rank/Freq` 依赖对象零初始化（SCM:363-375）。

### 2.3 `TCmdObject`（执行期对象，SCM:38-44）

| 字段 | 类型 | 含义 |
|---|---|---|
| `Command` | `string` | 可执行文件/文档/URL（已剥 `@*` 前缀、已展开环境变量） |
| `WorkingDir` | `string` | 工作目录；空则由 ShellExecute 决定 |
| `Param` | `string` | 已编码/未编码的参数值 |
| `ParamType` | `TParamType` | 参数模式（同一枚举复用） |

由 `Execute(ShortCutItem)` 创建，交给后台线程 `ExecuteCmd` 释放（注释明确“不在这里释放”，SCM:684-685、2089）。

### 2.4 需求中的字段 ↔ 原始实现对照（**重要**）

| 需求字段 | 原始实现 | 说明 |
|---|---|---|
| 关键字/别名 | `ShortCut`（唯一）；**无别名** | 匹配仅针对 `ShortCut`；`Name` 不是别名 |
| 目标路径 | 无独立字段；从 `CommandLine` 首段派生 | 执行时用 `FileExists`/引号解析拆出（SCM:699-735） |
| 参数 | 无独立字段；`CommandLine` 剩余部分或执行时输入 | 支持占位符替换（§8.5） |
| 工作目录 | 无字段；执行时派生 | `FileExists(命令行)` 时取 `ExtractFileDir`（SCM:705-706）；相对路径时取 exe 目录（SCM:187-189） |
| 图标 | **无字段** | 管理列表用固定 ImageList 索引 0/1（SCM:906、916）；主界面是纯文本 `TListBox`（FMAIN:37） |
| 备注 | **无字段** | 无备注功能；`scRemark` 是废弃概念 |
| 使用计数 | `Freq`（每项）+ `ShortCutRunCount`（全局） | 两者不同，见 §6 |
| 最后使用时间 | **不存储** | 仅有“最近使用顺序” `LatestList`（内存 + INI 索引串），无时间戳 |
| 是否收藏 | **不是条目字段** | 独立 `FavoriteList.txt`，映射“搜索关键字小写→Name”，见 §6.1 |
| 匹配串构造 | 无拼接串；直接对 `ShortCut` 匹配 | 无“关键字+分隔符+别名”这种串；`|` 只是存储/显示分隔符 |

---

## 3. 存储与序列化格式

### 3.1 `ShortCutList.txt` 行格式（当前版本）

写出格式（SCM:1874-1886）：

```
Format('F%-8d|%-20s|%-30s|%-30s|%s', [Freq, ParamTypeString, ShortCut, Name, CommandLine])
```

| 序号 | 内容 | 宽度 | 补齐 | 截断 | 说明 |
|---|---|---|---|---|---|
| 0 | `F` + `Freq` 十进制 | 8 | 右补空格 | **不截断**（超过 8 位直接变长） | 解析时 >8 位数字按 `10000` 处理（SCM:2016-2018） |
| 1 | 参数模式字符串 | 20 | 右补空格 | 不截断 | `''` / `No_Encoding` / `URL_Query` / `UTF8_Query` |
| 2 | `ShortCut`（关键字） | 30 | 右补空格 | 不截断 | |
| 3 | `Name`（显示名） | 30 | 右补空格 | 不截断 | |
| 4 | `CommandLine` | 无 | 无 | 无 | 到行尾，可含 `|`、逗号、空格、中文 |

- 字段分隔符：半角竖线 `|`；**无任何转义/引用机制**（SCM:1879）。
- 空白条目 `scBlank` 写成**空行** `''`（SCM:1881-1882）。空行既是“列表分隔”，也会在管理界面显示为一行。
- 行结束符：`WriteLn` → `CRLF`。文件末尾无额外标记、无文件头（文件头代码已被注释，SCM:1393-1396）。
- 示例（`Freq=8, ptNone, ShortCut=ie, Name=IE浏览器, CommandLine=iexplore.exe`）：

```
F8       |                    |ie                  |IE浏览器              |iexplore.exe
```

> 宽度按 `Format` 的“字符数”语义；若编译为 Delphi 2007（`AnsiString`），`%-30s` 的 30 是**字节数**，中文占 2 字节，实际补空格比字符数少（**不确定**，取决于编译器版本，见 §10）。

### 3.2 解析算法（SCM:1947-2059，逐行）

1. 读入一行后先 `Trim`（SCM:1508-1509）。
2. 全角逗号 `，` → 半角 `,`（SCM:1957，仅为兼容旧的逗号格式）。
3. 初始化默认值：`scOther`、`ShortCut=''`、`Name=整行`、`CommandLine=''`、`Freq=0`（SCM:1959-1966）。
4. 若整行为空 → `scBlank`，返回（SCM:1968-1974）。
5. 先用 `,` 切分到 `ShortCutSubItemList`（SCM:1989）。
6. 若（切分结果 < 3 段）**或**（整行首字符是 `F`）→ 改用 `|` 重新切分（SCM:1992-1993）。
7. 若切分结果 ≥ 3 段 → 认定为 `scItem`（SCM:1995-1997），继续：
   1. `offset := 0`；若第 0 段去空格后首字符为 `F`：去掉 `F`，若余下是纯数字则 `Freq := StrToInt(...)`（长度 > 8 位则 `Freq=10000`），否则 `Freq=0`；`Inc(offset)`（SCM:2006-2026）。
   2. 若 `StringToParamType(Trim(段[offset]))` 成功（**空串也会成功**）→ `Inc(offset)`（SCM:2028-2030）。
   3. `ShortCut := Trim(段[offset])`；`Name := Trim(段[offset+1])`（SCM:2032-2033）。
   4. `CommandLine` 通过“从原行中删除已识别字段的原始文本，再定位剩余文本”的方式还原（SCM:2035-2044）：
      `strTemp := 原行`；对 `i=0..offset+1` 执行 `StringReplace(strTemp, 段[i], '', [rfIgnoreCase])`（**替换全部出现**）；再 `Pos(段[offset+2], strTemp)` 定位，`Copy` 到行尾。
8. `LoadShortCutList` 只接收 `scItem/scBlank`，其余释放（SCM:1517-1523）。

### 3.3 旧格式兼容与解析缺陷（SCM:1988-1993）

历史版本用**逗号**分隔，字段为 `ShortCut,Name,CommandLine`（无 `F` 前缀、无参数类型）。解析器靠“是否首字符 `F`”区分新旧。已知缺陷（重写时必须修正或明确继承）：

| 编号 | 缺陷 | 影响 | 证据 |
|---|---|---|---|
| P1 | 旧格式行若关键字以 `F` 开头（如 `Firefox,...`），会因 `Pos('F',str)=1` 被强制改用 `|` 切分，切分不足 3 段 → 解析为 `scOther` 被丢弃 | **旧文件条目丢失** | SCM:1992 |
| P2 | 第 4 步“删除已识别字段”是 `rfIgnoreCase` 全局替换且用 `Trim` 后的字段去匹配原行（含补齐空格差异） | 若 `ShortCut`/`Name` 是 `CommandLine` 子串，或命令行含 20/30 连续空格，命令行会被截断/错位 | SCM:2039-2044 |
| P3 | 字段无转义：`Name` 含 `|` 会破坏字段切分；`Name` 不允许含 `,` 仅为旧格式打补丁（FSC:157-165），未校验 `|` | 数据损坏 | SCM:1879、FSC:157 |
| P4 | 全角逗号替换发生在切分前，但 `Name` 中本允许的全角逗号会被改成半角（若含逗号本就非法） | 轻微数据改写 | SCM:1957 |
| P5 | 非纯数字或负数频率 → 0；>8 位数字 → 10000 | 频率失真 | SCM:2014-2023 |
| P6 | 解析异常只在 `LoadShortCutList` 外层 `try...except Exit` 捕获，**已清空并部分加载**的列表保持半成品状态 | 半加载 | SCM:1498-1530 |
| P7 | 旧格式没有“空白行”概念，但空行会被当作 `scBlank` 保留 | 兼容可接受 | SCM:1968-1974 |

### 3.4 写入与提交时机

| 触发 | 是否立即落盘 | 证据 |
|---|---|---|
| 程序正常退出（主窗口 Close） | 是：`ShortCutMan.SaveShortCutList`（FMAIN:248），随后 `Application.Terminate` | FMAIN:230-251 |
| 关闭配置窗体 / 保存配置 | 是：Save + Load + `NeedRefresh` | FMAIN:430-433 |
| 主窗口 F2 编辑条目 | 是：改内存后立即 Save + Load | FMAIN:615-660 |
| 主窗口 Del 删除条目 | 是：`DeleteShortCutItem` 内部 Save + Load | SCM:636-654、FMAIN:559-582 |
| 管理窗体“OK”关闭 | 是：`LoadFromListView` → `SaveShortCutList` → `LoadShortCutList` | FMAIN:947-952 |
| 管理窗体“Cancel”关闭 | **否**，全部改动丢弃（仅窗口尺寸/列宽被保存） | FMAIN:947、FSCM:575-588 |
| 通过拖放/资源管理器“Add to ALTRun”添加 | 是：Add 内部 Save + Load | SCM:466-467、479-480 |
| 仅执行条目（Freq+1） | **否**，等待下一次任何 Save（通常退出时） | SCM:2071-2077 |
| 收藏列表 | 是，但只在管理器**析构**时写（退出时） | SCM:669 |

### 3.5 顺序与“排序”

- 文件中的物理顺序 = `m_ShortCutList` 顺序 = 管理界面 ListView 顺序（加载按行序，见 SCM:1506-1524；保存按列表序，见 SCM:1817-1821；`FillListView` 按列表序，SCM:893-922；`LoadFromListView` 按界面序重建，SCM:1337-1352）。
- 管理界面**没有**按列排序功能，列点击处理器为空实现（FSCM:622-625）。
- 内存中另有 `m_SortedShortCutList`（`TStringList`，元素为显示串 + 对象指针），由 `Sort()` 在**每次 Load 后**构建（SCM:1532、1894-1929）；它只决定“搜索框为空时”的显示顺序，**不改变** `m_ShortCutList`/文件顺序。
- `Sort()` 的排序键（降序，`Rank` 大在前）：

```
Rank := Freq * 1000
k := 8
for j := 1..3: k := k div 2 (即 4,2,1)
    Rank := Rank - k * (Ord(UpCase(ShortCut[j])) - Ord('A'))
Rank := Rank - Length(ShortCut)
```
（SCM:1909-1921；只取关键字前 3 个字符；非字母/中文会得到很大的负贡献，可能导致排名异常靠后）

- 搜索结果的排序另有一套打分（§5.5），与 `Sort()` 不同。

### 3.6 编码

- 读取/写入使用 Delphi `TextFile` + `AssignFile/Reset/ReWrite/Readln/WriteLn`（SCM:1362、1797 等），即**系统 ANSI 代码页**文本；简体中文系统为 GBK/936。
- 无 BOM、CRLF、无文件头。UTF-8 中文会被按 ANSI 解码成乱码，反之亦然。
- 语言文件 `.lang`、`ALTRun.ini` 同为 ANSI/INI 格式（OPT:743、901）。
- 结论：**旧数据 = GBK（或安装环境 ANSI 代码页）**，不是 UTF-8/JSON。

### 3.7 并发与损坏处理

| 机制 | 行为 | 证据 |
|---|---|---|
| 退出备份 | 析构时若主文件大小 > 0，复制为 `ShortCutList.txt.bak`（覆盖） | SCM:656-662 |
| 0 字节恢复 | 加载前若主文件大小为 0，从 `.bak` 复制回来 | SCM:1476-1482 |
| 外部修改检测 | 记录 `GetFileModifyTime`（`yyyy-mm-dd hh:mm:ss`）；未变则直接 `Exit`（**不重载**，返回 False） | SCM:1484-1494；UT:192-199 |
| UI 刷新标志 | 时间戳变化时置 `NeedRefresh=True`；主窗口显示时按需刷新 | SCM:1493；FMAIN:987、996-1001 |
| 单实例 | 命名互斥体 `ALTRUN_MUTEX`；第二次启动把 `%1` 通过 `WM_SETTEXT` 发给运行中实例，或直接写文件 | UT:94-101；FMAIN:2006-2013、2029-2034、2767-2775 |
| 写文件 | `ReWrite` 直接截断重写，**非原子**、无锁、无临时文件 | SCM:1812-1827 |
| 异常 | 读写异常被 `try/except Exit` 吞掉，返回值不置 True（`SaveShortCutList` 永远返回 False，SCM:1803-1828） | SCM:1360-1535、1794-1828 |
| 运行中外部修改 | 程序在退出时会用**内存中的旧列表**覆盖文件 → 外部修改丢失 | FMAIN:248 |

### 3.8 `FavoriteList.txt` 格式（SCM:1272-1326、1758-1787）

- 每行：`关键字|显示名`。写出代码：`Format('%-30s|%-30s%', [LowerCase(名字), 值])`（SCM:1778-1779）。
  - 两个字段各左对齐补空格到 30；`Name` 超长不截断。
  - 末尾那个孤立的 `%` 很可疑（可能是笔误）。**不确定**它是否会让 Delphi `Format` 抛 `EConvertError`；若抛出，异常被吞掉且文件已被截断 → **收藏数据会丢失**（见 §10）。
- 加载：逐行 `Trim` → `SplitString(line,'|')` → `Values[Trim(第0段)] := Trim(第1段)`（SCM:1308-1315）。字段不足 2 段会触发异常并 `Exit`，中止后续行加载。
- 内存表示：`TStringList`，用 `Values` 属性形成 `小写关键字=显示名`（Delphi VCL 行为）；默认 `CaseSensitive=False`，`IndexOfName` 对 ASCII 大小写不敏感。
- 文件不存在时创建一个空文件（SCM:1284-1296）。

### 3.9 “最近使用”列表（不是独立文件）

- 内存：`m_LatestList: TStringList`，容量上限 `MAX_LATEST_NUM = 10`，最新在前（SCM:27、489-506）。
- 持久化：写入 `ALTRun.ini` 的 `[DEBUG] LatestList`，格式为**以 `, ` 连接的下标串**（如 `41, 12, 3`），由 `GetLatestShortCutIndexList` 生成（SCM:1091-1111；OPT:111、817、889）。
- 启动时 `SetLatestShortCutIndexList(LatestList)` 把下标映射回条目对象（FMAIN:1099；SCM:1839-1867）。
- 由 `Execute(ShortCutItem, KeyWord)` 更新（SCM:2074）；`Execute(ShortCutItem)` 单参版本**不更新**（因此“最近项热键”执行不会刷新最近列表，FMAIN:2405）。
- 严重问题：**存的是数组下标**，一旦管理界面拖拽/插入/删除条目导致顺序变化，重启后最近列表会指向错误条目；删除条目时只清理内存中的最近列表（SCM:644-646），INI 中的旧下标未清理。
- 其它缺陷：`LoadLatestList`/`SaveLatestList` 是空函数（SCM:1355-1358、1789-1792）；`GetLatestShortCutItemList` 没有 `Result := True`（SCM:1113-1163）；`SetLatestShortCutIndexList` 越界判断写成 `<= Count`（应为 `< Count`），可能触发列表越界异常（SCM:1861）。

---

## 4. CRUD 操作语义

### 4.1 新增（Add）

#### A. 管理界面“添加”按钮 / Insert 键（FSCM:103-170、667-680）

1. 打开 `TShortCutForm`（字段：ShortCut、Name、CommandLine、ParamType 单选 0-3），控件见 FSC.dfm:86-165。
2. 用户确认（`mrOk`）后，若 `Trim(ShortCut)=''` **或** `Trim(CommandLine)=''` → 生成**空白行**（所有列清空、图标 1），即使 Name 有值也会丢弃（FSCM:125-140）。
3. 去重检查 `ExistListItem`：四列（Caption、SubItems 0/1/2，即 ShortCut/Name/参数类型串/CommandLine）**全部相等**才视为重复（区分大小写、精确比较），重复弹 `This ShortCut has already existed!` 并放弃（FSCM:142-150、511-532）。**允许不同参数模式或不同命令行但同关键字的条目共存**。
4. 插入位置：若无选中行 → 追加到末尾；否则插入到当前选中行之前（FSCM:152-156）。
5. 刷新首列显示（单字符 Caption 的 VCL 刷新 hack，FSCM:163-165）。
6. **不立即落盘**：等管理窗体 OK（FMAIN:947-952）。

#### B. 主窗口“无匹配时添加” / 拖放 / 资源管理器右键（SCM:377-487）

`AddFileShortCut(FileName)`：
1. `ExtractShortCutItemFromFileName` 由文件名推导 ShortCut/Name/CommandLine（规则见 §8.1）；失败提示（SCM:394-401）。
2. 打开 `TShortCutForm`，预填 ShortCut/Name/Trim(CommandLine)，参数模式=0（SCM:403-406）。
3. Cancel → 不添加（SCM:410-414）。
4. ShortCut 与 CommandLine 都非空 → `scItem` + 用户选择的参数模式；否则提示 `resBlankLineAdded` 并生成**空白行**（SCM:416-434）。
5. 相对路径建议：若 CommandLine 含 exe 目录 → 询问是否把绝对路径替换成 `.`（SCM:437-444）。
6. 冲突处理：若已有条目 `ShortCut` 相同（`GetShortCutItemByShortCut`，**只比 ShortCut，精确/区分大小写，返回第一个**，SCM:1170-1187），弹 `已有 [关键字 (Name) = CommandLine]，是否替换？`；选“是”→ 用 `CloneShortCutItem` **覆盖现有条目**（只改 ShortCutType/ParamType/ShortCut/Name/CommandLine，`Freq` 保留原值，SCM:544-553、464）；选“否”→ 放弃（SCM:458-475）。
7. 追加并立即 `SaveShortCutList` + `LoadShortCutList`（SCM:478-481）。

入口：主窗口“添加”动作 / Insert 键（FMAIN:220-228、2240-2244）、第二次实例转发（`WM_SETTEXT`，FMAIN:2006-2013、2767-2775）、资源管理器右键“Add To ALTRun”（UT:300-333 注册表，参数 `"%1"` 由命令行传入，FMAIN:1949-2025）、SendTo（UT:335-345）。
补充：**主窗口本身不接受文件拖放**（无 `WM_DropFiles`/`DragAcceptFiles`）；拖放只在“编辑对话框”（FSC:255、264-301）和“管理窗体”（FSCM:571、695-782）中生效，且只处理拖入的第一个文件（UT:1047-1058）。

### 4.2 删除（Delete）

| 场景 | 二次确认 | 落盘时机 | 证据 |
|---|---|---|---|
| 管理界面删除（工具栏/右键/Delete 键） | 是：普通行 `Delete %s(%s)?`；空白行 `Delete Blank Line?`；均为 `MB_OKCANCEL` | 管理窗体 OK 后 | FSCM:182-199；文案 OPT:1079-1080 |
| 主窗口列表删除（右键/Delete 菜单/Delete 键） | 是：`%s %s(%s)?`（“Delete 关键字(Name)?”） | 立即 | FMAIN:559-582、2246-2253 |
| 批量校验后删除无效项 | 是（进入校验前确认 + 勾选清单） | 管理窗体 OK 后 | FSCM:327-394 |
| 删除对最近列表的影响 | 移除对应对象 | — | SCM:644-646 |
| 删除对收藏列表的影响 | **不清理**（`FavoriteList.txt` 可能残留已删除条目的映射） | — | 无相关代码 |

`DeleteShortCutItem(Index)` 语义：越界返回 False；从 `m_LatestList` 删除该对象；`m_ShortCutList.Delete`；立即 Save + Load（SCM:636-654）。

### 4.3 修改（Edit）

| 场景 | 入口 | 提交方式 | 证据 |
|---|---|---|---|
| 管理界面 | F2 / 双击 / 右键 Edit | 修改 ListView 行（原地），**管理窗体 OK 后**统一 `LoadFromListView`+Save+Load | FSCM:201-269；FMAIN:947-952 |
| 管理界面内联编辑 | 单击 Name/CommandLine 等单元格？——VCL `TListView` 仅支持编辑 Caption（即 ShortCut 列）；`OnEditing/OnEdited` 只切换默认按钮，**不做校验/去重**；OK 后同样被提交 | FSCM.dfm:108-109、FSCM:655-665 |
| 主窗口 | F2（`actEditItemExecute`；按键映射见 FMAIN:2234-2238） | 直接改内存对象，立即 Save+Load | FMAIN:615-660 |
| 条目“替换” | 添加冲突时 | 覆盖已有条目、保留 `Freq` | SCM:458-475、544-553 |

管理界面编辑限制：只有 `ImageIndex = Ord(siItem)`（真实条目）可编辑；空白行不可编辑（FSCM:209）。

### 4.4 复制（Duplicate/Copy）

- **不存在**用户可见的复制/克隆条目功能。
- `CloneShortCutItem` 只用于“添加冲突时覆盖现有条目”（SCM:464），且**不复制 Freq/Rank**。
- `CloneShortCutList` 是浅拷贝（`NewList.Add(SrcList.Items[i])`，共享同一批对象指针，SCM:555-564），未被任何界面代码调用（死代码）。
- 管理界面右键菜单名为 `mniCut/mniInsert/mniDelete`，但实际绑定的是 Add/Edit/Delete 三个 Action（FSCM.dfm:663-675），没有“剪切/复制”。

### 4.5 排序 / 拖拽调整顺序

- 无“按关键字/名称排序”命令。
- 管理界面的拖拽（`DragMode=dmAutomatic`，FSCM.dfm:94）：在目标行位置插入被拖行的副本，再删除源行（FSCM:627-647）；`OnDragOver` 恒 Accept（FSCM:649-653）。
  - 拖拽只改 ListView 顺序；OK 后 `LoadFromListView` 按界面顺序重建列表并写盘（FMAIN:950-951）。
  - 拖到空白区域直接忽略（FSCM:634）。
  - 拖拽不校验重复，允许把条目拖到任意位置。
- 主窗口的最近/搜索结果顺序由算法决定（§5、§6），不支持手工排序。

### 4.6 批量功能

| 功能 | 是否存在 | 语义 | 证据 |
|---|---|---|---|
| 批量导入 | **无**（无“从文件导入”菜单/对话框）。外部“导入”仅限单条：拖放、右键“Add To ALTRun”、SendTo、命令行 `%1` | — | FSCM.dfm:663-675；全仓库无 Import 相关代码 |
| 批量导出 | **无** | — | 同上 |
| 批量校验（Validate） | 有 | 遍历所有 CommandLine，用 `IsValidCommandLine` 找“可疑”项（文件/目录不存在等），弹无效项列表（可全选/全不选/逐条 Test），确认后从 ListView 删除勾选项；取消则不改 | FSCM:327-394、396-509；FINV:62-111 |
| 路径转换（Path Convert） | 有 | 全部条目 CommandLine：绝对路径（含 exe 目录）⇄ `.` 相对路径，双向转换并统计数量 | FSCM:277-325 |
| 参数历史 | 独立文件 | `ParamHistory.txt`，参数对话框下拉历史（与条目无关） | FPARAM:26、252-377 |
| 单条测试执行 | 有 | 编辑对话框“Test”按钮：真实执行当前命令行（`Execute(ShortCutItem)` 单参版本），**不计 Freq、不写收藏/最近列表**；“校验”窗体的“Test”同理 | FSC:189-218；FINV:62-93；SCM:673-797 |

> 关于“导入格式要求”：原始版本没有批量导入，因此**不存在批量导入格式**。单条“导入”的输入形态只有：文件路径（可含空格，需引号）、`.lnk`、URL、拖放文件（只取第一个，UT:1047-1058）、以及资源管理器/SendTo 传参；由 `ExtractShortCutItemFromFileName` 推导字段（§8.1）。

`IsValidCommandLine` 判定规则（FSCM:396-509）：
1. 去前导 `@+`/`@-`/`@`（SHOW 标记）。
2. 含 `\\`（网络路径）→ 视为有效。
3. 不含 `\` → 视为有效（URL、`notepad`、`calc`、`appwiz.cpl`、`::{GUID}` 等不检查）。
4. 展开环境变量；去外层引号；`.\` 开头换成 exe 目录。
5. 文件存在 / 目录存在 → 有效。
6. 否则尝试以“最后一个 `\`”截出目录判断；再尝试“第一个引号段”作为可执行文件判断；否则无效。
> 因此“校验”只是启发式，不验证能否真正启动，也不验证 `%p` 等占位符。

### 4.7 管理窗体的提交模型

- `ModalResult = mrOk`（Close 按钮）→ 主窗口执行 `LoadFromListView` → `SaveShortCutList` → `LoadShortCutList`（FMAIN:947-952）。
- `ModalResult = mrCancel` → 不做任何持久化（FMAIN:947 条件不成立），下次打开重新 `FillListView`（FSCM:572、617-620）。
- 窗体创建时禁用系统关闭按钮（FSCM:536-537），避免“关闭”语义歧义。
- 窗体销毁时保存窗口位置/尺寸/列宽（`ManWinTop/Left/Width/Height`、`ManColWidth[0..3]`，FSCM:575-588；OPT:775-781、862-868）。
- 管理界面按键：F2=编辑、Insert=添加、Delete=删除（FSCM:667-680）。

---

## 5. 匹配相关

### 5.1 参与匹配的字符串

**只有 `TShortCutItem.ShortCut` 一个字段参与匹配。** `Name`、`CommandLine` 不参与（SCM:999、1013）。不存在“名称/别名/路径/备注”多串匹配，也不存在“关键字+别名”拼接串。

### 5.2 大小写、中文、全角半角

| 项 | 行为 | 证据 |
|---|---|---|
| 大小写 | `LowerCase(UserInput)` 与 `LowerCase(Item.ShortCut)` 后比较；Delphi `LowerCase` **只折叠 ASCII A-Z** | SCM:953、999、1013 |
| 中文 | 不做 Unicode 大小写/拼音/注音处理；中文按码元比较 | SCM:953、1013 |
| 全角/半角 | **不统一**：匹配前不做 NFKC/全半角转换；唯一涉及全角的地方是旧格式解析时 `，`→`,`（与搜索无关） | SCM:1957 |
| 去空格 | 搜索框文本**不 Trim**（直接 `LowerCase(KeyWord)`；仅特殊值 `' '` 被完全忽略），条目关键字在解析时已 `Trim` | SCM:945-946、953、2032 |
| 空输入 | 显示 `m_SortedShortCutList` 全量（按 `Sort()` 的 Rank 降序） | SCM:949-950 |
| 输入恰好为单个空格 `' '` | 直接返回空结果（多个空格不特判） | SCM:945-946 |
| 长度守卫 | `if Length(KeyWord) > Length(Item.ShortCut) then Continue`（在正则展开**之后**执行，见 §5.4 缺陷） | SCM:993 |

### 5.3 非正则模式（`EnableRegex=False`）

```
Item.Rank := Pos(LowerCase(KeyWord), LowerCase(Item.ShortCut));   // 1 基
if (not MatchAnywhere) and (Item.Rank > 1) then Continue;         // 仅允许前缀命中
if Item.Rank > 0 then 收录
```
（SCM:1011-1017；`MatchAnywhere` 默认 True，OPT:753 / INI:7）

### 5.4 正则模式（`EnableRegex=True`，默认开启，OPT:751 / INI:5）

1. 对用户输入做“通配符→正则”预处理（SCM:957-981）：
   - 先保护已存在的 `.*` → 临时串 `~!@#`；
   - 全部 `*` → `.*`；
   - 临时串还原为 `.*`；
   - 全部 `?` → `.`（注意：注释掉的版本才是 `?`→`.?`，即当前 `?` 表示**恰好一个字符**）。
2. 逐条目执行 `TRegExpr.Exec(LowerCase(Item.ShortCut))`（SCM:997-999）。
3. `TRegExpr` 默认 `ModifierI=False`（REGEX:130、1139），即**正则本身区分大小写**；由于两侧都 `LowerCase`，ASCII 实际不区分大小写，但用户写的字符类如 `[A-Z]` 会被折叠成小写。
4. 若非 `MatchAnywhere`：要求 `MatchPos[0] == 1`（从头匹配），否则跳过（SCM:1006-1007）。
5. 命中后 `Item.Rank := MatchPos[0]`（1 基起点，用于后续打分，SCM:1009）。
6. `Exec` 抛异常时：仅 `Result := False` 然后继续，**没有 `Continue`**，后续仍读取 `MatchPos[0]`（可能为上次残留值）→ 非法正则的行为**不确定**（SCM:1000-1009）。
7. 缺陷：`Length(KeyWord) > Length(Item.ShortCut)` 的长度守卫在正则展开后执行，`*`→`.*` 会让长度虚增，可能错误跳过本应命中的条目（SCM:993、958-981）。

### 5.5 命中结果的打分与排序（SCM:1021-1040）

`Rank` 公式（整数运算）：

```
Item.Rank := 1024 + Item.Freq * 4
             - MatchPos * 128
             - (Length(Item.ShortCut) - Length(KeyWord)) * 16
```

- 仅当 `Rank > 0` 才收录（SCM:1021）。
- 意义：命中越靠前越好（每提前 1 字符 +128）；关键字越短越好（每多 1 字符 -16）；使用次数越多越好（每次 +4）；所有命中至少 +896 基础值。
- 然后对该结果列表执行**随机化快速排序**（降序）：`QuickSort(StringList, 0, Count-1)`（SCM:1040、1723-1744）。
  - 实现细节：`Q_MIN = 12`（≤12 元素退化为冒泡）；`SelectPivot` 每次 `Randomize` 后随机取支点（SCM:1702、1725、1830-1837）。
  - 后果：**排序不稳定**，同分条目顺序不确定；重写若要求确定性结果，必须改为稳定排序 + 明确的 tie-break。
- 结果字符串：`Format(ListFormat, [Item.ShortCut, Item.Name])`（SCM:1030）。`ListFormat` 默认 `'%-25s| %s'`（OPT:99、INI:44），可选 `'%s (%s)'`、`'%s [%s]'`（OPT:659-663），用户可在配置界面自定义（FCFG:60-62、223-227）。

### 5.6 收藏优先、数字前缀、TopTen

1. **收藏优先**（`RememberFavouratMatch=True` 时，默认 False，OPT:756 / INI:10）：
   - 若 `FavoriteList` 中存在键 = 当前输入（小写），且其值等于结果中某条目的 `Name`，把该结果移到第 1 位（`StringList.Move(i,0)`，不改变其余顺序）（SCM:1045-1054）。
2. **数字前缀**（`EnableNumberKey=True`，默认 True，OPT:754 / INI:8）：
   - 前 10 条：无参数条目前缀 `' %d|'`，有参数条目前缀 `'*%d|'`（`*` 表示该条执行时会弹参数框）；其余条目前缀 `'  |'`（SCM:1058-1081；最近列表同样处理，SCM:1140-1162）。
   - 编号基准：`IndexFrom0to9=False`（默认）时显示 1..9,0；True 时显示 0..9（SCM:1064-1077；OPT:755 / INI:9）。
   - 前缀直接改写显示串（`StringList.Strings[i]`），与 `ListFormat` 结果叠加（默认格式本身含 `|`，因此会出现两个 `|`）。
3. **TopTen**（`ShowTopTen=True`，默认 True，OPT:761 / INI:15）：
   - 主窗口只把过滤结果的前 10 条放入列表框（FMAIN:1394-1404）。
   - **不是**持久化榜单、无独立 TopTen 列表/评分；搜索框为空时它等价于“按 Freq 等权重的 `Sort()` 前 10”。

### 5.7 结果列表的自动执行行为（上下文，便于对齐交互）

| 行为 | 条件 | 证据 |
|---|---|---|
| 只有 1 条结果时立即执行 | `ExecuteIfOnlyOne=True`（默认 False） | FMAIN:1480-1483；OPT:770 / INI:26 |
| 输入末尾是空格 → 执行当前选中项 | 输入非空且末字符为空格，且上一轮有选中项 | FMAIN:1452-1470 |
| 输入末尾是数字 0-9 → 执行第 N 条（Ctrl/Alt + 数字亦同） | `EnableNumberKey=True`；`IndexFrom0to9` 决定 0 的语义 | FMAIN:1418-1450、1503-1608 |
| 输入末尾是 `;` / `'` → 执行第 2/3 条 | 同上 | FMAIN:1588-1608 |
| “最近项”热键（默认 ALT+L）→ 执行 `LatestList[0]` | 不改变 Freq、不刷新最近列表 | FMAIN:2380-2409 |
| 无结果时按执行 → 询问是否添加 | 弹 `resNoItemAndAdd` | FMAIN:747-753 |

### 5.8 主窗口展示与目录标记

- 选中项显示 `Name`（标签）+ `CommandLine`（Hint/命令显示行）（FMAIN:843-855、1474-1477）。
- `DirAvailable` 为真时名称加 `[...]`，并允许 Ctrl+D 打开所在目录（FMAIN:1256-1319、1488、1610-1620、2577）。
  - 判定：去 `@*`/引号；含 `\\` → False；`FileExists(CommandLine)` → True；`.\` 开头 → True；否则按最后一个 `\` 截目录判断 `DirectoryExists`（FMAIN:1282-1313）。

---

## 6. 收藏（Favourite）、TopTen、使用计数

### 6.1 收藏的实际语义（`FavoriteList.txt`）

- 数据结构：`m_FavoriteList: TStringList`，键 = **搜索关键字小写**，值 = 该次执行条目的 `Name`（SCM:70、629）。
- 写入时机：**每次通过搜索结果执行**（`Execute(ShortCutItem, KeyWord)`）都写 `m_FavoriteList.Values[LowerCase(KeyWord)] := ShortCutItem.Name`（SCM:2071-2077）。与 `RememberFavouratMatch` 开关无关——开关只控制“是否在排序时优先”。
- 落盘：只在管理器析构时 `SaveFavoriteList`（SCM:669）；加载在构造时 `LoadFavoriteList`（SCM:630）。
- 用途：开启 `RememberFavouratMatch` 后，输入同一关键字时把“上次为该关键字执行的条目”置顶（SCM:1045-1054）。
- 产生的典型效果：输入 `b` + 回车执行“百度搜索” → 收藏表记录 `b=百度搜索引擎`；下次输入 `b` 时该条目排第一。
- 条目删除后收藏不清理；关键字改名后旧映射残留；收藏不是条目的布尔属性，重写时不要建模成 `bool favourite`（与现有 C++ 骨架 `Shortcut.h:25` 不同，见 §11.2）。

### 6.2 使用计数

| 计数器 | 作用域 | 何时 +1 | 持久化位置 | 用于排序 | 证据 |
|---|---|---|---|---|---|
| `TShortCutItem.Freq` | 单个条目 | `Execute(ShortCutItem, KeyWord)`，即“从搜索结果/数字键/空格执行” | `ShortCutList.txt` 的 `F` 字段，随下一次 Save 落盘 | 是：`Sort()` 权重 1000/次；过滤打分 `Freq*4` | SCM:2075、1909、1026 |
| `ShortCutRunCount` | 全局 | `Execute(cmdobj)` 每次真正启动进程（无论最终成功与否） | `ALTRun.ini` `[DEBUG] ShortCutRunCount` | 否 | SCM:2092-2097；OPT:425、813、886 |

- `ShortCutRunCount` 每满 10000 弹一次“恭喜”对话框（SCM:2094-2097）。
- `Freq` 只在 2 参 `Execute` 中 +1；**单参 `Execute(ShortCutItem)`（如“最近项”热键）与 `Execute(cmdobj)` 不计入条目 Freq**（SCM:2071-2100、FMAIN:2405）。
- `Freq` 的落盘依赖“下一次任何 Save”，异常退出会丢失（见 §3.4）。

### 6.3 最近使用（LatestList）与排序

- 容量 10，最新在前；重复执行同一对象时仅 `Move` 到首位，不新增（SCM:494-505）。
- 数据结构混用：从 INI 恢复时 `Strings[i]` 保存的是下标数字、`Objects[i]` 是条目指针；新执行插入时 `Strings[i]` 保存的是 `Name`。展示时只用 `Objects[i]`，因此无影响（SCM:1091-1163、1839-1867）。
- 排序：无打分，纯“最近优先”（SCM:489-506）。
- 与 TopTen 无关。

---

## 7. `untUtilities.pas` 公共函数清单（接口 UT:17-84，实现见各行）

### 7.1 字符串与解析

| 函数 / 签名 | 用途 | 边界行为与注意 | 行号 |
|---|---|---|---|
| `function CutLeftString(str: string; Num: Integer): string` | 去掉左侧 `Num` 个字符 | `Copy(str, Num+1, ...)`；`Num` 超长返回空串；**按字符/字节取决于编译器版本** | 17、217-220 |
| `function RemoveQuotationMark(str, QuotationMark: string): string` | 去掉首尾成对引号 | 长度不足 2×引号长 → 原样返回；去掉后若内部仍含引号 → 原样返回（只处理“一刀”引号） | 18、222-241 |
| `procedure SplitString(str, Delimiter: string; var strList: TStringList)` | 按分隔符切分，结果**不 Trim**、保留空段 | 先 `Clear`；`str=''` → 空列表；末尾分隔符会产生最后一个空段；空分隔符会死循环（未防护） | 19、261-278 |
| `function IsNumericStr(const str: string): Boolean` | 判断是否纯数字 | 空串 False；只接受 `0..9`，不接受符号/空格/小数点 | 38、894-908 |
| `function GetUTF8QueryString(Keyword: string): string` | UTF-8 百分号编码 | 先 `Utf8Encode`，再逐字节；`[A-Za-z*@._-]` 原样，空格 `+`，其余 `%XX`（大写十六进制） | 65、1117-1134 |
| `function GetURLQueryString(Keyword: string): string` | ANSI 百分号编码 | 同上但按系统 ANSI 字节编码（中文 → GBK 双字节 `%XX%XX`） | 66、1136-1152 |

### 7.2 路径与文件

| 函数 / 签名 | 用途 | 边界行为与注意 | 行号 |
|---|---|---|---|
| `GetFileModifyTime(const FileName): string` | 取修改时间字符串 `yyyy-mm-dd hh:mm:ss` | 文件不存在时 `FileAge` 返回 -1，结果取决于 RTL（约 1899-12-30），**未实测** | 25、192-199 |
| `SetFileModifyTime(FileName; FileTime: TDateTime): Boolean` | 设置修改时间 | 文件不存在返回 False；转发到 `FileSetDate` | 26、212-215 |
| `SetFileModifyTime(FileName; AgeOfFile: Integer): Boolean` | 同上（文件时间戳整数） | 同上 | 27、201-210 |
| `GetFileSize(const FileName): Integer` | 取文件字节数 | 打不开返回 -1；实现里 `fs` 未初始化就 `finally fs.Free`，异常路径可能二次异常（源码缺陷） | 28、243-259 |
| `GetFileListInDir(var List; const Dir; Ext='*'; IsPlusDir=True): Boolean` | 列目录 | 用 `FindFirst` + `while FindNext`，**跳过第一个匹配项**（缺陷）；`Ext` 用 `Pos` 子串匹配非后缀匹配；无匹配时仍返回 True | 39-40、910-936 |
| `WriteLineToFile(FileName, Line): Boolean` | 写一行 | 名称像追加，实际 `ReWrite` **覆盖**整个文件 | 30、280-298 |
| `ExtractRes(ResType, ResName, ResNewName): Boolean` | 释放资源到文件 | 失败返回 False | 41、978-993 |
| `ResolveLink(ALinkFile): string` | 解析 `.lnk` 目标 | 返回“目标路径 + 空格 + 参数”；带引号的 `.lnk` 路径会把引号去掉后再 Load（代码自注“这么做不对”）；失败返回空串 | 37、859-892 |
| `CreateLink(FileName, Arg, LinkPath, LinkName, Description): Boolean` | 创建 `.lnk` | 用 `IShellLink`+`IPersistFile`；目标目录取 `ExtractFilePath(FileName)`；成功返回 True | 36、802-857 |
| `GetSpecialFolderDir(SpecailFolderID: Integer): string` | 取 Shell 特殊目录（CSIDL） | 失败返回空串 | 34、774-784 |
| `GetSendToDir: string` | 取 SendTo 目录 | 从注册表读 `Shell Folders\SendTo`；失败空串 | 35、786-800 |
| `RefreshEnvironmentVars: Boolean` | 从注册表重新加载环境变量 | 跳过值中含 `%` 的变量（避免破坏 `%SystemRoot%` 等）；读不到返回 False | 46、1311-1345 |

### 7.3 环境变量

| 函数 / 签名 | 用途 | 边界行为与注意 | 行号 |
|---|---|---|---|
| `function ReplaceEnvStr(str: string): string` | 把 `%VAR%` 替换为环境变量值 | 用 `TRegExpr` 表达式 `%(.*?)%`；循环所有匹配，但**每次都用原串 `StringReplace` 覆盖 `Result`**，因此多变量时只有最后一个被替换（**缺陷**）；变量不存在时该处保持原样 | 42、995-1019 |

> 注意：`untShortCutMan` 里还有一份**同名不同实现**的 `ReplaceEnvStr`（SCM:1746-1756）：只处理**第一个** `%VAR%`（正则首次匹配 + `rfReplaceAll` 全量替换该单一变量）。执行路径会对同一命令**先后调用两份实现**（SCM:697、339），实际最多展开两个不同变量。

### 7.4 编码转换

| 函数 / 签名 | 用途 | 边界行为与注意 | 行号 |
|---|---|---|---|
| `UnicodeEncode(Str: string; CodePage: integer): WideString` | 多字节 → 宽字符 | 按指定代码页；空串得空 | 62、1060-1068 |
| `UnicodeDecode(Str: WideString; CodePage: integer): string` | 宽字符 → 多字节 | 按指定代码页；空串得空 | 61、1070-1078 |
| `Utf8Encode(const WS: WideString): UTF8String` | UTF-16 → UTF-8 | 空串返回空；用 `UnicodeToUtf8` | 58、1098-1115 |
| `Big52Gb(Str): string` | 繁体(Big5) → 简体(GB) | 先 Big5→Unicode→GB，再 `LCMapString(LCMAP_SIMPLIFIED_CHINESE)` | 59、1089-1096 |
| `Gb2Big5(Str): string` | 简体(GB) → 繁体(Big5) | 先 `LCMapString(LCMAP_TRADITIONAL_CHINESE)`，再 GB→Unicode→Big5 | 60、1080-1087 |

### 7.5 正则

- **没有正则封装函数**。全项目直接在业务代码里 `TRegExpr.Create`/`Exec`/`Replace`（如 SCM:205-207、995-1009；UT:1005-1017）。
- 唯一与正则相关的公共函数是 `ReplaceEnvStr`（见 7.3）。
- `TRegExpr` 默认 `ModifierI=False`（区分大小写，REGEX:130、1139）。

### 7.6 图标

- **`untUtilities` 没有任何图标提取函数**；全项目无 `ExtractIcon`/`SHGetFileInfo` 调用。
- 图标全部来自 DFM 内置 `TImageList`：管理窗体 `ilShortCutMan` 共 11 张图（`FSCM.dfm:257-262` 的位图头 `494C01010B00` → 11 images），仅用索引 0/1（`siItem`/`siInfo`，SCM:36、906、916）。主窗口条目列表是无图标的 `TListBox`（FMAIN:37）。

### 7.7 系统/注册表/自启动

| 函数 / 签名 | 用途 | 边界行为 | 行号 |
|---|---|---|---|
| `IsRunningInstance(const MutexName): Boolean` | 单实例检测 | 创建互斥体；`ERROR_ALREADY_EXISTS` 返回 True | 21、94-101 |
| `SetAutoRun(ApplicationName, ExecutablePath; AutoRun=True): Boolean` | 写注册表 Run 项自启动 | Vista+ 写 HKCU，XP 写 HKLM（`IsVista` 判定，UT:1021-1024）；失败 False | 22、115-146 |
| `SetAutoRunInStartUp(ApplicationName, ExecutablePath; AutoRun=True): Boolean` | 在“所有用户启动”目录建/删 `.lnk` | 失败 False | 23、148-167 |
| `GetAutoRunItemPath(ApplicationName): string` | 读 HKLM Run 项 | 读不到空串 | 24、169-190 |
| `AddMeToShortCutMenu(IsAdd): Boolean` | 在 `HKCR\*\shell` 注册“Add To ALTRun” | 命令为 `"exe" "%1"`；失败 False | 31、300-333 |
| `AddMeToSendTo(Name; IsAdd): Boolean` | 建/删 SendTo 快捷方式 | 失败 False | 32、335-345 |
| `ReduceWorkingSize: Boolean` | 压缩工作集 | 仅 NT 平台；失败 False | 29、103-113 |
| `IsVista: Boolean` | 主版本号 ≥ 6 | — | 43、1021-1024 |
| `GetCurrentUserName: string` | 当前用户名 | 依赖 `GetUserName` | 44、1026-1038 |
| `GetLocaleInformation(LocaleFlag): string` | 取区域信息 | 失败返回空串 | 45、1298-1309 |

### 7.8 输入/窗口/剪贴板/拖放/排序

| 函数 / 签名 | 用途 | 边界行为 | 行号 |
|---|---|---|---|
| `PostKeyEx32(key: Word; const shift: TShiftState; specialkey: Boolean)` | 合成单次按键（keybd_event） | 无返回值；`specialkey` 控制扩展键标志 | 20、347-405 |
| `SendKeys(SendKeysString: PChar; Translate, Wait: Boolean): Boolean` | 发送按键序列（`{ENTER}`、`^`、`%`、`+`、`~` 等） | 解析失败弹 MessageBox；`Wait=True` 时泵消息 | 33、416-772 |
| `FindWindowByCaption(Caption): HWND` | 按标题遍历顶层窗口 | 找不到返回 0；`DEBUG_MODE` 时把窗口列表写 `WindowList.txt` | 79、1214-1253 |
| `KillMessage(Wnd, Msg)` | 移除某消息，若是 `WM_QUIT` 则 `PostQuitMessage` | 无返回值 | 81、1289-1296 |
| `SetClipboardText(AStr)` | 写 Unicode 文本到剪贴板 | 分配 `Length*4` 字节；失败抛异常；只写 `CF_UNICODETEXT` | 76、1255-1287 |
| `GetDragFileCount(hDrop): Integer` | 拖放文件数 | 恒返回文件数（`DragQueryFile(-1)`） | 54、1040-1045 |
| `GetDragFileName(hDrop; FileIndex=1): string` | 取第 N 个拖入文件名（**默认 1 → 实际索引 0，即第一个文件**） | 文件名缓冲 255；拖入多个时界面只处理第一个 | 55、1047-1058 |
| `QuickSort(var SortNum: array of integer; p, r: integer)` | 整数数组快排 | 升序；典型 Hoare 分区；不检查空区间外的边界 | 49、938-976 |
| `SC_DRAGMOVE: LongInt = $F012` | 无边框窗体拖动常量（`WM_SYSCOMMAND`） | 被主窗体拖拽使用（FMAIN:2448、2471） | 53 |
| `SetLayeredWindowAttributes(...)` | user32 外部声明 | 用于半透明窗体 | 83-84 |

### 7.9 日志（`untLogger.pas`，被管理器/工具库使用）

| 函数 / 签名 | 用途 | 行号 |
|---|---|---|
| `InitLogger(IsTraceEnable=False; IsCnDebugEnable=False; IsAppendMode=False)` | 初始化日志（`exe.用户名.log`） | LOG:20、43-67 |
| `AddLog(const str)` | 追加一行（临界区保护） | LOG:21、69-83 |
| `TraceErr(Msg)` / `TraceErr(AFormat, Args)` | 错误日志 | LOG:22-23、85-102 |
| `TraceMsg(Msg)` / `TraceMsg(AFormat, Args)` | 调试日志（`TraceEnable=False` 时无操作） | LOG:24-25、104-122 |

---

## 8. 快捷项与文件系统的关系

### 8.1 从文件/快捷方式推导条目（`ExtractShortCutItemFromFileName`，SCM:799-882）

| 输入 | 结果 |
|---|---|
| 空串/空白 | `scBlank`，全空 |
| `*.lnk`（含被引号包裹的） | `ResolveLink` 解析出目标（目标+参数）；若解析为空或指向 `%windir%\Installer\{...}` 下的图标桩，则保留 `.lnk` 本身作为 CommandLine（SCM:820-833） |
| `http://...` | CommandLine 保留完整 URL；ShortCut = 主机名（`http://` 之后、第一个 `/` 之前）；Name=ShortCut（SCM:840-847） |
| 普通文件/目录 | CommandLine=原始路径；ShortCut=去路径、去扩展名的文件名（目录则保留完整目录名，如“1.2图片”不会被截成“1”）；Name=ShortCut（SCM:849-877） |
| Chrome 风格 `exe --app=http://...`（ExtractFileName 后以 `//` 开头） | ShortCut = 去掉 `//` 的网址（SCM:856-866） |

### 8.2 是否扫描开始菜单/桌面

**否。** 全项目没有 `CSIDL_PROGRAMS`/开始菜单遍历/桌面扫描；`GetFileListInDir` 唯一调用点是列语言文件（FMAIN:2335）。条目来源只有：用户手工添加、拖放文件、资源管理器右键、SendTo、命令行参数、默认列表。

### 8.3 支持的“目标”类型

| 目标类型 | 支持 | 说明 / 证据 |
|---|---|---|
| EXE/文档（文件关联） | 是 | `ShellExecute`，失败降级 `WinExec`（SCM:341-355） |
| 文件夹 | 是 | `ShellExecute` 打开；`DirectoryExists` 用于工作目录/校验（SCM:706、870；FSCM:436） |
| URL | 是 | 默认条目含百度/Google 等；`ptURLQuery/ptUTF8Query` 拼查询串（SCM:1447-1452） |
| Shell 命名空间 GUID（`::{20D04FE0-...}`） | 是（依赖 ShellExecute 支持） | 默认 `Computer` 条目（SCM:1401）；校验函数对不含 `\` 的串直接放过 |
| `.lnk` | 添加时解析；也可直接执行 | SCM:820-833 |
| 环境变量 `%VAR%` | 是 | `ReplaceEnvStr`；默认条目 `%WINDIR%`、`%PROGRAMFILES%`、`%USERPROFILE%\Recent`（SCM:1442-1445） |
| 相对路径 `.\`、`..\` | 是 | 执行时把 WorkingDir 设为 exe 目录（SCM:187-189）；管理界面提供批量“相对/绝对路径转换”（FSCM:277-325）；添加时建议转相对（SCM:437-444） |
| 网络路径 `\\` | 是（不校验、不当文件检查） | FSCM:414-415；FMAIN:1282 |
| `@`/`@+`/`@-` 前缀 | 是 | 控制窗口显示方式，见 §9.2 |
| 每项自定义图标 | **否** | 无字段、无提取 API |
| 工作目录单独指定 | **否** | 自动派生（`FileExists` 命中时取文件目录；相对路径时取 exe 目录） |

### 8.4 `ptNone` 下行命令的拆分语义（SCM:699-748）

执行 `ptNone` 条目时：
1. 先展开环境变量（SCM:697）。
2. 去两端成对引号（`RemoveQuotationMark`，SCM:700）。
3. 若形如 `"C:\...\firefox.exe" -Profile "Profiles"`（首字符非 `\`、非 `\\`、`FileExists(整串)=False`）：
   - 首字符为 `"` 时：截出第一个引号内的真实文件；`FileExists` 成立则拆成 `Command=文件`、`WorkingDir=文件目录`、`Param=剩余参数`（SCM:725-730），并把参数模式**动态升级为 `ptNoEncoding`**（SCM:742-745）→ 剩余参数作为 `ShellExecute` 的 lpParameters 传入。
4. 否则整串作为 `Command` 原样交给 `ShellExecute`。含空格的普通命令行（如 `explorer.exe /e,::{...}`）通常先失败，再降级 `WinExec`（其内部按命令行拆分）完成启动（SCM:341-355）。

### 8.5 参数占位符（`OPT:27-35`）

| 占位符 | 含义 | 取值来源 | 替换行为 |
|---|---|---|---|
| `%p` | 旧参数占位符 | 用户输入 / 剪贴板等 | 正则替换（`UseSubstitution=False`，即 `$`/`\` 不转义），整串替换 |
| `{%p}` | 新参数占位符 | 同上 | 同上；**优先级最高**（先判断） |
| `{%c}` | 剪贴板文本 | `m_Param[0]`（FMAIN:2424/2388） | 同上 |
| `{%wd}` | 前台窗口句柄（十进制字符串） | `m_Param[1]`（FMAIN:2427） | 同上 |
| `{%wt}` | 前台窗口标题 | `m_Param[2]`（FMAIN:2429） | 同上 |
| `{%wc}` | 前台窗口类名 | `m_Param[3]`（FMAIN:2430-2431） | 同上 |

规则：
- 只有**一个**占位符会被替换（按优先级短路：`{%p}` → `%p` → `{%c}` → `{%wd}` → `{%wt}` → `{%wc}`）（SCM:202-284、292-331）。
- 若命令行**没有任何**占位符：
  - `ptNoEncoding/ptURLQuery/ptUTF8Query` 且命令中未命中剪贴板/窗口占位符 → 弹出参数输入框（`ParamForm`），输入值作为 `Param`（SCM:771-781）。
  - `ptURLQuery`：`Command + 编码后参数`（直接拼接，不自动加 `=` 或 `&`）（SCM:330、785-793）。
  - `ptUTF8Query`：同上，UTF-8 编码。
- `m_Param` 数组 `array[0..5]`（SCM:74），只使用 0-3；4/5 未使用。取值写入点：剪贴板/窗口 ID/标题/类名（FMAIN:2427-2431）。
- 占位符替换在**展开环境变量之后**执行；随后 `ExecuteCmd` 又会再展开一次环境变量（SCM:697 → 339），即参数内容里的 `%VAR%` 也会被展开（潜在注入点）。

### 8.6 窗口显示方式前缀（`OPT:33-35`、SCM:170-185）

| 前缀 | 效果 | 判断顺序 |
|---|---|---|
| `@+` | `SW_SHOWMAXIMIZED` | 最先 |
| `@-` | `SW_SHOWMINIMIZED` | 第二 |
| `@` | `SW_HIDE` | 最后（因此 `@任意内容` 都会被当作“隐藏 + 去掉首字符”） |

前缀位于 CommandLine 首字符、只剥一次、随后 `Trim`；`@.\WinCtl.exe Hide ...` 这类默认条目即依赖此机制。

---

## 9. 硬编码常量、上限、默认值

### 9.1 常量（`untShortCutMan.pas` / `untALTRunOption.pas`）

| 常量 | 值 | 含义 | 证据 |
|---|---|---|---|
| `SHORTCUT_FILENAME` | `'ShortCutList.txt'` | 条目文件名 | SCM:22 |
| `FAVOURIT_FILENAME` | `'FavoriteList.txt'` | 收藏文件名 | SCM:23 |
| `PARAM_DELIMITER` | `'/\'` | 声明但**全项目未使用** | SCM:25 |
| `MAX_LATEST_NUM` | `10` | 最近列表上限 | SCM:27、502 |
| `Q_MIN` | `12` | 快排退化为冒泡的阈值 | SCM:1702、1725 |
| 空关键字排序基数 | `1000`（×Freq）、字母系数 `4/2/1` | `Sort()` 权重 | SCM:1909-1921 |
| 过滤打分基数 | `1024`、`Freq*4`、`pos*128`、`len*16` | `FilterKeyWord` 打分 | SCM:1026 |
| Freq 数字上限 | `8` 位 / 超限取 `10000` | 解析保护 | SCM:2016-2018 |
| 运行计数提示间隔 | `10000` | 恭喜对话框 | SCM:2094 |
| 前 N 条 | `10` | 数字前缀与 TopTen | SCM:1062、FMAIN:1396 |
| `PARAM_FLAG` | `'%p'` | 参数占位符 | OPT:27 |
| `NEW_PARAM_FLAG` | `'{%p}'` | 参数占位符（新） | OPT:28 |
| `CLIPBOARD_FLAG` | `'{%c}'` | 剪贴板占位符 | OPT:29 |
| `FOREGROUND_WINDOW_ID_FLAG` | `'{%wd}'` | 前台窗口句柄 | OPT:30 |
| `FOREGROUND_WINDOW_TEXT_FLAG` | `'{%wt}'` | 前台窗口标题 | OPT:31 |
| `FOREGROUND_WINDOW_CLASS_FLAG` | `'{%wc}'` | 前台窗口类名 | OPT:32 |
| `SHOW_MAX_FLAG` / `SHOW_MIN_FLAG` / `SHOW_HIDE_FLAG` | `'@+'` / `'@-'` / `'@'` | 显示方式前缀 | OPT:33-35 |
| `RESTART_FLAG` / `CLEAN_FLAG` | `'Restart'` / `'Clean'` | 命令行参数（重启延迟 2s / 清理自启动） | OPT:37-38；FMAIN:1953-1970 |
| `CMD_DIR` | `'MyCmd'` | 历史遗留（相关代码已注释/废弃） | OPT:39；SCM:1378-1383 |
| `UPGRADE_URL` | `'http://journeyboy.blog.sohu.com/112820379.html'` | 默认 Upgrade 条目 | OPT:25 |
| `TITLE` / `ALTRUN_VERSION` | `'ALTRun'` / `'V1.46.7'` | 程序名/版本 | OPT:21-22 |
| `ALTRUN_MUTEX` | `'ALTRUN_MUTEX'` | 单实例互斥体名 | FMAIN:2029 |
| `DEFAULT_LIST_FORMAT` | `'%-25s| %s'` | 结果显示格式 | OPT:99 |
| `ListFormatList` | `'%-25s| %s'` / `'%s (%s)'` / `'%s [%s]'` | 可选格式 | OPT:659-663 |
| `DEFAULT_ALPHA` / `DEFAULT_ROUNDBORDERRADIUS` / `DEFAULT_FORMWIDTH` | `240` / `12` / `420` | 窗体外观 | OPT:100-103 |
| `DEFAULT_BGFILENAME` / `DEFAULT_LANG` | `'BG.jpg'` / `'EN'` | 背景/语言默认 | OPT:104-105 |

### 9.2 选项默认值（`LoadSettings`，OPT:735-820；样例 `INI`）

| 选项 | 默认 | 样例 INI | 行号 | 与条目模型的关联 |
|---|---|---|---|---|
| `Regex` | `True` | `1` | OPT:751 | 正则匹配开关 |
| `MatchAnywhere` | `True` | `1` | OPT:753 | 是否允许从中间匹配 |
| `NumberKey` | `True` | `1` | OPT:754 | 数字前缀/数字键执行 |
| `IndexFrom0to9` | `False` | `0` | OPT:755 | 编号基准 |
| `RememberFavouratMatch` | `False` | `0` | OPT:756 | 收藏优先 |
| `ShowTopTen` | `True` | `1` | OPT:761 | 只显示前 10 |
| `ExecuteIfOnlyOne` | `False` | `0` | OPT:770 | 唯一结果自动执行 |
| `ParamHistoryLimit` | `50` | `50` | OPT:757 | 参数历史条数 |
| `HideDelay` | `15` | `15` | OPT:752 | 隐藏延时（毫秒级，具体单位见 FMAIN 定时器） |
| `HotKey1` / `HotKey2` / `LastItemHotKey` | `'Alt + R'` / `'Pause'` / `'ALT+L'` | 同 | OPT:746-748 | 全局热键 |
| `ListFormat` | `'%-25s| %s'` | 同 | OPT:798 | 结果显示格式 |
| `ShortCutRunCount` | `0` | `11562` | OPT:813 | 全局运行计数 |
| `LatestList` | `''` | `41,` + 空格 | OPT:817 | 最近列表下标串 |
| `TraceEnable` | `False`（样例为 1） | `1` | OPT:811 | 日志开关；只读不写回（OPT:883-884） |
| 管理窗体尺寸/列宽 | 450×240 / 各 100 | 1230×808；100/257/100/543 | OPT:775-781 | 管理界面 UI 状态 |
| 主窗体尺寸 | `FormWidth=420` | `INI:49` | OPT:803 | UI |

### 9.3 内置默认快捷项列表（首次运行生成，SCM:1399-1466）

仅当 `ShortCutList.txt` 不存在时生成；`m_WriteDefaultShortCutList` 恒为 True（SCM:616），因此文件被删后会重新生成。`Name` 取自语言文件 `[DefaultShortCutName]`（LANG:212-250；EN 默认值见 OPT:1117-1154）。

| # | 关键字 | Name（键） | 参数模式 | CommandLine | Freq | 行号 |
|---|---|---|---|---|---|---|
| 1 | `Computer` | 0 我的电脑 | None | `::{20D04FE0-3AEA-1069-A2D8-08002B30309D}` | 100 | SCM:1401 |
| 2 | `Explorer` | 1 资源管理器 | None | `explorer.exe /e,::{20D04FE0-3AEA-1069-A2D8-08002B30309D}` | 0 | SCM:1402 |
| 3 | `MyDocument` | 2 我的文档 | None | `explorer.exe` | 0 | SCM:1403 |
| 4 | `ie` | 3 IE浏览器 | None | `iexplore.exe` | 8 | SCM:1404 |
| 5 | `notepad` | 4 记事本 | None | `notepad` | 7 | SCM:1405 |
| 6 | `mspaint` | 5 画图 | None | `mspaint` | 0 | SCM:1406 |
| 7 | `cmd` | 6 Dos窗口 | No_Encoding | `cmd /k %p` | 6 | SCM:1407 |
| 8 | `Calc` | 7 计算器 | None | `calc` | 5 | SCM:1408 |
| 9 | `Desktop` | 8 显示桌面 | None | `@.\WinCtl.exe MinAll`（依赖外部 `WinCtl.exe`，仓库内**不存在**） | 0 | SCM:1430 |
| 10 | `msconfig` | 9 系统配置 | None | `msconfig` | 0 | SCM:1432 |
| 11 | `Reg` | 10 注册表编辑器 | None | `regedt32` | 4 | SCM:1433 |
| 12 | `myip` | 11 我的IP地址 | None | `nslookup` | 0 | SCM:1434 |
| 13 | `dev` | 12 设备管理器 | None | `devmgmt.msc` | 0 | SCM:1435 |
| 14 | `Taskmgr` | 13 任务管理器 | None | `taskmgr` | 0 | SCM:1436 |
| 15 | `ShutDown` | 14 立即关机 | None | `shutdown /s /t 5` | 0 | SCM:1439 |
| 16 | `Reboot` | 15 立即重启 | None | `shutdown /r /t 5` | 0 | SCM:1440 |
| 17 | *(空白行)* | — | — | — | — | SCM:1441 |
| 18 | `Windows` | 16 Windows 目录 | None | `%WINDIR%` | 0 | SCM:1442 |
| 19 | `System32` | 17 System32 目录 | None | `%WINDIR%\System32` | 0 | SCM:1443 |
| 20 | `ProgramFiles` | 18 Program Files 目录 | None | `%PROGRAMFILES%` | 0 | SCM:1444 |
| 21 | `Recent` | 19 最近打开文件 目录 | None | `%USERPROFILE%\Recent` | 10 | SCM:1445 |
| 22 | *(空白行)* | — | — | — | — | SCM:1446 |
| 23 | `b` | 20 百度 搜索引擎 | URL_Query | `http://www.baidu.com/s?wd=` | 12 | SCM:1447 |
| 24 | `g` | 21 Google 搜索引擎 | UTF8_Query | `http://www.google.com/search?q=` | 14 | SCM:1448 |
| 25 | `s` | 22 搜狗MP3 搜索引擎 | URL_Query | `http://mp3.sogou.com/music.so?query=` | 0 | SCM:1449 |
| 26 | `zd` | 23 百度知道 搜索引擎 | URL_Query | `http://zhidao.baidu.com/q?ct=17&pn=0&tn=ikaslist&rn=10&word=` | 9 | SCM:1450 |
| 27 | `v` | 24 VeryCD 搜索引擎 | UTF8_Query | `http://www.verycd.com/search/folders?kw=` | 0 | SCM:1451 |
| 28 | `y` | 25 Yahoo 搜索引擎 | UTF8_Query | `http://search.yahoo.com/search?p=` | 0 | SCM:1452 |
| 29 | *(空白行)* | — | — | — | — | SCM:1453 |
| 30 | `Upgrade` | 26 前往升级网址 | None | `UPGRADE_URL` | 15 | SCM:1454 |
| 31 | `Config` | 27 编辑配置 | None | `.\ALTRun.ini` | 0 | SCM:1455 |
| 32 | *(空白行)* | — | — | — | — | SCM:1456 |
| 33 | `AddRemoveProgram` | 28 添加/删除程序 | None | `appwiz.cpl` | 0 | SCM:1457 |
| 34 | `ControlPanel` | 29 控制面板 | None | `control.exe` | 0 | SCM:1458 |
| 35 | `InternetConnection` | 30 Internet控制 | None | `control.exe Inetcpl.cpl ,4` | 0 | SCM:1459 |
| 36 | `SystemProperty` | 31 系统属性 | None | `Sysdm.cpl` | 0 | SCM:1460 |
| 37 | `r` | 32 运行 | No_Encoding | `{%p}` | 100 | SCM:1461 |
| 38 | `cb` | 33 百度 搜索剪贴板 | URL_Query | `http://www.baidu.com/s?wd={%c}` | 12 | SCM:1462 |
| 39 | `cg` | 34 Google 搜索剪贴板 | UTF8_Query | `http://www.google.com/search?q={%c}` | 24 | SCM:1463 |
| 40 | `ShowOnly` | 35 只显示本窗口 | No_Encoding | `@.\WinCtl.exe ShowOnly {%wd}` | 15 | SCM:1464 |
| 41 | `Hide` | 36 隐藏本窗口 | No_Encoding | `@.\WinCtl.exe Hide {%wd}` | 20 | SCM:1465 |
| 42 | `UnHide` | 37 恢复窗口 | None | `@.\WinCtl.exe UnHide` | 20 | SCM:1466 |

> 另有被注释掉的默认项：`ShutDown/Reboot` 的旧命令 `tsshutdn`（SCM:1437-1438）、`Desktop.scf` 方案（SCM:1411-1429）。

### 9.4 界面列与图标

| 项 | 值 | 证据 |
|---|---|---|
| 管理列表列 | ShortCut / Name / Param Type / Command Line，设计宽 100/100/100/400（实际宽度从 INI 恢复） | FSCM.dfm:76-92；FSCM:562-565；OPT:780-781 |
| 管理列表样式 | `vsReport`、`RowSelect`、`GridLines`、`DragMode=dmAutomatic`、`FullDrag` | FSCM.dfm:76-114 |
| 图标索引 | 0=`scItem`，1=`scBlank` | SCM:36、906、916 |
| 弹出菜单 | 仅 Add/Edit/Delete（mniCut 名称有误导） | FSCM.dfm:663-675 |
| 编辑对话框控件 | ShortCut / Name / CommandLine / ParamType(4 选项) / File / Dir / Help / Test | FSC.dfm:86-184 |

---

## 10. 不确定项与已知缺陷汇总

### 10.1 不确定项（无法从源码唯一确定）

| # | 事项 | 说明 |
|---|---|---|
| U1 | Delphi 编译器版本（`AnsiString` vs `UnicodeString`） | 仓库同时有 `.dproj` 与 `.bdsproj`；影响 `%-30s` 宽度按字节还是字符、`Length()` 计数、正则 `.` 对中文的匹配单位、文件编码路径 |
| U2 | `SaveFavoriteList` 末尾孤立 `%` 是否抛异常 | `Format('%-30s|%-30s%', ...)`（SCM:1778-1779）；若抛 `EConvertError`，异常被吞掉且文件已被截断 → 收藏每次退出清空。**必须实测** |
| U3 | 非法正则的影响 | 异常被捕获后未 `Continue`，仍读 `MatchPos[0]`（可能残留），结果未定义（SCM:1000-1009） |
| U4 | `GetFileModifyTime` 在文件不存在时的返回值 | `FileAge=-1` 的日期换算依赖 RTL（SCM:1484-1485；UT:198）；`LoadShortCutList` 可能在文件缺失时误判“未变化” |
| U5 | `GetFileSize` 异常路径 | `fs` 未初始化即 `finally fs.Free`（UT:243-259），异常时可能二次异常；未实测 |
| U6 | `TListView` 内联编辑是否可编辑 SubItems | 依赖 VCL 版本；从代码看只处理 `OnEdited` 的 Caption（FSCM:655-665） |
| U7 | 旧列表文件历史格式细节 | 只有“古时候用逗号、新版本用竖线”的注释（SCM:1988）；逗号格式字段顺序按代码推断为 `ShortCut,Name,CommandLine` |
| U8 | `TStringList.Values`/`IndexOfName` 的内部 `=` 表示 | Delphi VCL 行为（非本仓库源码），默认大小写不敏感 |
| U9 | `'.'` 在 `TRegExpr` 中是否匹配一个中文双字节 | 取决于 `TRegExpr` 字符串类型；AnsiString 下可能按字节（半个汉字）匹配 |
| U10 | `FavoriteList` 空文件初始内容 | 代码只 `ReWrite` 创建空文件（SCM:1284-1296），无默认内容 |
| U11 | “隐藏延时”单位 | 默认 15，`RestartHideTimer(HideDelay)`（FMAIN:966）中单位未在本次范围内确认 |

### 10.2 已知缺陷（重写时决定“修复”或“兼容”）

| # | 缺陷 | 位置 |
|---|---|---|
| D1 | 旧格式条目关键字以 `F` 开头会被丢弃 | SCM:1992 |
| D2 | 命令行还原使用全局字符串删除 | SCM:2039-2044 |
| D3 | 无字段转义，`|` 不可用于 Name/关键字 | SCM:1879 |
| D4 | `ReplaceEnvStr` 只展开一个/最后一个变量 | UT:1008-1014；SCM:1751-1755 |
| D5 | 执行路径两次展开环境变量 | SCM:697、339 |
| D6 | 长度守卫在正则通配符展开之后 | SCM:993 |
| D7 | 随机化快排 → 结果不稳定 | SCM:1830-1837、1723-1744 |
| D8 | `GetFileListInDir` 跳过第一个文件 | UT:922-930 |
| D9 | `SaveShortCutList`/`SaveFavoriteList` 永不返回 True | SCM:1803-1828、1767-1787 |
| D10 | 最近列表用数组下标持久化，重排后错位 | SCM:1091-1111、1839-1867 |
| D11 | `GetLatestShortCutItemList` 未设置 `Result` | SCM:1113-1163 |
| D12 | `SetLatestShortCutIndexList` 越界判断 off-by-one | SCM:1861 |
| D13 | `InsertShortCutItem`/`ModifyShortCutItem` 不设置 `ParamType` | SCM:1219-1225、1544-1547 |
| D14 | `ContainShortCutItem(ShortCutType,...)` 使用未创建的类指针（潜在 AV）；且该 API 全项目无调用 | SCM:596-608 |
| D15 | 运行中外部修改文件会在退出时被内存旧数据覆盖 | FMAIN:248 |
| D16 | `Freq` 仅在搜索执行路径累加，异常退出丢失 | SCM:2075 |
| D17 | 删除条目不清收藏、不改 LatestList 持久化串 | SCM:644-646 |
| D18 | `Format` 的 `%-Ns` 不截断，超长字段破坏列对齐（但不破坏解析） | SCM:1879 |
| D19 | 管理界面去重是“四字段全等”，允许同关键字多条；与主窗口添加的“仅关键字判重”不一致 | FSCM:511-532 vs SCM:458、1170-1187 |
| D20 | 添加时若 ShortCut 或 CommandLine 为空会静默丢弃 Name 生成空白行 | FSCM:125-140；SCM:416-434 |
| D21 | 默认条目依赖仓库内不存在的 `WinCtl.exe` | SCM:1430、1464-1466 |
| D22 | `ExtractShortCutItemFromFileName` **恒返回 True**（无论解析成功/失败），调用方的失败提示分支是死代码 | SCM:799-882（881） vs SCM:394-401、FSC:277-283、FSCM:710-716 |

---

## 11. 重写注意点（C++）

### 11.1 建议的数据结构（1:1 对齐优先）

原始模型是“**扁平命令行 + 派生执行信息**”，不要把它拆成 `target/args/workdir` 三个持久化字段，否则旧数据迁移与行为对齐都会变复杂。建议：

```cpp
namespace alt::legacy {

enum class ItemKind : uint8_t { Item = 0, Blank = 1 };            // scItem / scBlank；scOther/scRemark 不入库
enum class ParamMode : uint8_t { None = 0, NoEncoding = 1, UrlQuery = 2, Utf8Query = 3 };

struct ShortcutItem {
    ItemKind   kind = ItemKind::Item;
    ParamMode  param = ParamMode::None;
    std::string keyword;       // 原 ShortCut —— 唯一参与匹配的字段
    std::string display_name;  // 原 Name —— 仅显示 / 收藏映射 / 最近列表文字
    std::string command_line;  // 原 CommandLine —— 原样保存（含 @ 前缀与 %p 等占位符）
    int32_t     run_count = 0; // 原 Freq —— 持久化
    // 以下为运行时字段，不持久化
    mutable int64_t rank = 0;  // 原 Rank
    uint64_t    stable_id = 0; // 新增：替代“数组下标”的稳定标识（修 D10）
};

}  // namespace alt::legacy
```

执行期再派生：

```cpp
struct LaunchPlan {
    std::string program;    // 由 command_line 首段/引号规则拆分（ptNone 特例）
    std::string arguments;  // ShellExecute 的 lpParameters
    std::string workdir;    // FileExists 命中 → 文件目录；相对路径 → exe 目录
    int         show_cmd;   // SW_SHOWNORMAL / MAX / MIN / HIDE
    bool        use_shellexecute;
};
LaunchPlan BuildLaunchPlan(ShortcutItem&, const ParamContext&);  // ParamContext 提供剪贴板/前台窗口/用户输入
```

若产品确需 `alias` / `note` / `favourite` / `last_used` 等**原始版本没有**的字段（现有骨架已假设存在），必须明确它们只是**扩展**：不参与旧格式导入导出、不改变原始匹配语义，并在新存储格式中单独列字段；否则无法做到“1:1 对齐”。

### 11.2 与当前 C++ 骨架的差异（必须裁决）

| 现有骨架 | 原始实现 | 建议 |
|---|---|---|
| `Shortcut.alias`（`Shortcut.h:13-14`，逗号分隔、参与匹配） | **不存在**；`Name` 不参与匹配 | 若保留扩展，需要用户明确同意；默认匹配仍只用 `keyword` |
| `Shortcut.target/args/workdir`（`Shortcut.h:16-20`） | 只有 `command_line`；`workdir` 是派生量 | 存储层保留 `command_line`；执行层派生 |
| `Shortcut.note`（`Shortcut.h:22`，参与匹配） | 不存在 | 同上 |
| `Shortcut.favourite` / `last_used`（`Shortcut.h:25-27`） | 收藏是独立映射；无最后使用时间 | 收藏单独存 `favourite_key → display_name`；`last_used` 若需要则新增并写明非原始行为 |
| `Matcher` 对 `note/target` 也匹配（`Matcher.cpp:71-95`） | 只匹配 `ShortCut` | 必须收窄到 `keyword`，否则同输入的结果集合与排序会与原版不同 |
| `PopularityBonus` 上限 200/4=50（`Matcher.cpp:13-18`） | `Freq*4` 无上限、另含命中位置与长度惩罚 | 换成 §5.5 的完整公式，并用 `int64_t` 防溢出 |
| 存储路径 `%APPDATA%\ALTRun\shortcuts.db`（`ShortcutStore.cpp:23-38`） | `exe 目录\ShortCutList.txt` | 迁移阶段必须同时探测 exe 目录（见 §11.3） |
| 字段分隔 `\x1f`、9 字段（`ShortcutStore.cpp:20、66-82`） | `|`、5 字段（F + 参数 + 关键字 + 名称 + 命令行） | 新格式可自定，但旧格式解析器必须按本文 §3 实现 |
| `Text.h:2-6` 注释称“原始把所有数据存在 INI 里” | 只有选项/最近列表在 INI；条目在 `ShortCutList.txt`，收藏在 `FavoriteList.txt` | 修正注释，避免误导迁移实现 |

### 11.3 持久化格式建议 + 旧格式兼容读取

**建议**：内部 UTF-8；新格式带版本头 + 显式转义（JSON 或“TSV + 转义”）；写入原子化。示例（TSV 变体）：

```
ALTRUN-SHORTCUTS\t2
# kind\tparam\tkeyword\tname\tcommand_line\trun_count\tid
item\tnone\tnotepad\t记事本\tnotepad\t7\t1
item\turl_query\tb\t百度 搜索引擎\thttp://www.baidu.com/s?wd=\t12\t2
blank\t\t\t\t\t0\t0
```

规则：
- 字段内 `\t`、`\n`、`\\` 转义；不使用 `|`/`,` 作为语义分隔符。
- `kind=blank` 保留空白分隔行（UI 依赖）。
- 写入：临时文件 → `FlushFileBuffers` → `MoveFileEx(REPLACE_EXISTING|WRITE_THROUGH)`；同时保留 `ShortCutList.txt.bak` 兼容层。
- 保留 `id` 稳定标识，最近列表/收藏改用 `id`（修 D10）。

**旧格式迁移（必须一次成功）**：
1. 探测候选路径：`<exe目录>\ShortCutList.txt`（原始）、`%APPDATA%\ALTRun\`（新骨架）、以及 `ShortCutList.txt.bak`（主文件 0 字节时）。
2. 编码探测：有 BOM → UTF-8/UTF-16；无 BOM 且能严格 UTF-8 解码 → UTF-8；否则按 GBK(936) 解码（可配置代码页）。**不要默认 UTF-8**，否则中文全乱。
3. 解析：按 §3.2 状态机同时支持“旧逗号格式”和“新 `F...|...` 格式”；**修复 D1**（不要用首字符 `F` 决定分隔符：先尝试按 `|` 解析且首段匹配 `^F\d+$`，失败再按 `,` 解析）。
4. 校验：解析出的条目数 + 空白行数 ≥ 非空行数的 95% 才允许自动覆盖；否则保留原文件并提示用户。
5. 备份原文件为 `ShortCutList.txt.migrated-YYYYMMDD-HHMMSS`；不要删除 `.bak`。
6. 导入 `FavoriteList.txt`（`keyword|name`，小写键；容忍行尾孤立 `%` 与多余空格）。
7. 导入 `ALTRun.ini`：`[DEBUG] LatestList` → 解析下标 → 映射为 `stable_id`（仅在条目顺序未被用户改动过的首次迁移时有效）；`[DEBUG] ShortCutRunCount` 原样保留为全局计数。
8. 生成默认列表的逻辑仅在“迁移后仍为空且文件从未存在”时执行（对齐 `m_WriteDefaultShortCutList`，SCM:616、1399）。

### 11.4 匹配与排序复刻要点

1. 匹配对象：**仅 `keyword`**；不要匹配 `display_name`/`command_line`。
2. 归一化：仅 ASCII 小写（`ToLowerUtf8` 对 CJK 不变）；不做全半角/NFKC；不 Trim 用户输入（但空串和 `" "` 单独处理，对齐 SCM:945-950）。
3. 正则模式：逐字符实现原通配符预处理（`*`→`.*`、`?`→`.`、保护已有 `.*`）；用 `std::regex`（ECMAScript）或 PCRE2 编译一次并缓存；非法正则视为“不匹配”并记日志（修 U3）。
4. 优先用 PCRE2/UTF-16 以获得与原版对中文一致的“单字符”语义；若用字节正则，必须明确 `.` 的字节语义（U9）。
5. 排序：完全照抄 §5.5 打分公式；用 `std::stable_sort` + `(rank desc, 原始顺序 asc)` 作为确定性 tie-break；不要复刻随机快排。
6. 空输入：按 `Sort()` 公式（Freq*1000、前 3 字符字母权重、长度惩罚）生成“默认列表”，并保持与过滤结果一致的显示格式/数字前缀逻辑。
7. 收藏置顶：条件与顺序完全照 SCM:1045-1054（只移动第一个匹配到 0 位）。
8. 数字前缀：`IndexFrom0to9` 与参数模式共同决定 ` %d|` / `*%d|` / `  |`。
9. 长度守卫：放在**原始输入长度**上判断（修 D6）。

### 11.5 编码、并发与健壮性

- 内部 UTF-8（或 UTF-16），Win32 边界转 UTF-16；旧数据按 GBK 读取（见 11.3）。
- 注册 `CreateMutexW(L"ALTRUN_MUTEX")` 单实例（UT:94-101）；第二次启动把 `%1` 转发给已运行实例（`WM_SETTEXT`/自定义消息，FMAIN:2006-2013）。
- 保存单点收口：`SaveNow()` 原子写 + 备份；对 `Freq` 采用“执行后 1~2 秒去抖保存”，避免原版“只在退出时保存”的丢失问题。
- 文件监视（`ReadDirectoryChangesW`）：外部修改时若本地无脏数据则重载；有脏数据则提示合并或备份后覆盖，绝不静默丢失（修 D15）。
- 0 字节文件恢复 `.bak`；解析失败保留原文件并把坏行写入 `.rejected` 日志，而不是静默丢弃（原版静默丢弃，SCM:1517-1523）。
- 所有 `Format`/解析使用有界缓冲区与长度检查，禁止原版那种“从整行删子串再定位”的还原方式（D2）。

### 11.6 验收测试清单（对齐原始行为）

- [ ] 读取一份中文 GBK 的 `ShortCutList.txt`，条目数、关键字、名称、命令行、Freq、参数模式逐字段一致。
- [ ] 旧逗号格式文件（含以 `F` 开头关键字）可导入（修复 D1 后不得丢失）。
- [ ] 含 `|` 的命令行（如 Google 翻译 URL）与含空格/引号/中文的路径往返不损坏。
- [ ] 空白行（`scBlank`）保持为分隔行，管理界面显示为“图标 1 的空行”。
- [ ] `Freq > 8` 位、非数字频率的行为与原版一致（`10000`/0）。
- [ ] 搜索：非正则/正则、`MatchAnywhere` 开/关、空输入、单空格输入、中文、大写输入，结果集合与相对顺序一致（顺序按确定性 tie-break 校验集合）。
- [ ] 打分：构造 Freq=0/7/100、命中位置 1/3/末尾、关键字长度差，验证与公式逐项吻合。
- [ ] 收藏：执行后 `keyword→Name` 落盘；`RememberFavouratMatch` 开启后置顶。
- [ ] 最近列表：执行 11 次后保持 10 条、最新在前；重启后顺序一致（改用 stable id 后）。
- [ ] 添加冲突替换：`Freq` 保留；管理界面四字段去重与主窗口“仅关键字”去重分别符合原版。
- [ ] 删除确认、管理窗体 Cancel 不落盘、OK 一次性落盘。
- [ ] `@`/`@+`/`@-` 前缀、`%p`/`{%p}`/`{%c}`/`{%wd}`/`{%wt}`/`{%wc}`、URL/UTF-8 编码、环境变量、`.\` 相对路径、`.lnk` 解析。
- [ ] 默认列表首次生成（含 4 个空白分隔行）。
- [ ] 损坏文件：截断行、0 字节、`.bak` 恢复、非法正则不崩溃。

