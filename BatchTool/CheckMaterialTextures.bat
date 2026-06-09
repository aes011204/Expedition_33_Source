@echo off
cd /d "%~dp0"

echo =============================================
echo  Check Material Textures
echo =============================================
echo.
echo  .material.json 에서 텍스처가 비어 있는
echo  머테리얼 인스턴스를 검사합니다.
echo.
echo  Usage (레벨 이름):
echo    python CheckMaterialTextures.py --level SmallLevel_MF_Zone_01
echo.
echo  Usage (파일 직접 지정):
echo    python CheckMaterialTextures.py --file path/to/file.material.json
echo.
echo =============================================
echo.
set /p CMD=">> python CheckMaterialTextures.py "
python CheckMaterialTextures.py %CMD%

echo.
pause
