@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
set "BOOTSTRAP_PS1=%SCRIPT_DIR%SekaiLink-bootstrapper.ps1"
set "APP_EXE=%SCRIPT_DIR%SekaiLink Client.exe"

if exist "%BOOTSTRAP_PS1%" (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "%BOOTSTRAP_PS1%" %*
  exit /b %ERRORLEVEL%
)

if exist "%APP_EXE%" (
  start "" "%APP_EXE%" %*
  exit /b 0
)

echo SekaiLink Client executable was not found next to this launcher:
echo %APP_EXE%
pause
exit /b 1
