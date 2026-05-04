@echo off
cd /d "%~dp0"

echo Updating Engine outputs...

::: Headers
if exist ".\Engine\Public\" (
    echo Mirroring Engine headers...
    robocopy ".\Engine\Public" ".\EngineSDK\inc\Engine" /MIR
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

::: Runtime DLL/PDB for consumer apps
if exist ".\Engine\Bin\Debug\Engine.dll" (
    echo Copying Engine.dll [Debug] to app bins...
    xcopy /y /i /d ".\Engine\Bin\Debug\Engine.dll" ".\MainEditor\Bin\Debug\"
    xcopy /y /i /d ".\Engine\Bin\Debug\Engine.dll" ".\GameLauncher\Bin\Debug\"
    xcopy /y /i /d ".\Engine\Bin\Debug\Engine.dll" ".\EffectEditor\Bin\Debug\"
)

if exist ".\Engine\Bin\Debug\Engine.pdb" (
    echo Copying Engine.pdb [Debug] to app bins...
    xcopy /y /i /d ".\Engine\Bin\Debug\Engine.pdb" ".\MainEditor\Bin\Debug\"
    xcopy /y /i /d ".\Engine\Bin\Debug\Engine.pdb" ".\GameLauncher\Bin\Debug\"
    xcopy /y /i /d ".\Engine\Bin\Debug\Engine.pdb" ".\EffectEditor\Bin\Debug\"
)

if exist ".\Engine\Bin\Release\Engine.dll" (
    echo Copying Engine.dll [Release] to app bins...
    xcopy /y /i /d ".\Engine\Bin\Release\Engine.dll" ".\MainEditor\Bin\Release\"
    xcopy /y /i /d ".\Engine\Bin\Release\Engine.dll" ".\GameLauncher\Bin\Release\"
    xcopy /y /i /d ".\Engine\Bin\Release\Engine.dll" ".\EffectEditor\Bin\Release\"
)

if exist ".\Engine\Bin\Release\Engine.pdb" (
    echo Copying Engine.pdb [Release] to app bins...
    xcopy /y /i /d ".\Engine\Bin\Release\Engine.pdb" ".\MainEditor\Bin\Release\"
    xcopy /y /i /d ".\Engine\Bin\Release\Engine.pdb" ".\GameLauncher\Bin\Release\"
    xcopy /y /i /d ".\Engine\Bin\Release\Engine.pdb" ".\EffectEditor\Bin\Release\"
)

echo Update complete!
exit /b 0
