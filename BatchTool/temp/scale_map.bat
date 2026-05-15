@echo off
chcp 65001 > nul
echo =============================================
echo  Map Scale Tool (OriginJson -^> SaveJson)
echo =============================================
echo.
echo Reads from OriginJson folder, saves to SaveJson folder.
echo If a _Generated_ subfolder exists (World Partition cells), it is merged too.
echo Original files are NOT modified.
echo.
echo [Usage]
echo   python scale_map.py [filename] [--factor ratio] [--folder modelFolderName]
echo.
echo [Examples]
echo   python scale_map.py SmallLevel_SimonArea.json
echo   python scale_map.py SmallLevel_SimonArea.json --factor 1.0
echo   python scale_map.py SmallLevel_SimonArea.json --factor 0.5
echo   python scale_map.py MyMap.json --folder SmallLevel_SimonArea
echo.
echo [Folder Structure]
echo   OriginJson/
echo     MyMap.json                    ^<-- main file
echo     MyMap/
echo       _Generated_/
echo         xxxxxxxxxxxxxxxx.json     ^<-- streaming cells (auto-merged)
echo.
echo =============================================
echo.
cd /d "%~dp0"
set /p CMD=">> "
%CMD%
echo.
pause
