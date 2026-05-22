#include "Level_MainTitle.h"

#include "GameInstance.h"
#include "Level_Loading.h"
#include "BackGround.h"
#include "Client_Constants.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"
#include "UI_Button.h"

using namespace UI_Utils;

Level_MainTitle::Level_MainTitle(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Level{ device, context }
{
}

HRESULT Level_MainTitle::Initialize()
{
    if (FAILED(Ready_Layer_UI(TEXT("Layer_UI"))))
        return E_FAIL;

    return S_OK;
}

void Level_MainTitle::Update(float timeDelta)
{
    Handle_Button();
}

HRESULT Level_MainTitle::Render()
{
#ifdef _DEBUG
    SetWindowText(g_hWnd, TEXT("MainTitle Level"));
#endif

    return S_OK;
}

void Level_MainTitle::Handle_Button()
{
    if (_continueButton &&  _continueButton->Was_Clicked())
    {
        if (FAILED(GAME->Change_Level(ETOI(LevelType::Loading), Level_Loading::Create(_device.Get(), _context.Get(), LevelType::GamePlay))))
            return;

        return; 
    }

    if (_testMap1Button && _testMap1Button->Was_Clicked())
    {
        if (FAILED(GAME->Change_Level(ETOI(LevelType::Loading), Level_Loading::Create(_device.Get(), _context.Get(), LevelType::MapTest))))
            return;

        return;
    }
}

HRESULT Level_MainTitle::Ready_Layer_UI(const wstring& strLayerTag)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = ETOI(LevelType::MainTitle);
    desc.jsonPath = Constants::UI::UI_MainTitle_Path;
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _continueButton = Find_TypedUI<UI_Button>(
        _uiByName,
        L"Button_MainTitle_Continue",
        "Level_MainTitle"); 

    _testMap1Button = Find_TypedUI<UI_Button>(
        _uiByName,
        L"Button_MainTitle_TestMap1",
        "Level_MainTitle"); 

    return S_OK;
}

Shared<Level_MainTitle> Level_MainTitle::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto pInstance = make_shared<Level_MainTitle>(device, context);

    if (FAILED(pInstance->Initialize()))
    {
        MSG_BOX("Failed to Created : Level_MainTitle");
        return nullptr;
    }
    return pInstance;
}

void Level_MainTitle::Free()
{
    __super::Free();
}
