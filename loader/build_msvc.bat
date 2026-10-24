@echo off
rem Build version.dll (plugin loader proxy) with MSVC. No CRT: imports only KERNEL32.dll.
setlocal
set "SRC=%~dp0"
set "OUT=%~dp0..\dist"
set "OBJ=%~dp0_build"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSROOT="
if not exist "%VSWHERE%" goto :no_vs
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT goto :no_vs
call "%VSROOT%\VC\Auxiliary\Build\vcvars32.bat" >nul 2>&1
if errorlevel 1 goto :no_vs
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OBJ%" mkdir "%OBJ%"

cl /nologo /c /O2 /GS- /GR- /Zl /W3 /arch:SSE2 /Fo"%OBJ%\dsound_proxy.obj" "%SRC%dsound_proxy.c"
if errorlevel 1 goto :fail
link /nologo /DLL /NODEFAULTLIB /NOIMPLIB /ENTRY:DllMain@12 /DEF:"%SRC%dsound.def" /OUT:"%OUT%\dsound.dll" "%OBJ%\dsound_proxy.obj" kernel32.lib
if errorlevel 1 goto :fail
if exist "%OUT%\dsound.exp" del "%OUT%\dsound.exp"
if exist "%OUT%\dsound.lib" del "%OUT%\dsound.lib"
rmdir /s /q "%OBJ%" 2>nul
echo [+] built: %OUT%\dsound.dll
endlocal
exit /b 0
:no_vs
echo [x] Visual Studio x86 toolset not found
exit /b 1
:fail
echo [x] build failed
if exist "%OBJ%" rmdir /s /q "%OBJ%" 2>nul
exit /b 1
