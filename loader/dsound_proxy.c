/* RE5 plugin loader, dsound.dll proxy (v2).
 *
 * Why dsound
 * ----------
 * Plugins were loaded from plugins\ by a dinput8.dll proxy.  That file was replaced by RE5Fix's
 * dinput8 proxy, so plugins\ stopped being scanned.  A version.dll proxy never loaded either:
 * this process already has the system version.dll before it resolves the exe imports.
 * re5dx9.exe imports dsound.dll by *ordinal 11* (DirectSoundCreate8), which is a normal static
 * import and is not preloaded.
 *
 * Forwarding without recursion
 * ----------------------------
 * Loading "...\System32\dsound.dll" from a module that is itself "dsound.dll" returns this very
 * module, so every forwarder would jump into itself.  Two things fix that:
 *   - the real DLL is shipped next to the game as dsound_orig.dll (a copy of the 32-bit system
 *     DLL) and is loaded by that different name;
 *   - it is loaded lazily on the first forwarded call, never from DllMain, so the process can
 *     finish binding its imports to this proxy first.
 *
 * The export table keeps the system ordinals (see dsound.def), because the game imports
 * DirectSoundCreate8 as ordinal 11.
 *
 * Only one plugin loader may run: a named mutex guards the plugins\ scan.
 *
 * Freestanding: no CRT, only KERNEL32.dll is imported.
 */
typedef void* HMODULE;
typedef void* HANDLE;
typedef void* LPVOID;
typedef unsigned long DWORD;
typedef long HRESULT;
typedef int BOOL;
typedef unsigned int UINT;
typedef const char* LPCSTR;
typedef char* LPSTR;

typedef DWORD (__stdcall* ThreadProc)(LPVOID);
typedef HRESULT (__stdcall* Fn3)(void*, void*, void*);
typedef HRESULT (__stdcall* Fn2)(void*, void*);
typedef HRESULT (__stdcall* Fn10)(void*, void*, void*, void*, void*, void*, void*, void*, void*, void*);

typedef struct _FILETIME_S { DWORD lo, hi; } FILETIME_S;
typedef struct _WIN32_FIND_DATAA {
    DWORD dwFileAttributes;
    FILETIME_S ftCreationTime;
    FILETIME_S ftLastAccessTime;
    FILETIME_S ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    char cFileName[260];
    char cAlternateFileName[14];
} WIN32_FIND_DATAA;

__declspec(dllimport) HMODULE __stdcall LoadLibraryA(LPCSTR);
__declspec(dllimport) void*   __stdcall GetProcAddress(HMODULE, LPCSTR);
__declspec(dllimport) HANDLE  __stdcall FindFirstFileA(LPCSTR, WIN32_FIND_DATAA*);
__declspec(dllimport) BOOL    __stdcall FindNextFileA(HANDLE, WIN32_FIND_DATAA*);
__declspec(dllimport) BOOL    __stdcall FindClose(HANDLE);
__declspec(dllimport) HANDLE  __stdcall CreateThread(void*, unsigned long, ThreadProc, void*, DWORD, DWORD*);
__declspec(dllimport) void    __stdcall Sleep(DWORD);
__declspec(dllimport) BOOL    __stdcall DisableThreadLibraryCalls(HMODULE);
__declspec(dllimport) DWORD   __stdcall GetModuleFileNameA(HMODULE, LPSTR, DWORD);
__declspec(dllimport) HANDLE  __stdcall CreateFileA(LPCSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE);
__declspec(dllimport) BOOL    __stdcall WriteFile(HANDLE, const void*, DWORD, DWORD*, void*);
__declspec(dllimport) BOOL    __stdcall CloseHandle(HANDLE);
__declspec(dllimport) int     __stdcall lstrlenA(LPCSTR);
__declspec(dllimport) LPSTR   __stdcall lstrcpyA(LPSTR, LPCSTR);
__declspec(dllimport) LPSTR   __stdcall lstrcatA(LPSTR, LPCSTR);
__declspec(dllimport) DWORD   __stdcall GetLastError(void);
__declspec(dllimport) HANDLE  __stdcall CreateMutexA(void*, BOOL, LPCSTR);
__declspec(dllimport) BOOL    __stdcall CopyFileA(LPCSTR, LPCSTR, BOOL);
__declspec(dllimport) UINT    __stdcall GetSystemDirectoryA(LPSTR, UINT);

#define MAX_PATH 260
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 0x1u
#define CREATE_ALWAYS 2u
#define FILE_ATTRIBUTE_NORMAL 0x80u
#define FILE_ATTRIBUTE_DIRECTORY 0x10u
#define INVALID_HANDLE_VALUE ((HANDLE)(long)-1)
#define DLL_PROCESS_ATTACH 1u
#define ERROR_ALREADY_EXISTS 183u

