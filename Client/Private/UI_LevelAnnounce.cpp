#include "UI_LevelAnnounce.h"

#include "Client_Constants.h"
#include "GameInstance.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr wchar_t kAnimationName[] = L"Level_Announce_Intro";
    constexpr wchar_t kRootName[] = L"Panel_Level_Announce";
    constexpr wchar_t kImageName[] = L"Image_Level_Announce";
    constexpr wchar_t kLevelTextName[] = L"Text_Level_Announce_Current_Level";
    constexpr wchar_t kLevelTextBgName[] = L"Text_Level_Announce_Current_Level_BG";
    constexpr float kLevelAnnounceDurationSec = 4.f; // Level_Announce_Intro.uianim: 0~60f / 15fps

    bool Try_MarkLevelAnnounceSoundPlayed(LevelType levelType)
    {
        const uint32 levelIndex = ETOI(levelType);
        static array<bool, ETOI(LevelType::END)> playedByLevel{};

        if (levelIndex >= playedByLevel.size())
            return false;

        if (playedByLevel[levelIndex])
            return false;

        playedByLevel[levelIndex] = true;
        return true;
    }
}

HRESULT UI_LevelAnnounce::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;
    _lifeTime = kLevelAnnounceDurationSec;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close_Immediately();

    return S_OK;
}

void UI_LevelAnnounce::Update(float timeDelta)
{
    if (!Is_Open())
        return;

    _elapsed += max(0.f, timeDelta);
    if (_elapsed >= _lifeTime)
        Close_Immediately();
}

void UI_LevelAnnounce::Play_ForLevel(LevelType levelType)
{
    const wchar_t* levelName = Resolve_LevelDisplayName(levelType);
    if (levelName == nullptr)
        return;

    if (_currentLevelText)
        _currentLevelText->Set_Text(levelName);

    if (_currentLevelBgText)
        _currentLevelBgText->Set_Text(levelName);

    _currentLevelType = levelType;
    _elapsed = 0.f;

    Stop_Animation();
    UI_RuntimePanelBase::Open();
    Play_Animation();

    if (Try_MarkLevelAnnounceSoundPlayed(levelType))
        GAME->Play_SoundCue(Constants::SoundCue::UI_Level_Announce);
}

void UI_LevelAnnounce::Close()
{
    Close_Immediately();
}

void UI_LevelAnnounce::Close_Immediately()
{
    _elapsed = 0.f;
    _currentLevelType = LevelType::END;

    Stop_Animation();
    UI_RuntimePanelBase::Close();
}

bool UI_LevelAnnounce::Is_SupportedLevel(LevelType levelType)
{
    return Resolve_LevelDisplayName(levelType) != nullptr;
}

HRESULT UI_LevelAnnounce::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Level_Announce_Path;
    desc.animationPath = Constants::UI_Anim::UI_LevelAnnounceIntroAnim_Path;
    desc.prototypeTagPrefix = L"LevelAnnounce";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, kRootName);
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_LevelAnnounce::Bind_UI()
{
    const Shared<UI_Base> image = Find_UI(_uiByName, kImageName);
    CHECK_NULL(image, E_FAIL);

    _currentLevelText = Find_TypedUI<UI_Text>(_uiByName, kLevelTextName);
    _currentLevelBgText = Find_TypedUI<UI_Text>(_uiByName, kLevelTextBgName);

    CHECK_NULL(_currentLevelText, E_FAIL);
    CHECK_NULL(_currentLevelBgText, E_FAIL);

    _animationTargets.clear();
    _animationTargets.reserve(3);
    _animationTargets.push_back(image);
    _animationTargets.push_back(_currentLevelBgText);
    _animationTargets.push_back(_currentLevelText);

    return S_OK;
}

void UI_LevelAnnounce::Play_Animation()
{
    for (const Shared<UI_Base>& target : _animationTargets)
    {
        if (target == nullptr)
            continue;

        if (!target->Play_UIAnimation(kAnimationName, true))
            target->Stop_UIAnimation();
    }
}

void UI_LevelAnnounce::Stop_Animation()
{
    for (const Shared<UI_Base>& target : _animationTargets)
    {
        if (target)
            target->Stop_UIAnimation();
    }
}

const wchar_t* UI_LevelAnnounce::Resolve_LevelDisplayName(LevelType levelType)
{
    switch (levelType)
    {
    case LevelType::GamePlay:
        return L"\ub208\uc758 \uc12c";

    case LevelType::Stage01:
        return L"\uc2ec\uc5f0";

    case LevelType::GestralVillage:
        return L"\uc81c\uc2a4\ud2b8\ub784 \ub9c8\uc744";

    default:
        return nullptr;
    }
}

Unique<UI_LevelAnnounce> UI_LevelAnnounce::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_LevelAnnounce>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_LevelAnnounce");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_LevelAnnounce::Free()
{
    Close_Immediately();

    _currentLevelText.reset();
    _currentLevelBgText.reset();
    _animationTargets.clear();
    _root.reset();
    _uiByName.clear();

    UI_RuntimePanelBase::Free();
}
