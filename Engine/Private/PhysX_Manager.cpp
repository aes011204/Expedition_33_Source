#include "PhysX_Manager.h"
#include "ColliderCom.h"
#include "Log_Define.h"
#include "PhysX_Filter.h"
#include "ModelCom.h"
#include "cooking/PxCooking.h"
#include <cmath>

#include "StaticCollisionData.h"

namespace
{
    void Decompose_Matrix(const Matrix& matrix, Vec3& outScale, Quat& outRotation, Vec3& outTranslation)
    {
        Matrix tempMatrix = matrix;
        tempMatrix.Decompose(outScale, outRotation, outTranslation);
    }

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

        // Triangle mesh는 음수 스케일을 지원하므로 부호를 유지해야 mirrored 배치의 winding/normal이 맞는다.
        return PxMeshScale(PxVec3(
            (scale.x >= 0.f ? 1.f : -1.f) * max(0.0001f, fabsf(scale.x)),
            (scale.y >= 0.f ? 1.f : -1.f) * max(0.0001f, fabsf(scale.y)),
            (scale.z >= 0.f ? 1.f : -1.f) * max(0.0001f, fabsf(scale.z))));
    }

    Vec3 Multiply_AbsComponents(const Vec3& value, const Vec3& scale)
    {
        return Vec3(
            fabsf(value.x) * fabsf(scale.x),
            fabsf(value.y) * fabsf(scale.y),
            fabsf(value.z) * fabsf(scale.z));
    }

    Vec3 Scale_Point_ByWorldScale(const Vec3& point, const Vec3& worldScale)
    {
        return Vec3(
            point.x * worldScale.x,
            point.y * worldScale.y,
            point.z * worldScale.z);
    }

    PxTransform Build_PrimitiveLocalPose(
        const ModelCollisionProxy& proxy,
        const Matrix& preTransformMatrix,
        const Vec3& worldScale,
        const Quat& preRotation)
    {
        const Vec3 preLocalCenter = Vec3::Transform(proxy.center, preTransformMatrix);
        const Vec3 localCenter = Scale_Point_ByWorldScale(preLocalCenter, worldScale);
        const Quat localRotation = preRotation * proxy.rotation;

        return PxTransform(
            PxVec3(localCenter.x, localCenter.y, localCenter.z),
            PxQuat(localRotation.x, localRotation.y, localRotation.z, localRotation.w));
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

            // preset query는 Ignore만 제외하고 hit 후보로 남긴다.
            // Player_Target처럼 Overlap 기반 타게팅 query도 sweep 결과를 받아야 한다.
            if (response == CollisionResponse::Ignore)
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

    class TargetChannel_QueryFilterCallback final : public PxQueryFilterCallback
    {
    public:
        explicit TargetChannel_QueryFilterCallback(Collision_Channel targetChannel)
            : _targetChannel(targetChannel)
        {
        }

        PxQueryHitType::Enum preFilter(
            const PxFilterData& filterData,
            const PxShape* shape,
            const PxRigidActor* actor,
            PxHitFlags& queryFlags) override
        {
            UNREFERENCED_PARAMETER(filterData);
            UNREFERENCED_PARAMETER(actor);
            UNREFERENCED_PARAMETER(queryFlags);

            if (shape == nullptr)
                return PxQueryHitType::eNONE;

            const PxFilterData targetFilterData = shape->getQueryFilterData();
            if ((targetFilterData.word0 & ETOI(_targetChannel)) == 0)
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
        Collision_Channel _targetChannel{};
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
    PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 0);
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

	_controllerManager = PxCreateControllerManager(*_scene);
    CHECK_NULL(_controllerManager, E_FAIL);

    return S_OK;
}

void PhysX_Manager::Update(float timeDelta)
{
    if (!_scene || timeDelta <= 0.f)
    {
        return;
    }

    static constexpr float kMaxPhysicsTimeDelta = 1.f / 15.f;
    const float clampedTimeDelta = ::min(timeDelta, kMaxPhysicsTimeDelta);

    if (_scene->simulate(clampedTimeDelta))
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

void PhysX_Manager::Unregister_Collider(ColliderCom* collider)
{
    if (collider == nullptr)
        return;

    PxRigidActor* actor = collider->Get_PxActor();
    if (actor == nullptr)
        return;

    auto iter = _actorColliderMap.find(actor);
    if (iter != _actorColliderMap.end())
        _actorColliderMap.erase(iter);

    if (_scene != nullptr)
        _scene->removeActor(*actor);

    collider->Set_PhysXRegistered(false);
}

void PhysX_Manager::Gather_FrameEvents(vector<PhysXPairEvent>& outEvents)
{
    outEvents = _frameEvents;
}

PxController* PhysX_Manager::Create_CapsuleController(const CharacterControllerDesc& desc,
    const Vec3& initialFootPosition)
{
	CHECK_NULL(_controllerManager, nullptr);
    CHECK_NULL(_defaultStaticMaterial, nullptr);

    const float cylinderHeight = max(0.01f, desc.height - (desc.radius * 2.f));
    // PxControllerDesc.position은 발 위치가 아니라 캡슐 중심 위치를 기대한다.
    const float centerYOffset = desc.contactOffset + desc.radius + (cylinderHeight * 0.5f);

    PxCapsuleControllerDesc controllerDesc{};
    controllerDesc.material = _defaultStaticMaterial;
    controllerDesc.radius = desc.radius;
    controllerDesc.height = cylinderHeight; // PhysX는 반구 제외한 실린더 높이 사용
    controllerDesc.stepOffset = desc.stepOffset;
    controllerDesc.contactOffset = desc.contactOffset;
    controllerDesc.slopeLimit = cosf(XMConvertToRadians(desc.slopeLimitDeg));
    controllerDesc.position = PxExtendedVec3(
        initialFootPosition.x,
        initialFootPosition.y + centerYOffset,
        initialFootPosition.z);
    controllerDesc.upDirection = PxVec3(0.f, 1.f, 0.f);

    if (!controllerDesc.isValid())
        return nullptr;

    return _controllerManager->createController(controllerDesc);
}

void PhysX_Manager::Release_Controller(PxController* controller)
{
	if (controller != nullptr)
        controller->release();
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

PhysXStaticActorHandle PhysX_Manager::Register_StaticCollision(const StaticCollisionData& collisionData,
    const Matrix& worldMatrix, Collision_Preset preset)
{
    PhysXStaticActorHandle emptyHandle{};

    CHECK_NULL(_physics, emptyHandle);
    CHECK_NULL(_scene, emptyHandle);
    CHECK_NULL(_defaultStaticMaterial, emptyHandle);

    const bool hasPrimitive = !collisionData.primitiveProxies.empty();
    const bool hasTriangles = !collisionData.triangleMeshes.empty() || !collisionData.cookedTriangleMeshes.empty();
    if (!hasPrimitive && !hasTriangles)
        return emptyHandle;

    const vector<PxTriangleMesh*> triangleMeshes = Build_OrGetTriangleMeshes(collisionData);
    size_t triangleMeshIndex = 0;

    Vec3 preScale = Vec3::One;
    Quat preRotation = Quat::Identity;
    Vec3 preTranslation = Vec3::Zero;
    Decompose_Matrix(collisionData.preLocalTransform, preScale, preRotation, preTranslation);

    Vec3 worldScale = Vec3::One;
    Quat worldRotation = Quat::Identity;
    Vec3 worldTranslation = Vec3::Zero;
    Decompose_Matrix(worldMatrix, worldScale, worldRotation, worldTranslation);

    PxRigidStatic* actor = _physics->createRigidStatic(To_PxTransform(worldMatrix));
    if (actor == nullptr)
        return emptyHandle;

    const PxFilterData filterData = Build_PresetFilterData(preset);
    uint32 attachedShapeCount = 0;

    for (const ModelCollisionProxy& proxy : collisionData.primitiveProxies)
    {
        PxShape* shape = nullptr;

        switch (proxy.type)
        {
        case ModelCollisionProxyType::Box:
        {
            const Vec3 scaledExtents =
                Multiply_AbsComponents(
                    Multiply_AbsComponents(proxy.extents, preScale),
                    worldScale);

            shape = _physics->createShape(
                PxBoxGeometry(scaledExtents.x, scaledExtents.y, scaledExtents.z),
                *_defaultStaticMaterial,
                true);

            if (shape != nullptr)
            {
                const PxTransform localPose =
                    Build_PrimitiveLocalPose(proxy, collisionData.preLocalTransform, worldScale, preRotation);
                shape->setLocalPose(localPose);
            }
            break;
        }

        case ModelCollisionProxyType::Sphere:
        {
            const Vec3 combinedScale = Multiply_AbsComponents(preScale, worldScale);
            const float scaledRadius =
                proxy.radius * max(combinedScale.x, max(combinedScale.y, combinedScale.z));

            shape = _physics->createShape(
                PxSphereGeometry(scaledRadius),
                *_defaultStaticMaterial,
                true);

            if (shape != nullptr)
            {
                const PxTransform localPose =
                    Build_PrimitiveLocalPose(proxy, collisionData.preLocalTransform, worldScale, preRotation);
                shape->setLocalPose(localPose);
            }
            break;
        }

        case ModelCollisionProxyType::Capsule:
        {
            const Vec3 combinedScale = Multiply_AbsComponents(preScale, worldScale);
            const float scaledHalfHeight = proxy.halfHeight * fabsf(combinedScale.y);
            const float scaledRadius = proxy.radius * max(fabsf(combinedScale.x), fabsf(combinedScale.z));

            shape = _physics->createShape(
                PxCapsuleGeometry(scaledRadius, scaledHalfHeight),
                *_defaultStaticMaterial,
                true);

            if (shape != nullptr)
            {
                const PxTransform localPose =
                    Build_PrimitiveLocalPose(proxy, collisionData.preLocalTransform, worldScale, preRotation);
                shape->setLocalPose(localPose);
            }
            break;
        }

        default:
            break;
        }

        if (shape == nullptr)
            continue;

        shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
        shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
        shape->setSimulationFilterData(filterData);
        shape->setQueryFilterData(filterData);

        if (actor->attachShape(*shape))
            ++attachedShapeCount;

        shape->release();
    }

    const size_t triangleShapeCount = !collisionData.triangleMeshes.empty()
        ? collisionData.triangleMeshes.size()
        : triangleMeshes.size();

    for (size_t i = 0; i < triangleShapeCount; ++i)
    {
        if (triangleMeshIndex >= triangleMeshes.size())
            break;

        PxTriangleMesh* triangleMesh = triangleMeshes[triangleMeshIndex++];
        if (triangleMesh == nullptr)
            continue;

        PxShape* shape = _physics->createShape(
            PxTriangleMeshGeometry(triangleMesh, To_PxMeshScale(worldMatrix)),
            *_defaultStaticMaterial,
            true);

        if (shape == nullptr)
            continue;

        shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
        shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
        shape->setSimulationFilterData(filterData);
        shape->setQueryFilterData(filterData);

        if (actor->attachShape(*shape))
            ++attachedShapeCount;

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

vector<PxTriangleMesh*> PhysX_Manager::Build_OrGetTriangleMeshes(const StaticCollisionData& collisionData)
{
    const wstring key = collisionData.cacheKey;

    auto iter = _triangleMeshCache.find(key);
    if (iter != _triangleMeshCache.end())
        return iter->second;

    vector<PxTriangleMesh*> cookedMeshes;

    if (!collisionData.cookedTriangleMeshes.empty())
    {
        for (const auto& cookedData : collisionData.cookedTriangleMeshes)
        {
            PxTriangleMesh* triangleMesh = Create_TriangleMeshFromCooked(cookedData);
            if (triangleMesh != nullptr)
            {
                cookedMeshes.push_back(triangleMesh);
            }
        }
    }
    else
    {
        for (const ModelCollisionMesh& mesh : collisionData.triangleMeshes)
        {
            PxTriangleMesh* triangleMesh =
                Create_TriangleMesh(mesh, collisionData.preLocalTransform);

            if (triangleMesh == nullptr)
                continue;

            cookedMeshes.push_back(triangleMesh);
        }
    }

    _triangleMeshCache.emplace(key, cookedMeshes);
    return cookedMeshes;
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

#ifdef _DEBUG
    const auto queryBegin = chrono::steady_clock::now();
#endif

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

#ifdef _DEBUG
    {
        const double elapsedMs =
            chrono::duration<double, milli>(chrono::steady_clock::now() - queryBegin).count();

        const bool hasBlock = hit && hitBuffer.hasBlock;
        const_cast<PhysX_Manager*>(this)->Accumulate_QueryPerf(
            const_cast<PhysX_Manager*>(this)->_queryPerfWindow.sweepSphere,
            hasBlock,
            elapsedMs);
    }
#endif

    if (!hit || !hitBuffer.hasBlock)
        return false;

    const PxSweepHit& block = hitBuffer.block;

    outHit.hit = true;
    outHit.distance = block.distance;
    outHit.position = Vec3(block.position.x, block.position.y, block.position.z);
    outHit.normal = Vec3(block.normal.x, block.normal.y, block.normal.z);

    ColliderCom* hitCollider = nullptr;
    if (block.shape != nullptr)
        hitCollider = static_cast<ColliderCom*>(block.shape->userData);

    outHit.collider = hitCollider;
    outHit.owner = (hitCollider != nullptr && hitCollider->Get_Owner() != nullptr)
        ? hitCollider->Get_Owner().get() : nullptr;

    return true;

}

bool PhysX_Manager::Raycast_Environment(
    const Vec3& origin,
    const Vec3& direction,
    float distance,
    PhysXQueryHit& outHit) const
{
    outHit = {};

    CHECK_NULL(_scene, false);

    if (distance <= 0.f)
        return false;

    if (direction.LengthSquared() <= 0.f)
        return false;

    Vec3 unitDir = direction;
    unitDir.Normalize();

    TargetChannel_QueryFilterCallback filterCallback(Collision_Channel::Enviroment);

    PxQueryFilterData queryFilterData;
    queryFilterData.flags =
        PxQueryFlag::eSTATIC |
        PxQueryFlag::eDYNAMIC |
        PxQueryFlag::ePREFILTER;

    PxRaycastBuffer hitBuffer;
    const bool hit = _scene->raycast(
        PxVec3(origin.x, origin.y, origin.z),
        PxVec3(unitDir.x, unitDir.y, unitDir.z),
        distance,
        hitBuffer,
        PxHitFlag::eDEFAULT,
        queryFilterData,
        &filterCallback);

    if (!hit || !hitBuffer.hasBlock)
        return false;

    const PxRaycastHit& block = hitBuffer.block;

    outHit.hit = true;
    outHit.distance = block.distance;
    outHit.position = Vec3(block.position.x, block.position.y, block.position.z);
    outHit.normal = Vec3(block.normal.x, block.normal.y, block.normal.z);

    ColliderCom* hitCollider = nullptr;
    if (block.shape != nullptr)
        hitCollider = static_cast<ColliderCom*>(block.shape->userData);

    outHit.collider = hitCollider;
    outHit.owner = (hitCollider != nullptr && hitCollider->Get_Owner() != nullptr)
        ? hitCollider->Get_Owner().get() : nullptr;

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

#ifdef _DEBUG
    const auto queryBegin = chrono::steady_clock::now();
#endif

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

#ifdef _DEBUG
    {
        const double elapsedMs =
            chrono::duration<double, milli>(chrono::steady_clock::now() - queryBegin).count();

        const bool hasBlock = hit && hitBuffer.hasBlock;
        const_cast<PhysX_Manager*>(this)->Accumulate_QueryPerf(
            const_cast<PhysX_Manager*>(this)->_queryPerfWindow.sweepCollider,
            hasBlock,
            elapsedMs);
    }
#endif

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

#ifdef _DEBUG
    const auto queryBegin = chrono::steady_clock::now();
#endif

    const bool hit = Sweep_Collider(collider, worldMatrix, Vec3(0.f, -1.f, 0.f), probeDistance, outHit);

#ifdef _DEBUG
    {
        const double elapsedMs =
            chrono::duration<double, milli>(chrono::steady_clock::now() - queryBegin).count();

        const_cast<PhysX_Manager*>(this)->Accumulate_QueryPerf(
            const_cast<PhysX_Manager*>(this)->_queryPerfWindow.probeGround,
            hit,
            elapsedMs);
    }
#endif

    return hit;
}

#ifdef _DEBUG
bool PhysX_Manager::Try_ConsumeQueryPerfSummary(float windowSec, PhysXQueryPerfSummary& outSummary)
{
    outSummary = {};

    const bool hasSamples =
        _queryPerfWindow.cctMove.sampleCount > 0 ||
        _queryPerfWindow.sweepCollider.sampleCount > 0 ||
        _queryPerfWindow.sweepSphere.sampleCount > 0 ||
        _queryPerfWindow.probeGround.sampleCount > 0;

    if (windowSec <= 0.f || !hasSamples)
        return false;

    outSummary.valid = true;

    outSummary.cctMovePerSec = static_cast<float>(
        Safe_Divide(static_cast<double>(_queryPerfWindow.cctMove.sampleCount), static_cast<double>(windowSec)));
    outSummary.sweepColliderPerSec = static_cast<float>(
        Safe_Divide(static_cast<double>(_queryPerfWindow.sweepCollider.sampleCount), static_cast<double>(windowSec)));
    outSummary.sweepSpherePerSec = static_cast<float>(
        Safe_Divide(static_cast<double>(_queryPerfWindow.sweepSphere.sampleCount), static_cast<double>(windowSec)));
    outSummary.probeGroundPerSec = static_cast<float>(
        Safe_Divide(static_cast<double>(_queryPerfWindow.probeGround.sampleCount), static_cast<double>(windowSec)));

    outSummary.cctMoveAvgMs = static_cast<float>(
        Safe_Divide(_queryPerfWindow.cctMove.accumulatedMs, static_cast<double>(_queryPerfWindow.cctMove.sampleCount)));
    outSummary.cctMoveMaxMs = static_cast<float>(_queryPerfWindow.cctMove.maxMs);

    outSummary.sweepColliderAvgMs = static_cast<float>(
        Safe_Divide(_queryPerfWindow.sweepCollider.accumulatedMs, static_cast<double>(_queryPerfWindow.sweepCollider.sampleCount)));
    outSummary.sweepColliderMaxMs = static_cast<float>(_queryPerfWindow.sweepCollider.maxMs);

    outSummary.sweepSphereAvgMs = static_cast<float>(
        Safe_Divide(_queryPerfWindow.sweepSphere.accumulatedMs, static_cast<double>(_queryPerfWindow.sweepSphere.sampleCount)));
    outSummary.sweepSphereMaxMs = static_cast<float>(_queryPerfWindow.sweepSphere.maxMs);

    outSummary.probeGroundAvgMs = static_cast<float>(
        Safe_Divide(_queryPerfWindow.probeGround.accumulatedMs, static_cast<double>(_queryPerfWindow.probeGround.sampleCount)));
    outSummary.probeGroundMaxMs = static_cast<float>(_queryPerfWindow.probeGround.maxMs);

    Reset_QueryPerfWindow(_queryPerfWindow);
    return true;
}

void PhysX_Manager::Accumulate_QueryPerf(QueryPerfCounter& counter, bool hit, double elapsedMs)
{
    ++counter.sampleCount;

    if (hit)
        ++counter.hitCount;

    counter.accumulatedMs += elapsedMs;
    counter.maxMs = max(counter.maxMs, elapsedMs);
}

void PhysX_Manager::Reset_QueryPerfWindow(QueryPerfWindow& window)
{
    window = {};
}

double PhysX_Manager::Safe_Divide(const double numerator, const double denominator)
{
    if (denominator <= 0.0)
        return 0.0;

    return numerator / denominator;
}

void PhysX_Manager::Record_CCTMovePerf(bool hit, double elapsedMs)
{
    Accumulate_QueryPerf(_queryPerfWindow.cctMove, hit, elapsedMs);
}
#endif

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

	auto ReleaseManager = [](auto& manager)
	{
        if (manager != nullptr)
        {
            manager->release();
			manager = nullptr;
        }  
	};

	ReleaseManager(_defaultStaticMaterial);
	ReleaseManager(_controllerManager);
	ReleaseManager(_scene);
	ReleaseManager(_dispatcher);
	ReleaseManager(_physics);

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
    ReleaseManager(_foundation);
}

bool PhysX_Manager::Cook_TriangleMesh(const ModelCollisionMesh& meshData, const Matrix& preTransformMatrix, vector<uint8_t>& outCookedData)
{
    if (_physics == nullptr)
        return false;

    if (meshData.positions.empty() || meshData.indices.size() < 3)
        return false;

    if (meshData.indices.size() % 3 != 0)
        return false;

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
        return false;

    PxTriangleMeshDesc meshDesc{};
    meshDesc.points.count = static_cast<PxU32>(pxVertices.size());
    meshDesc.points.stride = sizeof(PxVec3);
    meshDesc.points.data = pxVertices.data();

    meshDesc.triangles.count = static_cast<PxU32>(pxIndices.size() / 3);
    meshDesc.triangles.stride = sizeof(PxU32) * 3;
    meshDesc.triangles.data = pxIndices.data();

    PxCookingParams params(_physics->getTolerancesScale());
    params.midphaseDesc = PxMeshMidPhase::eBVH34;

    PxDefaultMemoryOutputStream outBuffer;
    if (!PxCookTriangleMesh(params, meshDesc, outBuffer))
        return false;

    outCookedData.assign(outBuffer.getData(), outBuffer.getData() + outBuffer.getSize());
    return true;
}

PxTriangleMesh* PhysX_Manager::Create_TriangleMeshFromCooked(const vector<uint8_t>& cookedData)
{
    if (_physics == nullptr || cookedData.empty())
        return nullptr;

    PxDefaultMemoryInputData stream(const_cast<PxU8*>(cookedData.data()), static_cast<PxU32>(cookedData.size()));
    return _physics->createTriangleMesh(stream);
}
