#include "PhysX_Manager.h"
#include "ColliderCom.h"
#include "PhysX_Filter.h"
#include "ModelCom.h"
#include "cooking/PxCooking.h"
#include <cmath>

namespace
{
    PxTransform To_PxTransform(const Matrix& worldMatrix)
    {
        Matrix tempMatix = worldMatrix;

        Vec3 scale{};
        Quat rotation{};
        Vec3 translation{};
        tempMatix.Decompose(scale, rotation, translation);

        return PxTransform(
            PxVec3(translation.x, translation.y, translation.z),
            PxQuat(rotation.x, rotation.y, rotation.z, rotation.w));
    }

    PxMeshScale To_PxMeshScale(const Matrix& worldMatrix)
    {
        Matrix tempMatix = worldMatrix;

        Vec3 scale{};
        Quat rotation{};
        Vec3 translation{};
        tempMatix.Decompose(scale, rotation, translation);

        return PxMeshScale(PxVec3(
            max(0.0001f, fabsf(scale.x)),
            max(0.0001f, fabsf(scale.y)),
            max(0.0001f, fabsf(scale.z))));
    }

    PxFilterData Build_PresetFilterData(Collision_Preset preset)
    {
        const Collision_Preset_Data& presetData = Get_PresetData(preset);
        return Build_FilterData(
            presetData.channel,
            presetData.overlapMask,
            presetData.blockMask);
    }

