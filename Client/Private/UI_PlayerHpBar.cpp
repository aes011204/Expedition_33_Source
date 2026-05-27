#include "UI_PlayerHpBar.h"
#include "UI_Base.h"
#include "UI_Utils.h"
#include "GameInstance.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "Monster_BattleBase.h"
#include "CombatStatCom.h"
#include "Client_Constants.h"

using namespace UI_Utils;

HRESULT UI_PlayerHpBar::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();

    return S_OK;
}

void UI_PlayerHpBar::Update(float timeDelta)
{
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

    Open();
    Set_Hp(target.statCom->Get_CurHp(), target.statCom->Get_StatInfo().maxHp);
    Update_DelayedHpGauge(timeDelta, _hpRatio);

    if (_root && _root->Get_Transform())
        _root->Get_Transform()->Set_Position(screenPos);
}

void UI_PlayerHpBar::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_PlayerHpBar::Close()
{
    _displayTarget = {};
    _hasDisplayTarget = false;
    _displayElapsed = 0.f;
    _displayDuration = 0.f;

    _hpRatio = 1.f;
    _delayedHpRatio = 1.f;

    UI_RuntimePanelBase::Close();
}

void UI_PlayerHpBar::Show_DamagedTarget(const BattleChar& target, float damage, float duration)
{
    if (!Is_ValidPlayerTarget(target))
        return;

    const float maxHp = max(target.statCom->Get_StatInfo().maxHp, 1.f);
    const float curHp = clamp(target.statCom->Get_CurHp(), 0.f, maxHp);
    const float prevHp = clamp(curHp + max(damage, 0.f), 0.f, maxHp);

    _displayTarget = target;
    _hasDisplayTarget = true;
    _displayElapsed = 0.f;
    _displayDuration = max(0.f, duration);

    _hpRatio = curHp / maxHp;
    _delayedHpRatio = max(_delayedHpRatio, prevHp / maxHp);

    Apply_HpGauge(_hpRatio);
    Apply_DelayedHpGauge(_delayedHpRatio);
}

HRESULT UI_PlayerHpBar::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Player_Hp_Path;
    desc.prototypeTagPrefix = L"PlayerHpBar";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_Player_HP_Bar");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_PlayerHpBar::Bind_UI()
{
    Shared<UI_Base> hpGaugeBase = Find_UI(_uiByName, L"Image_Player_HP_Gauge");
    CHECK_NULL(hpGaugeBase, E_FAIL);

    Shared<UI_Base> hpGaugeBgBase = Find_UI(_uiByName, L"Image_Player_HP_Gauge_BG");
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

void UI_PlayerHpBar::Set_Hp(float curHp, float maxHp)
{
    maxHp = max(maxHp, 1.f);
    curHp = clamp(curHp, 0.f, maxHp);

    _hpRatio = curHp / maxHp; 
    Apply_HpGauge(_hpRatio);
}

void UI_PlayerHpBar::Apply_HpGauge(float ratio)
{
    if (_hpGauge == nullptr)
        return;

    _hpGauge->Set_FillRatio(clamp(ratio, 0.f, 1.f));
}

void UI_PlayerHpBar::Apply_DelayedHpGauge(float ratio)
{
    if (_hpGaugeBG == nullptr)
        return;

    _hpGaugeBG->Set_FillRatio(clamp(ratio, 0.f, 1.f));
}

void UI_PlayerHpBar::Update_DelayedHpGauge(float timeDelta, float targetRatio)
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

bool UI_PlayerHpBar::Try_GetDisplayTarget(BattleChar& outTarget) const
{
    if (!_hasDisplayTarget)
        return false;

    if (!Is_ValidPlayerTarget(_displayTarget))
        return false;

    if (_displayElapsed >= _displayDuration)
        return false;

    outTarget = _displayTarget;

    return true;
}

bool UI_PlayerHpBar::Is_ValidPlayerTarget(const BattleChar& target) const
{
    if (target.actor == nullptr || target.statCom == nullptr)
        return false;

    if (target.actor->Get_Transform() == nullptr)
        return false;

    if (!target.statCom->Is_Player())
        return false;

    if (target.statCom->Get_AliveStatus() != EAliveStatus::Alive)
        return false;

    return true;
}

Unique<UI_PlayerHpBar> UI_PlayerHpBar::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_PlayerHpBar>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        MSG_BOX("Failed to Created : UI_PlayerHpBar");
        return nullptr;
    }

    return instance;
}

void UI_PlayerHpBar::Free()
{
    Base::Free();
}
