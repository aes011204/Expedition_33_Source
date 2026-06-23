#pragma once
#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Image;
class UI_Text;
NS_END

NS_BEGIN(Client)

class PlayerStatCom;
class SL_Base;
class SL_Maelle;

class UI_SL_HUD final : public UI_RuntimePanelBase
{
public:
    UI_SL_HUD() = default;
    ~UI_SL_HUD() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

public:
    void Update_PlayerHUD(const Shared<SL_Maelle>& player);
    void Clear_PlayerHUD();
    void Update_BossHUD(const Shared<SL_Base>& boss, float timeDelta);
    void Clear_BossHUD();

    void Set_HealingPotionCount(uint32 count);
    void Show_BossDamage(float damage);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Apply_PlayerHp(float curHp, float maxHp);
    void    Apply_BossHp(float curHp, float maxHp, float timeDelta);
    void    Apply_BossName(const wstring& name);
    void    Apply_SkillIcons(const Shared<SL_Maelle>& player);
    void    Apply_SkillCooldowns(const Shared<SL_Maelle>& player);
    void    Update_BossDamageText(float timeDelta);
    void    Clear_BossDamageText();
    void    Clear_SkillSlot(uint32 slotIndex);
    void    Clear_SkillCooldown(uint32 slotIndex);
    void    Restore_DefaultSkillIcon(uint32 slotIndex);

private:
    Shared<UI_Image> _playerHpGauge{};
    Shared<UI_Text>  _playerCurrentHpText{};
    Shared<UI_Text>  _playerCurrentHpBgText{};
    Shared<UI_Text>  _playerMaxHpText{};
    Shared<UI_Text>  _playerMaxHpBgText{};

    array<Shared<UI_Image>, 4> _skillIcons{};
    array<Shared<UI_Image>, 4> _skillIconBgs{};
    array<Shared<UI_Image>, 4> _skillCooldownOverlays{};
    array<Shared<UI_Text>, 4>  _skillCooldownTexts{};

    Shared<UI_Base>  _bossHpRoot{};
    Shared<UI_Image> _bossHpGauge{};
    Shared<UI_Image> _bossHpGaugeBg{};
    Shared<UI_Text>  _bossNameText{};
    Shared<UI_Text>  _bossNameBgText{};
    float            _bossHpDelayedRatio = 1.f;

    Shared<UI_Text> _bossDamageText{};
    Shared<UI_Text> _bossDamageBgText{};
    uint32          _bossDamageAccumulated = 0;
    float           _bossDamageElapsed = 0.f;
    bool            _bossDamageVisible = false;

    Shared<UI_Base> _staminaRoot{};
    Shared<UI_Base> _potionRoot{};
    Shared<UI_Text> _healingPotionCountText{};

public:
    static Unique<UI_SL_HUD> Create(uint32 levelIndex);
    void Free() override;
};

NS_END

