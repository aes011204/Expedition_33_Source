#include "UI_MonsterHpBar.h"
#include "UI_Base.h"
#include "UI_Utils.h"
#include "GameInstance.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "Monster_BattleBase.h"
#include "CombatStatCom.h"

using namespace UI_Utils;

HRESULT UI_MonsterHpBar::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();

    return S_OK;
}

void UI_MonsterHpBar::Update(float timeDelta)
{
    if (_displayMode == DisplayMode::Timed)
        _displayElapsed += timeDelta;

    BattleChar target{};
    if (!Try_GetDisplayTarget(target))
    {
        Close();
        return;
    }

    auto targetTr = target.actor->Get_Transform();
    CHECK_NULL(targetTr);

    Vec3 worldPos = targetTr->Get_WorldPosition();
    worldPos.y += 1.5f;

    Vec2 screenPos{};
    if (!Try_ProjectWorldToScreen(worldPos, screenPos))
    {
        Close();
        return;
    }

    Apply_EditorOffsets(screenPos);

    Open();
    Set_Hp(target.statCom->Get_CurHp(), target.statCom->Get_StatInfo().maxHp);
    Update_DelayHpGauge(timeDelta, _hpRatio);

    if (_root && _root->Get_Transform())
        _root->Get_Transform()->Set_Position(screenPos);
}

void UI_MonsterHpBar::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_MonsterHpBar::Close()
{
    _targetWorldPos = Vec3::Zero;
    _hasTargetWorldPos = false;

    _displayTarget = {};
    _displayMode = DisplayMode::None;
    _displayElapsed = 0.f;
    _displayDuration = 0.f;

    _hpRatio = 1.f;
    _delayedHpRatio = 1.f;

    UI_RuntimePanelBase::Close();
}

void UI_MonsterHpBar::Set_TargetWorldPosition(const Vec3& worldPos)
{
    // GameplayUI가 넘긴 위치 저장
    _targetWorldPos = worldPos;
    _hasTargetWorldPos = true;
}

void UI_MonsterHpBar::Set_Hp(float curHp, float maxHp)
{
    maxHp = max(maxHp, 1.f);
    curHp = clamp(curHp, 0.f, maxHp);

    _hpRatio = curHp / maxHp;
    Apply_HpGauge(_hpRatio);
}

void UI_MonsterHpBar::Show_TrackedTarget(const BattleChar& target)
{
    if (!Is_ValidMonsterTarget(target))
        return;

    _displayTarget = target;
    _displayMode = DisplayMode::Tracked;
    _displayElapsed = 0.f;
    _displayDuration = 0.f;

    const float maxHp = max(target.statCom->Get_StatInfo().maxHp, 1.f);
    Sync_DelayedHpGauge(clamp(target.statCom->Get_CurHp() / maxHp, 0.f, 1.f)); 
}

void UI_MonsterHpBar::Clear_TrackedTarget()
{
    if (_displayMode != DisplayMode::Tracked)
        return;

    _displayTarget = {};
    _displayMode = DisplayMode::None;
    _displayElapsed = 0.f;
    _displayDuration = 0.f;
}

void UI_MonsterHpBar::Show_TimedTarget(const BattleChar& target, float duration)
{
    if (!Is_ValidMonsterTarget(target))
        return;

    _displayTarget = target;
    _displayMode = DisplayMode::Timed;
    _displayElapsed = 0.f;
    _displayDuration = max(0.f, duration);
}

void UI_MonsterHpBar::Show_DamagedTarget(const BattleChar& target, float damage, float duration)
{
    if (!Is_ValidMonsterTarget(target))
        return;

    const float maxHp = max(target.statCom->Get_StatInfo().maxHp, 1.f);
    const float curHp = clamp(target.statCom->Get_CurHp(), 0.f, maxHp);
    const float prevHp = clamp(curHp + max(damage, 0.f), 0.f, maxHp);

    _displayTarget = target;
    _displayMode = DisplayMode::Timed;
    _displayElapsed = 0.f;
    _displayDuration = max(0.f, duration);

    _hpRatio = curHp / maxHp;                                   
    _delayedHpRatio = max(_delayedHpRatio, prevHp / maxHp);

    Apply_HpGauge(_hpRatio);
    Apply_DelayedHpGauge(_delayedHpRatio);
}

