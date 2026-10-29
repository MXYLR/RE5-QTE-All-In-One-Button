/* Offline harness for plugins\RE5QTE.dll.
 *
 * The plugin patches a call inside the game image at a fixed address, so a plain unit test
 * cannot reach it. This little 32-bit host reserves the game's address range, writes the few
 * signatures the plugin validates, a fake stock ActionSlot::Check and a fake ButtonMgr::checkAll
 * call site, then loads the real DLL and drives it:
 *
 *   host_qte.exe <path-to-RE5QTE.dll> hold        normal QTE behaviour, E held
 *   host_qte.exe <path-to-RE5QTE.dll> tap         Mode=Tap
 *   host_qte.exe <path-to-RE5QTE.dll> forceall    ForceAll=1
 *
 * GetAsyncKeyState is redirected to a local fake (the DLL caches the pointer at load, so the
 * host overwrites that cached value in the module after LoadLibrary), which makes the result
 * deterministic and keeps the real keyboard out of the test.
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define IMAGE_BASE    0x50000000u
#define REGION_BASE   0x50000000u
#define REGION_SIZE   0x01600000u
#define FN_CHECK      0x50366F00u
#define FN_BUTTONMGR  0x50367390u
#define CALL_CHECK    0x50367576u
#define FN_PROLOGUE   0x5043A560u
#define FAKE_STUB     0x50300000u
#define REQ_ADDR      0x50D00000u
#define SLOT_ADDR     0x50E00000u
#define ORIG_ANSWER   0x50F00000u

static int g_keyDown;
static short __stdcall FakeGetAsyncKeyState(int vk) {
    (void)vk;
    return g_keyDown ? (short)0x8000 : 0;
}

static int g_fail;

static void put32(unsigned at, unsigned v) { *(volatile unsigned*)at = v; }
static void put8(unsigned at, unsigned v) { *(volatile unsigned char*)at = (unsigned char)v; }
static void putn(unsigned at, const void* p, unsigned n) { memcpy((void*)at, p, n); }

static void prepare_image(void) {
    static const unsigned char sig_unpacked[6] = { 0x8B, 0x44, 0x24, 0x04, 0xD9, 0x00 };
    static const unsigned char sig_buttongr[8] = { 0x83, 0xEC, 0x0C, 0x53, 0x55, 0x56, 0x8B, 0xD9 };
    /* The real type -> pad mask table at image+0xB58E50. */
    static const unsigned masks[32] = {
        0x0000u, 0x8000u, 0x1000u, 0x4000u, 0x2000u, 0x0400u, 0x0800u, 0x0100u,
        0x0200u, 0x8000u, 0x1000u, 0x4000u, 0x2000u, 0x9000u, 0x6000u, 0xC000u,
        0x3000u, 0x0C00u, 0x0300u, 0x0C00u, 0x0000u, 0x0000u, 0x8000u, 0x1000u,
        0x4000u, 0x2000u, 0x0000u, 0x0000u, 0x0000u, 0x8001u, 0x0000u, 0x0000u
    };
    static const unsigned char stock_check[] = {
        0x53, 0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x0C,   /* the stock prologue the plugin accepts */
        0x5B, 0x5E,                                 /* pop ebx / pop esi (balance the frame) */
        0xA1, 0x00, 0x00, 0xF0, 0x50,               /* mov eax, ds:[01400000h]               */
        0xC2, 0x08, 0x00                            /* ret 8                                 */
    };
    /* Fake ButtonMgr::checkAll fragment: the call the plugin patches, then the caller's test.
     * al == 1 -> ret 12; otherwise the jne lands on a second ret 12. */
    static const unsigned char site[] = {
        0xE8, 0x85, 0xF9, 0xFF, 0xFF,               /* call 00766F00h                        */
        0x3C, 0x01,                                 /* cmp al,1                              */
        0x75, 0x07,                                 /* jne +7                                */
        0xC2, 0x0C, 0x00,                           /* ret 12                                */
        0x90, 0x90, 0x90, 0x90,
        0xC2, 0x0C, 0x00                            /* ret 12                                */
    };
    /* __stdcall stub(self, pad, slot): push slot, push pad, ecx = self, jmp into the call site. */
    unsigned char stub[64];
    unsigned n = 0, rel;
    putn(FN_PROLOGUE, sig_unpacked, sizeof(sig_unpacked));
    putn(FN_BUTTONMGR, sig_buttongr, sizeof(sig_buttongr));
    putn(FN_CHECK, stock_check, sizeof(stock_check));
    putn(CALL_CHECK, site, sizeof(site));
    {   unsigned i;
        for (i = 0; i < 32; i++) put32(IMAGE_BASE + 0x00B58E50u + i * 4u, masks[i]);
    }
    put32(ORIG_ANSWER, 0);
    memset((void*)REGION_BASE, 0xCC, 0x1000);

    stub[n++] = 0x8B; stub[n++] = 0x4C; stub[n++] = 0x24; stub[n++] = 0x04;   /* mov ecx,[esp+4] */
    stub[n++] = 0xFF; stub[n++] = 0x74; stub[n++] = 0x24; stub[n++] = 0x0C;   /* push [esp+12]  */
    stub[n++] = 0xFF; stub[n++] = 0x74; stub[n++] = 0x24; stub[n++] = 0x0C;   /* push [esp+12]  */
    stub[n++] = 0xE9;
    rel = CALL_CHECK - (FAKE_STUB + n + 4);
    stub[n++] = (unsigned char)(rel & 0xFF);
    stub[n++] = (unsigned char)((rel >> 8) & 0xFF);
    stub[n++] = (unsigned char)((rel >> 16) & 0xFF);
    stub[n++] = (unsigned char)((rel >> 24) & 0xFF);
    stub[n++] = 0xCC;
    putn(FAKE_STUB, stub, n);
}

