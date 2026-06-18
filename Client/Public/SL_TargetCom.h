#pragma once

#include "Component.h"

NS_BEGIN(Engine)
class Character;
NS_END

NS_BEGIN(Client)

class SL_TargetCom : public Component
{
    GENERATED_COMPONENT(SL_TargetCom)

public:
    SL_TargetCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    SL_TargetCom(const SL_TargetCom& prototype);
    virtual ~SL_TargetCom() = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;

    void    Update_Targeting(float timeDelta);

public:
    bool Is_LockOn() const { return _isLocked; }
    Shared<Character> Get_LockedTarget() const { return _lockedTarget.lock(); }

    void Register_Candidate(const Shared<Character>& candidate);
    void Clear_Candidates();

    void Toggle_LockOn();
    void Clear_Lock();

    // 보스 스폰 전 임시 연결/테스트용 진입점.
    void Set_DebugTarget(const Shared<Character>& target);

private:
    void Update_Candidates();

    void Refresh_LockedTarget();
    void LockOn_NearestTarget();
    Shared<Character> Find_NearestTarget() const;
    bool Is_TargetInCandidates(const Shared<GameObject>& target) const;

private:
    Shared<ColliderCom> _targetCollider;
    Weak<Character> _lockedTarget;
    vector<Weak<Character>> _candidates;

    bool _isLocked = false;
    float _targetRadius = 15.f;

public:
    static Shared<SL_TargetCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
};

NS_END
