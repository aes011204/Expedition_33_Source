#include "UI_BossHpBar.h"

#include "Client_Constants.h"
#include "MonsterStatCom.h"
#include "UI_Base.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr wchar_t kBrokenShieldAnimationName[] = L"Broken_Shield";
    constexpr float kBossShieldGaugeCenterX = 399.745f;
    constexpr float kBossShieldGaugeCenterY = 33.781f;
    constexpr float kBossShieldIconSpacing = 36.f;

    wstring Resolve_BossDisplayName(wstring name)
    {
        if (!name.empty() && (name.back() == L'\uAC00' || name.back() == L'\uC774'))
            name.pop_back();

        return name;
    }
}

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
    Update_ShieldIconAnimations(timeDelta);
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
    const bool wasOpen = Is_Open() || (_root != nullptr && _root->Is_Open());

    Close_ShieldIcons();

    if (!wasOpen)
        return;

    _displayTarget = {};
    _hasDisplayTarget = false;
    _hpRatio = 1.f;
    _delayedHpRatio = 1.f;
    _maxShield = 0;
    _lastShieldIconCount = 0;
    _hasShieldIconState = false;

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
        _maxShield = Resolve_MaxShield(target);
        Apply_HpGauge(_hpRatio);
        Apply_DelayedHpGauge(_delayedHpRatio);
        Apply_ShieldGauge(target);
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

    const bool changedTarget = (!_hasDisplayTarget || _displayTarget.actor != target.actor);

    _displayTarget = target;
    _hasDisplayTarget = true;

    if (changedTarget)
        _maxShield = Resolve_MaxShield(target);

    _hpRatio = curHp / maxHp;
    _delayedHpRatio = max(_delayedHpRatio, prevHp / maxHp);

    Apply_HpGauge(_hpRatio);
    Apply_DelayedHpGauge(_delayedHpRatio);
    Apply_ShieldGauge(target);
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

    CHECK_FAILED(Load_ShieldIcons(levelIndex), E_FAIL);

    return S_OK;
}

HRESULT UI_BossHpBar::Load_ShieldIcons(uint32 levelIndex)
{
    for (uint32 i = 0; i < kMaxBossShieldIconCount; ++i)
    {
        umap<wstring, Shared<UI_Base>> shieldUIByName{};

        UI_RuntimeLoader::UI_LoadDesc desc{};
        desc.levelIndex = levelIndex;
        desc.jsonPath = Constants::UI::UI_Shield_Path;
        desc.animationPaths.push_back(Constants::UI_Anim::UI_BrokenShieldAnim_Path);
        desc.prototypeTagPrefix = L"BossHpBarShield_" + to_wstring(i);
        desc.outUIByName = &shieldUIByName;

        CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

        Shared<UI_Base> shieldIcon = Find_UI(shieldUIByName, L"Image_Shield");
        CHECK_NULL(shieldIcon, E_FAIL);

        shieldIcon->Clear_Parent();

        if (UI_Transform* transform = shieldIcon->Get_Transform())
        {
            const Vec2 screenCenter = Resolve_ShieldIconScreenCenter();
            const float leftOffset =
                (static_cast<float>(kMaxBossShieldIconCount) - 1.f) * 0.5f * kBossShieldIconSpacing;

            transform->Set_Anchor(Vec2(0.f, 0.f));
            transform->Set_Position(
                Vec2(screenCenter.x - leftOffset + static_cast<float>(i) * kBossShieldIconSpacing, screenCenter.y));
        }

        shieldIcon->Refresh_OpenBaseTransform();
        shieldIcon->Close();
        _shieldIcons[i] = shieldIcon;

        for (auto& [name, ui] : shieldUIByName)
            _uiByName[L"ShieldIcon_" + to_wstring(i) + L"_" + name] = ui;
    }

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
    _shieldGauge->Close();

    if (_breakGauge)
        _breakGauge->Close();

    return S_OK;
}

void UI_BossHpBar::Sync_BossState(const BattleChar& target)
{
    const float maxHp = max(target.statCom->Get_StatInfo().maxHp, 1.f);
    const float curHp = clamp(target.statCom->Get_CurHp(), 0.f, maxHp);

    _hpRatio = curHp / maxHp;
    _maxShield = max(_maxShield, Resolve_MaxShield(target));
    Apply_HpGauge(_hpRatio);
    Apply_ShieldGauge(target);

    const Shared<MonsterStatCom> monsterStat =
        dynamic_pointer_cast<MonsterStatCom>(target.statCom);

    const wstring name = monsterStat ? Resolve_BossDisplayName(monsterStat->Get_MonsterDesc().battleAnnounceName) : L"";

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
    if (_shieldGauge == nullptr || _hpGauge == nullptr || target.statCom == nullptr)
        return;

    uint32 currentShield = target.statCom->Get_CurrentShield();
    if (!_hasShieldIconState && currentShield == 0 && _maxShield > 0)
        currentShield = min(_maxShield, kMaxBossShieldIconCount);

    const bool hasShield = (currentShield > 0);

    Apply_ShieldIcons(currentShield);

    if (hasShield)
    {
        if (_hpGauge->Is_Open())
            _hpGauge->Close();

        if (!_shieldGauge->Is_Open())
            _shieldGauge->Open();

        _shieldGauge->Set_FillRatio(clamp(_hpRatio, 0.f, 1.f));
        return;
    }

    if (!_hpGauge->Is_Open())
        _hpGauge->Open();

    if (_shieldGauge->Is_Open())
        _shieldGauge->Close();
}

