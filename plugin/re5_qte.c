/* RE5 QTE v1.1.1 - one key, or no key at all (auto-counter), for every QTE prompt in Resident Evil 5 (DX9).
 *
 * Loaded as plugins\RE5QTE.dll by the dinput8.dll plugin loader that already ships with this
 * install. It is independent of RE5EnhancedMovement.dll, RE5PartnerAI.dll and of the
 * xinput1_3.dll RE3Controls build: nothing here touches their code, and they do not touch ours.
 *
 * What a "QTE" is in this build
 * ----------------------------
 * The engine drives every on-screen button prompt - doors, pick-ups, melee finishers and the
 * QTE presses in cutscenes - through sActionButton (one manager per player, 4 managers, stride
 * 0x2D0). Each manager owns two action slots; once a slot has a request it calls
 *
 *     ActionSlot::Check(request, pad, slot)        0x00766F00   (__thiscall, ret 8)
 *
 * once per frame, and a non-zero return means "the button the prompt wants is down". The caller
 * immediately consumes the request (sets request+0x0C = 1 and fires it), so Check is the single
 * authority on whether a prompt is answered.
 *
 * Check is called from exactly one place:
 *
 *     0x00767576   call 0x00766F00                  inside ButtonMgr::checkAll (0x00767390)
 *
 * and that call site is the hook. The five bytes are re-encoded as a call to QteCheck(), which
 * keeps the stock Check in its original position and calls it through 0x00766F00 - so an entry
 * hook that another plugin put on 0x00766F00 (RE5PartnerAI does exactly that) keeps working.
 *
 * Because the prompt carries its own "strict" flag (request+0x6C: any other button fails), the
 * plugin can tell a QTE prompt from an ordinary context prompt without a case table:
 *
 *     request+0x0C  u8   1 = already accepted (Check returns 0 at once; QteCheck mirrors that)
 *     request+0x20  u32  button type, index into the mask table at 0x00F58E50
 *     request+0x68  u32  stamp, tells a reused request object apart
 *     request+0x6C  u8   strict: set for QTE prompts, "any other button fails"
 *     request+0x70  u32  per-player "already answered" bits
 *     request+0x74  f32  remaining time / script timing value (logged, not used)
 *
 * With the key down and a strict prompt live, QteCheck returns 1 without running the stock
 * Check, which is exactly what a correct button press would do one frame later - and it cannot
 * fail the prompt for pressing "some other button" first, because the stock check never sees the
 * key. Non-QTE prompts are never affected. The game state is untouched: no code, no data of the
 * game is ever written beyond those five call bytes, and the plugin can put them back.
 *
 * Safety model
 * ------------
 *  - Nothing is patched until the unpacked image answers (the packer's entry point has run and
 *    the camera prologue at 0x0083A560 is readable), i.e. never during the integrity check.
 *  - The call site, its caller and the stock Check prologue are compared byte for byte first;
 *    on any mismatch the plugin logs the bytes and patches nothing.
 *  - The default INI only forces strict (QTE) prompts. ForceAll=1 exists as a fallback for a
 *    prompt that turns out to be flagged non-strict; it is off by default.
 *  - The plugin writes only the log file and the five call bytes; it never writes game data.
 */
#define RE5QTE_VERSION 197u

#include <stdint.h>
#include <stddef.h>

#if defined(_MSC_VER)
#  define STDCALL  __stdcall
#  define FASTCALL __fastcall
#  define K32(name)                       /* resolved against the real kernel32.lib */
#  define U32(name)
#else
#  error "build with MSVC (source_qte\\build_msvc.bat); the inline import names below are MSVC syntax"
#endif

int _fltused = 0;

typedef void* HANDLE; typedef void* HMODULE; typedef void* LPVOID;
typedef unsigned long DWORD; typedef int BOOL; typedef const char* LPCSTR;
typedef DWORD (STDCALL *ThreadProc)(LPVOID);

typedef struct _MBI {
    LPVOID BaseAddress; LPVOID AllocationBase; DWORD AllocationProtect;
    size_t RegionSize; DWORD State; DWORD Protect; DWORD Type;
} MBI;

typedef BOOL   (STDCALL *VirtualProtectFn)(LPVOID, size_t, DWORD, DWORD*);
typedef size_t (STDCALL *VirtualQueryFn)(const void*, MBI*, size_t);
typedef BOOL   (STDCALL *FlushInstructionCacheFn)(HANDLE, const void*, size_t);
typedef HANDLE (STDCALL *GetCurrentProcessFn)(void);
typedef HANDLE (STDCALL *CreateThreadFn)(void*, size_t, ThreadProc, void*, DWORD, DWORD*);
typedef void   (STDCALL *SleepFn)(DWORD);
typedef BOOL   (STDCALL *CloseHandleFn)(HANDLE);
typedef HANDLE (STDCALL *CreateFileAFn)(LPCSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE);
typedef BOOL   (STDCALL *ReadFileFn)(HANDLE, void*, DWORD, DWORD*, void*);
typedef BOOL   (STDCALL *WriteFileFn)(HANDLE, const void*, DWORD, DWORD*, void*);
typedef BOOL   (STDCALL *DisableThreadLibraryCallsFn)(HMODULE);
typedef DWORD  (STDCALL *GetTickCountFn)(void);
typedef DWORD  (STDCALL *GetCurrentProcessIdFn)(void);
typedef short  (STDCALL *GetAsyncKeyStateFn)(int);
typedef void*  (STDCALL *GetForegroundWindowFn)(void);
typedef DWORD  (STDCALL *GetWindowThreadProcessIdFn)(void*, DWORD*);

__declspec(dllimport) BOOL   STDCALL VirtualProtect(LPVOID, size_t, DWORD, DWORD*) K32("VirtualProtect");
__declspec(dllimport) size_t STDCALL VirtualQuery(const void*, MBI*, size_t) K32("VirtualQuery");
__declspec(dllimport) BOOL   STDCALL FlushInstructionCache(HANDLE, const void*, size_t) K32("FlushInstructionCache");
__declspec(dllimport) HANDLE STDCALL GetCurrentProcess(void) K32("GetCurrentProcess");
__declspec(dllimport) HANDLE STDCALL CreateThread(void*, size_t, ThreadProc, void*, DWORD, DWORD*) K32("CreateThread");
__declspec(dllimport) void   STDCALL Sleep(DWORD) K32("Sleep");
__declspec(dllimport) BOOL   STDCALL CloseHandle(HANDLE) K32("CloseHandle");
__declspec(dllimport) HANDLE STDCALL CreateFileA(LPCSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE) K32("CreateFileA");
__declspec(dllimport) BOOL   STDCALL ReadFile(HANDLE, void*, DWORD, DWORD*, void*) K32("ReadFile");
__declspec(dllimport) BOOL   STDCALL WriteFile(HANDLE, const void*, DWORD, DWORD*, void*) K32("WriteFile");
__declspec(dllimport) BOOL   STDCALL DisableThreadLibraryCalls(HMODULE) K32("DisableThreadLibraryCalls");
__declspec(dllimport) DWORD  STDCALL GetTickCount(void) K32("GetTickCount");
__declspec(dllimport) DWORD  STDCALL GetCurrentProcessId(void) K32("GetCurrentProcessId");
__declspec(dllimport) short  STDCALL GetAsyncKeyState(int) U32("GetAsyncKeyState");
__declspec(dllimport) void*  STDCALL GetForegroundWindow(void) U32("GetForegroundWindow");
__declspec(dllimport) DWORD  STDCALL GetWindowThreadProcessId(void*, DWORD*) U32("GetWindowThreadProcessId");

static VirtualProtectFn pVirtualProtect;
static VirtualQueryFn pVirtualQuery;
static FlushInstructionCacheFn pFlushInstructionCache;
static GetCurrentProcessFn pGetCurrentProcess;
static CreateThreadFn pCreateThread;
static SleepFn pSleep;
static CloseHandleFn pCloseHandle;
static CreateFileAFn pCreateFileA;
static ReadFileFn pReadFile;
static WriteFileFn pWriteFile;
static DisableThreadLibraryCallsFn pDisableThreadLibraryCalls;
static GetAsyncKeyStateFn pGetAsyncKeyState;
static GetForegroundWindowFn pGetForegroundWindow;
static GetWindowThreadProcessIdFn pGetWindowThreadProcessId;

#define MEM_COMMIT 0x00001000u
#define PAGE_NOACCESS 0x00000001u
#define PAGE_GUARD 0x00000100u
#define PAGE_EXECUTE_READWRITE 0x40u
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 0x1u
#define OPEN_EXISTING 3u
#define CREATE_ALWAYS 2u
#define FILE_ATTRIBUTE_NORMAL 0x80u
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define DLL_PROCESS_ATTACH 1u

/* ---- addresses in the unpacked image (base 0x400000, no ASLR) ----
 * The offsets are fixed for this build; RE5QTE_IMAGE_BASE only exists so the offline harness
 * in tests\ can compile the same source against a synthetic image at a free address. */
#ifndef RE5QTE_IMAGE_BASE
#define RE5QTE_IMAGE_BASE   0x00400000u
#endif
#define FN_ACTION_CHECK     (RE5QTE_IMAGE_BASE + 0x00366F00u)  /* ActionSlot::Check, __thiscall, ret 8 */
#define FN_BUTTONMGR_UPDATE (RE5QTE_IMAGE_BASE + 0x00367390u)  /* sActionButton::ButtonMgr::checkAll   */
#define CALL_ACTION_CHECK   (RE5QTE_IMAGE_BASE + 0x00367576u)  /* the only "call 0x00766F00"           */
#define FN_GAME_PROLOGUE    (RE5QTE_IMAGE_BASE + 0x0043A560u)  /* camera SetLookAt, after unpacking    */