static void set_request(unsigned type, unsigned strict, unsigned state, unsigned used, unsigned slot) {
    unsigned char* r = (unsigned char*)REQ_ADDR;
    memset(r, 0, 0x80);
    put32(REQ_ADDR + 0x0C, state & 0xFF);
    put32(REQ_ADDR + 0x20, type);
    put32(REQ_ADDR + 0x68, 0x11112222);
    put32(REQ_ADDR + 0x6C, strict & 0xFF);
    put32(REQ_ADDR + 0x70, used);
    put32(REQ_ADDR + 0x74, 0x3F800000);
    put32(SLOT_ADDR + 0x0C, REQ_ADDR);
    (void)slot;
}

static int invoke(void) {
    typedef int (__stdcall *StubFn)(unsigned self, unsigned pad, unsigned slot);
    return ((StubFn)(void*)FAKE_STUB)(SLOT_ADDR, 0x00123456, 0);
}

/* The DLL caches GetAsyncKeyState in its .data at load time; replace every stored copy. */
static int redirect_key(HMODULE mod) {
    unsigned base = (unsigned)(uintptr_t)mod;
    unsigned char* p = (unsigned char*)mod;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)p;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(p + dos->e_lfanew);
    IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    unsigned target = (unsigned)(uintptr_t)&GetAsyncKeyState;
    unsigned fake = (unsigned)(uintptr_t)&FakeGetAsyncKeyState;
    unsigned i, hits = 0, size = nt->OptionalHeader.SizeOfImage;
    (void)base;
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (!(sec[i].Characteristics & IMAGE_SCN_MEM_WRITE)) continue;
        {
            unsigned char* s = p + sec[i].VirtualAddress;
            unsigned j;
            for (j = 0; j + 4 <= sec[i].Misc.VirtualSize; j += 4) {
                unsigned v; memcpy(&v, s + j, 4);
                if (v == target) { memcpy(s + j, &fake, 4); hits++; }
            }
        }
    }
    return hits != 0;
}

static void write_ini(const char* mode) {
    FILE* f = fopen("RE5QTE.ini", "wb");
    int autoOn = !strcmp(mode, "auto_all") || !strcmp(mode, "auto_enemy");
    if (!f) return;
    fprintf(f, "[General]\nEnabled = 1\nKey = E\n");
    if (!strcmp(mode, "tap")) fprintf(f, "Mode = Tap\n");
    else fprintf(f, "Mode = Hold\n");
    fprintf(f, "ForceAll = %d\n", !strcmp(mode, "forceall") ? 1 : 0);
    fprintf(f, "ForegroundOnly = 0\nLog = 1\nMaxPromptLog = 50\n");
    fprintf(f, "Glyph = 0\nSingleKey = 0\n");
    fprintf(f, "[Auto]\n");
    fprintf(f, "Enabled = %d\n", autoOn);
    fprintf(f, "Grab = 1\nCombo = 1\nSpecial = 1\n");
    fprintf(f, "Strict = %d\n", !strcmp(mode, "auto_all") ? 1 : 0);
    fclose(f);
}

