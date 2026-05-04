#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class ColliderCom;

class ENGINE_DLL Collision_Manager : public Base
{
public:
    Collision_Manager() = default;
    virtual ~Collision_Manager() = default;

public:
    HRESULT Initialize();
    void    Update();

#ifdef _DEBUG
    bool Try_ConsumePerfSummary(float windowSec, CollisionPerfSummary& outSummary);
    void Render_Debug();
#endif

public:
    void    Add_Collider(Shared<ColliderCom> collider);
    void    Clear_Colliders();

private:
    vector<Weak<ColliderCom>> _colliders;
    bool    _isDebug = true;

#ifdef _DEBUG
private:
    struct UpdatePerfSample
    {
        size_t activeColliderCount{};
        size_t pairCandidateCount{};
        size_t intersectsCallCount{};
        size_t hitCount{};
        double updateMs{};
    };

    struct CollisionPerfWindow
    {
        size_t updateFrameCount{};
        size_t activeColliderCount{};
        size_t pairCandidateCount{};
        size_t intersectsCallCount{};
        size_t hitCount{};
        double updateAccumulatedMs{};
        double updateMaxMs{};
    };

    CollisionPerfWindow _collisionPerfWindow{};

    void Accumulate_UpdatePerf(const UpdatePerfSample& sample);
    static void Reset_CollisionPerfWindow(CollisionPerfWindow& window);
    static double Safe_Divide(double numerator, double denominator);
#endif

public:
    static Unique<Collision_Manager> Create();
    virtual void Free() override;

};

NS_END
