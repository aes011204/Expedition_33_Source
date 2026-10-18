#pragma once

#include "Player_State.h"

NS_BEGIN(Engine)
class OffsetAnimationModifier;
NS_END

NS_BEGIN(Client)

class Monster_WorldBase;

class Player_FreeAim final : public Player_State
{
private:
    struct FreeAimHitInfo
    {
        Vec3  hitPosition = Vec3::Zero;
        Vec3  shotDirection = Vec3::Look;
        Vec3  hitNormal = Vec3::Up;
    };

public:
    Player_FreeAim(const Shared<Player_WorldBase>& owner);
    ~Player_FreeAim() override = default;

public:
    HRESULT Initialize() override;
    void Begin_Play() override;
    void Refresh_BodyPlayer() override;
    bool EnterInvoke() override;
    void Update(float timeDelta) override;
    void LateUpdate(float timeDelta) override;
    void ExitInvoke() override;

public:
    bool Is_HitObject() const { return _hasHitObject; }
    Shared<GameObject> Get_HitObject() const { return _hitObject.lock(); }

    // 여기서 맞았는지 마크하고, 애니메이션의 AnimNotify에서, 충돌 판정 세팅
    void Trigger_PendingFreeAimShotHit();

private:
    void Bind_AnimationModifiers();
    void Update_DirectionalModifiers();
    void Update_EnterTurnBurst();
    void Update_FreeAimHitState();

    // 좌클릭 입력 순간의 조준 결과를 스냅샷으로 보관한다.
    void Cache_PendingFreeAimShot();
    void Clear_PendingFreeAimShot();

    Shared<GameObject> Find_FreeAimHitTarget(FreeAimHitInfo* outHitInfo = nullptr) const;
    void Play_FreeAimHitEffect() const;

private:
    Shared<OffsetAnimationModifier> _offsetModifier = nullptr;
    Shared<OffsetAnimationModifier> _walkModifiers[6];

    float _freeAimTurnSpeedDegree = 1800.f;
    float _freeAimTurnResponse = 18.f;

    float _prevTurnSpeedDegree = 0.f;
    float _prevTurnResponse = 0.f;
    bool _isEnterTurnBurstActive = false;

    float _rotationSensitivity = 0.5f;

private:
    bool _hasHitObject = false;
    Weak<GameObject> _hitObject{};

    FreeAimHitInfo _currentHitInfo {};

    // 입력 프레임의 히트 후보를 발사 notify까지 유지한다.
    bool _hasPendingShotHit = false;
    bool _hasConsumedPendingShotHit = false;
    Weak<GameObject> _pendingShotObject{};
    FreeAimHitInfo _pendingShotHitInfo{};

public:
    static Shared<Player_FreeAim> Create(const Shared<Player_WorldBase>& owner);
    virtual void Free() override;

};

NS_END
