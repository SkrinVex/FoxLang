@echo off
setlocal DisableDelayedExpansion
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" %*
set "SKRUMI_EXIT=%ERRORLEVEL%"
echo.
if "%SKRUMI_EXIT%"=="0" (
  echo Installation finished.
) else (
  echo Installation failed with exit code %SKRUMI_EXIT%.
)
pause
exit /b %SKRUMI_EXIT%
