/* Static-import test for the dsound.dll plugin loader, mirroring re5dx9.exe:
 * the exe links dsound.lib, so loading it from a directory that holds the proxy proves the
 * app-local dsound.dll wins.  DirectSoundEnumerateA returns DS_OK (0) even with no devices. */
#include <windows.h>
#include <dsound.h>
#include <stdio.h>

static BOOL CALLBACK EnumCb(LPGUID guid, LPCSTR desc, LPCSTR module, LPVOID ctx) {
    (void)guid; (void)desc; (void)module; (void)ctx;
    return TRUE;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    Sleep(500);
    printf("before DirectSoundEnumerateA (static IAT)\n");
    { HMODULE dm = GetModuleHandleA("dsound.dll"); printf("module=%p export=%p iat_target=%p\n", (void*)dm, (void*)GetProcAddress(dm, "DirectSoundEnumerateA"), *(void**)&DirectSoundEnumerateA); }
    HRESULT hr = DirectSoundEnumerateA((LPDSENUMCALLBACKA)EnumCb, NULL);
    printf("static import DirectSoundEnumerateA -> 0x%08lx\n", (unsigned long)hr);
    Sleep(1200);
    return hr == DS_OK ? 0 : 1;
}
