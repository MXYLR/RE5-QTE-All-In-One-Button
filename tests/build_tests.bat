@echo off
rem Build the offline test variant of the plugin (synthetic image base 0x50000000) and the host.
setlocal
set "SRC=%~dp0"
set "ROOT=%~dp0.."
set "RUN=%~dp0run"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSROOT="
if not exist "%VSWHERE%" goto :no_vs
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT goto :no_vs
call "%VSROOT%\VC\Auxiliary\Build\vcvars32.bat" >nul 2>&1
if errorlevel 1 goto :no_vs
if not exist "%RUN%" mkdir "%RUN%"

cl /nologo /c /O2 /GS- /GR- /Zl /W3 /arch:SSE2 /DRE5QTE_IMAGE_BASE=0x50000000u /Fo"%RUN%\re5qte_test.obj" "%ROOT%\plugin\re5_qte.c"
if errorlevel 1 goto :fail
link /nologo /DLL /NODEFAULTLIB /NOIMPLIB /ENTRY:DllMain@12 /OUT:"%RUN%\RE5QTE_test.dll" "%RUN%\re5qte_test.obj" kernel32.lib user32.lib
if errorlevel 1 goto :fail
if exist "%RUN%\RE5QTE_test.exp" del "%RUN%\RE5QTE_test.exp"
if exist "%RUN%\re5qte_test.obj" del "%RUN%\re5qte_test.obj"

cl /nologo /O2 /W3 /Fe"%RUN%\host_qte.exe" "%SRC%host_qte.c" /link /BASE:0x20000000 /DYNAMICBASE:NO /NOLOGO user32.lib
if errorlevel 1 goto :fail
echo [+] built: %RUN%\RE5QTE_test.dll and %RUN%\host_qte.exe
endlocal
exit /b 0
:no_vs
echo [x] Visual Studio x86 toolset not found
exit /b 1
:fail
echo [x] build failed
exit /b 1
