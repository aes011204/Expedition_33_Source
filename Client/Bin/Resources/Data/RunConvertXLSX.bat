@echo off
chcp 949 > nul
setlocal
pushd "%~dp0"

set "SHOW_PAUSE="

if "%~1"=="" goto prompt
if /I "%~1"=="1" goto local_mode
if /I "%~1"=="2" goto sync_mode
if /I "%~1"=="sync" goto sync_mode
if /I "%~1"=="google" goto sync_mode
if /I "%~1"=="sync-google" goto sync_mode
if /I "%~1"=="help" goto help

echo 잘못된 옵션입니다: %~1
echo.
goto help_error

:prompt
set "SHOW_PAUSE=1"
cls
echo ========================================
echo   Table Convert
echo ========================================
echo.
echo   1^) xlsx to json으로 변환
echo   2^) 구글 시트 가져오기 ^(다른 사람이 수정한거^)
echo.
set /p USER_CHOICE=번호 선택 ^(1-2^): 
if "%USER_CHOICE%"=="1" goto local_mode
if "%USER_CHOICE%"=="2" goto sync_mode
echo.
echo 1 또는 2를 입력해주세요.
pause
goto prompt

:local_mode
set "SYNC_ARG="
set "MODE_NAME=Local XLSX to JSON"
goto run

:sync_mode
set "SYNC_ARG=--sync-google"
set "MODE_NAME=Google Sheet Sync to XLSX to JSON"
goto run

:help
echo Usage:
echo   RunConvertXLSX.bat
echo     - 메뉴를 띄우고 선택합니다.
echo   RunConvertXLSX.bat 1
echo     - 로컬 xlsx만 json으로 변환합니다.
echo   RunConvertXLSX.bat 2
echo     - 구글 시트를 먼저 가져온 뒤 변환합니다.
echo   RunConvertXLSX.bat sync
echo     - 2번과 같은 동작입니다.
popd
pause
exit /b 0

:help_error
echo Usage:
echo   RunConvertXLSX.bat
echo   RunConvertXLSX.bat 1
echo   RunConvertXLSX.bat 2
echo   RunConvertXLSX.bat sync
popd
pause
exit /b 1

:run
cls
echo [Mode] %MODE_NAME%
echo.
python ConvertXLSX.py %SYNC_ARG%
set "EXIT_CODE=%ERRORLEVEL%"
echo.
if not "%EXIT_CODE%"=="0" (
    echo ConvertXLSX failed.
    popd
    pause
    exit /b %EXIT_CODE%
)
echo ConvertXLSX completed.
popd
if defined SHOW_PAUSE pause
exit /b 0
