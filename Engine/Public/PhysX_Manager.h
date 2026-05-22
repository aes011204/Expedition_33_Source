#pragma once
#include "Base.h"
#include "Collision_Define.h"
#include "PhysX_Types.h"
#include "CollisionProxy_Types.h"

NS_BEGIN(Engine)

class ColliderCom;
struct PhysXQueryPerfSummary;
struct StaticCollisionData;

class ENGINE_DLL PhysX_Manager final
    : public Base
    , public PxSimulationEventCallback // 충돌 이벤트 콜벡을 받기 위해서 상속
{
public:
    PhysX_Manager();
    ~PhysX_Manager() override;

public:
    PxPhysics*              Get_Physics() const { return _physics; }
    PxScene*                Get_Scene() const { return _scene; }
    PxControllerManager*    Get_Controller() const {return _controllerManager; }

public:
    HRESULT     Initialize();
    void        Update(float timeDelta);

public:
    void Begin_Frame();

    // 아직 actor가 없으면 만들고 scene에 추가, 있으면 위치만 맞추기
    void Register_OrSync_Collider(Shared<ColliderCom> collider, const Matrix& worldMatrix);
    void Gather_FrameEvents(vector<PhysXPairEvent>& outEvents);

    PxController* Create_CapsuleController(
        const CharacterControllerDesc& desc,
        const Vec3& initialFootPosition);

    void Release_Controller(PxController* controller);

public: /* PxSimulationEventCallback */
    void onConstraintBreak(PxConstraintInfo* constraints, PxU32 count) override {}
    void onWake(PxActor** actors, PxU32 count) override {}
    void onSleep(PxActor** actors, PxU32 count) override {}
    void onAdvance(const PxRigidBody* const* bodyBuffer, const PxTransform* poseBuffer, const PxU32 count) override {}

    void onTrigger(PxTriggerPair* pairs, PxU32 count) override;
    void onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs) override;

public:
    PhysXStaticActorHandle Register_StaticCollision(
        const StaticCollisionData& collisionData,
        const Matrix& worldMatrix,
        Collision_Preset preset);

    vector<PxTriangleMesh*> Build_OrGetTriangleMeshes(const StaticCollisionData& collisionData);
    void Unregister_StaticActor(PhysXStaticActorHandle handle);

    bool Cook_TriangleMesh(const ModelCollisionMesh& meshData, const Matrix& preTransformMatrix, vector<uint8_t>& outCookedData);
    PxTriangleMesh* Create_TriangleMeshFromCooked(const vector<uint8_t>& cookedData);

#ifdef _DEBUG
    bool Try_ConsumeQueryPerfSummary(float windowSec, PhysXQueryPerfSummary& outSummary);
#endif

private:
    Shared<ColliderCom> Find_Collider(PxActor* actor) const;

    void Push_PairEvent(Shared<ColliderCom> self, Shared<ColliderCom> other,
						CollisionResponse response, bool began, bool staying, bool ended);

public: /* Camera Collision Test */
    bool Sweep_Sphere(
            const Vec3& origin,             // 구체 sweep 시작 위치
            const Vec3& direction,          // 이동 방향
            float distance,                 // 얼마나 멀리?
            float radius,                   // 구체 반지름, 카메라 probe 크기
            Collision_Preset queryPreset,   // 충돌 규칙
            PhysXQueryHit& outHit) const;   // 충돌 결과

	// 현재 충돌체 모양을 가져와서 주어진 방향으로, 주어진 거리만큼 query를 날려서
	// 맞았는지, 얼마나 갔는지, 어디에 맞았는지 결과를 채워줌
	bool Sweep_Collider(ColliderCom* collider, const Matrix& worldMatrix,
		                const Vec3& direction, float distance, PhysXQueryHit& outHit) const;

	// 현재 캐릭터 발 밑으로 짧게 아래 방향 query를 날리고 바닥이 있는지 판별용
	bool Probe_Ground(ColliderCom* collider, const Matrix& worldMatrix,
		                float probeDistance, PhysXQueryHit& outHit) const;


private:
    PxTriangleMesh* Create_TriangleMesh(const ModelCollisionMesh& meshData, const Matrix& preTransformMatrix);

private:
    PxDefaultAllocator          _allocator;
    PxDefaultErrorCallback      _errorCallback;

    PxFoundation*               _foundation = {};
    PxPhysics*                  _physics = {};
    PxScene*                    _scene = {};
    PxDefaultCpuDispatcher*     _dispatcher = {};
    PxControllerManager*        _controllerManager = {};

private:
    vector<PhysXPairEvent>              _frameEvents;
    umap<PxActor*, Weak<ColliderCom>>   _actorColliderMap;

#ifdef _DEBUG
    PxPvd* _pvd = {};

private:
    struct QueryPerfCounter
    {
        size_t sampleCount{};
        size_t hitCount{};
        double accumulatedMs{};
        double maxMs{};
    };

    struct QueryPerfWindow
    {
        QueryPerfCounter cctMove{};
        QueryPerfCounter sweepCollider{};
        QueryPerfCounter sweepSphere{};
        QueryPerfCounter probeGround{};
    };

    QueryPerfWindow _queryPerfWindow{};

    void Accumulate_QueryPerf(QueryPerfCounter& counter, bool hit, double elapsedMs);
    static void Reset_QueryPerfWindow(QueryPerfWindow& window);
    static double Safe_Divide(double numerator, double denominator);
    void Record_CCTMovePerf(bool hit, double elapsedMs);

    friend class PhysicsCharacterController;
#endif

private: // Static Mesh 물리충돌
    uint64 _nextStaticActorId = 1;

    // MapStaticMesh 하나당 PxRigidStatic 하나를 가진다.
    umap<uint64, PxRigidStatic*> _staticActors;

    // 같은 모델은 triangle mesh cooking 결과를 재사용
    umap<wstring, vector<PxTriangleMesh*>> _triangleMeshCache;

    PxMaterial* _defaultStaticMaterial = nullptr;

public:
    static Unique<PhysX_Manager> Create();
    void Free() override;
};

NS_END
