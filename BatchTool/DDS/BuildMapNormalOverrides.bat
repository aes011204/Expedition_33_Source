@echo off
setlocal

set "SCRIPT_DIR=%~dp0"

rem Interactive map normal override builder.
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%BuildMapNormalOverrides.ps1" %*
exit /b %ERRORLEVEL%
