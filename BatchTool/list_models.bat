@echo off
chcp 65001 > nul
echo =============================================
echo  Model List Tool (SaveJson ^→ Models JSON)
echo =============================================
echo.
echo SaveJson 파일을 읽어 필요한 모델 목록을 저장하고
echo 모델 파일 존재 여부를 확인합니다.
echo 출력 파일: SaveJson\{맵이름}_models.json
echo.
echo [Usage]
echo   python list_models.py [filename]
echo.
echo [Examples]
echo   python list_models.py SmallLevel_SimonArea.json
echo.
echo =============================================
echo.
cd /d "%~dp0"
set /p CMD=">> "
%CMD%
echo.
pause
