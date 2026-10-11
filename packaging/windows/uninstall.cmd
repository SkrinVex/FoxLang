@echo off
setlocal DisableDelayedExpansion
cd /d "%TEMP%"
rem Parse the entire block before uninstall.ps1 removes this launcher from disk.
(
  powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0uninstall.ps1" %*
  if errorlevel 1 (
    echo.
    echo Uninstallation failed.
    pause
    exit /b 1
  )
  echo.
  echo Uninstallation finished.
  pause
  exit /b 0
)
