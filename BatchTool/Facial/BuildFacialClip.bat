@echo off
setlocal
cd /d "%~dp0"

echo =============================================
echo  FacialClip Builder
echo =============================================
echo.
echo  Builds .facialclip.json from exported AnimSequence JSON + PSA.
echo.
echo  Default input:
echo    Client\Bin\Resources\Animations\Facial\Metahuman_Common\MicroMovements\A_MH_RandomMovement_GameMenu.json
echo    Client\Bin\Resources\Animations\Facial\Metahuman_Common\MicroMovements\A_MH_RandomMovement_GameMenu.psa
echo.
echo  Extra args can override paths.
echo.

set "REPO_ROOT=%~dp0..\.."
set "FACIAL_DIR=%REPO_ROOT%\Client\Bin\Resources\Animations\Facial\Metahuman_Common\MicroMovements"
set "GUSTAVE_HEAD=%REPO_ROOT%\Client\Bin\Resources\Models\Heros\Gustave\GustaveHead"

if "%~1"=="" (
    py -3 BuildFacialClip.py ^
        --json "%FACIAL_DIR%\A_MH_RandomMovement_GameMenu.json" ^
        --psa "%FACIAL_DIR%\A_MH_RandomMovement_GameMenu.psa" ^
        --model-meta "%GUSTAVE_HEAD%\GustaveHead.model.meta" ^
        --morph "%GUSTAVE_HEAD%\GustaveHead.morph" ^
        --out "%FACIAL_DIR%\A_MH_RandomMovement_GameMenu.facialclip.json" ^
        --compact
) else (
    py -3 BuildFacialClip.py %*
)

echo.
pause
