#pragma once

#include "Base.h"
#include "Engine_Constants.h"
#include "PhysX_Manager.h"
#include "Log_Define.h"

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
    void    Update_FromPhysX(const vector<PhysXPairEvent>& events);

#ifdef _DEBUG
    bool Try_ConsumePerfSummary(float windowSec, CollisionPerfSummary& outSummary);
#endif

    void Render_Debug();

public:
    void    Add_Collider(Shared<ColliderCom> collider);
    void    Clear_Colliders();

private:
    vector<Weak<ColliderCom>> _colliders;
    bool    _isDebug = false;

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
    chrono::steady_clock::time_point _lastPerfSampleTime{};
    bool _hasLastPerfSampleTime{ false };

    void Accumulate_UpdatePerf(const UpdatePerfSample& sample);
    bool Should_SampleUpdatePerf();
    static void Reset_CollisionPerfWindow(CollisionPerfWindow& window);
    static double Safe_Divide(double numerator, double denominator);
#endif

public:
    static Unique<Collision_Manager> Create();
    virtual void Free() override;

};

NS_END
