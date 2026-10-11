@echo off
setlocal DisableDelayedExpansion
cd /d "%TEMP%"
rem End the batch context before deleting its file, then execute the buffered block.
rem A child CMD sets the result without EXIT /B trying to reopen the deleted batch.
(goto) 2>nul & (
  powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0uninstall.ps1" %*
  if errorlevel 1 (
    echo.
    echo Uninstallation failed.
    pause
    cmd /d /c exit 1
  ) else (
    echo.
    echo Uninstallation finished.
    pause
    cmd /d /c exit 0
  )
)
