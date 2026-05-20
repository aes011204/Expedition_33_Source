#pragma once

#include "UI_RuntimePanelBase.h"
#include "Battle_Enum.h"

NS_BEGIN(Engine)
class UI_Base;
class UI_Button;
NS_END

NS_BEGIN(Client)

class UI_BattleCommandHUD final : public UI_RuntimePanelBase
{
public:
    UI_BattleCommandHUD() = default;
    ~UI_BattleCommandHUD() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;

    void    Refresh_ForState(BattleTurnState state);

public:
    void    Set_TargetWorldPosition(const Vec3& worldPos);

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Apply_DefaultHighlight();
    void    Set_ButtonHighlighted(const Shared<UI_Base>& button, bool highlighted);

private:
    Shared<UI_Base> _attackButton{};
    Shared<UI_Base> _skillButton{};
    Shared<UI_Base> _itemButton{};
    Shared<UI_Base> _shotButton{};

private:
    Vec2 _normalScale = Vec2(1.f, 1.f);
    Vec2 _highlightScale = Vec2(1.08f, 1.08f);

    Vec3 _targetWorldPos = Vec3::Zero;
    bool _hasTargetWorldPos = false;

public:
    static Unique<UI_BattleCommandHUD> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
