#include "Level_GameEntry.h"

#include "ClientInstance.h"
#include "GameInstance.h"
#include "Level_Loading.h"
#include "Client_Constants.h"
#include "Level_MainTitle.h"
#include "MainTitleMenuSound.h"
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

    GAME->Set_RenderEnvironment(false);

    return S_OK;
}

void Level_GameEntry::Update(float timeDelta)
{
    Handle_Button();
}

HRESULT Level_GameEntry::Render()
{
    Update_WindowTitleFps(_windowTitleFps, L"GameEntry Level");

    return S_OK;
}

void Level_GameEntry::Set_RevealedLevelOnEntry(LevelType levelType)
{
    _revealedLevelOnEntry = levelType;
    Apply_RevealedLevelVisibility();
}

void Level_GameEntry::Handle_Button()
{
    if (CLIENT->Is_FadeTransitionPlaying())
        return;

    MainTitleMenuSound::Update_ButtonHoverSound(_firstLevelButton, _wasFirstLevelHovered);
    MainTitleMenuSound::Update_ButtonHoverSound(_secondLevelButton, _wasSecondLevelHovered);
    MainTitleMenuSound::Update_ButtonHoverSound(_thirdLevelButton, _wasThirdLevelHovered);
    MainTitleMenuSound::Update_ButtonHoverSound(_testLevelButton, _wasTestLevelHovered);

    if (GAME->KeyDown(KEY_TYPE::ESCAPE))
    {
        if (FAILED(GAME->Change_Level(ETOI(LevelType::MainTitle), Level_MainTitle::Create(_device.Get(), _context.Get()))))
            return;

        return;
    }

    if (_firstLevelButton && _firstLevelButton->Was_Clicked())
    {
        GAME->Play_SoundCue(Constants::SoundCue::UI_MainTitle_NewGame);

        if (!CLIENT->Request_LevelTransition(LevelType::GamePlay))
            return;

        return;
    }

    if (_secondLevelButton && _secondLevelButton->Was_Clicked())
    {
        GAME->Play_SoundCue(Constants::SoundCue::UI_MainTitle_NewGame);

        // 수정
        if (!CLIENT->Request_LevelTransition(LevelType::GestralVillage))
            return;

        return;
    }

    if (_thirdLevelButton && _thirdLevelButton->Was_Clicked())
    {
        GAME->Play_SoundCue(Constants::SoundCue::UI_MainTitle_NewGame);

        if (!CLIENT->Request_LevelTransition(LevelType::Stage01))
            return;

        return;
    }

    if (_testLevelButton && _testLevelButton->Was_Clicked())
    {
        GAME->Play_SoundCue(Constants::SoundCue::UI_MainTitle_NewGame);

        if (!CLIENT->Request_LevelTransition(LevelType::MapTest))
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

    _thirdLevelButton = Find_TypedUI<UI_Button>(
        _uiByName,
        L"Button_ThirdLevel",
        "Level_GameEntry");

    _testLevelButton = Find_TypedUI<UI_Button>(
        _uiByName,
        L"Button_TestLevel",
        "Level_GameEntry");

    Apply_RevealedLevelVisibility();

    return S_OK;
}

void Level_GameEntry::Apply_RevealedLevelVisibility()
{
    const wchar_t* thumbnailName = nullptr;
    const wchar_t* textName = nullptr;
    const wchar_t* buttonName = nullptr;

    switch (_revealedLevelOnEntry)
    {
    case LevelType::GestralVillage:
        buttonName = L"Button_SecondLevel";
        thumbnailName = L"Image_Second_Level_Thumnail";
        textName = L"Text_Second_Level_Name";
        break;

    case LevelType::Stage01:
    case LevelType::Simon:
        buttonName = L"Button_ThirdLevel";
        thumbnailName = L"Image_Third_Level_Thumnail";
        textName = L"Text_Third_Level_Name";
        break;

    default:
        return;
    }

    Shared<UI_Button> button = Find_TypedUI<UI_Button>(
        _uiByName,
        buttonName,
        "Level_GameEntry");
    if (button != nullptr)
    {
        button->Open();
        button->Set_SuppressSelfStateAnimation(true);
        button->Set_AlphaMultiplier(1.f);
    }

    const wchar_t* targetNames[] =
    {
        thumbnailName,
        textName,
    };

    for (const wchar_t* targetName : targetNames)
    {
        Shared<UI_Base> ui = Find_UI(_uiByName, targetName);
        if (ui == nullptr)
            continue;

        ui->Open();
        ui->Set_AlphaMultiplier(1.f);
    }
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