static HMODULE g_self;
static HMODULE g_real;
static HANDLE g_log = INVALID_HANDLE_VALUE;
static char g_dir[MAX_PATH];
static int g_resolving;

static Fn3  g_fn_DirectSoundCreate;
static Fn3  g_fn_DirectSoundCreate8;
static Fn3  g_fn_DirectSoundCaptureCreate;
static Fn3  g_fn_DirectSoundCaptureCreate8;
static Fn2  g_fn_DirectSoundEnumerateA;
static Fn2  g_fn_DirectSoundEnumerateW;
static Fn2  g_fn_DirectSoundCaptureEnumerateA;
static Fn2  g_fn_DirectSoundCaptureEnumerateW;
static Fn10 g_fn_DirectSoundFullDuplexCreate;
static HRESULT (__stdcall* g_fn_DllCanUnloadNow)(void);
static Fn3  g_fn_DllGetClassObject;
static Fn2  g_fn_GetDeviceID;

/* ------------------------------- tiny log ------------------------------- */

static void raw(const char* s) {
    DWORD w = 0;
    if (g_log != INVALID_HANDLE_VALUE && s) WriteFile(g_log, s, (DWORD)lstrlenA(s), &w, 0);
}
static void line(const char* s) { raw(s); raw("\r\n"); }
static void line2(const char* a, const char* b) {
    char buf[MAX_PATH + 64];
    lstrcpyA(buf, a);
    lstrcatA(buf, b);
    line(buf);
}
static void line_num(const char* a, DWORD v) {
    char num[12]; int i = 0;
    char buf[128];
    if (!v) num[i++] = '0';
    while (v) { num[i++] = (char)('0' + (char)(v % 10u)); v /= 10u; }
    { int j = 0; char rev[12]; while (i) rev[j++] = num[--i]; rev[j] = 0; lstrcpyA(buf, a); lstrcatA(buf, rev); }
    line(buf);
}

static void log_open(void) {
    char path[MAX_PATH];
    lstrcpyA(path, g_dir);
    lstrcatA(path, "plugins\\RE5Loader.log");
    g_log = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, 0);
    if (g_log == INVALID_HANDLE_VALUE) {
        lstrcpyA(path, g_dir);
        lstrcatA(path, "RE5Loader.log");
        g_log = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, 0);
    }
}

/* ------------------------------- forwarding ------------------------------- */

static void resolve_real(void) {
    char path[MAX_PATH];
    if (g_real || g_resolving) return;
    g_resolving = 1;
    lstrcpyA(path, g_dir);
    lstrcatA(path, "dsound_orig.dll");
    g_real = LoadLibraryA(path);
    if (!g_real) {
        /* Self-install: copy the 32-bit system DLL next to the game as dsound_orig.dll. */
        char sys[MAX_PATH];
        UINT n = GetSystemDirectoryA(sys, MAX_PATH);
        if (n && n + 16 < MAX_PATH) {
            lstrcatA(sys, "\\dsound.dll");
            if (CopyFileA(sys, path, 0)) {
                line("copied system dsound.dll -> dsound_orig.dll");
                g_real = LoadLibraryA(path);
            }
        }
    }
    if (g_real) {
        g_fn_DirectSoundCreate            = (Fn3)GetProcAddress(g_real, "DirectSoundCreate");
        g_fn_DirectSoundCreate8           = (Fn3)GetProcAddress(g_real, "DirectSoundCreate8");
        g_fn_DirectSoundCaptureCreate     = (Fn3)GetProcAddress(g_real, "DirectSoundCaptureCreate");
        g_fn_DirectSoundCaptureCreate8    = (Fn3)GetProcAddress(g_real, "DirectSoundCaptureCreate8");
        g_fn_DirectSoundEnumerateA        = (Fn2)GetProcAddress(g_real, "DirectSoundEnumerateA");
        g_fn_DirectSoundEnumerateW        = (Fn2)GetProcAddress(g_real, "DirectSoundEnumerateW");
        g_fn_DirectSoundCaptureEnumerateA = (Fn2)GetProcAddress(g_real, "DirectSoundCaptureEnumerateA");
        g_fn_DirectSoundCaptureEnumerateW = (Fn2)GetProcAddress(g_real, "DirectSoundCaptureEnumerateW");
        g_fn_DirectSoundFullDuplexCreate  = (Fn10)GetProcAddress(g_real, "DirectSoundFullDuplexCreate");
        g_fn_DllCanUnloadNow              = (HRESULT(__stdcall*)(void))GetProcAddress(g_real, "DllCanUnloadNow");
        g_fn_DllGetClassObject            = (Fn3)GetProcAddress(g_real, "DllGetClassObject");
        g_fn_GetDeviceID                  = (Fn2)GetProcAddress(g_real, "GetDeviceID");
        line("dsound_orig.dll loaded");
    } else {
        line_num("dsound_orig.dll load error=", GetLastError());
    }
    g_resolving = 0;
}

