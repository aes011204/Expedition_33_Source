#include "SL_TargetCom.h"
#include "Bounding_Sphere.h"
#include "ColliderCom.h"
#include "Collision_Define.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "TransformCom.h"
#include "Character.h"

IMPLEMENT_REFLECTION(SL_TargetCom)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.category = "Component";
    info.displayName = "TargetComponent";

    return true;
}

SL_TargetCom::SL_TargetCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component(device, context)
{
}

SL_TargetCom::SL_TargetCom(const SL_TargetCom& prototype)
    : Component(prototype)
    , _targetRadius(prototype._targetRadius)
{
}

HRESULT SL_TargetCom::Initialize_Prototype()
{
    CHECK_FAILED(Component::Initialize_Prototype(), E_FAIL);

    _isLocked = false;
    _lockedTarget.reset();
    _candidates.clear();

    return S_OK;
}

HRESULT SL_TargetCom::Initialize(void* arg)
{
    CHECK_FAILED(Component::Initialize(arg), E_FAIL);

    Bounding_Sphere::FBoundingSphereDesc sphereDesc{};
    sphereDesc.radius = _targetRadius;

    _targetCollider = dynamic_pointer_cast<ColliderCom>(
        GAME->Clone_Prototype(Prototype::Comopnent, ETOI(LevelType::Static), TEXT("Com_Collider_Sphere"), &sphereDesc));
    CHECK_NULL(_targetCollider, E_FAIL);

    // 공격 판정이 아니라 락온 후보 검색용 overlap sphere다.
    _targetCollider->Set_CollisionPreset(Collision_Preset::SL_TargetSensor);
    _targetCollider->Set_IsActive(true);

    _isLocked = false;
    _lockedTarget.reset();
    _candidates.clear();

    return S_OK;
}

void SL_TargetCom::BeginPlay()
{
    Component::BeginPlay();

    if (_targetCollider)
        _targetCollider->Set_Owner(Get_Owner());
}

void SL_TargetCom::Update_Targeting(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    Shared<GameObject> owner = Get_Owner();
    if (owner == nullptr || owner->Get_Transform() == nullptr || _targetCollider == nullptr)
        return;

    if (_targetCollider->Get_Owner() == nullptr)
        _targetCollider->Set_Owner(owner);

    _targetCollider->Update_Collider(owner->Get_Transform()->Get_WorldMatrix());
    GAME->Add_Collider(_targetCollider);

    Update_Candidates();
    Refresh_LockedTarget();
}

void SL_TargetCom::Register_Candidate(const Shared<Character>& candidate)
{
    if (!candidate || candidate->Is_MarkDestroy())
        return;

    if (Is_TargetInCandidates(candidate))
        return;

    _candidates.push_back(candidate);
}

void SL_TargetCom::Clear_Candidates()
{
    _candidates.clear();
    Clear_Lock();
}

void SL_TargetCom::Set_DebugTarget(const Shared<Character>& target)
{
    if (target == nullptr || target->Is_MarkDestroy())
    {
        Clear_Lock();
        return;
    }

    _lockedTarget = target;
    _isLocked = true;
}

void SL_TargetCom::Toggle_LockOn()
{
    if (_isLocked)
    {
        Clear_Lock();
        return;
    }

    LockOn_NearestTarget();
}

void SL_TargetCom::Clear_Lock()
{
    _isLocked = false;
    _lockedTarget.reset();
}

void SL_TargetCom::Update_Candidates()
{
     _candidates.clear();

    if (_targetCollider == nullptr)
        return;

    Shared<GameObject> owner = Get_Owner();
    const auto& overlapSet = _targetCollider->Get_OverlapSet();

    for (auto& weakOtherCollider : overlapSet)
    {
        if (weakOtherCollider.expired())
            continue;

        Shared<ColliderCom> otherCollider = weakOtherCollider.lock();
        if (otherCollider == nullptr)
            continue;

        Shared<GameObject> otherOwner = otherCollider->Get_Owner();
        if (otherOwner == nullptr || otherOwner->Is_MarkDestroy())
            continue;

        if (owner != nullptr && otherOwner.get() == owner.get())
            continue;

        Shared<Character> candidate = dynamic_pointer_cast<Character>(otherOwner);
        if (candidate == nullptr)
            continue;

        _candidates.push_back(candidate);
    }
}

void SL_TargetCom::Refresh_LockedTarget()
{
    if (!_isLocked)
        return;

    Shared<Character> lockedTarget = _lockedTarget.lock();
    if (lockedTarget == nullptr || lockedTarget->Is_MarkDestroy() || !Is_TargetInCandidates(lockedTarget))
        Clear_Lock();
}

void SL_TargetCom::LockOn_NearestTarget()
{
    Shared<Character> nearestTarget = Find_NearestTarget();
    if (nearestTarget == nullptr)
    {
        Clear_Lock();
        return;
    }

    _lockedTarget = nearestTarget;
    _isLocked = true;
}

Shared<Character> SL_TargetCom::Find_NearestTarget() const
{
    Shared<GameObject> owner = Get_Owner();
    if (owner == nullptr || owner->Get_Transform() == nullptr)
        return nullptr;

    const Vec3 ownerPos = owner->Get_Transform()->Get_WorldPosition();

    float minDistance = FLT_MAX;
    Shared<Character> nearestTarget = nullptr;

    for (const Weak<Character>& weakCandidate : _candidates)
    {
        if (weakCandidate.expired())
            continue;

        Shared<Character> candidate = weakCandidate.lock();
        if (candidate == nullptr || candidate->Is_MarkDestroy() || candidate->Get_Transform() == nullptr)
            continue;

        const float distance = Vec3::Distance(ownerPos, candidate->Get_Transform()->Get_WorldPosition());
        if (distance < minDistance)
        {
            minDistance = distance;
            nearestTarget = candidate;
        }
    }

    return nearestTarget;
}

bool SL_TargetCom::Is_TargetInCandidates(const Shared<GameObject>& target) const
{
    if (target == nullptr)
        return false;

    for (const Weak<Character>& weakCandidate : _candidates)
    {
        if (!weakCandidate.expired() && weakCandidate.lock() == target)
            return true;
    }

    return false;
}

Shared<SL_TargetCom> SL_TargetCom::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<SL_TargetCom>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : SL_TargetComponent");
        return nullptr;
    }

    return instance;
}

Shared<Component> SL_TargetCom::Clone(void* arg)
{
    auto clone = make_shared<SL_TargetCom>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : SL_TargetComponent");
        return nullptr;
    }

    return clone;
}

void SL_TargetCom::Free()
{
    _candidates.clear();
    _lockedTarget.reset();
    _targetCollider.reset();

    Component::Free();
}
