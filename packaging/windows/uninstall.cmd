@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0uninstall.ps1" %*
set "FOXLANG_EXIT=%ERRORLEVEL%"
echo.
if "%FOXLANG_EXIT%"=="0" (
  echo Uninstallation finished.
) else (
  echo Uninstallation failed with exit code %FOXLANG_EXIT%.
)
pause
exit /b %FOXLANG_EXIT%
