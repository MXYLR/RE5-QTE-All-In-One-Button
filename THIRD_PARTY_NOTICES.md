# Third-party notices

## RE3Controls (MIT)

`plugin/re5_qte.c` 中的敌人/Boss DTI 常量表与 `InEnemyGrab` 判定思路来自 RE3Controls
（作者 palecvzhope / bananzaza，MIT 许可），仅用于识别“玩家正被敌人抓取”的状态。
未包含其源代码，仅使用从二进制中得到的常量与调用约定。

```
MIT License

Copyright (c) 2026 palecvzhope

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## 其他

- 本仓库不包含 RE5Fix、RE3Controls、MinHook 的二进制或源代码。
- `dist/dsound_orig.dll` 不是仓库内容：构建/安装时从系统 `SysWOW64\dsound.dll` 复制。
