@echo off
echo =============================================
echo  Collect Map Textures Tool
echo =============================================
echo.
echo Reads NeedTextureJson/[LevelName]_textures.txt and copies
echo texture files (+ .json) from Sandfall exports to:
echo   Resources/Map/[LevelName]/Textures/
echo.
echo [Usage]
echo   python CollectMapTextures.py --level-dir [LevelName]
echo   python CollectMapTextures.py --level-dir [LevelName] --overwrite
echo.
echo [Examples]
echo   python CollectMapTextures.py --level-dir SmallLevel_SimonArea
echo   python CollectMapTextures.py --level-dir SmallLevel_SimonArea --overwrite
echo.
echo =============================================
echo.
cd /d "%~dp0"
set /p CMD=">> "
%CMD%
echo.
pause
