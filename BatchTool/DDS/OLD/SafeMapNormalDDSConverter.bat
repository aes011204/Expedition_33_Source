@echo off
setlocal

set "SCRIPT_DIR=%~dp0"

rem Level-scoped Normal folder DDS overwrite converter.
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%SafeMapNormalDDSConverter.ps1" %*
exit /b %ERRORLEVEL%
