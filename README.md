# RE5 QTE All-In-One-Button

把《生化危机 5》（Steam 黄金版 1.2.0 / `re5dx9.exe`，DX9）里所有 **QTE** 统一成一个键——
默认 **E**。包括：

- 被敌人抓住时的**摇杆挣脱**（原本要左右摇 / 按 A+D）；
- 剧情 QTE 里的**按键与组合键**（A、B、X、A+X……），例如威斯克战；
- 被收割者 / 舔食者 / 威斯克攻击时的 QTE。

**普通场景提示保持原键位**（开门、捡东西、A+D 转动机关等），不会被接管。

> 实机验证：抓取挣脱、威斯克战 QTE 均可只按 E 通过；A+D 转动机关保持原样。

---

## 功能

| 功能 | 说明 |
|---|---|
| 单键 QTE | 所有被判定为 QTE 的提示，按 `Key`（默认 E）即可完成 |
| 摇杆挣脱 | 不需要真的摇摇杆，按 E 即可 |
| 提示合并 | 能识别的 QTE 提示会从 `A+D`、`E+Q` 这类组合合并成单个 `E` |
| 原生键屏蔽 | `EOnlyTypes` 中的提示每帧清掉原生按键（鼠标左键等），只能按 E |
| 连段入口 | 多段 QTE 前的非 strict 入口提示（`EntryTypes`，默认 type 9）也可用 E 触发 |
| 普通提示不动 | 开门 / 拾取 / 机关（A+D）等保持游戏原键位 |
| 可选全自动 | `[Auto] Enabled=1` 时不用按键，QTE 自动完成 |
| 日志 | `plugins\RE5QTE.log` 记录每条提示与每次触发，便于排查 |

## 安装

1. 把 `dist\RE5QTE.dll` 放进游戏目录的 `plugins\`。
2. 把 `dist\dsound.dll`、`dist\dsound_orig.dll` 和 `RE5QTE.ini` 放进游戏根目录（和 `re5dx9.exe` 同级）。
3. 启动游戏。日志里应看到：

```
[cfg] Key VK=69
[ui] SingleKey -> 1
[cfg] ExtraTypes mask -> 33558530
[cfg] EOnlyTypes mask -> 4098
[cfg] EntryTypes mask -> 512
[patch] strict-fail sites patched=1
[boot] hook installed: 0x00767576 (ButtonMgr::checkAll) -> QteCheck
```

> `dsound.dll` / `dsound_orig.dll` 是插件加载器：本整合包的插件由它从 `plugins\` 载入，
> 不依赖、也不修改 dinput8（RE5Fix）和 xinput1_3（RE3Controls）。

## 配置（`RE5QTE.ini`）

```ini
[Auto]
Enabled = 0         ; 0 = 手动按 E（默认）；1 = 全自动
Grab = 1            ; 敌人事件动作里的提示算 QTE
Combo = 1           ; 两键及以上的提示算 QTE
Special = 1         ; 掩码为 0 的摇杆/特殊输入提示算 QTE
Strict = 0          ; 带 strict 的提示算 QTE