    bool Is_Finite(const PxVec3& value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    bool Is_ValidTriangle(const PxVec3& v0, const PxVec3& v1, const PxVec3& v2)
    {
        if (!Is_Finite(v0) || !Is_Finite(v1) || !Is_Finite(v2))
            return false;

        const PxVec3 edge0 = v1 - v0;
        const PxVec3 edge1 = v2 - v0;
        const PxVec3 normal = edge0.cross(edge1);

        return normal.magnitudeSquared() > 1e-12f;
    }

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

    class Preset_QueryFilterCallback final : public PxQueryFilterCallback
    {
    public:
        explicit Preset_QueryFilterCallback(const PxFilterData& selfFilterData)
            : _selfFilterData(selfFilterData)
        {
        }

        PxQueryHitType::Enum preFilter(
            const PxFilterData& filterData,
            const PxShape* shape,
            const PxRigidActor* actor,
            PxHitFlags& queryFlags) override
        {
            UNREFERENCED_PARAMETER(filterData);
            UNREFERENCED_PARAMETER(shape);
            UNREFERENCED_PARAMETER(actor);
            UNREFERENCED_PARAMETER(queryFlags);

            const CollisionResponse response =
                Calculate_PhysXResponse(_selfFilterData, shape->getQueryFilterData());

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

    // Static Mesh Collision Shape가 공통적으로 사용할 material
    // 마찰계수는 일단 기본값으로 넣고, 테스트해보면서 조절 예정
    _defaultStaticMaterial = _physics->createMaterial(0.5f, 0.5f, 0.5f);
    CHECK_NULL(_defaultStaticMaterial, E_FAIL);

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

        collider->Sync_InitialPose_ToPhysX(worldMatrix);

        _scene->addActor(*actor);
        _actorColliderMap[actor] = collider;

        collider->Set_PhysXRegistered(true);
        return;
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

        if (pair.shapes[0] == nullptr || pair.shapes[1] == nullptr)
            continue;

        const PxPairFlags events = pair.events;

        const bool began = events.isSet(PxPairFlag::eNOTIFY_TOUCH_FOUND);
        const bool staying = events.isSet(PxPairFlag::eNOTIFY_TOUCH_PERSISTS);
        const bool ended = events.isSet(PxPairFlag::eNOTIFY_TOUCH_LOST);

        if (!began && !staying && !ended)
            continue;

        const PxFilterData filterDataA = pair.shapes[0]->getSimulationFilterData();
        const PxFilterData filterDataB = pair.shapes[1]->getSimulationFilterData();

        const CollisionResponse response =
            Calculate_PhysXResponse(filterDataA, filterDataB);

        if (response == CollisionResponse::Ignore)
            continue;

        // 필터 결과가 Overlap이면 OnBeginOverlap 경로로 전달되어야 한다.
        Push_PairEvent(colliderA, colliderB, response, began, staying, ended);
        Push_PairEvent(colliderB, colliderA, response, began, staying, ended);
    }
}

PhysXStaticActorHandle PhysX_Manager::Register_StaticModel(const ModelCom& model, const Matrix& worldMatrix,
    Collision_Preset preset)
{
    PhysXStaticActorHandle emptyHandle{};

    CHECK_NULL(_physics, emptyHandle);
    CHECK_NULL(_scene, emptyHandle);
    CHECK_NULL(_defaultStaticMaterial, emptyHandle);

    vector<PxTriangleMesh*> triangleMeshes = Build_OrGetTriangleMeshes(model);
    if (triangleMeshes.empty())
        return emptyHandle;

    PxRigidStatic* actor = _physics->createRigidStatic(To_PxTransform(worldMatrix));
    if (actor == nullptr)
    {
        return emptyHandle;
    }

    const PxFilterData filterData = Build_PresetFilterData(preset);
    uint32 attachedShapeCount = 0;

    for (PxTriangleMesh* triangleMesh : triangleMeshes)
    {
        if (triangleMesh == nullptr)
            continue;

        PxTriangleMeshGeometry geometry(triangleMesh, To_PxMeshScale(worldMatrix));
        if (!geometry.isValid())
            continue;

        PxShape* shape = _physics->createShape(geometry, *_defaultStaticMaterial, true);
        if (shape == nullptr)
            continue;

        // Static map collision은 simulation/query 둘 다 켠다.
        // MovementCom의 sweep/probe는 query filter data를 본다.
        shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
        shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
        shape->setSimulationFilterData(filterData);
        shape->setQueryFilterData(filterData);

        if (actor->attachShape(*shape))
            ++attachedShapeCount;

        // actor가 shape reference를 잡으므로 local reference는 놓는다.
        shape->release();
    }

    if (attachedShapeCount == 0)
    {
        actor->release();
        return emptyHandle;
    }

    _scene->addActor(*actor);

    PhysXStaticActorHandle handle{};
    handle.id = _nextStaticActorId++;
    _staticActors.emplace(handle.id, actor);

    return handle;
}

void PhysX_Manager::Unregister_StaticActor(PhysXStaticActorHandle handle)
{
    if (!handle.Is_Valid())
        return;

    auto iter = _staticActors.find(handle.id);
    if (iter == _staticActors.end())
        return;

    PxRigidStatic* actor = iter->second;
    _staticActors.erase(iter);

    if (actor == nullptr)
        return;

    if (_scene)
        _scene->removeActor(*actor);

    actor->release();
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

bool PhysX_Manager::Sweep_Sphere(const Vec3& origin, const Vec3& direction, float distance, float radius,
    Collision_Preset queryPreset, PhysXQueryHit& outHit) const
{
    outHit = {};

    CHECK_NULL(_scene, false);

    if (distance <= 0.f || radius <= 0.f)
        return false;

    if (direction.LengthSquared() <= 0.f)
        return false;

    // 정규화해서 사용
    Vec3 unitDir = direction;
    unitDir.Normalize();

    const Collision_Preset_Data& presetData = Get_PresetData(queryPreset);
    const PxFilterData selfFilterData = Build_FilterData(
        presetData.channel, presetData.overlapMask, presetData.blockMask);

    Preset_QueryFilterCallback filterCallback(selfFilterData);

    PxQueryFilterData queryFilterData;
    queryFilterData.flags =
        PxQueryFlag::eSTATIC |
        PxQueryFlag::eDYNAMIC |
        PxQueryFlag::ePREFILTER;

    PxSweepBuffer hitBuffer;
    const bool hit = _scene->sweep(
        PxSphereGeometry(radius),
        PxTransform(PxVec3(origin.x, origin.y, origin.z)),
        PxVec3(unitDir.x, unitDir.y, unitDir.z),
        distance,
        hitBuffer,
        PxHitFlag::eDEFAULT,
        queryFilterData,
        &filterCallback);

    if (!hit || !hitBuffer.hasBlock)
        return false;

    const PxSweepHit& block = hitBuffer.block;

    outHit.hit = true;
    outHit.distance = block.distance;
    outHit.position = Vec3(block.position.x, block.position.y, block.position.z);
    outHit.normal = Vec3(block.normal.x, block.normal.y, block.normal.z);

    return true;
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

vector<PxTriangleMesh*> PhysX_Manager::Build_OrGetTriangleMeshes(const ModelCom& model)
{
    const wstring key = model.Get_SourceModelPath();

    auto iter = _triangleMeshCache.find(key);
    if (iter != _triangleMeshCache.end())
        return iter->second;

    vector<PxTriangleMesh*> cookedMeshes;

    for (const ModelCollisionMesh& collisionMesh : model.Get_CollisionMeshes())
    {
        PxTriangleMesh* triangleMesh = Create_TriangleMesh(collisionMesh, model.Get_PreLocalTransformMatrix());
        if (triangleMesh == nullptr)
            continue;

        cookedMeshes.push_back(triangleMesh);
    }

    _triangleMeshCache.emplace(key, cookedMeshes);
    return cookedMeshes;
}

PxTriangleMesh* PhysX_Manager::Create_TriangleMesh(const ModelCollisionMesh& meshData, const Matrix& preTransformMatrix)
{
    if (_physics == nullptr)
        return nullptr;

    if (meshData.positions.empty() || meshData.indices.size() < 3)
        return nullptr;

    if (meshData.indices.size() % 3 != 0)
        return nullptr;

    vector<PxVec3> sourceVertices;
    sourceVertices.reserve(meshData.positions.size());

    for (const Vec3& position : meshData.positions)
    {
        const Vec3 transformedPosition = Vec3::Transform(position, preTransformMatrix);
        sourceVertices.emplace_back(transformedPosition.x, transformedPosition.y, transformedPosition.z);
    }

    static constexpr PxU32 kInvalidIndex = 0xffffffffu;

    vector<PxVec3> pxVertices;
    pxVertices.reserve(sourceVertices.size());

    vector<PxU32> remappedIndices(sourceVertices.size(), kInvalidIndex);
    vector<PxU32> pxIndices;
    pxIndices.reserve(meshData.indices.size());

    for (size_t i = 0; i + 2 < meshData.indices.size(); i += 3)
    {
        const PxU32 i0 = static_cast<PxU32>(meshData.indices[i]);
        const PxU32 i1 = static_cast<PxU32>(meshData.indices[i + 1]);
        const PxU32 i2 = static_cast<PxU32>(meshData.indices[i + 2]);

        if (i0 >= sourceVertices.size() || i1 >= sourceVertices.size() || i2 >= sourceVertices.size())
            continue;

        if (i0 == i1 || i1 == i2 || i2 == i0)
            continue;

        if (!Is_ValidTriangle(sourceVertices[i0], sourceVertices[i1], sourceVertices[i2]))
            continue;

        const PxU32 sourceTriangle[3] = { i0, i1, i2 };
        for (PxU32 sourceIndex : sourceTriangle)
        {
            if (remappedIndices[sourceIndex] == kInvalidIndex)
            {
                remappedIndices[sourceIndex] = static_cast<PxU32>(pxVertices.size());
                pxVertices.push_back(sourceVertices[sourceIndex]);
            }

            pxIndices.push_back(remappedIndices[sourceIndex]);
        }
    }

    if (pxVertices.empty() || pxIndices.empty())
        return nullptr;

    PxTriangleMeshDesc meshDesc{};
    meshDesc.points.count = static_cast<PxU32>(pxVertices.size());
    meshDesc.points.stride = sizeof(PxVec3);
    meshDesc.points.data = pxVertices.data();

    meshDesc.triangles.count = static_cast<PxU32>(pxIndices.size() / 3);
    meshDesc.triangles.stride = sizeof(PxU32) * 3;
    meshDesc.triangles.data = pxIndices.data();

    PxCookingParams params(_physics->getTolerancesScale());
    params.midphaseDesc = PxMeshMidPhase::eBVH34;

    // 일단ㅇ은 offline serialize까지 가지 않고 runtime cooking으로 시작한다.
    // 맵 로딩이 너무 느려지면 이후 cooked binary cache를 추가한다.
    return PxCreateTriangleMesh(
        params,
        meshDesc,
        _physics->getPhysicsInsertionCallback());
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
    for (auto& [id, actor] : _staticActors)
    {
        UNREFERENCED_PARAMETER(id);

        if (actor)
            actor->release();
    }
    _staticActors.clear();

    for (auto& [path, meshes] : _triangleMeshCache)
    {
        UNREFERENCED_PARAMETER(path);

        for (PxTriangleMesh* mesh : meshes)
        {
            if (mesh)
                mesh->release();
        }
    }
    _triangleMeshCache.clear();

    if (_defaultStaticMaterial)
    {
        _defaultStaticMaterial->release();
        _defaultStaticMaterial = nullptr;
    }

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