void UI_BossHpBar::Apply_ShieldIcons(uint32 currentShield)
{
    const uint32 maxShield = min(kMaxBossShieldIconCount, max(_maxShield, currentShield));
    const uint32 visibleShield = min(currentShield, maxShield);
    const uint32 consumedShield = maxShield - visibleShield;

    if (_hasShieldIconState && currentShield < _lastShieldIconCount)
    {
        const uint32 previousVisibleShield = min(_lastShieldIconCount, maxShield);
        const uint32 previousConsumedShield = maxShield - previousVisibleShield;

        for (uint32 i = previousConsumedShield; i < consumedShield; ++i)
            Play_BrokenShieldIcon(i);
    }

    const bool deferReposition = Has_ActiveBrokenShieldAnimation();

    for (uint32 i = 0; i < kMaxBossShieldIconCount; ++i)
    {
        if (_shieldIcons[i] == nullptr)
            continue;

        if (i < maxShield && i >= consumedShield)
        {
            Reset_ShieldIconDefaultVisual(i);

            if (deferReposition)
                Set_ShieldIconPosition(i, i, maxShield);
            else
                Set_ShieldIconPosition(i, i - consumedShield, visibleShield);

            if (!_shieldIcons[i]->Is_Open())
                _shieldIcons[i]->Open();
        }
        else if (_shieldIconCloseTimers[i] <= 0.f)
        {
            _shieldIcons[i]->Close();
        }
    }

    _lastShieldIconCount = currentShield;
    _hasShieldIconState = true;
}

void UI_BossHpBar::Reset_ShieldIconDefaultVisual(uint32 iconIndex)
{
    if (iconIndex >= kMaxBossShieldIconCount || _shieldIcons[iconIndex] == nullptr)
        return;

    _shieldIcons[iconIndex]->Stop_UIAnimation();
    _shieldIcons[iconIndex]->Set_AlphaMultiplier(1.f);
    _shieldIcons[iconIndex]->Set_Color(Vec4(1.f, 1.f, 1.f, 1.f));

    if (UI_Transform* transform = _shieldIcons[iconIndex]->Get_Transform())
    {
        transform->Set_Scale(Vec2(1.f, 1.f));
        transform->Set_RotationRadians(0.f);
    }
}

void UI_BossHpBar::Set_ShieldIconPosition(uint32 iconIndex, uint32 visibleOrder, uint32 visibleCount)
{
    if (iconIndex >= kMaxBossShieldIconCount || visibleCount == 0 || _shieldIcons[iconIndex] == nullptr)
        return;

    UI_Transform* transform = _shieldIcons[iconIndex]->Get_Transform();
    if (transform == nullptr)
        return;

    const float leftOffset =
        (static_cast<float>(visibleCount) - 1.f) * 0.5f * kBossShieldIconSpacing;
    const Vec2 screenCenter = Resolve_ShieldIconScreenCenter();

    transform->Set_Anchor(Vec2(0.f, 0.f));
    transform->Set_Position(
        Vec2(screenCenter.x - leftOffset + static_cast<float>(visibleOrder) * kBossShieldIconSpacing, screenCenter.y));
    _shieldIcons[iconIndex]->Refresh_OpenBaseTransform();
}

Vec2 UI_BossHpBar::Resolve_ShieldIconScreenCenter() const
{
    if (_root == nullptr)
        return Vec2(kBossShieldGaugeCenterX, kBossShieldGaugeCenterY);

    const Vec2 rootTopLeft = _root->Get_ResolvedLocalTopLeft();
    return Vec2(rootTopLeft.x + kBossShieldGaugeCenterX, rootTopLeft.y + kBossShieldGaugeCenterY);
}

void UI_BossHpBar::Play_BrokenShieldIcon(uint32 iconIndex)
{
    if (iconIndex >= kMaxBossShieldIconCount || _shieldIcons[iconIndex] == nullptr)
        return;

    _shieldIcons[iconIndex]->Open();
    _shieldIcons[iconIndex]->Play_UIAnimation(kBrokenShieldAnimationName, true);
    _shieldIconCloseTimers[iconIndex] = kBrokenShieldAnimationDurationSec;
}

bool UI_BossHpBar::Has_ActiveBrokenShieldAnimation() const
{
    for (const float closeTimer : _shieldIconCloseTimers)
    {
        if (closeTimer > 0.f)
            return true;
    }

    return false;
}

void UI_BossHpBar::Update_ShieldIconAnimations(float timeDelta)
{
    for (uint32 i = 0; i < kMaxBossShieldIconCount; ++i)
    {
        if (_shieldIconCloseTimers[i] <= 0.f)
            continue;

        _shieldIconCloseTimers[i] = max(0.f, _shieldIconCloseTimers[i] - max(0.f, timeDelta));
        if (_shieldIconCloseTimers[i] > 0.f)
            continue;

        if (_shieldIcons[i])
            _shieldIcons[i]->Close();
    }
}

void UI_BossHpBar::Close_ShieldIcons()
{
    for (Shared<UI_Base>& shieldIcon : _shieldIcons)
    {
        if (shieldIcon)
            shieldIcon->Close();
    }

    _shieldIconCloseTimers.fill(0.f);
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

uint32 UI_BossHpBar::Resolve_MaxShield(const BattleChar& target) const
{
    if (target.statCom == nullptr)
        return 0;

    const uint32 spawnShield =
        static_cast<uint32>(max(target.statCom->Get_StatInfo().commonCharacterStats.ShieldPointsAtSpawn, 0));

    return max(spawnShield, target.statCom->Get_CurrentShield());
}

bool UI_BossHpBar::Is_ValidBossTarget(const BattleChar& target) const
{
    if (target.statCom == nullptr)
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

    for (Shared<UI_Base>& shieldIcon : _shieldIcons)
        shieldIcon.reset();

    Base::Free();
}
