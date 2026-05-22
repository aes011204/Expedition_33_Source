@echo off
cd /d "%~dp0"
echo =============================================
echo  Map JSON to Binary (MsgPack) Converter
echo =============================================
echo.
py binarize_map.py
echo.
pause