HRESULT UI_MonsterHpBar::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Monster_Hp_Path;
    desc.prototypeTagPrefix = L"MonsterHpBar";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_Monster_HP_Bar");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_MonsterHpBar::Bind_UI()
{
    Shared<UI_Base> hpGaugeBase = Find_UI(_uiByName, L"Image_Monster_HP_Gauge"); 
    CHECK_NULL(hpGaugeBase, E_FAIL);

    Shared<UI_Base> hpGaugeBgBase = Find_UI(_uiByName, L"Image_Monster_HP_Gauge_BG");
    CHECK_NULL(hpGaugeBgBase, E_FAIL);

    _hpGauge = dynamic_pointer_cast<UI_Image>(hpGaugeBase);
    CHECK_NULL(_hpGauge, E_FAIL);

    _hpGaugeBG = dynamic_pointer_cast<UI_Image>(hpGaugeBgBase);
    CHECK_NULL(_hpGaugeBG, E_FAIL);

    _hpGauge->Set_UseFillRatio(true); 
    _hpGauge->Set_FillOrigin(UI_Image::FillOrigin::LeftToRight);

    _hpGaugeBG->Set_UseFillRatio(true); 
    _hpGaugeBG->Set_FillOrigin(UI_Image::FillOrigin::LeftToRight);

    return S_OK;
}

void UI_MonsterHpBar::Apply_HpGauge(float ratio)
{
    if (_hpGauge == nullptr)
        return;

    auto hpTr = _hpGauge->Get_Transform();
    if (hpTr == nullptr)
        return;

    ratio = ::clamp(ratio, 0.f, 1.f);
    _hpGauge->Set_FillRatio(ratio); // HP 바 실루엣은 유지하고 채워진 영역만 잘라낸다.
}

void UI_MonsterHpBar::Apply_DelayedHpGauge(float ratio)
{
    if (_hpGaugeBG == nullptr)
        return;

    _hpGaugeBG->Set_FillRatio(clamp(ratio, 0.f, 1.f));
}

void UI_MonsterHpBar::Sync_DelayedHpGauge(float ratio)
{
    ratio = clamp(ratio, 0.f, 1.f);
    _hpRatio = ratio;
    _delayedHpRatio = ratio;

    Apply_HpGauge(_hpRatio);
    Apply_DelayedHpGauge(_delayedHpRatio);
}

void UI_MonsterHpBar::Update_DelayHpGauge(float timeDelta, float targetRatio)
{
    targetRatio = clamp(targetRatio, 0.f, 1.f);

    if (_delayedHpRatio <= targetRatio)
    {
        _delayedHpRatio = targetRatio;
        Apply_DelayedHpGauge(_delayedHpRatio);
        return;
    }

    const float alpha = clamp(timeDelta * kDelayedHpLerpSpeed, 0.f, 1.f);
    _delayedHpRatio = std::lerp(_delayedHpRatio, targetRatio, alpha);

    if (abs(_delayedHpRatio - targetRatio) <= 0.001f)
        _delayedHpRatio = targetRatio;

    Apply_DelayedHpGauge(_delayedHpRatio);
}

bool UI_MonsterHpBar::Try_GetDisplayTarget(BattleChar& outTarget) const
{
    if (_displayMode == DisplayMode::None)
        return false;

    if (!Is_ValidMonsterTarget(_displayTarget))
        return false;

    if (_displayMode == DisplayMode::Timed && _displayElapsed >= _displayDuration)
        return false;

    outTarget = _displayTarget;

    return true;
}

bool UI_MonsterHpBar::Is_ValidMonsterTarget(const BattleChar& target) const
{
    if (target.actor == nullptr || target.statCom == nullptr)
        return false;

    if (target.actor->Get_Transform() == nullptr)
        return false;

    if (target.statCom->Is_Player())
        return false;

    if (target.statCom->Get_AliveStatus() != EAliveStatus::Alive)
        return false;

    return true;
}

Unique<UI_MonsterHpBar> UI_MonsterHpBar::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_MonsterHpBar>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        MSG_BOX("Failed to Created : UI_MonsterHpBar");
        return nullptr;
    }

    return instance;
}

void UI_MonsterHpBar::Free()
{
    Base::Free();
}
