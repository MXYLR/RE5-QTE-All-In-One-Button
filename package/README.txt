RE5 QTE All-In-One-Button  v1.5.9
==================================

把《生化危机 5》（Steam 黄金版 1.2.0 / re5dx9.exe，DX9）里所有 QTE 统一成一个键（默认 E）：
  - 被敌人抓住时的摇杆挣脱（原本左右摇 / A+D）
  - 剧情 QTE 的按键与组合键（A、B、X、A+X……，例如威斯克战）
  - 收割者 / 舔食者 / 威斯克攻击时的 QTE

普通场景提示保持原键位：开门、拾取、A+D 转动机关等都不会被接管。


安装
----
1. 先关闭游戏。
2. 把本压缩包里的所有文件解压到游戏根目录（和 re5dx9.exe 同一层），即：
      <游戏目录>\RE5QTE.ini
      <游戏目录>\dsound.dll
      <游戏目录>\plugins\RE5QTE.dll
3. 启动游戏。首次启动会自动从系统目录复制一份 dsound_orig.dll 到游戏根目录，
   这是插件加载器的转发目标（不随压缩包分发微软的系统 DLL）。
4. 成功后 plugins\RE5QTE.log 里会出现：
      [boot] hook installed: 0x00767576 (ButtonMgr::checkAll) -> QteCheck

如果游戏没有声音、或启动失败：删掉根目录的 dsound.dll、dsound_orig.dll 即可完全恢复，
并反馈问题。


配置
----
RE5QTE.ini（游戏根目录）：
  [Auto] Enabled = 0     0 = 手动按 E（默认）；1 = 全自动
  [General] Key = E      QTE 键
  [General] Mode = Hold  Hold = 按住生效；Tap = 只在按下瞬间生效
  [General] SingleKey = 1  1 = 可识别的 QTE 提示合并成单个 E；0 = 保留原组合显示
  [General] ForceAll = 0   1 = 所有提示都接受 E（紧急开关，会连开门/拾取一起接管）
详见仓库 README。


卸载
----
删除以下文件即可，游戏本体一个字节都不会被修改：
  <游戏目录>\plugins\RE5QTE.dll
  <游戏目录>\dsound.dll
  <游戏目录>\dsound_orig.dll
  <游戏目录>\RE5QTE.ini


注意
----
- Steam「验证游戏文件完整性」会删除 dsound.dll / dsound_orig.dll（不在清单里），验证后需重新解压。
- 本插件只对当前这份 re5dx9.exe（Enigma 1.31 壳、1.2.0）有效；换版本会安全地不安装并写日志。
- 地址与偏移来自逆向，未在联机模式验证。

项目主页：https://github.com/MXYLR/RE5-QTE-All-In-One-Button
