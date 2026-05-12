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
set ROOT=%~dp0

if not exist "%ROOT%Client\Bin\ShaderFiles\" mkdir "%ROOT%Client\Bin\ShaderFiles\"
if not exist "%ROOT%MapEditor\Bin\ShaderFiles\" mkdir "%ROOT%MapEditor\Bin\ShaderFiles\"
if not exist "%ROOT%ModelEditor\Bin\ShaderFiles\" mkdir "%ROOT%ModelEditor\Bin\ShaderFiles\"

if exist "%ROOT%Engine\Bin\ShaderFiles\" (
    echo Copying Engine shader files...

    xcopy /y /i /d "%ROOT%Engine\Bin\ShaderFiles\*.hlsli" "%ROOT%Client\Bin\ShaderFiles\"
    xcopy /y /i /d "%ROOT%Engine\Bin\ShaderFiles\*.hlsli" "%ROOT%MapEditor\Bin\ShaderFiles\"
    xcopy /y /i /d "%ROOT%Engine\Bin\ShaderFiles\*.hlsli" "%ROOT%ModelEditor\Bin\ShaderFiles\"

    xcopy /y /i /d "%ROOT%Engine\Bin\ShaderFiles\*.hlsl" "%ROOT%Client\Bin\ShaderFiles\"
    xcopy /y /i /d "%ROOT%Engine\Bin\ShaderFiles\*.hlsl" "%ROOT%MapEditor\Bin\ShaderFiles\"
    xcopy /y /i /d "%ROOT%Engine\Bin\ShaderFiles\*.hlsl" "%ROOT%ModelEditor\Bin\ShaderFiles\"
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
