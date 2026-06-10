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

void UI_BattleAnnounce::Show_TurnStart(const BattleChar& actor, BattleTurnState state, const Skill_Data* skillData)
{
    if (_announceText == nullptr)
        return;

    const wstring announceText = Build_TurnStartText(actor, state, skillData);
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

void UI_BattleAnnounce::Show_Counter(const vector<BattleChar>& counterActors)
{
    if (_announceText == nullptr)
        return;

    const wstring announceText = Build_CounterText(counterActors);
    if (announceText.empty())
        return;

    Close();

    _announceText->Set_Text(announceText);
    Open();

    _isShowing = true;
    _elapsed = 0.f;

    if (_root)
        _root->Play_UIAnimation(L"BattleSystem_Announce", true);
}

void UI_BattleAnnounce::Show_WeakPointHit(const BattleChar& target)
{
    if (_announceText == nullptr)
        return;

    const wstring announceText = Build_WeakPointHitText(target);
    if (announceText.empty())
        return;

    // 기존 Announce 닫기
    Close();

    _announceText->Set_Text(announceText);
    Open();

    _isShowing = true;
    _elapsed = 0.f;

    if (_root)
        _root->Play_UIAnimation(L"BattleSystem_Announce", true);
}

wstring UI_BattleAnnounce::Build_TurnStartText(const BattleChar& actor, BattleTurnState state,
                                               const Skill_Data* skillData)
{
    const wstring subjectName = Resolve_ActorSubjectName(actor);
    if (subjectName.empty())
        return L"";

    if (skillData != nullptr && !skillData->battleAnnounceActionText.empty())
        return subjectName + L" " + skillData->battleAnnounceActionText;

    switch (state)
    {
    case BattleTurnState::ChooseAction:
    case BattleTurnState::None:
    default:
        return subjectName + L" 공격합니다.";
    }
}

wstring UI_BattleAnnounce::Build_CounterText(const vector<BattleChar>& counterActors)
{
    vector<wstring> subjectNames{};

    for (const BattleChar& actor : counterActors)
    {
        if (actor.statCom == nullptr)
            continue;

        if (!actor.statCom->Is_Player())
            continue;

        const wstring subjectName = Resolve_ActorSubjectName(actor);
        if (subjectName.empty())
            continue;

        subjectNames.push_back(subjectName);
    }

    if (subjectNames.empty())
        return L"";

    if (subjectNames.size() >= 2)
        return L"원정대가 카운터를 수행합니다.";

    return subjectNames.front() + L" 카운터를 수행합니다.";
}

wstring UI_BattleAnnounce::Build_WeakPointHitText(const BattleChar& target)
{
    const wstring possessiveName = Build_PossessiveSubjectName(target);
    if (possessiveName.empty())
        return L"";

    return possessiveName + L" 약점을 공격했습니다.";
}

wstring UI_BattleAnnounce::Build_PossessiveSubjectName(const BattleChar& target)
{
    wstring subjectName = Resolve_ActorSubjectName(target);
    if (subjectName.empty())
        return L"";

    const wchar_t lastChar = subjectName.back();
    if (lastChar == L'이' || lastChar == L'가')
        subjectName.pop_back();

    return subjectName + L"의";
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
