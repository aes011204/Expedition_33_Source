@echo off
echo =============================================
echo  Generate Map Materials Tool
echo =============================================
echo.
echo Reads _MI folder and generates:
echo   Resources/Materials/[LevelName].material.json
echo   Resources/Map/NoMatJson/[LevelName]_textures.txt
echo.
echo [Usage]
echo   python GenerateMapMaterials.py --level-dir [LevelName]
echo   python GenerateMapMaterials.py --level-dir [LevelName] --overwrite
echo.
echo [Examples]
echo   python GenerateMapMaterials.py --level-dir SmallLevel_SimonArea
echo   python GenerateMapMaterials.py --level-dir SmallLevel_SimonArea --overwrite
echo.
echo =============================================
echo.
cd /d "%~dp0"
set /p CMD=">> "
%CMD%
echo.
pause
