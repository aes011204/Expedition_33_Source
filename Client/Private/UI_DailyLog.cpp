#include "UI_DailyLog.h"

#include "Client_Constants.h"
#include "GameInstance.h"
#include "UI_Base.h"
#include "UI_Button.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

HRESULT UI_DailyLog::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();
    return S_OK;
}

void UI_DailyLog::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void UI_DailyLog::Open()
{
    Open(JournalOpenAnimation::JournalUI);
}

void UI_DailyLog::Open(JournalOpenAnimation openAnimation)
{
    if (_root == nullptr)
        return;

    const bool wasOpen = Is_Open();

    UI_RuntimePanelBase::Open();
    Suppress_ButtonHoverAnimations();

    if (!wasOpen)
        GAME->Play_SoundCue(Constants::SoundCue::UI_Global_GenericEnter);

    Play_OpenAnimation(openAnimation);

    GAME->Unlock_Mouse();
    GAME->Set_GameplayPaused(true);
}

void UI_DailyLog::Close()
{
    UI_RuntimePanelBase::Close();

    GAME->Set_GameplayPaused(false);
    GAME->Lock_Mouse();
}

HRESULT UI_DailyLog::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Journal_Path;
    desc.prototypeTagPrefix = L"DailyLog";
    desc.outUIByName = &_uiByName;
    desc.animationPaths =
    {
        Constants::UI_Anim::UI_JournalUIIntroAnim_Path,
        Constants::UI_Anim::UI_JournalMapIntroAnim_Path
    };

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = UI_Utils::Find_UI(_uiByName, L"Panel_Journal");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_DailyLog::Bind_UI()
{
    _journalUIButton = UI_Utils::Find_TypedUI<UI_Button>(
        _uiByName, L"Button_Journal_UI", "DailyLog");
    _journalMapButton = UI_Utils::Find_TypedUI<UI_Button>(
        _uiByName, L"Button_Journal_Map", "DailyLog");

    CHECK_NULL(_journalUIButton, E_FAIL);
    CHECK_NULL(_journalMapButton, E_FAIL);

    return S_OK;
}

void UI_DailyLog::Suppress_ButtonHoverAnimations()
{
    if (_journalUIButton)
        _journalUIButton->Suppress_HoverStateAnimationUntilMouseMove();
    if (_journalMapButton)
        _journalMapButton->Suppress_HoverStateAnimationUntilMouseMove();
}

void UI_DailyLog::Play_OpenAnimation(JournalOpenAnimation openAnimation)
{
    const wchar_t* animationName = L"Journal_UI_Intro";

    switch (openAnimation)
    {
    case JournalOpenAnimation::JournalMap:
        animationName = L"Journal_Map_Intro";
        break;

    case JournalOpenAnimation::JournalUI:
    default:
        animationName = L"Journal_UI_Intro";
        break;
    }

    for (auto& [uiName, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(uiName);

        if (ui == nullptr)
            continue;

        ui->Play_UIAnimation(animationName, true);
    }
}

Unique<UI_DailyLog> UI_DailyLog::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_DailyLog>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_DailyLog");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_DailyLog::Free()
{
    if (_root)
        Close();

    _journalUIButton.reset();
    _journalMapButton.reset();
    _root.reset();
    UI_RuntimeLoader::Release_Document(_uiByName);

    UI_RuntimePanelBase::Free();
}
