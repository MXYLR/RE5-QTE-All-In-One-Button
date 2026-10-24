@echo off
rem ===========================================================================
rem  Build plugins\RE5QTE.dll with MSVC (Visual Studio Build Tools).
rem
rem    build_msvc.bat                        auto-detect the toolchain
rem    build_msvc.bat "C:\path\vcvars32.bat" explicit toolchain
rem
rem  Needs only Build Tools + "Desktop development with C++". No CRT is linked
rem  (/NODEFAULTLIB), so the DLL imports nothing but KERNEL32.dll and USER32.dll.
rem
rem  This script deliberately uses "goto" instead of "( ... )" blocks: paths on
rem  Windows contain "(x86)", and a ")" inside a parenthesised block closes the
rem  block early, which makes cmd.exe report nonsense errors.
rem ===========================================================================
setlocal

set "SRC=%~dp0"
set "OUT=%~dp0..\dist"
set "OBJ=%~dp0_build"
if not defined QTE_OUT set "QTE_OUT=RE5QTE.dll"
if not defined QTE_OUTDIR set "QTE_OUTDIR=%OUT%"
set "VCVARS=%~1"
if not "%VCVARS%"=="" goto :have_toolchain

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :no_vswhere

if not exist "%OBJ%" mkdir "%OBJ%"
set "VSROOT="
"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%OBJ%\vs.txt" 2>nul
if exist "%OBJ%\vs.txt" set /p VSROOT=<"%OBJ%\vs.txt"
if exist "%OBJ%\vs.txt" del "%OBJ%\vs.txt"
if not defined VSROOT goto :no_toolset
set "VCVARS=%VSROOT%\VC\Auxiliary\Build\vcvars32.bat"

:have_toolchain
if not exist "%VCVARS%" goto :no_vcvars
call "%VCVARS%" >nul 2>&1
if errorlevel 1 goto :no_vcvars

if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OBJ%" mkdir "%OBJ%"

set "CFLAGS=/nologo /c /O2 /GS- /GR- /Zl /W3 /arch:SSE2"

echo [*] compiling
cl %CFLAGS% /Fo"%OBJ%\re5qte.obj" "%SRC%re5_qte.c"
if errorlevel 1 goto :fail

echo [*] linking RE5QTE.dll
link /nologo /DLL /NODEFAULTLIB /NOIMPLIB /ENTRY:DllMain@12 /OUT:"%QTE_OUTDIR%\%QTE_OUT%" "%OBJ%\re5qte.obj" kernel32.lib user32.lib
if errorlevel 1 goto :fail

if exist "%OUT%\RE5QTE.exp" del "%OUT%\RE5QTE.exp"
rmdir /s /q "%OBJ%" 2>nul
echo [+] built: %QTE_OUTDIR%\%QTE_OUT%
endlocal
exit /b 0

:no_vswhere
echo [x] vswhere.exe not found - Visual Studio Build Tools are not installed.
echo     Install "Desktop development with C++", or pass vcvars32.bat as argument 1.
exit /b 1

:no_toolset
echo [x] no Visual Studio install with the x86/x64 C++ toolset was found.
echo     Pass vcvars32.bat as argument 1 to override.
exit /b 1

:no_vcvars
echo [x] could not initialise the MSVC x86 environment from "%VCVARS%"
exit /b 1

:fail
echo [x] build failed
if exist "%OBJ%" rmdir /s /q "%OBJ%" 2>nul
endlocal
exit /b 1
