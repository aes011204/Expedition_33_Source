#pragma once

#include "UI_RuntimePanelBase.h"
#include "Battle_Enum.h"

NS_BEGIN(Engine)
class UI_Base;
NS_END

NS_BEGIN(Client)

class UI_MarkEffect final : public UI_RuntimePanelBase
{
public:
    UI_MarkEffect() = default;
    ~UI_MarkEffect() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

public:
    void    Open() override;
    void    Close() override;

    void    Show_Target(const BattleChar& target);
    void    Clear_Target();

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    bool    Is_ValidTarget(const BattleChar& target) const;
    bool    Try_GetMarkWorldPosition(const BattleChar& target, Vec3& outWorldPos) const;

private:
    Shared<UI_Base> _markEffectPanel{};

    BattleChar _target{};
    bool       _hasTarget = false;

public:
    static Unique<UI_MarkEffect> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
