#include "PhysX_Manager.h"
#include "ColliderCom.h"
#include "PhysX_Filter.h"

namespace
{
    class Movement_QueryFilterCallback final : public PxQueryFilterCallback
    {
    public:
        Movement_QueryFilterCallback(PxActor* selfActor, const PxFilterData& selfFilterData)
            : _selfActor(selfActor)
            , _selfFilterData(selfFilterData)
        {
        }

        PxQueryHitType::Enum preFilter(
            const PxFilterData& filterData,
            const PxShape* shape,
            const PxRigidActor* actor,
            PxHitFlags& queryFlags) override
        {
            UNREFERENCED_PARAMETER(filterData);
            UNREFERENCED_PARAMETER(queryFlags);

            if (actor == nullptr || shape == nullptr)
                return PxQueryHitType::eNONE;

            if (actor == _selfActor)
                return PxQueryHitType::eNONE;

            const PxFilterData targetFilterData = shape->getQueryFilterData();
            const CollisionResponse response = Calculate_PhysXResponse(_selfFilterData, targetFilterData);

            // movement query는 Block 관계만 막는다.
            if (response != CollisionResponse::Block)
                return PxQueryHitType::eNONE;

            return PxQueryHitType::eBLOCK;
        }

        PxQueryHitType::Enum postFilter(
            const PxFilterData& filterData,
            const PxQueryHit& hit,
            const PxShape* shape,
            const PxRigidActor* actor) override
        {
            UNREFERENCED_PARAMETER(filterData);
            UNREFERENCED_PARAMETER(hit);
            UNREFERENCED_PARAMETER(shape);
            UNREFERENCED_PARAMETER(actor);

            return PxQueryHitType::eBLOCK;
        }

    private:
        PxActor* _selfActor = nullptr;
        PxFilterData _selfFilterData{};
    };
}

PhysX_Manager::PhysX_Manager()
{
}

PhysX_Manager::~PhysX_Manager()
{
}

HRESULT PhysX_Manager::Initialize()
{
    _foundation = PxCreateFoundation(PX_PHYSICS_VERSION, _allocator, _errorCallback);
    CHECK_NULL(_foundation, E_FAIL);

#ifdef _DEBUG
    _pvd = PxCreatePvd(*_foundation);
    PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
    _pvd->connect(*transport, PxPvdInstrumentationFlag::eALL);
#endif

    _physics = PxCreatePhysics(PX_PHYSICS_VERSION, *_foundation, PxTolerancesScale(), true,
#ifdef _DEBUG
        _pvd
#else
        nullptr
#endif
    );
    CHECK_NULL(_physics, E_FAIL);

    _dispatcher = PxDefaultCpuDispatcherCreate(2);

    PxSceneDesc sceneDesc(_physics->getTolerancesScale());
    sceneDesc.gravity = PxVec3(0.f, -9.81f, 0.f);
    sceneDesc.cpuDispatcher = _dispatcher;
    // 기본 피직스 규칙 말고, 커스텀으로 만든 충돌 규칙 사용하게
    // word0 = channel, word1 = overlapMaks, word2 = blockMask
    sceneDesc.filterShader = CustomFilterShader;
    sceneDesc.simulationEventCallback = this;

    sceneDesc.kineKineFilteringMode = PxPairFilteringMode::eKEEP;
    sceneDesc.staticKineFilteringMode = PxPairFilteringMode::eKEEP;

    _scene = _physics->createScene(sceneDesc);
    CHECK_NULL(_scene, E_FAIL);

    return S_OK;
}

void PhysX_Manager::Update(float timeDelta)
{
    if (!_scene || timeDelta <= 0.f)
    {
        return;
    }

    if (_scene->simulate(timeDelta))
        _scene->fetchResults(true);
}

void PhysX_Manager::Begin_Frame()
{
    _frameEvents.clear();
}

void PhysX_Manager::Register_OrSync_Collider(Shared<ColliderCom> collider, const Matrix& worldMatrix)
{
    CHECK_NULL(collider);
    CHECK_NULL(_physics);
    CHECK_NULL(_scene);

    if (collider->Is_PhysXRegistered() == false)
    {
        if (FAILED(collider->Build_PhysXActor(_physics)))
            return;

        PxRigidActor* actor = collider->Get_PxActor();
        CHECK_NULL(actor);

        _scene->addActor(*actor);
        _actorColliderMap[actor] = collider;

        collider->Set_PhysXRegistered(true);
    }

    collider->Sync_ToPhysX(worldMatrix);
}

void PhysX_Manager::Gather_FrameEvents(vector<PhysXPairEvent>& outEvents)
{
    outEvents = _frameEvents;
}

void PhysX_Manager::onTrigger(PxTriggerPair* pairs, PxU32 count)
{
    for (PxU32 i = 0; i < count; ++i)
    {
        const PxTriggerPair& pair = pairs[i];

        if (pair.flags.isSet(PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER) ||
            pair.flags.isSet(PxTriggerPairFlag::eREMOVED_SHAPE_OTHER))
        {
            continue;
        }

        Shared<ColliderCom> triggerCollider = Find_Collider(pair.triggerActor);
        Shared<ColliderCom> otherCollider = Find_Collider(pair.otherActor);

        if (triggerCollider == nullptr || otherCollider == nullptr)
            continue;

        const PxPairFlags status = pair.status;

        const bool began = status.isSet(PxPairFlag::eNOTIFY_TOUCH_FOUND);
        const bool ended = status.isSet(PxPairFlag::eNOTIFY_TOUCH_LOST);
        const bool staying = false;

        Push_PairEvent(
            triggerCollider,
            otherCollider,
            CollisionResponse::Overlap,
            began,
            staying,
            ended);

        Push_PairEvent(
            otherCollider,
            triggerCollider,
            CollisionResponse::Overlap,
            began,
            staying,
            ended);
    }
}

