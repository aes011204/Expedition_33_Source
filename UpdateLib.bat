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
    echo Syncing Engine shader files...
    call :MirrorEngineShaderFiles "%ROOT%Client\Bin\ShaderFiles"
    if errorlevel 1 exit /b %errorlevel%
    call :MirrorEngineShaderFiles "%ROOT%MapEditor\Bin\ShaderFiles"
    if errorlevel 1 exit /b %errorlevel%
    call :MirrorEngineShaderFiles "%ROOT%ModelEditor\Bin\ShaderFiles"
    if errorlevel 1 exit /b %errorlevel%
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

:MirrorEngineShaderFiles
set "TARGET=%~1"

echo   Mirroring Engine_Shader_* into "%TARGET%"
robocopy "%ROOT%Engine\Bin\ShaderFiles" "%TARGET%" Engine_Shader_*.hlsl Engine_Shader_*.hlsli /MIR /LEV:1 /R:1 /W:1
if errorlevel 8 exit /b %errorlevel%

exit /b 0
