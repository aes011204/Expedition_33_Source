#pragma once
#include "Player_State.h"

NS_BEGIN(Client)

class Player_Move final : public Player_State
{
public:
    Player_Move(const Shared<Player_WorldBase>& owner);
    ~Player_Move() override = default;

public:
    HRESULT Initialize() override;
    void Begin_Play() override { Player_State::Begin_Play(); }
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void LateUpdate(float timeDelta) override;
    void ExitInvoke() override;

private:
    bool isInvokePreSprint{ false };

    float _enterMoveBlendTime = 0.35f;

    // Free Aim 종료 직후 첫 이동 애니메이션에만 빠른 blend 적용
    bool  _useOneShotEnterBlendTime = false;
    const float _defaultMoveBlendTime = 0.35f;
    // Free Aim에서 손 내리며 Move로 복귀할 때만 빠르게 블렌드
    const float _freeAimExitMoveBlendTime = 0.2f;

public:
    static Shared<Player_Move> Create(const Shared<Player_WorldBase>& owner);
    virtual void Free() override;

};

NS_END
