@echo off
setlocal

set "SCRIPT_DIR=%~dp0"

rem Normal-map only DDS conversion entry.
rem Keeps ConvertMapTexturesToDDS.bat separate for the broad map/model/effect pass.
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%ConvertNormalTexturesToDDS.ps1" %*
exit /b %ERRORLEVEL%