/* Request field offsets (see the header comment). */
#define REQ_STATE   0x0Cu
#define REQ_TYPE    0x20u
#define REQ_STAMP   0x68u
#define REQ_STRICT  0x6Cu
#define REQ_USED    0x70u
#define REQ_TIME    0x74u
#define MASK_TABLE  (RE5QTE_IMAGE_BASE + 0x00B58E50u)
#define GLYPH_TABLE (RE5QTE_IMAGE_BASE + 0x00B590B8u)   /* u8[32]: type -> UI icon id */

/* ---- state ---- */
static int g_enabled = 1;      /* master switch                          */
static int g_noStrictFail = 1; /* patch out the "wrong button fails the prompt" rule */
static int g_padInput = 0;      /* make the QTE handlers read the passed pad */
static int g_qteKeys = 1;       /* answer strict QTE key queries with the player key   */
static uint32_t g_qteKeyTick;   /* a strict prompt was checked this recently           */
static uint32_t g_swReq, g_swStamp, g_swTick, g_strictProbes, g_strictPatched;
static int g_forceAll = 0;     /* 1 = the key answers every prompt, not only QTE */
static int g_auto = 0;         /* 0 = manual key (default), 1 = auto-answer */
static int g_autoGrab = 1;     /* auto while the player is in an enemy event act */
static int g_autoCombo = 1;    /* prompts that need two buttons at once               */
static int g_autoSpecial = 1;  /* mask == 0: stick wiggle / special input prompts    */
static int g_autoStrict = 0;   /* auto for requests that carry the "strict" byte  */
static int g_tapMode = 0;      /* 0 = hold, 1 = only the key's press edge */
static int g_fgOnly = 1;       /* only when this process owns the focus  */
static int g_logPrompts = 1;   /* write every distinct prompt seen       */
static uint32_t g_key = 0x45;  /* E                                      */
static uint32_t g_maxPromptLog = 600u;
static uint32_t g_qteGlyph = 69u;   /* icon id written over QTE prompt types; 0 = leave stock */
static uint32_t g_qteSingle = 1u;   /* answer strict QTEs with the key (native mask kept) */
static uint32_t g_extraTypes;      /* extra prompt types the key answers (ExtraTypes=) */
static uint32_t g_eOnlyTypes;      /* extra types whose native button is blocked (EOnlyTypes=) */
static uint32_t g_entryTypes;      /* non-strict prompt types that open a QTE chain (EntryTypes=) */

static int g_installed;
static int g_keyWasDown;
static uint32_t g_tapUntil;
static uint32_t g_keyLogs;
static uint32_t g_checks, g_promptsSeen, g_strictSeen, g_forced, g_autoForced, g_promptLogs;
static uint32_t g_entryProbes;
/* Entry hand-off window: after the chain-entry answer the scene keeps polling its own input for a
 * few frames.  The one-frame injection answers ActionSlot::Check but not that poll, so the event
 * aborts.  Mask/slot/deadline keep the entry's native button on every input copy like a real hold.
 * The window is FIXED: v1.8.9 extended it while the key stayed down, and the one stalled E entry
 * held the mask 11438 ms with no strict stage ever appearing; every chain that advanced did so
 * within ~300 ms of the hold ending. */
static uint32_t g_entryHoldMask, g_entryHoldSlot = 0xFFFFFFFFu, g_entryHoldUntil, g_entryHoldStart;
#define ENTRY_HOLD_MS 300u
static uint32_t g_holdLogs;
/* Entry answer timing: the entry prompt appears while the player is still in the hit reaction
 * (act 0x1b-0x1e / 0x00) and the scene drops the event one frame after a command submitted then
 * (every stalled attempt was answered in that state; the one that opened the chain was answered at
 * act 0x02).  A press arms a wait, the answer fires on the first ready frame.  v1.9.4 tried treating
 * act 0x00 as ready after 150 ms; both answers below 600 ms prompt age (266 and 329 ms) started the
 * event only for the scene to end it on the next probe, while every answer at >= 797 ms opened the
 * chain.  So act 0x00 keeps waiting for the cap: the scene itself needs roughly that long after the
 * prompt before it accepts the entry.  press= in the [entry] log measures the press-to-answer wait. */
static uint32_t g_entryArmReq, g_entryArmStamp, g_entryArmStart, g_entryArmedAt;
static int g_entryArmed, g_entryWaitLogged;
static uint32_t g_entryWaitLogs, g_entryBlockLogs, g_extraBlockLogs;
#define ENTRY_ACT_WAIT_MS 600u

typedef int (FASTCALL *CheckFn)(uint32_t self, void* edx, uint32_t pad, uint32_t slot);
static CheckFn g_origCheck = (CheckFn)FN_ACTION_CHECK;

/* ------------------------------- tiny formatting ------------------------------- */

static size_t slen(const char* s) { size_t n = 0; while (s && s[n]) n++; return n; }

static char* puts1(char* p, const char* s) { while (s && *s) *p++ = *s++; return p; }

static char* fmt_u(char* p, uint32_t v) {
    char t[10]; int n = 0;
    if (!v) { *p++ = '0'; return p; }
    while (v) { t[n++] = (char)('0' + (v % 10u)); v /= 10u; }
    while (n) *p++ = t[--n];
    return p;
}

static char* fmt_hex(char* p, uint32_t v, int digits) {
    static const char h[] = "0123456789abcdef";
    int i;
    for (i = digits - 1; i >= 0; i--) *p++ = h[(v >> (4 * i)) & 15u];
    return p;
}

/* ------------------------------- memory helpers ------------------------------- */

