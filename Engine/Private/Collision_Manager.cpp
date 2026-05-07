#include "Collision_Manager.h"
#include "ColliderCom.h"
#include "GameInstance.h"
#include "GameObject.h"

HRESULT Collision_Manager::Initialize()
{
    _colliders.clear();

    return S_OK;
}

void Collision_Manager::Update()
{
#ifdef _DEBUG
    if (GAME->KeyDown(KEY_TYPE::F1))
        _isDebug = !_isDebug;
#endif
}

void Collision_Manager::Update_FromPhysX(const vector<PhysXPairEvent>& events)
{
#ifdef _DEBUG
    const auto updateBegin = chrono::steady_clock::now();
    UpdatePerfSample perfSample{};
#endif

    for (const auto& weakCollider : _colliders)
    {
        Shared<ColliderCom> collider = weakCollider.lock();
        if (collider == nullptr)
            continue;

        collider->Set_IsColl(false);

#ifdef _DEBUG
        if (collider->Get_IsActive())
            ++perfSample.activeColliderCount;
#endif
    }

    for (const PhysXPairEvent& event : events)
    {
        Shared<ColliderCom> self = event.self.lock();
        Shared<ColliderCom> other = event.other.lock();

        if (self == nullptr || other == nullptr)
            continue;

        if (!self->Get_IsActive() || !other->Get_IsActive())
            continue;

        Shared<GameObject> selfOwner = self->Get_Owner();
        Shared<GameObject> otherOwner = other->Get_Owner();

        if (selfOwner == nullptr || otherOwner == nullptr)
            continue;

        if (selfOwner == otherOwner)
            continue;

#ifdef _DEBUG
        ++perfSample.hitCount;
#endif

        self->Set_IsColl(true);
        other->Set_IsColl(true);

        if (event.response == CollisionResponse::Overlap)
        {
            if (event.began)
            {
                self->Add_Overlap(other);
                selfOwner->OnBeginOverlap(self, other);
            }
            else if (self->Is_Overlapping(other))
            {
                selfOwner->OnStayOverlap(self, other);
            }

            if (event.ended)
            {
                self->Remove_Overlap(other);
                selfOwner->OnEndOverlap(self, other);
            }
        }
        else if (event.response == CollisionResponse::Block)
        {
            if (event.began)
                selfOwner->OnBlockBegin(self, other);

            if (event.staying)
                selfOwner->OnBlockStay(self, other);
        }
    }

    const size_t colliderCount = _colliders.size();

    for (size_t i = 0; i < colliderCount; ++i)
    {
        Shared<ColliderCom> src = _colliders[i].lock();
        if (src == nullptr || !src->Get_IsActive())
            continue;

        Shared<GameObject> srcOwner = src->Get_Owner();
        if (srcOwner == nullptr)
            continue;

        for (size_t j = i + 1; j < colliderCount; ++j)
        {
            Shared<ColliderCom> dst = _colliders[j].lock();
            if (dst == nullptr || !dst->Get_IsActive())
                continue;

            Shared<GameObject> dstOwner = dst->Get_Owner();
            if (dstOwner == nullptr || srcOwner == dstOwner)
                continue;

            if (!src->Is_Overlapping(dst))
                continue;

            src->Set_IsColl(true);
            dst->Set_IsColl(true);

            srcOwner->OnStayOverlap(src, dst);
            dstOwner->OnStayOverlap(dst, src);
        }
    }

#ifdef _DEBUG
    if (GAME->KeyDown(KEY_TYPE::F1))
        _isDebug = !_isDebug;

    perfSample.updateMs = chrono::duration<double, milli>(chrono::steady_clock::now() - updateBegin).count();
    Accumulate_UpdatePerf(perfSample);
#endif
}

#ifdef _DEBUG
bool Collision_Manager::Try_ConsumePerfSummary(const float windowSec, CollisionPerfSummary& outSummary)
{
    outSummary = {};

    if (windowSec <= 0.f || 0 == _collisionPerfWindow.updateFrameCount)
        return false;

    const double updateFrameCount = static_cast<double>(_collisionPerfWindow.updateFrameCount);

    outSummary.valid = true;
    outSummary.updateFrameCount = static_cast<uint32>(_collisionPerfWindow.updateFrameCount);
    outSummary.activeCollidersAvg = static_cast<float>(
        Safe_Divide(static_cast<double>(_collisionPerfWindow.activeColliderCount), updateFrameCount));
    outSummary.pairsPerSec = static_cast<float>(
        Safe_Divide(static_cast<double>(_collisionPerfWindow.pairCandidateCount), windowSec));
    outSummary.intersectsPerSec = static_cast<float>(
        Safe_Divide(static_cast<double>(_collisionPerfWindow.intersectsCallCount), windowSec));
    outSummary.hitsPerSec = static_cast<float>(
        Safe_Divide(static_cast<double>(_collisionPerfWindow.hitCount), windowSec));
    outSummary.updateAvgMs = static_cast<float>(
        Safe_Divide(_collisionPerfWindow.updateAccumulatedMs, updateFrameCount));
    outSummary.updateMaxMs = static_cast<float>(_collisionPerfWindow.updateMaxMs);

    Reset_CollisionPerfWindow(_collisionPerfWindow);
    return true;
}

void Collision_Manager::Render_Debug()
{
    if (!_isDebug)
        return;

    for (const auto& colliderWeak : _colliders)
    {
        auto collider = colliderWeak.lock();

        if (collider)
        {
            collider->Render_Debug();
        }
    }
}

void Collision_Manager::Accumulate_UpdatePerf(const UpdatePerfSample& sample)
{
    ++_collisionPerfWindow.updateFrameCount;
    _collisionPerfWindow.activeColliderCount += sample.activeColliderCount;
    _collisionPerfWindow.pairCandidateCount += sample.pairCandidateCount;
    _collisionPerfWindow.intersectsCallCount += sample.intersectsCallCount;
    _collisionPerfWindow.hitCount += sample.hitCount;
    _collisionPerfWindow.updateAccumulatedMs += sample.updateMs;
    _collisionPerfWindow.updateMaxMs = max(_collisionPerfWindow.updateMaxMs, sample.updateMs);
}

void Collision_Manager::Reset_CollisionPerfWindow(CollisionPerfWindow& window)
{
    window = {};
}

double Collision_Manager::Safe_Divide(const double numerator, const double denominator)
{
    if (denominator <= 0.0)
        return 0.0;

    return numerator / denominator;
}
#endif

void Collision_Manager::Add_Collider(Shared<ColliderCom> collider)
{
    CHECK_NULL(collider);

    _colliders.push_back(collider);
}

void Collision_Manager::Clear_Colliders()
{
    _colliders.clear();
}

Unique<Collision_Manager> Collision_Manager::Create()
{
    auto instance = make_unique<Collision_Manager>();
    instance->Initialize();

    return instance;
}

void Collision_Manager::Free()
{
    Base::Free();

    _colliders.clear();
}