[General]
Enabled = 1
Key = E             ; QTE 键（字母 / 数字 / F1-F12 / 0x45）
Mode = Hold         ; Hold = 按住生效；Tap = 只在按下瞬间生效
ForceAll = 0        ; 1 = 所有提示都接受 E（紧急开关）
ForegroundOnly = 1
Log = 1
MaxPromptLog = 600
SingleKey = 1       ; 1 = 把可识别的 QTE 提示合并成单个 E；0 = 保留原组合显示
PadInput = 1        ; 1 = 让 QTE 处理器读取插件注入的 pad（键盘模式下也生效）
QteKeys = 0         ; 1 = 用玩家按键回答严格 QTE 的按键查询
NoStrictFail = 1    ; 1 = 关闭游戏“按错即判失败”的规则（多段 QTE 靠它不判死）
ExtraTypes = 25,12,1  ; 额外让 E 应答的类型：25 = 急救喷雾，12 = 剧情闪避，1 = 普通体术
EOnlyTypes = 1,12     ; 只认 E 的类型（应为 ExtraTypes 的子集）；未列出的保留原生鼠标左键
EntryTypes = 9        ; 多段 QTE 的非 strict 入口类型（注入原生键并保持约 0.3 秒）
Glyph = 0           ; 旧版全局图标覆盖（已废弃，保持 0）
```

`ExtraTypes` 与 `EOnlyTypes` 都是按 `[prompt] type=N` 填的逗号列表。默认配置里：

- `25`（急救喷雾）只进 `ExtraTypes` —— 鼠标左键照旧可用，也能按 E；
- `1`（硬直中的普通体术）和 `12`（剧情“按 Q 闪避”）同时进两个列表 —— 原生键被清掉，只认 E；
- `EntryTypes = 9` 只影响非 strict 的 type 9 入口提示；若发现普通互动提示也被 E 触发，删掉这行。

## 日志

```
[pawn] slot=0 act=0x80000000 ev=0x0A67D850 cls=1 src=0x0AD6E8C0 dti=0x011EA8D4 nc=0 hp=818
[prompt] req=0x23A9E540 stamp=0x… type=9 mask=0x00008000 strict=1 state=0 used=0x0 slot=0 cat=1 kind=14 act=0x80000000 ev=1 t=0x…
[force] slot=0 type=1 req=0x23A9E540 mask=0x00008000 key=0x45
[native] block req=0x23A9E540 stamp=0x… type=1 slot=0 mask=0x00008000
[entry] req=0x23A9E540 stamp=0x… type=9 slot=0 mask=0x00008000 stock=1 hold=300 act=0x0 waited=610 press=203
[strict] req=0x23A9E540 stamp=0x… type=9 cat=1 mask=0x00008000 stock=1
[hold] end=timeout mask=0x00008000 held=300
```

- `[pawn]`：动作 id / 事件动作变化（`cls=1/2/3` 表示敌人 / 触手舌头 / Boss 抓取）；
- `[prompt]`：新的 QTE/场景提示；
- `[force]`：E 完成了一次提示；
- `[native] block`：`EOnlyTypes` 提示上收到了原生按键，已从三份输入中清掉；
- `[entry]` / `[hold]`：E 应答了多段 QTE 的入口，并保持注入约 0.3 秒；
- `[strict]`：严格 QTE 段交给原版 `Check` 的结果（`stock=1` = 该段已通过）。

## 构建

需要 Visual Studio Build Tools（C++ 桌面开发）。

```bat
plugin\build_msvc.bat     :: -> dist\RE5QTE.dll
loader\build_msvc.bat     :: -> dist\dsound.dll（dsound_orig.dll 需从系统 SysWOW64 复制）
tests\build_tests.bat     :: 构建离线测试版（合成镜像）与测试宿主
tests\run_tests.bat       :: 5 种模式全部应 PASS
```

`loader\` 还需要 `dist\dsound_orig.dll`：把 `C:\Windows\SysWOW64\dsound.dll` 复制过去改名即可。

## 多段 QTE（威斯克等）

游戏里所有屏幕按键提示都由 `sActionButton::ButtonMgr` 驱动，每帧调用
`ActionSlot::Check (0x00766F00)`；整份镜像里对该函数的调用只有一处
（`0x00767576`）。插件把这 5 字节改成自己的入口：

- 识别的 QTE 提示：按 E 时返回"已按对"，并把该提示要求的按键写进 pad 状态一帧；
- 同时把这条请求的类型/分类/图标改成单 X（键盘显示 E），让提示合并；
- 其它提示完全走原逻辑。

更细的地址、结构偏移、加载器原因见 [docs/TECHNICAL.md](docs/TECHNICAL.md)。

## 已知限制

- 只对**当前这份** `re5dx9.exe`（Enigma 1.31 壳、1.2.0）有效；换版本会安全地不安装并写日志。
- 提示合并只针对能识别为 QTE 的提示（`strict` 或敌人抓取中）；普通组合键提示（A+D 机关）保持原样。
- `EOnlyTypes` 必须是 `ExtraTypes` 的子集：只有同时出现在两个列表里的类型才清原生键。
- `EntryTypes` 按类型生效：默认的 `9` 会让所有非 strict 的 type 9 提示（包括普通近战提示）都能由 E 触发。
- 多段 QTE 的入口应答有约 600ms 的等待窗口（场景要在提示出现约 0.6 秒后才接受入口）；提前按 E 会被场景放弃。
- Steam「验证游戏文件完整性」会删除 `dsound.dll` / `dsound_orig.dll`，验证后需重新放回。
- 地址与偏移均为逆向结果，未在联机模式下验证。

## 许可

MIT，见 [LICENSE](LICENSE)。第三方声明见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
