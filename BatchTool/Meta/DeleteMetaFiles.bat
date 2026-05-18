@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "REPO_ROOT=%SCRIPT_DIR%..\.."

if "%~1"=="" (
    set "TARGET_DIR=%REPO_ROOT%"
) else (
    set "TARGET_DIR=%~1"
)

for %%I in ("%TARGET_DIR%") do set "TARGET_DIR=%%~fI"

if not exist "%TARGET_DIR%\" (
    echo Target directory does not exist:
    echo %TARGET_DIR%
    set "EXIT_CODE=1"
    goto finish
)

echo =============================================
echo  Delete .meta Files
echo =============================================
echo Target:
echo %TARGET_DIR%
echo.

set "META_COUNT=0"
for /r "%TARGET_DIR%" %%F in (*.meta) do set /a META_COUNT+=1

echo Found %META_COUNT% .meta file(s).
echo.

if "%META_COUNT%"=="0" (
    echo Nothing to delete.
    set "EXIT_CODE=0"
    goto finish
)

set /p CONFIRM="Delete all .meta files under this target? [y/N]: "
if /i not "%CONFIRM%"=="y" (
    echo Canceled.
    set "EXIT_CODE=0"
    goto finish
)

for /r "%TARGET_DIR%" %%F in (*.meta) do (
    del /f /q "%%F"
)

echo.
echo Deleted %META_COUNT% .meta file(s).
set "EXIT_CODE=0"

:finish
echo.
pause
exit /b %EXIT_CODE%