static int span_readable(uintptr_t at, size_t count) {
    MBI m; uintptr_t end, limit; size_t take;
    if (!pVirtualQuery) return 0;
    while (count) {
        if (pVirtualQuery((const void*)at, &m, sizeof(m)) < sizeof(m)) return 0;
        if (m.State != MEM_COMMIT || (m.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return 0;
        end = (uintptr_t)m.BaseAddress + (uintptr_t)m.RegionSize;
        if (end <= at || at < (uintptr_t)m.BaseAddress) return 0;
        limit = end - at; take = count < limit ? count : limit;
        at += take; count -= take;
    }
    return 1;
}

static uint8_t  rd8(uintptr_t at)  { return *(const volatile uint8_t*)at; }
static uint16_t rd16(uintptr_t at) { return *(const volatile uint16_t*)at; }
static uint32_t rd32(uintptr_t at) { return *(const volatile uint32_t*)at; }

static int memeq(uintptr_t p, const uint8_t* x, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) if (rd8(p + i) != x[i]) return 0;
    return 1;
}

static int write_mem(uintptr_t at, const void* src, size_t n) {
    DWORD old = 0, tmp = 0; size_t i; const uint8_t* s = (const uint8_t*)src;
    if (!pVirtualProtect || !pFlushInstructionCache || !pGetCurrentProcess) return 0;
    if (!pVirtualProtect((void*)at, n, PAGE_EXECUTE_READWRITE, &old)) return 0;
    for (i = 0; i < n; i++) *(volatile uint8_t*)(at + i) = s[i];
    pVirtualProtect((void*)at, n, old, &tmp);
    pFlushInstructionCache(pGetCurrentProcess(), (void*)at, n);
    for (i = 0; i < n; i++) if (rd8(at + i) != s[i]) return 0;
    return 1;
}

static int patch_call(uintptr_t at, void* fn) {
    uint8_t p[5];
    intptr_t rel = (intptr_t)fn - (intptr_t)(at + 5);
    if (rel < (intptr_t)0x80000000LL || rel > (intptr_t)0x7FFFFFFFLL) return 0;  /* E8 is rel32 */
    p[0] = 0xE8;
    p[1] = (uint8_t)(rel & 0xFF); p[2] = (uint8_t)((rel >> 8) & 0xFF);
    p[3] = (uint8_t)((rel >> 16) & 0xFF); p[4] = (uint8_t)((rel >> 24) & 0xFF);
    return write_mem(at, p, 5);
}

static uintptr_t call_target(uintptr_t at) {
    int32_t rel;
    if (rd8(at) != 0xE8) return 0;
    rel = (int32_t)rd32(at + 1);
    return at + 5 + (intptr_t)rel;
}

/* ------------------------------- log ------------------------------- */

static HANDLE g_log = INVALID_HANDLE_VALUE;

static void raw(const char* s) {
    DWORD w = 0;
    if (g_log != INVALID_HANDLE_VALUE && s) pWriteFile(g_log, s, (DWORD)slen(s), &w, 0);
}
static void log_line(const char* s) { raw(s); raw("\r\n"); }

static void log_open(void) {
    if (!pCreateFileA) return;
    g_log = pCreateFileA("plugins\\RE5QTE.log", GENERIC_WRITE, FILE_SHARE_READ, 0,
                         CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (g_log == INVALID_HANDLE_VALUE)
        g_log = pCreateFileA("RE5QTE.log", GENERIC_WRITE, FILE_SHARE_READ, 0,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
}

static void log_u(const char* key, uint32_t v) {
    char b[128], *p = b;
    p = puts1(p, key); p = fmt_u(p, v); *p = 0; log_line(b);
}

/* ------------------------------- configuration ------------------------------- */

static uint32_t parse_u(const char* v, uint32_t n) {
    uint32_t i = 0, x = 0;
    while (i < n && (v[i] == ' ' || v[i] == '\t')) i++;
    if (i + 1 < n && v[i] == '0' && (v[i + 1] == 'x' || v[i + 1] == 'X')) {
        i += 2;
        for (; i < n; i++) {
            char c = v[i]; uint32_t d;
            if (c >= '0' && c <= '9') d = (uint32_t)(c - '0');
            else if (c >= 'a' && c <= 'f') d = (uint32_t)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') d = (uint32_t)(c - 'A' + 10);
            else break;
            x = (x << 4) | d;
        }
        return x;
    }
    for (; i < n; i++) {
        char c = v[i];
        if (c < '0' || c > '9') break;
        x = x * 10u + (uint32_t)(c - '0');
    }
    return x;
}

/* "E", "e", "1".."9", "0x45", "F1".."F12" */
static uint32_t key_from_name(const char* v, uint32_t n) {
    uint32_t i;
    char c;
    while (n && (v[0] == ' ' || v[0] == '\t')) { v++; n--; }
    while (n && (v[n - 1] == ' ' || v[n - 1] == '\t' || v[n - 1] == '\r')) n--;
    if (!n) return 0;
    c = v[0];
    if (n == 1) {
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if (c >= 'A' && c <= 'Z') return (uint32_t)(unsigned char)c;
        if (c >= '0' && c <= '9') return (uint32_t)(unsigned char)c;
    }
    if (c == '0' && n > 2 && (v[1] == 'x' || v[1] == 'X')) return parse_u(v, n);
    if ((c == 'F' || c == 'f') && n >= 2) {
        uint32_t k = parse_u(v + 1, n - 1);
        if (k >= 1 && k <= 12) return 0x6Fu + k;   /* VK_F1 = 0x70 */
        return 0;
    }
    for (i = 0; i < n; i++) if (v[i] < '0' || v[i] > '9') return 0;
    (void)i;
    return parse_u(v, n);
}

static int key_eq(const char* a, uint32_t n, const char* lit) {
    uint32_t i;
    for (i = 0; i < n; i++) {
        char c = a[i], l = lit[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (l >= 'A' && l <= 'Z') l = (char)(l - 'A' + 'a');
        if (!lit[i] || c != l) return 0;
    }
    return lit[n] == 0;
}

static int g_section;   /* 0 = [General], 1 = [Auto] */

/* "9,11, 15" -> bits in *mask; unknown tokens end the list. */
static void parse_type_list(const char* v, uint32_t vn, uint32_t* mask) {
    uint32_t i = 0;
    while (i < vn) {
        uint32_t id = 0;
        int seen = 0;
        while (i < vn && (v[i] == ' ' || v[i] == ',' || v[i] == '\t')) i++;
        while (i < vn && v[i] >= '0' && v[i] <= '9') {
            id = id * 10u + (uint32_t)(v[i] - '0');
            i++;
            seen = 1;
        }
        if (seen && id < 32u) *mask |= 1u << id;
        if (!seen) break;
    }
}

static void config_line(const char* s, uint32_t n) {
    uint32_t i, eq = 0, eqRaw = 0, keyEnd = 0, k = 0;
    const char* v; uint32_t vn;
    while (n && (s[0] == ' ' || s[0] == '\t')) { s++; n--; }
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) n--;
    if (!n || s[0] == ';' || s[0] == '#') return;
    if (s[0] == '[') {
        const char* name = s + 1;
        uint32_t len = n > 1 ? n - 1 : 0;
        if (len && name[len - 1] == ']') len--;
        g_section = key_eq(name, len, "Auto") ? 1 : 0;
        return;
    }
    for (i = 0; i < n; i++) if (s[i] == '=') { eqRaw = i; eq = i; break; }
    if (!eqRaw) return;
    while (eq > 0 && (s[eq - 1] == ' ' || s[eq - 1] == '\t')) eq--;
    keyEnd = eq;
    v = s + eqRaw + 1; vn = n - eqRaw - 1;
    while (vn && (v[0] == ' ' || v[0] == '\t')) { v++; vn--; }
    while (vn && (v[vn - 1] == ' ' || v[vn - 1] == '\t' || v[vn - 1] == '\r')) vn--;
    (void)k;
    if (g_section == 1) {
        if (key_eq(s, keyEnd, "Enabled"))      g_auto = parse_u(v, vn) != 0;
        else if (key_eq(s, keyEnd, "Grab"))    g_autoGrab = parse_u(v, vn) != 0;
        else if (key_eq(s, keyEnd, "Combo"))   g_autoCombo = parse_u(v, vn) != 0;
        else if (key_eq(s, keyEnd, "Strict"))  g_autoStrict = parse_u(v, vn) != 0;
        else if (key_eq(s, keyEnd, "Special")) g_autoSpecial = parse_u(v, vn) != 0;
        else if (key_eq(s, keyEnd, "Scope")) {  /* legacy alias */
            const int all = !key_eq(v, vn, "Enemy");
            g_autoGrab = 1; g_autoCombo = 1; g_autoSpecial = 1; g_autoStrict = all ? 1 : 0;
        }
        return;
    }
    if (key_eq(s, keyEnd, "Enabled"))      g_enabled = parse_u(v, vn) != 0;
    else if (key_eq(s, keyEnd, "Key"))     { uint32_t kk = key_from_name(v, vn); if (kk) g_key = kk; }
    else if (key_eq(s, keyEnd, "Mode"))    g_tapMode = key_eq(v, vn, "Tap") || key_eq(v, vn, "edge") ? 1 : 0;
    else if (key_eq(s, keyEnd, "ForceAll")) g_forceAll = parse_u(v, vn) != 0;
    else if (key_eq(s, keyEnd, "ForegroundOnly")) g_fgOnly = parse_u(v, vn) != 0;
    else if (key_eq(s, keyEnd, "Log"))     g_logPrompts = parse_u(v, vn) != 0;
    else if (key_eq(s, keyEnd, "MaxPromptLog")) g_maxPromptLog = parse_u(v, vn);
    else if (key_eq(s, keyEnd, "Glyph"))    g_qteGlyph = parse_u(v, vn);
    else if (key_eq(s, keyEnd, "SingleKey")) g_qteSingle = parse_u(v, vn) != 0;
    else if (key_eq(s, keyEnd, "NoStrictFail")) g_noStrictFail = parse_u(v, vn) != 0;
    else if (key_eq(s, keyEnd, "PadInput")) g_padInput = parse_u(v, vn) != 0;
    else if (key_eq(s, keyEnd, "QteKeys")) g_qteKeys = parse_u(v, vn) != 0;
    else if (key_eq(s, keyEnd, "ExtraTypes")) parse_type_list(v, vn, &g_extraTypes);
    else if (key_eq(s, keyEnd, "EOnlyTypes")) parse_type_list(v, vn, &g_eOnlyTypes);
    else if (key_eq(s, keyEnd, "EntryTypes")) parse_type_list(v, vn, &g_entryTypes);
}

static void config_load(void) {
    static char buf[8192];
    HANDLE h = INVALID_HANDLE_VALUE;
    DWORD got = 0;
    uint32_t i, start;
    if (!pCreateFileA || !pReadFile) return;
    h = pCreateFileA("RE5QTE.ini", GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING,
                     FILE_ATTRIBUTE_NORMAL, 0);
    if (h == INVALID_HANDLE_VALUE)
        h = pCreateFileA("plugins\\RE5QTE.ini", GENERIC_READ, FILE_SHARE_READ, 0,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (h == INVALID_HANDLE_VALUE) { log_line("[cfg] RE5QTE.ini not found, defaults used (Key=E, hold)"); return; }
    if (!pReadFile(h, buf, sizeof(buf) - 1u, &got, 0)) { pCloseHandle(h); return; }
    pCloseHandle(h);
    buf[got] = 0;
    start = 0;
    for (i = 0; i <= got; i++) {
        if (i == got || buf[i] == '\n') {
            uint32_t n = i - start;
            if (n && buf[start + n - 1] == '\r') n--;
            config_line(buf + start, n);
            start = i + 1;
        }
    }
    if (g_maxPromptLog > 2000u) g_maxPromptLog = 2000u;
}

/* ------------------------------- key ------------------------------- */

static int key_down(void) {
    if (!pGetAsyncKeyState || !g_key) return 0;
    return (pGetAsyncKeyState((int)g_key) & 0x8000) != 0;
}

static int foreground_is_us(void) {
    void* w; DWORD pid = 0;
    if (!pGetForegroundWindow || !pGetWindowThreadProcessId) return 1;
    w = pGetForegroundWindow();
    if (!w) return 1;
    pGetWindowThreadProcessId(w, &pid);
    return pid == GetCurrentProcessId();
}

/* One poll per millisecond: checkAll runs both action slots in the same frame, and a press edge must
 * stay visible to the check that answers the prompt instead of being eaten by the other slot. */
static uint32_t g_keyPollTick;

/* A press/release trace, so a test log can tell "E was down while the prompt was on screen but
 * nothing answered" from "the player did not press E". */
static void log_key(int down, uint32_t now) {
    char b[64], *p = b;
    if (g_keyLogs >= 300u) return;
    g_keyLogs++;
    p = puts1(p, "[key] state="); p = puts1(p, down ? "down" : "up");
    p = puts1(p, " t=0x"); p = fmt_hex(p, now, 8);
    *p = 0; log_line(b);
}

static void key_poll(void) {
    const uint32_t now = GetTickCount();
    int down;
    if (now == g_keyPollTick) return;
    g_keyPollTick = now;
    down = (!g_fgOnly || foreground_is_us()) ? key_down() : 0;
    if (down != g_keyWasDown) log_key(down, now);
    if (down && !g_keyWasDown) g_tapUntil = now + 50u;
    g_keyWasDown = down;
}

/* Press edge that survives the two action slots of one frame: a tap counts for 50 ms. */
static int key_fires(void) {
    key_poll();
    if (g_tapMode) return (int32_t)(GetTickCount() - g_tapUntil) < 0;
    return g_keyWasDown;
}

/* ------------------------------- prompt logging ------------------------------- */

/* The same request is checked every frame. The old code compared only with the previous line,
 * so two alternating slots re-logged forever and ate the whole budget before the first QTE. */
static uint32_t g_seenReq[32], g_seenStamp[32];
static uint32_t g_seenNext;

static int prompt_seen_before(uint32_t req, uint32_t stamp) {
    int i;
    for (i = 0; i < 32; i++) if (g_seenReq[i] == req && g_seenStamp[i] == stamp) return 1;
    g_seenReq[g_seenNext & 31u] = req;
    g_seenStamp[g_seenNext & 31u] = stamp;
    g_seenNext++;
    return 0;
}

static void log_prompt(uint32_t req, uint32_t stamp, uint32_t type, uint32_t mask,
                       uint32_t strict, uint32_t state, uint32_t used, uint32_t slot,
                       uint32_t cat, uint32_t kind, uint32_t act, uint32_t ev) {
    char b[320], *p = b;
    if (prompt_seen_before(req, stamp)) return;
    if (!g_logPrompts || g_promptLogs >= g_maxPromptLog) return;
    g_promptLogs++;
    p = puts1(p, "[prompt] req=0x"); p = fmt_hex(p, req, 8);
    p = puts1(p, " stamp=0x");       p = fmt_hex(p, stamp, 8);
    p = puts1(p, " type=");          p = fmt_u(p, type);
    p = puts1(p, " mask=0x");        p = fmt_hex(p, mask, 8);
    p = puts1(p, " strict=");        p = fmt_u(p, strict);
    p = puts1(p, " state=");         p = fmt_u(p, state);
    p = puts1(p, " used=0x");        p = fmt_hex(p, used, 8);
    p = puts1(p, " slot=");          p = fmt_u(p, slot);
    p = puts1(p, " cat=");           p = fmt_u(p, cat);
    p = puts1(p, " kind=");          p = fmt_u(p, kind);
    p = puts1(p, " act=0x");         p = fmt_hex(p, act, 8);
    p = puts1(p, " ev=");            p = fmt_u(p, ev);
    p = puts1(p, " t=0x");           p = fmt_hex(p, rd32(req + REQ_TIME), 8);
    p = puts1(p, " g=");             p = fmt_u(p, (type < 32u) ? rd8(GLYPH_TABLE + type) : 0u);
    *p = 0; log_line(b);
}

static void log_force(uint32_t slot, uint32_t type, uint32_t req, uint32_t mask) {
    char b[128], *p = b;
    p = puts1(p, "[force] slot="); p = fmt_u(p, slot);
    p = puts1(p, " type=");        p = fmt_u(p, type);
    p = puts1(p, " req=0x");       p = fmt_hex(p, req, 8);
    p = puts1(p, " mask=0x");      p = fmt_hex(p, mask, 8);
    p = puts1(p, " key=0x");       p = fmt_hex(p, g_key, 2);
    *p = 0; log_line(b);
}

/* Auto-counter: which prompt is being answered with no key, and why. */
static void log_auto(uint32_t slot, uint32_t type, uint32_t req, const char* why) {
    char b[128], *p = b;
    p = puts1(p, "[auto] slot="); p = fmt_u(p, slot);
    p = puts1(p, " type=");       p = fmt_u(p, type);
    p = puts1(p, " req=0x");      p = fmt_hex(p, req, 8);
    p = puts1(p, " why=");        p = puts1(p, why);
    *p = 0; log_line(b);
}

/* Why the entry's native button stopped being re-injected: a strict stage took over, or the
 * deadline passed with no strict stage.  The second case is the "chain never opened" signature. */
static void log_hold_end(const char* why, uint32_t now) {
    char b[128], *p = b;
    if (g_holdLogs >= 80u) return;
    g_holdLogs++;
    p = puts1(p, "[hold] end="); p = puts1(p, why);
    p = puts1(p, " mask=0x"); p = fmt_hex(p, g_entryHoldMask, 8);
    p = puts1(p, " held="); p = fmt_u(p, now - g_entryHoldStart);
    *p = 0; log_line(b);
}

/* Answer logs once per (request, stamp) per distinct stock result: a stage that is answered on
 * every call would otherwise spend the probe budget in a few frames. */
static uint32_t g_ansReq[16], g_ansStamp[16], g_ansRes[16], g_ansNext;
static int answer_new(uint32_t req, uint32_t stamp, uint32_t res) {
    uint32_t i;
    for (i = 0; i < 16u; i++) {
        if (g_ansReq[i] == req && g_ansStamp[i] == stamp) {
            if (g_ansRes[i] == res) return 0;
            g_ansRes[i] = res;
            return 1;
        }
    }
    g_ansReq[g_ansNext & 15u] = req;
    g_ansStamp[g_ansNext & 15u] = stamp;
    g_ansRes[g_ansNext & 15u] = res;
    g_ansNext++;
    return 1;
}

/* -------------------------------------------------------------------------------------------
 * Scope=Enemy: the player is inside an enemy-driven event act (a grab or a boss hold/attack).
 *
 * While an enemy holds the player, the game puts him into an event action (action id
 * 0x80000000) whose cEventAct object lives at character+0x10F8 / +0x1100 and whose +0xC is the
 * object that drove the act. If that source is an enemy or a boss, any strict prompt on screen
 * belongs to that enemy interaction - exactly the grab/counter QTE this mode is for. Story QTEs
 * come from the scene/event system instead, so they stay manual.
 *
 * Portions derived from RE3Controls (c) bananzaza, MIT licence: src/melee.cpp InEnemyGrab(),
 * DtiOf()/KindOf() and the enemy/boss/kill DTI tables.
 * ------------------------------------------------------------------------------------------- */
#define SLOT_MGR_PTR  0x011B27DCu   /* -> player manager; +0x24 + slot*4 = pawn   */
#define CH_ACTION_ID  0x10E0u
#define CH_EVENT_A    0x10F8u
#define CH_EVENT_B    0x1100u
#define ACT_OWNER     0x08u        /* the player the act belongs to              */
#define ACT_SRC       0x0Cu        /* the object that started it                 */
#define EVENT_ACTION  0x80000000u
#define PLAGA_EAT     0x0103BAA4u

static const uint32_t kDtiEnemy = 0x011F4F4Cu;
static const uint32_t kDtiEnemyPart[] = { 0x011EBE10u, 0x011EC7FCu };  /* tentacle, tongue */
/* Bosses and story characters: Wesker em80/em81/ema2, em82-84 and the core, em87, em8c, em8e,
 * uEmNpc, Uroboros em40/41 and parts, em43/44, the giant em45 and em46 + tentacles. */
static const uint32_t kDtiBoss[] = {
    0x011F0498u, 0x011F1260u, 0x011F19A4u, 0x011F1F14u, 0x011F29A0u, 0x011F2AC0u,
    0x011F2B2Cu, 0x011F2EB0u, 0x011F4C10u, 0x011ECE74u, 0x011ED360u, 0x011ED72Cu,
    0x011ED750u, 0x011EDF50u, 0x011EE3B4u, 0x011EE880u, 0x011EEB74u, 0x011EEBE0u,
    0x011EED08u, 0x011F3018u, 0x011F3234u, 0x011F32A0u, 0x011F3498u, 0x011F35DCu,
    0x011F3648u, 0x011F3864u
};
/* Kill grabs: no counter prompt to answer. */
static const uint32_t kKillActs[] = {
    0x00F74210u, 0x00F756E0u, 0x00F7570Cu, 0x00F76F44u, 0x00F78B70u,
    0x00F7A6CCu, 0x00F80EF4u, 0x00F92DD4u, 0x00F88030u, 0x00F8805Cu
};

static uint32_t rd32s(uintptr_t at) { return span_readable(at, 4) ? rd32(at) : 0; }

static uint32_t PlayerPawn(uint32_t slot) {
    uint32_t mgr, pawn;
    if (slot > 3) return 0;
    mgr = rd32s(SLOT_MGR_PTR);
    if (!mgr) return 0;
    pawn = rd32s(mgr + 0x24u + slot * 4u);
    if (!pawn || !span_readable(pawn, 0x3000u)) return 0;
    return pawn;
}

/* The action the player must leave before the chain-entry command is submitted: the hit reaction
 * (0x1b-0x1e) and act 0x00.  A command sent while one of these runs starts the event only for the
 * scene to end it on the next probe.  act 0x00 is also idle; the caller lets the cap cover it. */
static int EntryActReady(uint32_t act) {
    if (act == 0u) return 0;
    if (act >= 0x1Bu && act <= 0x1Eu) return 0;
    return 1;
}

/* ch+0x2DA8: 0 = this player's own pad, 1 = AI, 2 = driven by the network partner. Auto-counter
 * only answers a locally controlled player's prompt; an AI or a remote partner keeps its own.
 * Unknown layout (or the offline harness, where the manager is not mapped) counts as local. */
static int LocalPlayer(uint32_t slot) {
    uint32_t pawn = PlayerPawn(slot);
    if (!pawn) return 1;
    return rd32(pawn + 0x2DA8u) == 0;
}

static uint32_t DtiOf(uint32_t obj) {
    uint32_t vt, fn;
    if (!obj || !span_readable(obj, 4)) return 0;
    vt = rd32(obj);
    if (!vt || !span_readable(vt + 0x10u, 4)) return 0;
    fn = rd32(vt + 0x10u);
    if (!fn || !span_readable(fn, 6) || rd8(fn) != 0xB8 || rd8(fn + 5) != 0xC3) return 0;
    return rd32(fn + 1);
}

static int KindOf(uint32_t dti, uint32_t base) {
    int i;
    for (i = 0; dti && i < 32; i++) {
        if (dti == base) return 1;
        if (!span_readable(dti + 0x10u, 4)) return 0;
        dti = rd32(dti + 0x10u);
    }
    return 0;
}

/* What kind of event act the player is in:
 *   0 none, 1 enemy, 2 enemy part (tentacle/tongue), 3 boss, 4 kill act, 5 plaga eat (dying) */
static int EventClass(uint32_t pawn, uint32_t* outEv, uint32_t* outSrc, uint32_t* outDti) {
    uint32_t ev = 0, act, src = 0, dti = 0;
    int i, n, cls = 0;
    if (outEv) *outEv = 0;
    if (outSrc) *outSrc = 0;
    if (outDti) *outDti = 0;
    if (!pawn || rd32(pawn + CH_ACTION_ID) != EVENT_ACTION) return 0;
    {
        const uint32_t offs[2] = { CH_EVENT_A, CH_EVENT_B };
        for (i = 0; i < 2; i++) {
            uint32_t p = rd32s(pawn + offs[i]);
            if (p && span_readable(p, 0x1Cu) && rd32(p + ACT_OWNER) == pawn) { ev = p; break; }
        }
    }
    if (!ev) return 0;
    act = rd32(ev);
    n = (int)(sizeof(kKillActs) / sizeof(kKillActs[0]));
    for (i = 0; i < n; i++) if (act == kKillActs[i]) cls = 4;
    if (!cls && act == PLAGA_EAT && (rd32(pawn + 0x2D98u) & 2u)) cls = 5;
    if (!cls) {
        src = rd32(ev + ACT_SRC);
        dti = DtiOf(src);
        if (KindOf(dti, kDtiEnemy)) cls = 1;
        else {
            n = (int)(sizeof(kDtiEnemyPart) / sizeof(kDtiEnemyPart[0]));
            for (i = 0; i < n && !cls; i++) if (KindOf(dti, kDtiEnemyPart[i])) cls = 2;
            n = (int)(sizeof(kDtiBoss) / sizeof(kDtiBoss[0]));
            for (i = 0; i < n && !cls; i++) if (KindOf(dti, kDtiBoss[i])) cls = 3;
        }
    }
    if (outEv) *outEv = ev;
    if (outSrc) *outSrc = src;
    if (outDti) *outDti = dti;
    return cls;
}

/* Scripted scene action: the player is locked into the event action. */
static int PlayerInEventAct(uint32_t slot) {
    const uint32_t pawn = PlayerPawn(slot);
    return pawn && rd32(pawn + CH_ACTION_ID) == 0x80000000u;
}

static int PlayerInEnemyEvent(uint32_t slot) {
    const int cls = EventClass(PlayerPawn(slot), 0, 0, 0);
    return cls == 1 || cls == 2 || cls == 3;
}

/* ------------------------------- pad injection -------------------------------
 * Some scripted QTEs (Wesker's scenes) do not consume the ActionSlot result: the scene polls the
 * pad/character buttons itself.  Faking Check's return value alone does nothing there, so when the
 * key answers such a prompt its mask is also written into the pads for that frame.  The game
 * rebuilds the pads every frame, so nothing sticks.
 */
#define SPAD_PTR    0x01249C40u
#define SPAD_PADS   0x34u
#define SPAD_STRIDE 0x2C0u
#define SPAD_ON     0x15Cu
#define SPAD_TYPE   0x07u
#define PP_ON       0x34u
#define PP_TRG      0x3Cu
#define CH_CTRL     0x2D64u
#define CTRL_PAD    0x708u

/* The game reads buttons from three copies: the ButtonMgr pad passed to Check, the player's ctrl
 * pad (pawn+0x2D64)+0x708, and the script pads.  The scene's own poll sees a press left in any of
 * them, so "this button must not answer" has to mean all three. */
static void scrub_button(uint32_t pad, uint32_t slot, uint32_t mask) {
    uint32_t pawn, ctl, spad, i;
    if (!mask) return;
    if (pad && span_readable(pad, 0x50u)) {
        *(volatile uint32_t*)(pad + PP_ON) &= ~mask;
        *(volatile uint32_t*)(pad + PP_TRG) &= ~mask;
    }
    pawn = PlayerPawn(slot);
    if (pawn) {
        ctl = rd32(pawn + CH_CTRL);
        if (ctl && span_readable(ctl + CTRL_PAD, 0x50u)) {
            *(volatile uint32_t*)(ctl + CTRL_PAD + PP_ON) &= ~mask;
            *(volatile uint32_t*)(ctl + CTRL_PAD + PP_TRG) &= ~mask;
        }
    }
    spad = rd32s(SPAD_PTR);
    if (spad && span_readable(spad, 0x800u)) {
        for (i = 0; i < 2u; i++) {
            uint32_t p = spad + SPAD_PADS + i * SPAD_STRIDE;
            if (!span_readable(p, 0x180u)) continue;
            *(volatile uint32_t*)(p + SPAD_ON) &= ~mask;
            *(volatile uint32_t*)(p + SPAD_ON + 4u) &= ~mask;
            *(volatile uint32_t*)(p + SPAD_ON + 8u) &= ~mask;
        }
    }
}

static void inject_button(uint32_t pad, uint32_t slot, uint32_t mask) {
    uint32_t pawn, ctl, spad, i;
    if (!mask) return;
    if (pad && span_readable(pad, 0x50u)) {
        *(volatile uint32_t*)(pad + PP_ON) |= mask;
        *(volatile uint32_t*)(pad + PP_TRG) |= mask;
    }
    pawn = PlayerPawn(slot);
    if (pawn) {
        ctl = rd32(pawn + CH_CTRL);
        if (ctl && span_readable(ctl + CTRL_PAD, 0x50u)) {
            *(volatile uint32_t*)(ctl + CTRL_PAD + PP_ON) |= mask;
            *(volatile uint32_t*)(ctl + CTRL_PAD + PP_TRG) |= mask;
        }
    }
    spad = rd32s(SPAD_PTR);
    if (spad && span_readable(spad, 0x800u)) {
        for (i = 0; i < 2u; i++) {
            uint32_t p = spad + SPAD_PADS + i * SPAD_STRIDE;
            if (!span_readable(p, 0x180u)) continue;
            if (rd8(p + SPAD_TYPE) == 0) continue;
            *(volatile uint32_t*)(p + SPAD_ON) |= mask;
        }
    }
}

/* ------------------------------- the hook ------------------------------- */

static int FASTCALL QteCheck(uint32_t self, void* edx, uint32_t pad, uint32_t slot) {
    uint32_t req = 0;
    uint32_t padTrgIn = 0;
    g_checks++;
    key_poll();
    /* Capture the pad's new-press bits before any injection below: the entry's own hold puts
     * 0x8000 on this pad, and that must not read as a fresh native press next time. */
    if (pad && span_readable((uintptr_t)pad, 0x40u)) padTrgIn = rd32((uintptr_t)pad + 0x3Cu);
    if (self && span_readable((uintptr_t)self, 0x40)) req = rd32((uintptr_t)self + 0x0C);
    /* Keep the entry's native button held for a FIXED hand-off window.  A strict stage ends the
     * hold before it is injected: a lingering 0x8000 would be a wrong extra button for stages whose
     * mask is not X and would burn their "used" bit.  Never extend the deadline from the key state:
     * a held key must not keep the mask on the pad, or the scene never shows the next stage. */
    if (g_entryHoldSlot != 0xFFFFFFFFu) {
        const uint32_t now = GetTickCount();
        if ((int32_t)(now - g_entryHoldUntil) >= 0) {
            log_hold_end("timeout", now);
            g_entryHoldSlot = 0xFFFFFFFFu;
        } else if (g_enabled && slot == g_entryHoldSlot) {
            const int strictUp = req && span_readable((uintptr_t)req, 0x80) &&
                                 rd8((uintptr_t)req + REQ_STRICT) && rd8((uintptr_t)req + REQ_STATE) != 1u;
            if (!strictUp) inject_button(pad, slot, g_entryHoldMask);
        }
    }
    if (req && span_readable((uintptr_t)req, 0x80)) {
        uint32_t type = rd32((uintptr_t)req + REQ_TYPE);
        const uint32_t stamp = rd32((uintptr_t)req + REQ_STAMP);
        const uint32_t strict = rd8((uintptr_t)req + REQ_STRICT);
        const uint32_t state = rd8((uintptr_t)req + REQ_STATE);
        const uint32_t used = rd32((uintptr_t)req + REQ_USED);
        const int extra = (type < 32u) && ((g_extraTypes & (1u << type)) != 0u);
        const int eonly = (type < 32u) && ((g_eOnlyTypes & (1u << type)) != 0u);
        /* The key produces pad bit 0x2000, which is outside every strict combo; the strict rule
         * fails on any extra button.  Drop that bit everywhere the handler reads input. */
        if (strict && state != 1u) {
            uint32_t xmask = 0x2000u;
            /* Same-frame ordering: the stale entry slot may already have injected its bit before the
             * strict slot is checked.  Drop any live entry mask here too, whichever slot reports the
             * strict stage - both slots belong to the same player and the pad copies are shared. */
            if (g_entryHoldSlot != 0xFFFFFFFFu) {
                xmask |= g_entryHoldMask;
                log_hold_end("strict", GetTickCount());
                g_entryHoldUntil = 0;
                g_entryHoldSlot = 0xFFFFFFFFu;
            }
            scrub_button(pad, slot, xmask);
        }
        /* The request is never rewritten.  Its type selects the button the stage waits for, the glyph
         * the HUD draws and the command checkAll submits after ActionSlot::Check returns 1; rewriting
         * it made every stage an X stage, so stages whose native button is not X could not be answered
         * and the scene stalled.  The single-key answer is produced by putting the NATIVE mask on the
         * pad below.  Only the slot's icon copy is pointed at E, which the stock code overwrites from
         * the glyph table on its next call and so cannot desync the scene. */
        if (type >= 1u && type < 0x1Eu && state != 1u) {
            const uint32_t mask = rd32(MASK_TABLE + type * 4u);
            const int combo = mask && (mask & (mask - 1u)) != 0;
            const int special = (mask == 0);   /* stick wiggle / special input prompt */
            const int grabbed = (g_autoGrab || g_forceAll) ? PlayerInEnemyEvent(slot) : 0;
            g_promptsSeen++;
            if (strict) g_strictSeen++;
            if (strict && state != 1u) g_qteKeyTick = GetTickCount();
            if (g_promptLogs < g_maxPromptLog) {
                const uint32_t pawn = PlayerPawn(slot);
                const uint32_t act = pawn ? rd32(pawn + CH_ACTION_ID) : 0;
                log_prompt(req, stamp, type, mask, strict, state, used, slot,
                           rd32((uintptr_t)req + 0x18u), rd32((uintptr_t)req + 0x1Cu), act,
                           (uint32_t)(grabbed ? 1 : 0));
            }
            /* The per-type handlers refuse a request whose "answered" bit for this player is
             * already set (request+0x70, bit = slot); mirror that so a held key cannot re-fire
             * a request that the stock code would have ignored. */
            if (g_enabled && !(used & (1u << slot))) {
                /* EOnlyTypes prompts are E-only.  Type 1 (the normal melee prompt shown during the
                 * hit reaction) is answered by the mouse's native 0x8000; the scene's own poll reads
                 * the ctrl pad and SPAD copies just like the entry scrub found in v1.9.3, so clear the
                 * mask from all three every live frame.  E still answers through the manual branch
                 * below (inject + forced accept).  Types in ExtraTypes but not EOnlyTypes (the spray)
                 * keep the native button and may additionally be answered with E. */
                if (!strict && g_qteSingle && extra && eonly && mask && LocalPlayer(slot)) {
                    const int nat = (padTrgIn & mask) != 0u;
                    scrub_button(pad, slot, mask);
                    if (nat && g_extraBlockLogs < 40u && answer_new(req, stamp, 4u)) {
                        char b[160], *pp = b;
                        g_extraBlockLogs++;
                        pp = puts1(pp, "[native] block req=0x"); pp = fmt_hex(pp, req, 8);
                        pp = puts1(pp, " stamp=0x"); pp = fmt_hex(pp, stamp, 8);
                        pp = puts1(pp, " type="); pp = fmt_u(pp, type);
                        pp = puts1(pp, " slot="); pp = fmt_u(pp, slot);
                        pp = puts1(pp, " mask=0x"); pp = fmt_hex(pp, mask, 8);
                        *pp = 0; log_line(b);
                    }
                }
                /* Show E on the prompt without touching the request (same icon copy as the strict and
                 * entry branches; the stock code rewrites it from the glyph table). */
                if (!strict && g_qteSingle && extra && self && span_readable((uintptr_t)self, 0x40u))
                    *(volatile uint32_t*)(uintptr_t)(self + 0x38u) = 69u;
                /* Scripted multi-stage scenes: answer every stage as soon as it appears.  The
                 * "wrong button fails" rule is patched out and the stage's own buttons are put on
                 * the pad, so no key press is needed and the sequence cannot stall on input. */
                const char* why = 0;
                if (g_auto && LocalPlayer(slot) && !strict) {
                    if (g_autoGrab && grabbed) why = "grab";
                    else if (g_autoCombo && combo) why = "combo";
                    else if (g_autoSpecial && special) why = "special";
                }
                if (why) {
                    g_autoForced++;
                    if (g_autoForced <= 60u) log_auto(slot, type, req, why);
                    inject_button(pad, slot, mask);
                    return 1;
                }
                /* Multi-stage scenes: with the key's own pad bit cleared, put the buttons this
                 * stage asks for on the pad and let the stock handler decide (its per-stage state
                 * machine keeps running, so the scene does not desync like a forced accept). */
                /* Show E on the strict prompt without touching the request: only the slot icon changes,
                 * so the scene keeps its own type/mask and cannot desync, while the injected buttons
                 * (and nothing else) answer the stage. */
                if (strict && g_qteSingle && state != 1u && self && span_readable((uintptr_t)self, 0x40u))
                    *(volatile uint32_t*)(uintptr_t)(self + 0x38u) = 69u;
                if (strict && g_qteSingle && key_fires()) {
                    /* Answer while the key is down, not only on a fresh edge: a press made during
                     * the grab animation, or a key still held from the previous stage, used to leave
                     * the prompt unanswered until the player released and pressed again.  The
                     * stage's NATIVE mask goes on the pad, so the stock handler and the command
                     * checkAll submits stay the ones the scene made. */
                    int r;
                    const uint32_t cat = rd32((uintptr_t)req + 0x18u);
                    inject_button(pad, slot, mask);
                    r = g_origCheck(self, edx, pad, slot);
                    if (g_strictProbes < 80u && answer_new(req, stamp, (uint32_t)(r ? 1u : 0u))) {
                        char b[160], *pp = b;
                        g_strictProbes++;
                        pp = puts1(pp, "[strict] req=0x"); pp = fmt_hex(pp, req, 8);
                        pp = puts1(pp, " stamp=0x"); pp = fmt_hex(pp, stamp, 8);
                        pp = puts1(pp, " type="); pp = fmt_u(pp, type);
                        pp = puts1(pp, " cat="); pp = fmt_u(pp, cat);
                        pp = puts1(pp, " mask=0x"); pp = fmt_hex(pp, mask, 8);
                        pp = puts1(pp, " stock="); pp = fmt_u(pp, (uint32_t)(r ? 1u : 0u));
                        *pp = 0; log_line(b);
                    }
                    return r;
                }
                /* Chain entry: the non-strict button that opens a scripted multi-stage QTE
                 * (Wesker's grab shows type 9 right before its strict stages).  It is neither a
                 * combo, a special nor an enemy-event prompt, so the rules below skip it;
                 * EntryTypes names the types explicitly.  The key answers while held - not only on
                 * a fresh edge - and the type's native mask goes on the pad, so the stock state
                 * machine still owns the request: the same contract the strict path uses. */
                if (!strict && type < 32u && (g_entryTypes & (1u << type))) {
                    if (g_qteSingle && self && span_readable((uintptr_t)self, 0x40u))
                        *(volatile uint32_t*)(uintptr_t)(self + 0x38u) = 69u;
                    {
                        const uint32_t now = GetTickCount();
                        const uint32_t pawn = PlayerPawn(slot);
                        uint32_t act = 0;
                        int native;
                        if (pawn) act = rd32(pawn + CH_ACTION_ID);
                        if (g_entryArmReq != req || g_entryArmStamp != stamp) {
                            g_entryArmReq = req;
                            g_entryArmStamp = stamp;
                            g_entryArmStart = now;
                            g_entryArmed = 0;
                            g_entryWaitLogged = 0;
                        }
                        /* The mouse answers on the pad's new-press bit.  Use the bit captured at
                         * the top of this call, before the entry hold re-injected its own 0x8000. */
                        native = mask && (padTrgIn & mask) != 0u;
                        if (key_fires()) {
                            if (!g_entryArmed) g_entryArmedAt = now;
                            g_entryArmed = 1;
                        }
                        /* One press of E is latched through the get-up; answering waits for the
                         * first frame the hit reaction is over, capped so an unreadable/stuck act
                         * still answers instead of swallowing the prompt.  Every stalled entry
                         * (v1.8.9 and v1.9.0) was answered while act was a hit reaction
                         * (0x1b-0x1e); both v1.9.0 mouse successes answered at 0x0.  act 0x00 is
                         * not ready by itself: v1.9.4's 150 ms settle answers at 266/329 ms prompt
                         * age both had the scene end the event on the next probe. */
                        if (g_entryArmed && (!pawn || EntryActReady(act) ||
                            (int32_t)(now - (g_entryArmStart + ENTRY_ACT_WAIT_MS)) >= 0)) {
                            int r;
                            inject_button(pad, slot, mask);
                            g_entryHoldMask = mask;
                            g_entryHoldSlot = slot;
                            g_entryHoldStart = now;
                            g_entryHoldUntil = now + ENTRY_HOLD_MS;
                            r = g_origCheck(self, edx, pad, slot);
                            if (g_entryProbes < 80u && answer_new(req, stamp, (uint32_t)(r ? 1u : 0u))) {
                                char b[192], *pp = b;
                                g_entryProbes++;
                                pp = puts1(pp, "[entry] req=0x"); pp = fmt_hex(pp, req, 8);
                                pp = puts1(pp, " stamp=0x"); pp = fmt_hex(pp, stamp, 8);
                                pp = puts1(pp, " type="); pp = fmt_u(pp, type);
                                pp = puts1(pp, " slot="); pp = fmt_u(pp, slot);
                                pp = puts1(pp, " mask=0x"); pp = fmt_hex(pp, mask, 8);
                                pp = puts1(pp, " stock="); pp = fmt_u(pp, (uint32_t)(r ? 1u : 0u));
                                pp = puts1(pp, " hold="); pp = fmt_u(pp, ENTRY_HOLD_MS);
                                pp = puts1(pp, " act=0x"); pp = fmt_hex(pp, act, 8);
                                pp = puts1(pp, " waited="); pp = fmt_u(pp, now - g_entryArmStart);
                                pp = puts1(pp, " press="); pp = fmt_u(pp, now - g_entryArmedAt);
                                *pp = 0; log_line(b);
                            }
                            g_entryArmed = 0;
                            g_entryWaitLogged = 0;
                            return r;
                        }
                        if (g_entryArmed && !g_entryWaitLogged && g_entryWaitLogs < 80u) {
                            char b[160], *pp = b;
                            g_entryWaitLogs++;
                            g_entryWaitLogged = 1;
                            pp = puts1(pp, "[entry] wait req=0x"); pp = fmt_hex(pp, req, 8);
                            pp = puts1(pp, " stamp=0x"); pp = fmt_hex(pp, stamp, 8);
                            pp = puts1(pp, " type="); pp = fmt_u(pp, type);
                            pp = puts1(pp, " slot="); pp = fmt_u(pp, slot);
                            pp = puts1(pp, " act=0x"); pp = fmt_hex(pp, act, 8);
                            pp = puts1(pp, " cap="); pp = fmt_u(pp, ENTRY_ACT_WAIT_MS);
                            *pp = 0; log_line(b);
                        }
                        /* The chain entry is E-only for the local player: the native button (the
                         * mouse's LMB) must not start it.  Every pad copy the scene polls has to
                         * lose the bit - clearing only the ButtonMgr pad still let the script's
                         * own poll see the click and start the grab. */
                        if (mask && LocalPlayer(slot)) {
                            scrub_button(pad, slot, mask);
                            if (native && g_entryBlockLogs < 40u && answer_new(req, stamp, 3u)) {
                                char b[160], *pp = b;
                                g_entryBlockLogs++;
                                pp = puts1(pp, "[entry] block req=0x"); pp = fmt_hex(pp, req, 8);
                                pp = puts1(pp, " stamp=0x"); pp = fmt_hex(pp, stamp, 8);
                                pp = puts1(pp, " type="); pp = fmt_u(pp, type);
                                pp = puts1(pp, " slot="); pp = fmt_u(pp, slot);
                                pp = puts1(pp, " mask=0x"); pp = fmt_hex(pp, mask, 8);
                                pp = puts1(pp, " act=0x"); pp = fmt_hex(pp, act, 8);
                                *pp = 0; log_line(b);
                            }
                        }
                    }
                }
                /* The manual key answers exactly the prompts the Grab/Combo/Special rules
                 * accept, so E replaces the QTE buttons (strict is never set in this build).
                 * ExtraTypes prompts were scrubbed above, so E is their only input. */
                if (!strict && ((g_autoCombo && combo) || (g_autoSpecial && special) ||
                     grabbed || g_forceAll ||
                     (type < 32u && (g_extraTypes & (1u << type)))) && key_fires()) {
                    g_forced++;
                    if (g_forced <= 40u) log_force(slot, type, req, mask);
                    inject_button(pad, slot, mask);
                    return 1;
                }
            }
        }
    }
    return g_origCheck(self, edx, pad, slot);
}

/* ------------------------------- install ------------------------------- */

/* 0x0083A560: camera SetLookAt prologue, present only after the packer has decrypted the image. */
static const uint8_t SIG_UNPACKED[6] = { 0x8B, 0x44, 0x24, 0x04, 0xD9, 0x00 };
/* 0x00767390: sub esp,0Ch / push ebx / push ebp / push esi / mov ebx,ecx */
static const uint8_t SIG_BUTTONMGR[8] = { 0x83, 0xEC, 0x0C, 0x53, 0x55, 0x56, 0x8B, 0xD9 };
/* 0x00766F00 stock: push ebx / push esi / mov esi,ecx / mov eax,[esi+0Ch] */
static const uint8_t SIG_CHECK[6] = { 0x53, 0x56, 0x8B, 0xF1, 0x8B, 0x46 };

/* 0 = ready; otherwise the first check that failed (for the boot log). */
static int image_state(void) {
    if (!span_readable(RE5QTE_IMAGE_BASE + 0x1000u, 16u)) return 1;
    if (!span_readable(FN_GAME_PROLOGUE, sizeof(SIG_UNPACKED))) return 2;
    if (!memeq(FN_GAME_PROLOGUE, SIG_UNPACKED, sizeof(SIG_UNPACKED))) return 2;
    if (!span_readable(FN_BUTTONMGR_UPDATE, sizeof(SIG_BUTTONMGR))) return 3;
    if (!memeq(FN_BUTTONMGR_UPDATE, SIG_BUTTONMGR, sizeof(SIG_BUTTONMGR))) return 3;
    if (!span_readable(FN_ACTION_CHECK, sizeof(SIG_CHECK))) return 4;
    /* The entry may be either stock or an already-installed jump from another plugin
     * (RE5PartnerAI patches 0x00766F00 to a stub and resumes at 0x00766F07). */
    if (!memeq(FN_ACTION_CHECK, SIG_CHECK, sizeof(SIG_CHECK)) && rd8(FN_ACTION_CHECK) != 0xE9) return 4;
    if (rd8(CALL_ACTION_CHECK) != 0xE8 || call_target(CALL_ACTION_CHECK) != FN_ACTION_CHECK) return 5;
    /* cmp al,1 / jne +7 - the caller must still treat 1 as "prompt answered". */
    if (rd8(CALL_ACTION_CHECK + 5) != 0x3C || rd8(CALL_ACTION_CHECK + 6) != 0x01 ||
        rd8(CALL_ACTION_CHECK + 7) != 0x75) return 5;
    return 0;
}



static int install_hooks(void) {
    g_origCheck = (CheckFn)FN_ACTION_CHECK;
    if (!patch_call(CALL_ACTION_CHECK, (void*)QteCheck)) {
        log_line("[hook] ERROR: could not write the call site");
        return 0;
    }
    if (call_target(CALL_ACTION_CHECK) != (uintptr_t)(void*)QteCheck) {
        log_line("[hook] ERROR: call site verify failed after the write");
        return 0;
    }
    /* The request is left exactly as the game made it: rewriting it to another type makes the stage
     * require that type's key binding (in this setup the mouse), which is what made the QTE answer to
     * the left button.  E answers the native request through the pad injection below. */
    g_installed = 1;
    return 1;
}

/* The request type selects both the button the game waits for and the icon it draws, through the
 * byte table at 0xF590B8 (normal prompts use letter ids: X=69 'E', A=70 'F', Y=71 'G', B=73 'I';
 * QTE types use icon ids 33..55 / 97..100).  Pointing the QTE types at the letter id of the key
 * makes the prompt show that letter.  The button the handlers require is the mask table, which is
 * untouched - and the key path answers the prompt anyway. */
static void apply_glyph_override(void) {
    /* Nothing is patched globally: the mask and glyph tables are shared with ordinary context
     * prompts.  Rewriting type 15 there also turned the "hold A+D to turn the mechanism" prompt
     * into a single X prompt, so E stopped the mechanism instead of turning it.  The QTE merge
     * happens per request and only for real QTEs (strict, or while an enemy holds the player).
     * For the extra types named in the ini, only that type's icon byte is pointed at 69 (E). */
    uint32_t i;
    for (i = 0; i < 32u; i++) {
        if (g_extraTypes & (1u << i)) {
            const uintptr_t at = GLYPH_TABLE + i;
            uint8_t id = 69u;
            if (span_readable(at, 1)) write_mem(at, &id, 1);
        }
    }
    log_u("[ui] SingleKey -> ", g_qteSingle);
    log_u("[cfg] ExtraTypes mask -> ", g_extraTypes);
    log_u("[cfg] EOnlyTypes mask -> ", g_eOnlyTypes);
    log_u("[cfg] EntryTypes mask -> ", g_entryTypes);
}

/* The game builds the glyph table from the key bindings; keep the extra types on the letter E in
 * case it regenerates the table (e.g. after the key options are changed). */
static void reapply_extra_glyphs(void) {
    uint32_t i;
    if (!g_extraTypes) return;
    for (i = 0; i < 32u; i++) {
        if (g_extraTypes & (1u << i)) {
            const uintptr_t at = GLYPH_TABLE + i;
            uint8_t id = 69u;
            if (span_readable(at, 1)) write_mem(at, &id, 1);
        }
    }
}

static void patch_strict_fail(void) {
    /* 0x00763ACB: je -> jmp, so an extra button never fails a strict prompt.  The second site used by
     * v1.8.4 (0x00E8F481) is not QTE code at all - it is list bookkeeping in an unrelated function, and
     * patching its conditional jump changed that function's behaviour, so it is gone. */
    const uintptr_t sites[1] = { 0x00763ACBu };
    uint32_t i;
    if (!g_noStrictFail) return;
    for (i = 0; i < 1u; i++) {
        if (span_readable(sites[i], 1u) && rd8(sites[i]) == 0x74u) {
            uint8_t v = 0xEBu;
            write_mem(sites[i], &v, 1);
            g_strictPatched++;
        }
    }
    log_u("[patch] strict-fail sites patched=", g_strictPatched);
}

/* The QTE handlers ask this gate which input source to read: in keyboard mode it makes them
 * read the keyboard bitmap array, so the pad we write never reaches them.  Force the pad path. */
static unsigned char STDCALL PadForce(unsigned dev) { (void)dev; return 1u; }

/* The QTE handlers read the keyboard through the game's own key queries (0x79A4D0 held, 0x79A3E0
 * pressed, 0x79A430 released).  When a strict prompt is on screen these calls are redirected here:
 * the key held down by the player answers every button the stage waits for, and with no key down
 * nothing answers it - so the mouse can no longer start a QTE and E answers all of them. */
typedef int (STDCALL *KeyFn)(int action, int player);
static KeyFn g_origKeyOn = (KeyFn)0x0079A4D0u;
static KeyFn g_origKeyTrg = (KeyFn)0x0079A3E0u;
static KeyFn g_origKeyRel = (KeyFn)0x0079A430u;
static uint32_t g_qteKeyTick;

static int STDCALL KeyOnHook(int action, int player) {
    if (g_qteKeyTick && (uint32_t)(GetTickCount() - g_qteKeyTick) < 150u) return key_down() ? 1 : 0;
    return g_origKeyOn(action, player);
}
static int STDCALL KeyTrgHook(int action, int player) {
    if (g_qteKeyTick && (uint32_t)(GetTickCount() - g_qteKeyTick) < 150u) return key_down() ? 1 : 0;
    return g_origKeyTrg(action, player);
}
static int STDCALL KeyRelHook(int action, int player) {
    if (g_qteKeyTick && (uint32_t)(GetTickCount() - g_qteKeyTick) < 150u) return 0;
    return g_origKeyRel(action, player);
}

static void patch_key_calls(void) {
    static const uint32_t onSites[] = {0x007639BDu,0x007639D4u,0x00763A27u,0x00763A3Eu,0x00763D44u,0x00763D62u,0x00763F0Au,0x00763F21u,0x00763F39u,0x00763F51u};
    static const uint32_t trgSites[] = {0x0076379Fu,0x007637B6u,0x007637CEu,0x007637E6u,0x007639ECu,0x00763A06u,0x00763A56u,0x00763A70u,0x00763BBBu,0x00763BD9u,0x00763BF8u,0x00763C17u,0x00766375u,0x00766393u,0x007663B2u,0x007663D1u,0x007664DDu,0x007664FBu};
    static const uint32_t relSites[] = {0x007638A5u,0x007638BCu,0x007638D4u,0x007638ECu};
    uint32_t i, ok = 0;
    if (!g_qteKeys) return;
    for (i = 0; i < sizeof(onSites)/sizeof(onSites[0]); i++) {
        const uintptr_t s = onSites[i];
        if (span_readable(s, 5u) && rd8(s) == 0xE8u && call_target(s) == 0x0079A4D0u && patch_call(s, (void*)KeyOnHook)) ok++;
    }
    for (i = 0; i < sizeof(trgSites)/sizeof(trgSites[0]); i++) {
        const uintptr_t s = trgSites[i];
        if (span_readable(s, 5u) && rd8(s) == 0xE8u && call_target(s) == 0x0079A3E0u && patch_call(s, (void*)KeyTrgHook)) ok++;
    }
    for (i = 0; i < sizeof(relSites)/sizeof(relSites[0]); i++) {
        const uintptr_t s = relSites[i];
        if (span_readable(s, 5u) && rd8(s) == 0xE8u && call_target(s) == 0x0079A430u && patch_call(s, (void*)KeyRelHook)) ok++;
    }
    log_u("[patch] QTE key-query calls redirected=", ok);
}
static void patch_pad_input(void) {
    static const uint32_t sites[11] = {
        0x0076377Du, 0x00763891u, 0x00763980u, 0x00763B88u, 0x00763D27u, 0x00763EF7u,
        0x00766342u, 0x007664C2u, 0x0076652Eu, 0x00766608u, 0x00766827u
    };
    uint32_t i, ok = 0;
    if (!g_padInput) return;
    for (i = 0; i < 11u; i++) {
        const uintptr_t s = sites[i];
        if (!span_readable(s, 5u) || rd8(s) != 0xE8u) continue;
        if (call_target(s) != 0x00799AF0u) continue;
        {
            const int32_t rel = (int32_t)((uintptr_t)(void*)PadForce - (s + 5u));
            write_mem(s + 1u, &rel, 4u);
            ok++;
        }
    }
    log_u("[patch] QTE handler input-gate calls redirected=", ok);
}

static void restore_stock(void) {
    if (g_installed && call_target(CALL_ACTION_CHECK) == (uintptr_t)(void*)QteCheck)
        patch_call(CALL_ACTION_CHECK, (void*)FN_ACTION_CHECK);
    g_installed = 0;
}

static int wait_for_image(void) {
    uint32_t attempt;
    int last = -1;
    log_line("[boot] waiting for the unpacked game image; nothing is patched yet");
    for (attempt = 0; attempt < 900u; attempt++) {
        const int st = image_state();
        if (st == 0) { log_u("[boot] image ready, attempt=", attempt + 1u); return 1; }
        if (st != last) {
            char b[96], *p = b;
            p = puts1(p, "[boot] image not ready: reason="); p = fmt_u(p, (uint32_t)st);
            p = puts1(p, " attempt="); p = fmt_u(p, attempt + 1u);
            *p = 0; log_line(b);
            last = st;
        }
        pSleep(200u);
    }
    log_line("[boot] ERROR: no compatible image within 180 s; nothing patched");
    return 0;
}

static void log_stats(void) {
    char b[192], *p = b;
    p = puts1(p, "[stat] checks="); p = fmt_u(p, g_checks);
    p = puts1(p, " prompts=");      p = fmt_u(p, g_promptsSeen);
    p = puts1(p, " strict=");       p = fmt_u(p, g_strictSeen);
    p = puts1(p, " forced=");       p = fmt_u(p, g_forced);
    p = puts1(p, " auto=");         p = fmt_u(p, g_autoForced);
    p = puts1(p, " hooked=");       p = fmt_u(p, (uint32_t)g_installed);
    *p = 0; log_line(b);
}

/* Ground truth for the grab rule: log every change of a player's action id / event act, whether
 * or not a prompt is on screen. If the user gets grabbed and no [pawn] line says cls=1/2/3, the
 * enemy-event detection is what has to be fixed, not the prompt filter. */
static uint32_t g_probeAct[4], g_probeEv[4], g_probeSrc[4];
static int g_probeCls[4];
static int g_probeInit;
static uint32_t g_probeLogs;

static void probe_players(void) {
    uint32_t slot;
    for (slot = 0; slot < 4; slot++) {
        const uint32_t pawn = PlayerPawn(slot);
        uint32_t act = 0, ev = 0, src = 0, dti = 0;
        int cls = 0;
        if (pawn) {
            act = rd32(pawn + CH_ACTION_ID);
            cls = EventClass(pawn, &ev, &src, &dti);
        }
        if (!g_probeInit || act != g_probeAct[slot] || ev != g_probeEv[slot] || cls != g_probeCls[slot]) {
            if (g_probeLogs < 400u) {
                char b[192], *p = b;
                g_probeLogs++;
                p = puts1(p, "[pawn] slot=");  p = fmt_u(p, slot);
                p = puts1(p, " act=0x");       p = fmt_hex(p, act, 8);
                p = puts1(p, " ev=0x");        p = fmt_hex(p, ev, 8);
                p = puts1(p, " cls=");         p = fmt_u(p, (uint32_t)(cls < 0 ? 0 : cls));
                p = puts1(p, " src=0x");       p = fmt_hex(p, src, 8);
                p = puts1(p, " dti=0x");       p = fmt_hex(p, dti, 8);
    p = puts1(p, " nc=");          p = fmt_u(p, pawn ? rd32(pawn + 0x2DA8u) : 0u);
                p = puts1(p, " hp=");          p = fmt_u(p, pawn ? (uint32_t)(int32_t)(int16_t)rd16(pawn + 0x1364u) : 0u);
                *p = 0; log_line(b);
            }
            g_probeAct[slot] = act; g_probeEv[slot] = ev; g_probeSrc[slot] = src; g_probeCls[slot] = cls;
        }
    }
    g_probeInit = 1;
}

static DWORD STDCALL Worker(void* unused) {
    uint32_t tick = 0;
    (void)unused;
    log_open();
    log_line("=== RE5 QTE single-key v1.9.7 (EOnlyTypes drives the native-button scrub; spray type 25 keeps left mouse; entry 600ms window kept) | plugins\\RE5QTE.dll ===");
    config_load();
    log_u("[cfg] Enabled=", (uint32_t)g_enabled);
    log_u("[cfg] Key VK=", g_key);
    log_u("[cfg] Mode=", (uint32_t)(g_tapMode ? 1u : 0u));
    log_u("[cfg] ForceAll=", (uint32_t)g_forceAll);
    log_u("[cfg] ForegroundOnly=", (uint32_t)g_fgOnly);
    log_u("[cfg] Log=", (uint32_t)g_logPrompts);
    log_u("[cfg] Auto=", (uint32_t)g_auto);
    log_u("[cfg] AutoGrab=", (uint32_t)g_autoGrab);
    log_u("[cfg] AutoCombo=", (uint32_t)g_autoCombo);
    log_u("[cfg] AutoSpecial=", (uint32_t)g_autoSpecial);
    log_u("[cfg] AutoStrict=", (uint32_t)g_autoStrict);
    if (!g_enabled) { log_line("[cfg] disabled by ini; exiting without patching"); return 0; }
    if (!wait_for_image()) return 0;
    if (!install_hooks()) { log_line("[boot] ERROR: hook install failed; nothing changed"); return 0; }
    apply_glyph_override();
    patch_strict_fail();
    patch_pad_input();
    patch_key_calls();
    log_line("[boot] hook installed: 0x00767576 (ButtonMgr::checkAll) -> QteCheck");
    log_line("[boot] stock ActionSlot::Check 0x00766F00 is still called for every non-QTE prompt");
    for (;;) {
        pSleep(500u);
        probe_players();
        if (++tick % 20u == 0u) { log_stats(); reapply_extra_glyphs(); }
    }
}

BOOL STDCALL DllMain(HMODULE h, DWORD reason, void* reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        HANDLE t;
        pVirtualProtect = VirtualProtect;
        pVirtualQuery = VirtualQuery;
        pFlushInstructionCache = FlushInstructionCache;
        pGetCurrentProcess = GetCurrentProcess;
        pCreateThread = CreateThread;
        pSleep = Sleep;
        pCloseHandle = CloseHandle;
        pCreateFileA = CreateFileA;
        pReadFile = ReadFile;
        pWriteFile = WriteFile;
        pDisableThreadLibraryCalls = DisableThreadLibraryCalls;
        pGetAsyncKeyState = GetAsyncKeyState;
        pGetForegroundWindow = GetForegroundWindow;
        pGetWindowThreadProcessId = GetWindowThreadProcessId;
        if (pDisableThreadLibraryCalls) pDisableThreadLibraryCalls(h);
        t = pCreateThread(0, 0, Worker, 0, 0, 0);
        if (t) pCloseHandle(t);
    }
    return 1;
}

__declspec(dllexport) uint32_t STDCALL RE5QTE_Version(void) { return RE5QTE_VERSION; }
__declspec(dllexport) uint32_t STDCALL RE5QTE_Status(void) {
    return (g_installed ? 0x100u : 0u) | (g_forced & 0xFFu);
}
__declspec(dllexport) void STDCALL RE5QTE_Restore(void) { restore_stock(); }

