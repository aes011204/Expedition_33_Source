@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "SANDFALL_ROOT=D:\Clair Obscur Expedition 33\Exports\Sandfall"
set "SK_JSON=C:\Users\moon\Desktop\Output\Exports\Sandfall\Content\Characters\Heros\Lune\SK_Lune_UE5.json"
set "OUT_JSON=%SCRIPT_DIR%Output\Lune_ResolvedMaterials.json"

python "%SCRIPT_DIR%ResolveSandfallMaterials.py" ^
  --sandfall-root "%SANDFALL_ROOT%" ^
  --sk-json "%SK_JSON%" ^
  --out-json "%OUT_JSON%"

if errorlevel 1 (
    echo.
    echo Resolve failed.
    pause
    exit /b 1
)

echo.
echo Output written to:
echo %OUT_JSON%
pause
