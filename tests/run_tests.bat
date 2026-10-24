@echo off
rem Run the offline harness in all three modes. Exit code 0 = all checks passed.
setlocal
set "RUN=%~dp0run"
if not exist "%RUN%\host_qte.exe" goto :build
pushd "%RUN%"
call :one hold
if errorlevel 1 goto :fail
call :one tap
if errorlevel 1 goto :fail
call :one forceall
if errorlevel 1 goto :fail
call :one auto_all
if errorlevel 1 goto :fail
call :one auto_enemy
if errorlevel 1 goto :fail
popd
echo [ok] all QTE plugin checks passed
endlocal
exit /b 0
:one
echo --- mode %1 ---
host_qte.exe RE5QTE_test.dll %1
exit /b %errorlevel%
:build
echo [x] run\host_qte.exe missing; run build_tests.bat first
exit /b 1
:fail
popd
echo [x] checks failed
exit /b 1
