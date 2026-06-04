#include "UI_BossHpBar.h"

#include "Client_Constants.h"
#include "MonsterStatCom.h"
#include "UI_Base.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_BossHpBar::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();
    return S_OK;
}

void UI_BossHpBar::Update(float timeDelta)
{
    if (!Is_ValidBossTarget(_displayTarget))
    {
        Close();
        return;
    }

    Open();
    Sync_BossState(_displayTarget);
    Update_DelayHpGauge(timeDelta, _hpRatio);
}

void UI_BossHpBar::Open()
{
    if (Is_Open())
        return;

    UI_RuntimePanelBase::Open();
}

void UI_BossHpBar::Close()
{
    if (!Is_Open())
        return;

    _displayTarget = {};
    _hasDisplayTarget = false;
    _hpRatio = 1.f;
    _delayedHpRatio = 1.f;

    UI_RuntimePanelBase::Close();
}

void UI_BossHpBar::Show_Boss(const BattleChar& target)
{
    if (!Is_ValidBossTarget(target))
        return;

    const bool changedTarget = (!_hasDisplayTarget || _displayTarget.actor != target.actor);

    _displayTarget = target;
    _hasDisplayTarget = true;

    if (changedTarget)
    {
        const float maxHp = max(target.statCom->Get_StatInfo().maxHp, 1.f);
        const float ratio = clamp(target.statCom->Get_CurHp() / maxHp, 0.f, 1.f);

        _hpRatio = ratio;
        _delayedHpRatio = ratio;
        Apply_HpGauge(_hpRatio);
        Apply_DelayedHpGauge(_delayedHpRatio);
    }
}

void UI_BossHpBar::Clear_Boss()
{
    Close();
}

void UI_BossHpBar::Show_DamagedBoss(const BattleChar& target, float damage)
{
    if (!Is_ValidBossTarget(target))
        return;

    const float maxHp = max(target.statCom->Get_StatInfo().maxHp, 1.f);
    const float curHp = clamp(target.statCom->Get_CurHp(), 0.f, maxHp);
    const float prevHp = clamp(curHp + max(damage, 0.f), 0.f, maxHp);

    _displayTarget = target;
    _hasDisplayTarget = true;

    _hpRatio = curHp / maxHp;
    _delayedHpRatio = max(_delayedHpRatio, prevHp / maxHp);

    Apply_HpGauge(_hpRatio);
    Apply_DelayedHpGauge(_delayedHpRatio);
}

HRESULT UI_BossHpBar::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Boss_Hp_Path;
    desc.prototypeTagPrefix = L"BossHpBar";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_Boss_HP_Bar");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_BossHpBar::Bind_UI()
{
    _hpGauge = Find_TypedUI<UI_Image>(_uiByName, L"Image_Boss_HP_Bar_Gauge");
    _hpGaugeBG = Find_TypedUI<UI_Image>(_uiByName, L"Image_Boss_HP_Bar_Gauge_BG");
    _shieldGauge = Find_TypedUI<UI_Image>(_uiByName, L"Image_Boss_Shield_HP_Bar_Gauge");
    _breakGauge = Find_TypedUI<UI_Image>(_uiByName, L"Image_Boss_Break_Bar_Gauge");
    _bossNameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Boss_Name");
    _bossNameBgText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Boss_Name_BG");

    CHECK_NULL(_hpGauge, E_FAIL);
    CHECK_NULL(_hpGaugeBG, E_FAIL);
    CHECK_NULL(_shieldGauge, E_FAIL);
    CHECK_NULL(_bossNameText, E_FAIL);
    CHECK_NULL(_bossNameBgText, E_FAIL);

    _hpGauge->Set_UseFillRatio(true);
    _hpGauge->Set_FillOrigin(UI_Image::FillOrigin::LeftToRight);

    _hpGaugeBG->Set_UseFillRatio(true);
    _hpGaugeBG->Set_FillOrigin(UI_Image::FillOrigin::LeftToRight);

    _shieldGauge->Set_UseFillRatio(true);
    _shieldGauge->Set_FillOrigin(UI_Image::FillOrigin::LeftToRight);

    if (_breakGauge)
        _breakGauge->Close();

    return S_OK;
}

void UI_BossHpBar::Sync_BossState(const BattleChar& target)
{
    const float maxHp = max(target.statCom->Get_StatInfo().maxHp, 1.f);
    const float curHp = clamp(target.statCom->Get_CurHp(), 0.f, maxHp);

    _hpRatio = curHp / maxHp;
    Apply_HpGauge(_hpRatio);
    Apply_ShieldGauge(target);

    const Shared<MonsterStatCom> monsterStat =
        dynamic_pointer_cast<MonsterStatCom>(target.statCom);

    const wstring name = monsterStat ? monsterStat->Get_MonsterDesc().battleAnnounceName : L"";

    if (_bossNameText && _bossNameText->Get_Text() != name)
        _bossNameText->Set_Text(name);

    if (_bossNameBgText && _bossNameBgText->Get_Text() != name)
        _bossNameBgText->Set_Text(name);
}

void UI_BossHpBar::Apply_HpGauge(float ratio)
{
    if (_hpGauge)
        _hpGauge->Set_FillRatio(clamp(ratio, 0.f, 1.f));
}

void UI_BossHpBar::Apply_DelayedHpGauge(float ratio)
{
    if (_hpGaugeBG)
        _hpGaugeBG->Set_FillRatio(clamp(ratio, 0.f, 1.f));
}

void UI_BossHpBar::Apply_ShieldGauge(const BattleChar& target)
{
    if (_shieldGauge == nullptr || target.statCom == nullptr)
        return;

    const uint32 maxShield =
        static_cast<uint32>(max(target.statCom->Get_StatInfo().commonCharacterStats.ShieldPointsAtSpawn, 0));

    if (maxShield == 0)
    {
        _shieldGauge->Close();
        return;
    }

    _shieldGauge->Open();
    _shieldGauge->Set_FillRatio(
        clamp(static_cast<float>(target.statCom->Get_CurrentShield()) / static_cast<float>(maxShield), 0.f, 1.f));
}

void UI_BossHpBar::Update_DelayHpGauge(float timeDelta, float targetRatio)
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

bool UI_BossHpBar::Is_ValidBossTarget(const BattleChar& target) const
{
    if (target.actor == nullptr || target.statCom == nullptr)
        return false;

    if (target.statCom->Is_Player())
        return false;

    if (target.statCom->Get_AliveStatus() != EAliveStatus::Alive)
        return false;

    const Shared<MonsterStatCom> monsterStat =
        dynamic_pointer_cast<MonsterStatCom>(target.statCom);

    return monsterStat != nullptr && monsterStat->Is_Boss();
}

Unique<UI_BossHpBar> UI_BossHpBar::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_BossHpBar>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        MSG_BOX("Failed to Created : UI_BossHpBar");
        return nullptr;
    }

    return instance;
}

void UI_BossHpBar::Free()
{
    Close();

    _root.reset();
    _uiByName.clear();

    _hpGauge.reset();
    _hpGaugeBG.reset();
    _shieldGauge.reset();
    _breakGauge.reset();
    _bossNameText.reset();
    _bossNameBgText.reset();

    Base::Free();
}
