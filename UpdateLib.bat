@echo off
cd /d "%~dp0"

echo Updating Engine outputs...

::: Headers
if exist ".\Engine\Public\" (
    echo Mirroring Engine headers...
    robocopy ".\Engine\Public" ".\EngineSDK\inc\Engine" /MIR /R:1 /W:1
    if errorlevel 8 exit /b %errorlevel%
)

::: Shared shader include files only
if exist ".\Engine\Bin\ShaderFiles\" (
    echo Copying Engine shader include files...
    xcopy /y /i /d ".\Engine\Bin\ShaderFiles\*.hlsli" ".\Client\Bin\ShaderFiles\"
)

::: Engine.lib
if exist ".\Engine\Bin\Debug\Engine.lib" (
    echo Copying Engine.lib [Debug]...
    xcopy /y /i /d ".\Engine\Bin\Debug\Engine.lib" ".\EngineSDK\lib\Debug\"
)

if exist ".\Engine\Bin\Release\Engine.lib" (
    echo Copying Engine.lib [Release]...
    xcopy /y /i /d ".\Engine\Bin\Release\Engine.lib" ".\EngineSDK\lib\Release\"
)

echo Update complete!
exit /b 0
