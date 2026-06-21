#include "pch.h"
#include "UI_GetCharacter.h"

#include "Client_Constants.h"
#include "ClientInstance.h"
#include "GameInstance.h"
#include "PlayerStatCom.h"
#include "Player_WorldBase.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

namespace
{
    constexpr wchar_t kAnimationName[] = L"Get_Character";
    constexpr float kGetCharacterDurationSec = 2.f; // Get_Character.uianim: 0~60f / 30fps
}

HRESULT UI_GetCharacter::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close_Immediately();

    return S_OK;
}

void UI_GetCharacter::Update(float timeDelta)
{
    if (!Is_Open())
        return;

    if (GAME->KeyDown(KEY_TYPE::F) || GAME->KeyDown(KEY_TYPE::ESCAPE))
    {
        Close_Immediately();
        return;
    }

    _elapsed += max(0.f, timeDelta);
    if (_elapsed >= kGetCharacterDurationSec)
        Close_Immediately();
}

void UI_GetCharacter::Open_ForHeroes(const vector<HeroId>& heroIds)
{
    if (heroIds.empty())
        return;

    _showingHeroes = heroIds;

    Set_HeroPanelVisible(HeroId::Lune, Contains_Hero(HeroId::Lune));
    Set_HeroPanelVisible(HeroId::Maelle, Contains_Hero(HeroId::Maelle));

    Set_HeroLevelText(HeroId::Lune);
    Set_HeroLevelText(HeroId::Maelle);

    _elapsed = 0.f;
    UI_RuntimePanelBase::Open();

    // Get Character는 클릭 UI가 아니라 연출 UI라서 마우스/UIOnly 전환 없이 월드 조작만 막는다.
    GAME->Set_PlayerInputBlocked(true);

    Play_TargetAnimation(kAnimationName);
}

void UI_GetCharacter::Close()
{
    Close_Immediately();
}

void UI_GetCharacter::Close_Immediately()
{
    _elapsed = 0.f;

    Stop_TargetAnimations();
    UI_RuntimePanelBase::Close();

    _showingHeroes.clear();

    // Try_ShowPendingGetCharacter가 기존 입력 차단 상태에서는 열지 않으므로, 여기서 우리가 건 차단만 복구한다.
    GAME->Set_PlayerInputBlocked(false);
}

HRESULT UI_GetCharacter::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Get_Character_Path;
    desc.prototypeTagPrefix = L"GetCharacter";
    desc.outUIByName = &_uiByName;
    desc.animationPath = Constants::UI_Anim::UI_GetCharacterAnim_Path;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = UI_Utils::Find_UI(_uiByName, L"Panel_Get_Character");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_GetCharacter::Bind_UI()
{
    _lunePanel = UI_Utils::Find_UI(_uiByName, L"Panel_Get_Character_Lune");
    _maellePanel = UI_Utils::Find_UI(_uiByName, L"Panel_Get_Character_Maelle");

    _luneLevelText = UI_Utils::Find_TypedUI<Engine::UI_Text>(_uiByName, L"Text_Get_Character_Lune_Level_Value");
    _maelleLevelText = UI_Utils::Find_TypedUI<Engine::UI_Text>(_uiByName, L"Text_Get_Character_Maelle_Level_Value");

    CHECK_NULL(_lunePanel, E_FAIL);
    CHECK_NULL(_maellePanel, E_FAIL);
    CHECK_NULL(_luneLevelText, E_FAIL);
    CHECK_NULL(_maelleLevelText, E_FAIL);

    return S_OK;
}

void UI_GetCharacter::Play_TargetAnimation(const wchar_t* animationName)
{
    for (auto& [uiName, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(uiName);

        if (ui == nullptr)
            continue;

        ui->Play_UIAnimation(animationName, true);
    }
}

void UI_GetCharacter::Stop_TargetAnimations()
{
    for (auto& [uiName, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(uiName);

        if (ui == nullptr)
            continue;

        ui->Stop_UIAnimation();
    }
}

void UI_GetCharacter::Set_HeroPanelVisible(HeroId heroId, bool visible)
{
    Shared<Engine::UI_Base> targetPanel{};

    switch (heroId)
    {
    case HeroId::Lune:
        targetPanel = _lunePanel;
        break;

    case HeroId::Maelle:
        targetPanel = _maellePanel;
        break;

    default:
        return;
    }

    if (targetPanel == nullptr)
        return;

    if (visible)
        targetPanel->Open();
    else
        targetPanel->Close();
}

void UI_GetCharacter::Set_HeroLevelText(HeroId heroId)
{
    Shared<Engine::UI_Text> targetText{};

    switch (heroId)
    {
    case HeroId::Lune:
        targetText = _luneLevelText;
        break;

    case HeroId::Maelle:
        targetText = _maelleLevelText;
        break;

    default:
        return;
    }

    if (targetText == nullptr)
        return;

    uint32 level = 1;

    const Shared<Player_WorldBase> hero = CLIENT->Find_Hero(heroId);
    if (hero)
    {
        const Shared<PlayerStatCom> statCom = hero->Get_Component<PlayerStatCom>();
        if (statCom)
            level = statCom->Get_PlayerRuntimeStat().CurLevel;
    }

    targetText->Set_Text(std::to_wstring(level));
}

bool UI_GetCharacter::Contains_Hero(HeroId heroId) const
{
    return std::find(_showingHeroes.begin(), _showingHeroes.end(), heroId) != _showingHeroes.end();
}

Unique<UI_GetCharacter> UI_GetCharacter::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_GetCharacter>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_GetCharacter");
        instance->Free();
        return nullptr;
    }

    return instance;
}

