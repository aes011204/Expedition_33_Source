@echo off

:menu
cls
echo =============================================
echo  Map Split Tool
echo =============================================
echo.
echo  Splits SaveJson into a grid of chunk files
echo  so you can load only the area you need
echo  in the Map Editor.
echo.
echo  Output: SaveJson/Chunks/[LevelName]/
echo          chunk_ROW_COL.json  (MapLoader-compatible)
echo          manifest.json       (bounds + object counts)
echo.
echo =============================================
echo.
echo  1. Split with 4x4 grid  (16 chunks, ~58k x 52k units each)
echo  2. Split with 6x6 grid  (36 chunks, ~38k x 35k units each)
echo  3. Split with 8x8 grid  (64 chunks, ~29k x 26k units each)
echo  4. Custom (enter command manually)
echo.
echo =============================================
echo.
set /p CHOICE="Select [1-4]: "
echo.

if "%CHOICE%"=="1" goto grid4
if "%CHOICE%"=="2" goto grid6
if "%CHOICE%"=="3" goto grid8
if "%CHOICE%"=="4" goto custom

echo Invalid choice. Please enter 1-4.
pause
goto menu

:grid4
echo [Split] 4x4 grid
echo Input : SaveJson\Level_SpringMeadows_Main_V2.json
echo Output: SaveJson\Chunks\Level_SpringMeadows_Main_V2\
echo.
cd /d "%~dp0"
python split_map.py --grid 4x4
if errorlevel 1 goto error
goto done

:grid6
echo [Split] 6x6 grid
echo Input : SaveJson\Level_SpringMeadows_Main_V2.json
echo Output: SaveJson\Chunks\Level_SpringMeadows_Main_V2\
echo.
cd /d "%~dp0"
python split_map.py --grid 6x6
if errorlevel 1 goto error
goto done

:grid8
echo [Split] 8x8 grid
echo Input : SaveJson\Level_SpringMeadows_Main_V2.json
echo Output: SaveJson\Chunks\Level_SpringMeadows_Main_V2\
echo.
cd /d "%~dp0"
python split_map.py --grid 8x8
if errorlevel 1 goto error
goto done

:custom
echo [Custom] Enter the full command below.
echo.
echo Usage  : python split_map.py [--input PATH] [--output DIR] [--grid NxM]
echo Example: python split_map.py --grid 5x5
echo Example: python split_map.py --input ..\Client\Bin\Resources\Map\SaveJson\MyLevel.json --grid 4x4
echo.
cd /d "%~dp0"
set /p CMD=">> "
%CMD%
if errorlevel 1 goto error
goto done

:error
echo.
echo Split failed. Check the error above.
pause
goto menu

:done
echo.
set /p AGAIN="Back to menu? [y/n]: "
if /i "%AGAIN%"=="y" goto menu