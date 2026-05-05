@echo off
pushd "%~dp0"
python ConvertXLSX.py
if errorlevel 1 (
    echo ConvertXLSX failed.
    popd
    pause
    exit /b 1
)
popd

