# dsound.dll 插件加载器（RE5）

`re5dx9.exe` 静态导入 `dsound.dll`（按序号 11 调 `DirectSoundCreate8`），所以放在游戏根目录的
同名 DLL 会在进程启动时被加载。利用这一点恢复 `plugins\` 的插件加载能力——原来的 dinput8
插件加载器已经被 RE5Fix 的 dinput8 代理替换，不再扫描插件目录。

- 产物：`dsound.dll`（代理/加载器）+ `dsound_orig.dll`（32 位系统 dsound.dll 的副本）
- 日志：`plugins\RE5Loader.log`
- 源码：`source_loader\dsound_proxy.c`、`source_loader\dsound.def`
- 构建：`source_loader\build_msvc.bat`（VS Build Tools，无 CRT 依赖，只导入 KERNEL32）

## 为什么是 dsound，不是 version 或 dinput8

- **dinput8**：游戏确实会加载同目录的 dinput8，但这个文件现在被 RE5Fix 占用，不能再拿来当加载器。
- **version.dll**：实测**没有被加载**——这个进程在解析 exe 导入之前就已经有系统 `version.dll`
  （兼容层/覆盖层），同目录同名 DLL 被忽略。
- **dsound.dll**：正常静态导入、不会被提前预加载，游戏按序号 11 调用 `DirectSoundCreate8`。

## 两个坑（都已处理）

1. **同名加载会拿回自己**：代理里 `LoadLibrary("...\System32\dsound.dll")` 会返回已经在加载的
   代理自己，转发函数就会跳进自己。所以真正的 DLL 以 `dsound_orig.dll` 的名字放在游戏目录，
   转发目标指向它。
2. **不能在 DllMain 里加载**：在 DllMain 里加载真正的 DLL 会赶在进程完成导入绑定之前，导致
   绑定到系统 DLL、甚至崩溃。现在改为**第一次被调用时才解析**（懒加载），加载器线程只负责
   枚举 `plugins\*.dll`。

导出表按系统 dsound 的**序号**编号（1..12），因为游戏是按序号 11 导入 `DirectSoundCreate8` 的；
名字导出也保留，方便别的程序按名字调用。

## 注意

- **不要同时装两个插件加载器**：如果恢复了 dinput8 版加载器，删掉 `dsound.dll` 和
  `dsound_orig.dll`，否则 `plugins\*.dll` 会被加载两次，插件钩子也会装两遍。
- Steam“验证游戏文件”会删掉这两个文件（不在清单里），验证后需要放回。
- 加载器只负责把 DLL 拉进进程；QTE 插件自己会等待游戏解包、校验特征后再挂钩子。

## 测试

`source_loader\tests\` 里有一个**静态导入** dsound 的小程序（和 re5dx9.exe 一样链接
`dsound.lib`），放在含 `dsound.dll`、`dsound_orig.dll`、`plugins\RE5QTE.dll` 的目录里运行，
结果：

```
static import DirectSoundEnumerateA -> 0x00000000     (DS_OK，转发正确)
=== RE5 dsound.dll plugin loader v2.0.0 (DllMain) ===
dsound_orig.dll loaded
loaded  RE5QTE.dll
plugins loaded=1
```

即：代理被选中、转发正常、插件被加载。