int main(int argc, char** argv) {
    const char* dll = argc > 1 ? argv[1] : "RE5QTE.dll";
    const char* mode = argc > 2 ? argv[2] : "hold";
    void* region;
    HMODULE mod;
    typedef unsigned (__stdcall *StatusFn)(void);
    StatusFn status;
    unsigned t0, results[12];
    int i;

    setvbuf(stdout, NULL, _IONBF, 0);
    region = VirtualAlloc((void*)REGION_BASE, REGION_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!region) {
        MEMORY_BASIC_INFORMATION mbi;
        unsigned a = REGION_BASE;
        printf("FAIL: VirtualAlloc at %08x (err %lu)\n", REGION_BASE, GetLastError());
        while (a < REGION_BASE + REGION_SIZE && VirtualQuery((void*)a, &mbi, sizeof(mbi))) {
            unsigned b = (unsigned)(uintptr_t)mbi.BaseAddress, e = b + (unsigned)mbi.RegionSize;
            printf("  region %08x-%08x state=%08x prot=%08x type=%08x\n", b, e, mbi.State, mbi.Protect, mbi.Type);
            if (e <= a) break;
            a = e;
        }
        return 2;
    }
    prepare_image();
    write_ini(mode);

    mod = LoadLibraryA(dll);
    if (!mod) { printf("FAIL: LoadLibrary %s (err %lu)\n", dll, GetLastError()); return 2; }
    if (!redirect_key(mod)) { printf("FAIL: could not redirect GetAsyncKeyState\n"); return 2; }
    status = (StatusFn)GetProcAddress(mod, "_RE5QTE_Status@0");
    if (!status) { printf("FAIL: no status export\n"); return 2; }
    t0 = GetTickCount();
    while (!(status() & 0x100u)) {
        if (GetTickCount() - t0 > 5000) { printf("FAIL: plugin did not install (status=0x%x)\n", status()); return 2; }
        Sleep(50);
    }
    printf("plugin installed (status=0x%x)\n", status());

    /* 1: strict prompt + key down -> key forces it                          */
    set_request(3, 1, 0, 0, 0); put32(ORIG_ANSWER, 1); g_keyDown = 1;
    results[0] = invoke();
    Sleep(80);  /* outside the 50 ms tap window */
    /* 2: same prompt, key up -> auto only if Strict=1                       */
    set_request(3, 1, 0, 0, 0); put32(ORIG_ANSWER, 0); g_keyDown = 0;
    results[1] = invoke();
    /* 3: context prompt (strict=0, one button) + key down                   */
    set_request(1, 0, 0, 0, 0); put32(ORIG_ANSWER, 0); g_keyDown = 1;
    results[2] = invoke();
    /* 4: accepted already (state=1) + key down -> stock                     */
    set_request(3, 1, 1, 0, 0); put32(ORIG_ANSWER, 0); g_keyDown = 1;
    results[3] = invoke();
    /* 5: used bit for this player set + key down -> stock                   */
    set_request(3, 1, 0, 1u << 0, 0); put32(ORIG_ANSWER, 0); g_keyDown = 1;
    results[4] = invoke();
    /* 6: stock answer propagates when nothing forces                        */
    set_request(3, 1, 1, 0, 0); put32(ORIG_ANSWER, 1); g_keyDown = 1;
    results[5] = invoke();
    /* 7: two-button combo prompt (type 13, A+X), no key -> Combo rule       */
    set_request(13, 0, 0, 0, 0); put32(ORIG_ANSWER, 0); g_keyDown = 0;
    results[6] = invoke();
    /* 8: one-button non-strict prompt, no key -> stays stock                */
    set_request(1, 0, 0, 0, 0); put32(ORIG_ANSWER, 0); g_keyDown = 0;
    results[7] = invoke();
    /* 9: special prompt (type 20, mask 0) + key down -> E answers it        */
    set_request(20, 0, 0, 0, 0); put32(ORIG_ANSWER, 0); g_keyDown = 1;
    results[8] = invoke();
    Sleep(80);
    /* 10: same special prompt, no key -> only auto modes answer it          */
    set_request(20, 0, 0, 0, 0); put32(ORIG_ANSWER, 0); g_keyDown = 0;
    results[9] = invoke();
    put32(ORIG_ANSWER, 0); g_keyDown = 0;

    {   /* expectations depend on the mode */
        static const char* names[10] = {
            "strict prompt + key down (key)", "strict prompt + key up (Strict rule)",
            "context prompt + key down (stock)", "accepted prompt state=1 (stock)",
            "answered bit for this player (stock)", "stock answer propagates",
            "two-button prompt, no key (Combo rule)", "one-button non-strict, no key (stock)",
            "stick prompt + key down (E)", "stick prompt, no key (Special rule)"
        };
        unsigned want[10];
        want[0] = 1;
        want[1] = 0;
        want[2] = !strcmp(mode, "forceall") ? 1u : 0u;
        want[3] = 0; want[4] = 0; want[5] = 1;
        want[6] = (!strcmp(mode, "auto_all") || !strcmp(mode, "auto_enemy")) ? 1u : 0u;
        want[7] = 0;
        want[8] = 1;
        want[9] = (!strcmp(mode, "auto_all") || !strcmp(mode, "auto_enemy")) ? 1u : 0u;
        for (i = 0; i < 10; i++) {
            int ok = results[i] == want[i];
            printf("%s  %-42s got %u want %u\n", ok ? "PASS " : "FAIL ", names[i], results[i], want[i]);
            if (!ok) g_fail = 1;
        }
    }

    if (!strcmp(mode, "tap")) {
        /* in Tap mode a held key must not repeat, a fresh press must fire     */
        set_request(20, 0, 0, 0, 0); put32(ORIG_ANSWER, 0); g_keyDown = 1;
        results[10] = invoke();    /* press edge -> 1                            */
        Sleep(80);
        results[11] = invoke();    /* still held, no new edge -> 0               */
        g_keyDown = 0;
        {
            int ok = results[10] == 1 && results[11] == 0;
            printf("%s  %-42s got %u/%u want 1/0\n", ok ? "PASS " : "FAIL ",
                   "tap: edge fires, holding does not", results[10], results[11]);
            if (!ok) g_fail = 1;
        }
    }
    printf(g_fail ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    (void)t0;
    return g_fail ? 1 : 0;
}
