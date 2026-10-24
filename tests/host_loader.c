/* Smoke test for the version.dll loader proxy.
 *
 * Loads the proxy by path, calls one forwarded version.dll export and gives the loader thread
 * time to pull in plugins\. Run from a directory that contains version.dll and plugins\.
 */
#include <windows.h>
#include <stdio.h>

typedef UINT (WINAPI *SizeAFn)(LPCSTR, LPDWORD);

int main(int argc, char** argv) {
    const char* dll = argc > 1 ? argv[1] : "version.dll";
    HMODULE m;
    SizeAFn size_a;
    DWORD handle = 0;
    UINT n;
    setvbuf(stdout, NULL, _IONBF, 0);
    m = LoadLibraryA(dll);
    if (!m) { printf("FAIL: LoadLibrary %s (err %lu)\n", dll, GetLastError()); return 2; }
    size_a = (SizeAFn)GetProcAddress(m, "GetFileVersionInfoSizeA");
    if (!size_a) { printf("FAIL: no GetFileVersionInfoSizeA export\n"); return 2; }
    n = size_a("C:\\Windows\\notepad.exe", &handle);
    printf("forwarded GetFileVersionInfoSizeA -> %u\n", n);
    Sleep(1500);
    printf(n ? "FORWARD: PASS\n" : "FORWARD: FAIL\n");
    return n ? 0 : 1;
}