HRESULT __stdcall DirectSoundCreate(void* a, void* b, void* c) {
    resolve_real(); return g_fn_DirectSoundCreate ? g_fn_DirectSoundCreate(a, b, c) : -1L;
}
HRESULT __stdcall DirectSoundCreate8(void* a, void* b, void* c) {
    resolve_real(); return g_fn_DirectSoundCreate8 ? g_fn_DirectSoundCreate8(a, b, c) : -1L;
}
HRESULT __stdcall DirectSoundCaptureCreate(void* a, void* b, void* c) {
    resolve_real(); return g_fn_DirectSoundCaptureCreate ? g_fn_DirectSoundCaptureCreate(a, b, c) : -1L;
}
HRESULT __stdcall DirectSoundCaptureCreate8(void* a, void* b, void* c) {
    resolve_real(); return g_fn_DirectSoundCaptureCreate8 ? g_fn_DirectSoundCaptureCreate8(a, b, c) : -1L;
}
HRESULT __stdcall DirectSoundEnumerateA(void* a, void* b) {
    resolve_real(); return g_fn_DirectSoundEnumerateA ? g_fn_DirectSoundEnumerateA(a, b) : -1L;
}
HRESULT __stdcall DirectSoundEnumerateW(void* a, void* b) {
    resolve_real(); return g_fn_DirectSoundEnumerateW ? g_fn_DirectSoundEnumerateW(a, b) : -1L;
}
HRESULT __stdcall DirectSoundCaptureEnumerateA(void* a, void* b) {
    resolve_real(); return g_fn_DirectSoundCaptureEnumerateA ? g_fn_DirectSoundCaptureEnumerateA(a, b) : -1L;
}
HRESULT __stdcall DirectSoundCaptureEnumerateW(void* a, void* b) {
    resolve_real(); return g_fn_DirectSoundCaptureEnumerateW ? g_fn_DirectSoundCaptureEnumerateW(a, b) : -1L;
}
HRESULT __stdcall DirectSoundFullDuplexCreate(void* a, void* b, void* c, void* d, void* e,
                                              void* f, void* g, void* h, void* i, void* j) {
    resolve_real();
    return g_fn_DirectSoundFullDuplexCreate ? g_fn_DirectSoundFullDuplexCreate(a, b, c, d, e, f, g, h, i, j) : -1L;
}
HRESULT __stdcall DllCanUnloadNow(void) {
    resolve_real(); return g_fn_DllCanUnloadNow ? g_fn_DllCanUnloadNow() : 1L;
}
HRESULT __stdcall DllGetClassObject(void* a, void* b, void* c) {
    resolve_real(); return g_fn_DllGetClassObject ? g_fn_DllGetClassObject(a, b, c) : -1L;
}
HRESULT __stdcall GetDeviceID(void* a, void* b) {
    resolve_real(); return g_fn_GetDeviceID ? g_fn_GetDeviceID(a, b) : -1L;
}

/* ------------------------------- plugin loading ------------------------------- */

static DWORD __stdcall LoaderThread(LPVOID unused) {
    WIN32_FIND_DATAA fd;
    char pattern[MAX_PATH];
    HANDLE h, mutex;
    DWORD count = 0;
    (void)unused;

    line("--- loader thread ---");
    mutex = CreateMutexA(0, 1, "RE5PluginLoader_v1");
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        line("another plugin loader already owns plugins\\; skipping");
        return 0;
    }
    Sleep(60);
    lstrcpyA(pattern, g_dir);
    lstrcatA(pattern, "plugins\\*.dll");
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        line("no plugins\\*.dll found");
    } else {
        do {
            char full[MAX_PATH];
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            lstrcpyA(full, g_dir);
            lstrcatA(full, "plugins\\");
            lstrcatA(full, fd.cFileName);
            if (LoadLibraryA(full)) {
                line2("loaded  ", fd.cFileName);
                count++;
            } else {
                line2("FAILED  ", fd.cFileName);
                line_num("  LoadLibrary error=", GetLastError());
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    line_num("plugins loaded=", count);
    return 0;
}

BOOL __stdcall DllMain(HMODULE mod, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        HANDLE t;
        DWORD n;
        g_self = mod;
        DisableThreadLibraryCalls(mod);
        n = GetModuleFileNameA(mod, g_dir, MAX_PATH);
        while (n > 0 && g_dir[n - 1] != '\\') n--;
        if (n < MAX_PATH) g_dir[n] = 0;
        log_open();
        line("=== RE5 dsound.dll plugin loader v2.0.1 (DllMain) ===");
        t = CreateThread(0, 0, LoaderThread, 0, 0, 0);
        if (t) CloseHandle(t);
    }
    return 1;
}
