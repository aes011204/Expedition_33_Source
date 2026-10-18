#pragma once

#include "Battle_State.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

class FreeAimTargeting final : public Battle_State
{
private:
    enum class FreeAimAnimState { None, EnterTransition, Idle, };

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
    void Notify_CanFreeAimShoot();

private:
    // 현재 배틀 카메라 기준 히트스캔으로 맞은 적 찾기
    bool Try_FindHitEnemy(BattleFreeAimHitTarget& outHitTarget) const; 
    void Refresh_HitTarget();                          
    void Clear_HitTarget();                            

    bool Is_CurrentActorAnimFinished() const;
    bool Play_CurrentActorBattleAnim(BattleAnimType animType, float blendTime, bool isLoop) const;

    // 플레이어 회전
    void Update_FreeAimPlayerYaw(float timeDelta);

private:
    BattleFreeAimHitTarget  _currentHitTarget = {};
    bool _hasCurrentHitTarget = false;

    FreeAimAnimState _freeAimAnimState = FreeAimAnimState::None;
    bool _canShootDuringCurrentShot = false;
    bool _isFreeAimExitRButtonArmed = false;

    Vec3 _freeAimStartRotation = Vec3::Zero;
    bool _hasFreeAimStartRotation = false;

public:
    static Shared<FreeAimTargeting> Create(const Shared<BattleController>& owner);
    void Free() override;
};

NS_END
