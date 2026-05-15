@echo off
echo =============================================
echo  Collect Map MI Tool
echo =============================================
echo.
echo Scans level model JSONs and copies MI JSON files
echo from Sandfall exports into a [LevelName]_MI folder.
echo.
echo [Usage]
echo   python CollectMapMI.py --level-dir [LevelName]
echo   python CollectMapMI.py --level-dir [LevelName] --overwrite
echo.
echo [Examples]
echo   python CollectMapMI.py --level-dir SmallLevel_SimonArea
echo   python CollectMapMI.py --level-dir NewLevel_MyArea --overwrite
echo.
echo =============================================
echo.
cd /d "%~dp0"
set /p CMD=">> "
%CMD%
echo.
pause
