#pragma once
#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Image;
class UI_Text;
NS_END

NS_BEGIN(Client)

class PlayerStatCom;
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

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Apply_PlayerHp(float curHp, float maxHp);
    void    Apply_SkillIcons(const Shared<PlayerStatCom>& playerStat);
    void    Clear_SkillSlot(uint32 slotIndex);
    wstring Resolve_SkillPortraitTexturePath(const string& skillKey) const;

private:
    Shared<UI_Image> _playerHpGauge{};
    Shared<UI_Text>  _playerCurrentHpText{};
    Shared<UI_Text>  _playerCurrentHpBgText{};
    Shared<UI_Text>  _playerMaxHpText{};
    Shared<UI_Text>  _playerMaxHpBgText{};

    array<Shared<UI_Image>, 4> _skillIcons{};
    array<Shared<UI_Image>, 4> _skillIconBgs{};

    Shared<UI_Base> _staminaRoot{};
    Shared<UI_Base> _potionRoot{};

public:
    static Unique<UI_SL_HUD> Create(uint32 levelIndex);
    void Free() override;
};

NS_END

