rem  명령어			옵션			원본 파일이 있는 위치		사본 파일을 저장할 위치

xcopy			/y			.\Engine\Bin\Engine.dll			.\Client\Bin\
xcopy			/y			.\Engine\Bin\ShaderFiles\*.*	.\Client\Bin\ShaderFiles\
xcopy			/y			.\Engine\Bin\Engine.lib			.\EngineSDK\lib\
xcopy			/y			.\Engine\ThirdPartyLib\*.lib	.\EngineSDK\lib\

xcopy			/y/s		.\Engine\Public\*.*				.\EngineSDK\inc\