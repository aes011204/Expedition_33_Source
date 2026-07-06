#pragma once

#include "Battle_Enum.h"
#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Base;
NS_END

NS_BEGIN(Client)

class UI_BattleDefense final : public UI_RuntimePanelBase
{
public:
    UI_BattleDefense() = default;
    ~UI_BattleDefense() override = default;

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;

    void    Open() override;
    void    Close() override;
    void    Play_UseAnimation(BattleReactionType reactionType);

private:
    struct UseAnimationTarget
    {
        Shared<Engine::UI_Base> ui{};
        Vec2 baseScale = Vec2(1.f, 1.f);
    };

    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();
    HRESULT Bind_UseAnimationTarget(const wchar_t* uiName, UseAnimationTarget& outTarget);
    void    Stop_UseAnimations();
    UseAnimationTarget* Resolve_UseAnimationTarget(BattleReactionType reactionType);
    const wchar_t* Resolve_UseAnimationName(BattleReactionType reactionType) const;

private:
    UseAnimationTarget _dodgeUseAnimTarget{};
    UseAnimationTarget _parryUseAnimTarget{};
    UseAnimationTarget _jumpUseAnimTarget{};

public:
    static Unique<UI_BattleDefense> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
