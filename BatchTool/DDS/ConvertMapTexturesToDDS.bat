@echo off
setlocal

set "SCRIPT_DIR=%~dp0"

rem Keep the user-facing entry as a batch file, but run the converter in PowerShell
rem so texconv receives each path as a real argument instead of a fragile cmd string.
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%ConvertMapTexturesToDDS.ps1" %*
exit /b %ERRORLEVEL%
