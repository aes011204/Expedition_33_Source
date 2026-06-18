@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set OUTPUT_DIR=%SCRIPT_DIR%..\..\Client\Bin\Resources\ModelPhysics\

echo ========================================================
echo Unreal Physics JSON to Cloth Collider Converter
echo ========================================================
echo.

:: Ensure output directory exists
if not exist "%OUTPUT_DIR%" (
    mkdir "%OUTPUT_DIR%"
)

set count=0

for %%f in ("%SCRIPT_DIR%*.json") do (
    set "INPUT_JSON=%%f"
    set "FILENAME=%%~nf"
    
    :: Output file will have the exact same name as the input file
    set "OUTPUT_JSON=!OUTPUT_DIR!!FILENAME!.json"
    
    echo Processing: !FILENAME!.json
    python "%SCRIPT_DIR%ConvertPhysics.py" "!INPUT_JSON!" "!OUTPUT_JSON!"
    set /a count+=1
)

echo.
if !count! equ 0 (
    echo No .json files found in %SCRIPT_DIR% to convert.
) else (
    echo Successfully processed !count! files.
)

echo ========================================================
pause
