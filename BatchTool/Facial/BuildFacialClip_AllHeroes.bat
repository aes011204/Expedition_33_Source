@echo off
setlocal
cd /d "%~dp0"

echo =============================================
echo  FacialClip Builder - All Heroes
echo =============================================
echo.
echo  Builds Gustave, Lune, and Maelle facial clips
echo  from the shared GameMenu AnimSequence JSON + PSA.
echo.

set "REPO_ROOT=%~dp0..\.."
set "FACIAL_DIR=%REPO_ROOT%\Client\Bin\Resources\Animations\Facial\Metahuman_Common\MicroMovements"
set "JSON_PATH=%FACIAL_DIR%\A_MH_RandomMovement_GameMenu.json"
set "PSA_PATH=%FACIAL_DIR%\A_MH_RandomMovement_GameMenu.psa"

set "GUSTAVE_HEAD=%REPO_ROOT%\Client\Bin\Resources\Models\Heros\Gustave\GustaveHead"
set "LUNE_HEAD=%REPO_ROOT%\Client\Bin\Resources\Models\Heros\Lune\LuneHead"
set "MAELLE_HEAD=%REPO_ROOT%\Client\Bin\Resources\Models\Heros\Maelle\MaelleHead"

echo [1/3] Gustave
py -3 BuildFacialClip.py ^
    --json "%JSON_PATH%" ^
    --psa "%PSA_PATH%" ^
    --model-meta "%GUSTAVE_HEAD%\GustaveHead.model.meta" ^
    --morph "%GUSTAVE_HEAD%\GustaveHead.morph" ^
    --out "%FACIAL_DIR%\A_MH_RandomMovement_GameMenu_Gustave.facialclip.json" ^
    --bin-out "%FACIAL_DIR%\A_MH_RandomMovement_GameMenu_Gustave.facialclipbin" ^
    --compact
if errorlevel 1 goto :fail

echo.
echo [2/3] Lune
py -3 BuildFacialClip.py ^
    --json "%JSON_PATH%" ^
    --psa "%PSA_PATH%" ^
    --model-meta "%LUNE_HEAD%\LuneHead.model.meta" ^
    --morph "%LUNE_HEAD%\LuneHead.morph" ^
    --out "%FACIAL_DIR%\A_MH_RandomMovement_GameMenu_Lune.facialclip.json" ^
    --bin-out "%FACIAL_DIR%\A_MH_RandomMovement_GameMenu_Lune.facialclipbin" ^
    --compact
if errorlevel 1 goto :fail

echo.
echo [3/3] Maelle
py -3 BuildFacialClip.py ^
    --json "%JSON_PATH%" ^
    --psa "%PSA_PATH%" ^
    --model-meta "%MAELLE_HEAD%\MaelleHead.model.meta" ^
    --morph "%MAELLE_HEAD%\MaelleHead.morph" ^
    --out "%FACIAL_DIR%\A_MH_RandomMovement_GameMenu_Maelle.facialclip.json" ^
    --bin-out "%FACIAL_DIR%\A_MH_RandomMovement_GameMenu_Maelle.facialclipbin" ^
    --compact
if errorlevel 1 goto :fail

echo.
echo =============================================
echo  Done.
echo =============================================
echo.
pause
exit /b 0

:fail
echo.
echo =============================================
echo  FacialClip build failed.
echo =============================================
echo.
pause
exit /b 1
