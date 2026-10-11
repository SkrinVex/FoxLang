@echo off
setlocal DisableDelayedExpansion
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1" %*
set "FOXLANG_EXIT=%ERRORLEVEL%"
echo.
if "%FOXLANG_EXIT%"=="0" (
  echo Installation finished.
) else (
  echo Installation failed with exit code %FOXLANG_EXIT%.
)
pause
exit /b %FOXLANG_EXIT%