void PhysX_Manager::onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs)
{
    Shared<ColliderCom> colliderA = Find_Collider(pairHeader.actors[0]);
    Shared<ColliderCom> colliderB = Find_Collider(pairHeader.actors[1]);

    if (colliderA == nullptr || colliderB == nullptr)
        return;

    for (PxU32 i = 0; i < nbPairs; ++i)
    {
        const PxContactPair& pair = pairs[i];
        const PxPairFlags events = pair.events;

        const bool began = events.isSet(PxPairFlag::eNOTIFY_TOUCH_FOUND);
        const bool staying = events.isSet(PxPairFlag::eNOTIFY_TOUCH_PERSISTS);
        const bool ended = events.isSet(PxPairFlag::eNOTIFY_TOUCH_LOST);

        if (!began && !staying && !ended)
            continue;

        Push_PairEvent(
            colliderA,
            colliderB,
            CollisionResponse::Block,
            began,
            staying,
            ended);

        Push_PairEvent(
            colliderB,
            colliderA,
            CollisionResponse::Block,
            began,
            staying,
            ended);
    }
}

Shared<ColliderCom> PhysX_Manager::Find_Collider(PxActor* actor) const
{
    if (actor == nullptr)
        return nullptr;

    auto iter = _actorColliderMap.find(actor);
    if (iter == _actorColliderMap.end())
    {
        return nullptr;
    }

    return iter->second.lock();
}

void PhysX_Manager::Push_PairEvent(Shared<ColliderCom> self, Shared<ColliderCom> other, CollisionResponse response,
    bool began, bool staying, bool ended)
{
    CHECK_NULL(self);
    CHECK_NULL(other);

    PhysXPairEvent event{};
    event.self = self;
    event.other = other;
    event.response = response;
    event.began = began;
    event.staying = staying;
    event.ended = ended;

    _frameEvents.push_back(event);
}

bool PhysX_Manager::Sweep_Collider(ColliderCom* collider, const Matrix& worldMatrix, const Vec3& direction,
    float distance, PhysXQueryHit& outHit) const
{
    outHit = {};

    CHECK_NULL(collider, false);
    CHECK_NULL(_scene, false);

    // 가까우면 무시
    if (distance <= 0.f)
        return false;

    if (direction.LengthSquared() <= 0.f)
        return false;

    PxShape* pxShape = collider->Get_PxShape();
    CHECK_NULL(pxShape, false);

    PxRigidActor* pxActor = collider->Get_PxActor();
    CHECK_NULL(pxActor, false);

    // 현재 collider shape를 query용 geometry로 다시 만ㄷ르기
    PxGeometryHolder geometryHolder = collider->Build_QueryGeometry(worldMatrix);
    PxTransform pose = collider->Build_QueryTransform(worldMatrix);

    Vec3 unitDir = direction;
    unitDir.Normalize();

    PxVec3 pxUnitDir(unitDir.x, unitDir.y, unitDir.z);

    // self filterData와 target filterData를 callback에서 비교하기
    const PxFilterData selfFilterData = pxShape->getQueryFilterData();
    Movement_QueryFilterCallback filterCallback(pxActor, selfFilterData);

    PxQueryFilterData queryFilterData;
    queryFilterData.flags =
        PxQueryFlag::eSTATIC |
        PxQueryFlag::eDYNAMIC |
        PxQueryFlag::ePREFILTER;

    PxSweepBuffer hitBuffer;
    const bool hit = _scene->sweep(
        geometryHolder.any(),
        pose,
        pxUnitDir,
        distance,
        hitBuffer,
        PxHitFlag::eDEFAULT,
        queryFilterData,
        &filterCallback);

    if (!hit || !hitBuffer.hasBlock)
        return false;

    // hit결과 채우기
    const PxSweepHit& block = hitBuffer.block;

    outHit.hit = true;
    outHit.distance = block.distance;
    outHit.position = Vec3(block.position.x, block.position.y, block.position.z);
    outHit.normal = Vec3(block.normal.x, block.normal.y, block.normal.z);

    return true;
}

bool PhysX_Manager::Probe_Ground(ColliderCom* collider, const Matrix& worldMatrix, float probeDistance,
    PhysXQueryHit& outHit) const
{
    if (probeDistance <= 0.f)
    {
        outHit = {};
        return false;
    }

    return Sweep_Collider(collider, worldMatrix, Vec3(0.f, -1.f, 0.f), probeDistance, outHit);
}

Unique<PhysX_Manager> PhysX_Manager::Create()
{
    auto instance = make_unique<PhysX_Manager>();

    if (FAILED(instance->Initialize()))
    {
        return nullptr;
    }
    return instance;
}

void PhysX_Manager::Free()
{
    if (_scene)
    {
        _scene->release();
        _scene = nullptr;
    }

    if (_dispatcher)
    {
        _dispatcher->release();
        _dispatcher = nullptr;
    }

    if (_physics)
    {
        _physics->release();
        _physics = nullptr;
    }

#ifdef _DEBUG
    if (_pvd)
    {
        PxPvdTransport* transport = _pvd->getTransport();
        _pvd->release();
        _pvd = nullptr;

        if (transport)
        {
            transport->release();
        }
    }
#endif
    if (_foundation)
    {
        _foundation->release();
        _foundation = nullptr;
    }
}
