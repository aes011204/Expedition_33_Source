#include "UI_BattleAnnounce.h"

#include "Client_Constants.h"
#include "CombatStatCom.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_BattleAnnounce::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();

    return S_OK;
}

void UI_BattleAnnounce::Update(float timeDelta)
{
    if (!Is_Open() || !_isShowing)
        return;

}

void UI_BattleAnnounce::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_BattleAnnounce::Close()
{
    _isShowing = false;
    _elapsed = 0.f;

    UI_RuntimePanelBase::Close();
}

void UI_BattleAnnounce::Show_TurnStart(const BattleChar& actor, BattleTurnState state)
{
    if (_announceText == nullptr)
        return;

    const wstring announceText = Build_TurnStartText(actor, state);
    if (announceText.empty())
    {
        Close();
        return;
    }

    // 이전 재생이 남아있어도 다시 처음부터
    Close();

    _announceText->Set_Text(announceText);
    Open();

    _isShowing = true;
    _elapsed = 0.f;

    if (_root)
        _root->Play_UIAnimation(L"BattleSystem_Announce", true);
}

HRESULT UI_BattleAnnounce::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_BattleSystem_Announce_Path;
    desc.animationPath = Constants::UI_Anim::UI_BattleSystem_AnnounceAnim_Path;
    desc.prototypeTagPrefix = L"BattleAnnounce";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_BattleSystem_Announce");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_BattleAnnounce::Bind_UI()
{
    _announceText = Find_TypedUI<UI_Text>(_uiByName, L"Text_BattleSystem_Announce");
    CHECK_NULL(_announceText, E_FAIL);

    return S_OK;
}

wstring UI_BattleAnnounce::Resolve_ActorSubjectName(const BattleChar& actor)
{
    if (actor.statCom == nullptr)
        return L"";

    return actor.statCom->Get_BattleAnnounceSubjectName();
}

wstring UI_BattleAnnounce::Build_TurnStartText(const BattleChar& actor, BattleTurnState state)
{
    const wstring subjectName = Resolve_ActorSubjectName(actor);
    if (subjectName.empty())
    {
        return L"";
    }

    switch (state)
    {
    case BattleTurnState::ChooseAction:
    case BattleTurnState::None: // 몬스터가 None으로 들어오고있음
    default:
        return subjectName + L" 공격합니다.";
    }
}

Unique<UI_BattleAnnounce> UI_BattleAnnounce::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_BattleAnnounce>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_BattleAnnounce");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_BattleAnnounce::Free()
{
    UI_RuntimePanelBase::Free();
}
