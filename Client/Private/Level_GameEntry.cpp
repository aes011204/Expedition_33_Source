
#include "Level_GameEntry.h"

#include "GameInstance.h"
#include "Level_Loading.h"
#include "Client_Constants.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"
#include "UI_Button.h"

using namespace UI_Utils;

Level_GameEntry::Level_GameEntry(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Level{ device, context }
{
}

HRESULT Level_GameEntry::Initialize()
{
    if (FAILED(Ready_Layer_UI(TEXT("Layer_UI"))))
        return E_FAIL;

    return S_OK;
}

void Level_GameEntry::Update(float timeDelta)
{
    Handle_Button();
}

HRESULT Level_GameEntry::Render()
{
#ifdef _DEBUG
    SetWindowText(g_hWnd, TEXT("GameEntry Level"));
#endif

    return S_OK;
}

void Level_GameEntry::Handle_Button()
{
    if (GAME->KeyDown(KEY_TYPE::ESCAPE))
    {
        if (FAILED(GAME->Change_Level(ETOI(LevelType::Loading), Level_Loading::Create(_device.Get(), _context.Get(), LevelType::MainTitle))))
            return;

        return;
    }

    if (_firstLevelButton && _firstLevelButton->Was_Clicked())
    {
        if (FAILED(GAME->Change_Level(ETOI(LevelType::Loading), Level_Loading::Create(_device.Get(), _context.Get(), LevelType::MapTest))))
            return;

        return;
    }

    if (_secondLevelButton && _secondLevelButton->Was_Clicked())
    {
        if (FAILED(GAME->Change_Level(ETOI(LevelType::Loading), Level_Loading::Create(_device.Get(), _context.Get(), LevelType::GamePlay))))
            return;

        return;
    }
}

HRESULT Level_GameEntry::Ready_Layer_UI(const wstring& strLayerTag)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = ETOI(LevelType::GameEntry);
    desc.jsonPath = Constants::UI::UI_GameEntry_Path;
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _firstLevelButton = Find_TypedUI<UI_Button>(
        _uiByName,
        L"Button_FirstLevel",
        "Level_GameEntry");

    _secondLevelButton = Find_TypedUI<UI_Button>(
        _uiByName,
        L"Button_SecondLevel",
        "Level_GameEntry");

    return S_OK;
}

Shared<Level_GameEntry> Level_GameEntry::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto pInstance = make_shared<Level_GameEntry>(device, context);

    if (FAILED(pInstance->Initialize()))
    {
        MSG_BOX("Failed to Created : Level_GameEntry");
        return nullptr;
    }
    return pInstance;
}

void Level_GameEntry::Free()
{
    __super::Free();
}
