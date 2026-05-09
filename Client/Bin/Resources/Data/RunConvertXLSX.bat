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

echo �߸��� �ɼ��Դϴ�: %~1
echo.
goto help_error

:prompt
set "SHOW_PAUSE=1"
cls
echo ========================================
echo   Table Convert
echo ========================================
echo.
echo   1^) xlsx to json���� ��ȯ
echo   2^) ���� ��Ʈ �������� ^(�ٸ� ����� �����Ѱ�^)
echo.
set /p USER_CHOICE=��ȣ ���� ^(1-2^): 
if "%USER_CHOICE%"=="1" goto local_mode
if "%USER_CHOICE%"=="2" goto sync_mode
echo.
echo 1 �Ǵ� 2�� �Է����ּ���.
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
echo     - �޴��� ���� �����մϴ�.
echo   RunConvertXLSX.bat 1
echo     - ���� xlsx�� json���� ��ȯ�մϴ�.
echo   RunConvertXLSX.bat 2
echo     - ���� ��Ʈ�� ���� ������ �� ��ȯ�մϴ�.
echo   RunConvertXLSX.bat sync
echo     - 2���� ���� �����Դϴ�.
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
