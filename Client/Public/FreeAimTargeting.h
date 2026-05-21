#pragma once

#include "Battle_State.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

class FreeAimTargeting final : public Battle_State
{
public:
    FreeAimTargeting(const Shared<BattleController>& owner);
    ~FreeAimTargeting() override = default;

public:
    HRESULT Initialize() override;
    void Begin_Play() override;
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void LateUpdate(float timeDelta) override;
    void ExitInvoke() override;

private:
    // 현재 배틀 카메라 기준 히트스캔으로 맞은 적 찾기
    bool Try_FindHitEnemy(BattleChar& outEnemy) const; 
    void Refresh_HitTarget();                          
    void Clear_HitTarget();                            

private:
    BattleChar _currentHitTarget = {};
    bool _hasCurrentHitTarget = false;

public:
    static Shared<FreeAimTargeting> Create(const Shared<BattleController>& owner);
    void Free() override;
};

NS_END
