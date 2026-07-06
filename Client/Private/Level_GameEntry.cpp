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
    Update_RevealedLevelPreview();
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
    case LevelType::GamePlay:
        buttonName = L"Button_FirstLevel";
        thumbnailName = L"Image_First_Level_Thumnail";
        textName = L"Text_First_Level_Name";
        break;

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

    const wchar_t* revealButtonNames[] =
    {
        L"Button_FirstLevel",
        L"Button_SecondLevel",
        L"Button_ThirdLevel",
        L"Button_TestLevel",
    };

    for (const wchar_t* revealButtonName : revealButtonNames)
    {
        Shared<UI_Button> revealButton = Find_TypedUI<UI_Button>(
            _uiByName,
            revealButtonName,
            "Level_GameEntry");
        if (revealButton == nullptr)
            continue;

        revealButton->Close(); // reveal 대상만 남기기 위해 JSON 기본 visible 상태를 먼저 정리한다.
        revealButton->Set_SuppressSelfStateAnimation(false);
    }

    const wchar_t* revealUiNames[] =
    {
        L"Image_First_Level_Thumnail",
        L"Text_First_Level_Name",
        L"Image_Second_Level_Thumnail",
        L"Text_Second_Level_Name",
        L"Image_Third_Level_Thumnail",
        L"Text_Third_Level_Name",
        L"Image_Test_Level_Thumnail",
        L"Text_Test_Level_Name",
    };

    for (const wchar_t* revealUiName : revealUiNames)
    {
        Shared<UI_Base> ui = Find_UI(_uiByName, revealUiName);
        if (ui == nullptr)
            continue;

        ui->Close(); // 여러 레벨명이 겹쳐 보이지 않게 reveal 대상 외 텍스트/썸네일을 닫는다.
        ui->Set_AlphaMultiplier(1.f);
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

    if (_revealedLevelOnEntry == LevelType::GamePlay)
    {
        Shared<UI_Button> gestralButton = Find_TypedUI<UI_Button>(
            _uiByName,
            L"Button_SecondLevel",
            "Level_GameEntry");
        if (gestralButton != nullptr)
        {
            gestralButton->Open(); // Bishop 보상은 첫 번째 눈의 섬에 포커싱하되, 해금된 제스트랄 카드도 같이 보여준다.
            gestralButton->Set_SuppressSelfStateAnimation(false);
            gestralButton->Set_AlphaMultiplier(1.f);
        }
    }
}

void Level_GameEntry::Update_RevealedLevelPreview()
{
    if (_revealedLevelOnEntry != LevelType::GamePlay)
        return;

    LevelType previewLevel = LevelType::GamePlay;
    if (_secondLevelButton != nullptr && _secondLevelButton->Is_Hovered())
        previewLevel = LevelType::GestralVillage; // Bishop 진입 후 해금된 제스트랄 카드를 고르면 프리뷰만 제스트랄로 바꾼다.

    Apply_RevealedLevelPreview(previewLevel);
}

void Level_GameEntry::Apply_RevealedLevelPreview(LevelType levelType)
{
    const wchar_t* thumbnailName = nullptr;
    const wchar_t* textName = nullptr;

    switch (levelType)
    {
    case LevelType::GamePlay:
        thumbnailName = L"Image_First_Level_Thumnail";
        textName = L"Text_First_Level_Name";
        break;

    case LevelType::GestralVillage:
        thumbnailName = L"Image_Second_Level_Thumnail";
        textName = L"Text_Second_Level_Name";
        break;

    case LevelType::Stage01:
    case LevelType::Simon:
        thumbnailName = L"Image_Third_Level_Thumnail";
        textName = L"Text_Third_Level_Name";
        break;

    default:
        return;
    }

    const wchar_t* previewUiNames[] =
    {
        L"Image_First_Level_Thumnail",
        L"Text_First_Level_Name",
        L"Image_Second_Level_Thumnail",
        L"Text_Second_Level_Name",
        L"Image_Third_Level_Thumnail",
        L"Text_Third_Level_Name",
        L"Image_Test_Level_Thumnail",
        L"Text_Test_Level_Name",
    };

    for (const wchar_t* previewUiName : previewUiNames)
    {
        Shared<UI_Base> ui = Find_UI(_uiByName, previewUiName);
        if (ui == nullptr)
            continue;

        ui->Close(); // 선택된 카드의 프리뷰만 남겨 이름/썸네일 겹침을 막는다.
        ui->Set_AlphaMultiplier(1.f);
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
