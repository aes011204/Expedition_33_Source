@echo off

:menu
cls
echo =============================================
echo  Map Build Tool
echo =============================================
echo.
echo  1. Scale Map
echo     Reads map JSON from OriginJson/, scales coordinates,
echo     and saves result to SaveJson/.
echo.
echo  2. Collect MI
echo     Scans model JSONs in the level folder and copies
echo     MI JSON files from Sandfall exports into [Level]_MI/.
echo.
echo  3. Generate Materials
echo     Reads [Level]_MI/ and generates a .material.json
echo     bundle and texture extraction list.
echo.
echo  4. Collect Textures
echo     Reads the texture list and copies texture files
echo     from Sandfall exports into [Level]/Textures/.
echo.
echo  5. Run All  (step 2 then 3 then 4)
echo     Run steps 2, 3, 4 in sequence.
echo.
echo =============================================
echo.
set /p CHOICE="Select [1-5]: "
echo.

if "%CHOICE%"=="1" goto step1
if "%CHOICE%"=="2" goto step2
if "%CHOICE%"=="3" goto step3
if "%CHOICE%"=="4" goto step4
if "%CHOICE%"=="5" goto step_all

echo Invalid choice. Please enter 1-5.
pause
goto menu

:step1
echo [Step 1] Scale Map
echo.
echo Usage  : python scale_map.py [filename] [--factor ratio]
echo Example: python scale_map.py SmallLevel_SimonArea.json --factor 1.0
echo.
cd /d "%~dp0"
set /p CMD=">> "
%CMD%
goto done

:step2
echo [Step 2] Collect MI
echo.
echo Scans model JSONs and copies MI JSON files from Sandfall exports.
echo Output : Map\[Level]_MI\
echo.
echo Usage  : python CollectMapMI.py --level-dir [LevelName]
echo Example: python CollectMapMI.py --level-dir SmallLevel_SimonArea
echo.
cd /d "%~dp0"
set /p CMD=">> "
%CMD%
goto done

:step3
echo [Step 3] Generate Materials
echo.
echo Reads [Level]_MI/ and generates a .material.json bundle
echo and NeedTextureJson\[Level]_textures.txt.
echo.
echo Usage  : python GenerateMapMaterials.py --level-dir [LevelName]
echo Example: python GenerateMapMaterials.py --level-dir SmallLevel_SimonArea
echo.
cd /d "%~dp0"
set /p CMD=">> "
%CMD%
goto done

:step4
echo [Step 4] Collect Textures
echo.
echo Reads texture list and copies files from Sandfall exports.
echo Output : Map\[Level]\Textures\
echo.
echo Usage  : python CollectMapTextures.py --level-dir [LevelName]
echo Example: python CollectMapTextures.py --level-dir SmallLevel_SimonArea
echo.
cd /d "%~dp0"
set /p CMD=">> "
%CMD%
goto done

:step_all
echo [Run All] Steps 2 then 3 then 4
echo.
echo Runs Collect MI, Generate Materials, Collect Textures in sequence.
echo.
echo Usage  : python CollectMapMI.py --level-dir [LevelName]
echo Example: python CollectMapMI.py --level-dir SmallLevel_SimonArea
echo.
echo Enter the level name below (same name used for all 3 steps).
echo Add --overwrite at the end to replace existing files.
echo.
cd /d "%~dp0"
set /p LEVEL="Level name: "
echo.

echo --- [2/3] Collect MI ---
python CollectMapMI.py --level-dir "%LEVEL%"
if errorlevel 1 goto error

echo.
echo --- [3/3] Generate Materials ---
python GenerateMapMaterials.py --level-dir "%LEVEL%"
if errorlevel 1 goto error

echo.
echo --- [4/3] Collect Textures ---
python CollectMapTextures.py --level-dir "%LEVEL%"
if errorlevel 1 goto error

goto done

:error
echo.
echo Build failed at the step above.
pause
goto menu

:done
echo.
set /p AGAIN="Back to menu? [y/n]: "
if /i "%AGAIN%"=="y" goto menu
