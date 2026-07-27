#include "pch.h"
#include "ColliderCom.h"

#include "GameInstance.h"
#include "Bounding.h"
#include "Bounding_AABB.h"
#include "Bounding_OBB.h"
#include "Bounding_Sphere.h"
#include "PhysX_Filter.h"

namespace
{
    constexpr float kPhysXSyncMatrixEpsilon = 0.0001f;
    constexpr float kCollisionDebugHighlightScale = 1.35f;

    const Color kDefaultCollisionDebugColor = Color(0.f, 1.f, 0.f, 1.f);
    const Color kInactiveCollisionDebugColor = Color(0.5f, 0.5f, 0.5f, 1.f);
    const Color kPlayerBodyCollisionDebugColor = Color(0.21f, 0.73f, 1.f, 1.f);
    const Color kMonsterBodyCollisionDebugColor = Color(1.f, 0.54f, 0.24f, 1.f);
    const Color kPlayerAttackCollisionDebugColor = Color(0.f, 0.9f, 1.f, 1.f);
    const Color kMonsterAttackCollisionDebugColor = Color(1.f, 0.25f, 0.35f, 1.f);
    const Color kItemCollisionDebugColor = Color(0.31f, 0.88f, 0.71f, 1.f);
    const Color kTriggerCollisionDebugColor = Color(0.46f, 0.4f, 1.f, 1.f);
    const Color kWeakPointCollisionDebugColor = Color(1.f, 0.85f, 0.29f, 1.f);

    Color Resolve_CollisionDebugColor(Collision_Preset preset)
    {
        switch (preset)
        {
        case Collision_Preset::Player:
        case Collision_Preset::Player_Body:
        case Collision_Preset::Player_WorldBody:
        case Collision_Preset::Player_Target:
        case Collision_Preset::Player_WeakPointTarget:
        case Collision_Preset::SL_PlayerBody:
        case Collision_Preset::SL_TargetSensor:
        case Collision_Preset::Player_ParkourSensor:
            return kPlayerBodyCollisionDebugColor;

        case Collision_Preset::Player_Attack:
        case Collision_Preset::SL_PlayerAttack:
            return kPlayerAttackCollisionDebugColor;

        case Collision_Preset::Monster:
        case Collision_Preset::Monster_Body:
        case Collision_Preset::Monster_WorldBody:
        case Collision_Preset::SL_MonsterBody:
            return kMonsterBodyCollisionDebugColor;

        case Collision_Preset::Monster_Attack:
        case Collision_Preset::SL_MonsterAttack:
            return kMonsterAttackCollisionDebugColor;

        case Collision_Preset::Item:
            return kItemCollisionDebugColor;

        case Collision_Preset::Trigger:
        case Collision_Preset::WorldActionTrigger:
            return kTriggerCollisionDebugColor;

        case Collision_Preset::Monster_WeakPoint:
            return kWeakPointCollisionDebugColor;

        default:
            return kDefaultCollisionDebugColor;
        }
    }

    Color Brighten_CollisionDebugColor(const Color& color)
    {
        return Color(
            min(1.f, color.x * kCollisionDebugHighlightScale),
            min(1.f, color.y * kCollisionDebugHighlightScale),
            min(1.f, color.z * kCollisionDebugHighlightScale),
            color.w);
    }

    bool NearlyEqual(float lhs, float rhs)
    {
        const float diff = lhs - rhs;
        return diff <= kPhysXSyncMatrixEpsilon && diff >= -kPhysXSyncMatrixEpsilon;
    }

    bool NearlyEqual(const Matrix& lhs, const Matrix& rhs)
    {
        return NearlyEqual(lhs._11, rhs._11) && NearlyEqual(lhs._12, rhs._12) &&
            NearlyEqual(lhs._13, rhs._13) && NearlyEqual(lhs._14, rhs._14) &&
            NearlyEqual(lhs._21, rhs._21) && NearlyEqual(lhs._22, rhs._22) &&
            NearlyEqual(lhs._23, rhs._23) && NearlyEqual(lhs._24, rhs._24) &&
            NearlyEqual(lhs._31, rhs._31) && NearlyEqual(lhs._32, rhs._32) &&
            NearlyEqual(lhs._33, rhs._33) && NearlyEqual(lhs._34, rhs._34) &&
            NearlyEqual(lhs._41, rhs._41) && NearlyEqual(lhs._42, rhs._42) &&
            NearlyEqual(lhs._43, rhs._43) && NearlyEqual(lhs._44, rhs._44);
    }
}

ColliderCom::ColliderCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component(device, context)
{
}

ColliderCom::ColliderCom(const ColliderCom& Prototype)
    : Component(Prototype)
    , _shape(Prototype._shape)
    , _shapeDesc(Prototype._shapeDesc)
    , _channel(Prototype._channel)
    , _overlapMask(Prototype._overlapMask)
    , _blockMask(Prototype._blockMask)
    , _preset(Prototype._preset)
    , _isActive(Prototype._isActive)
    , _isColl(false)
    , _pxActor(nullptr)
    , _pxShape(nullptr)
    , _physxRegistered(false)
    , _isTrigger(Prototype._isTrigger)
{
}

HRESULT ColliderCom::Initialize_Prototype()
{
    CHECK_FAILED(Component::Initialize_Prototype(), E_FAIL);

    CHECK_FAILED(Ready_DebugRenderResources(), E_FAIL);

    return S_OK;
}

HRESULT ColliderCom::Initialize_Prototype(CollisionShape shape)
{
    CHECK_FAILED(Initialize_Prototype(), E_FAIL);

    _shape = shape;

    return S_OK;
}

HRESULT ColliderCom::Initialize(void* arg)
{
    CHECK_FAILED(Component::Initialize(arg), E_FAIL);

    if (nullptr == _batch || nullptr == _effect || nullptr == _inputLayout)
        CHECK_FAILED(Ready_DebugRenderResources(), E_FAIL);

    if (nullptr == arg)
        return E_FAIL;

    switch (_shape)
    {
    case CollisionShape::AABB:
    {
        const auto& aabbDesc = *static_cast<Bounding_AABB::BoundingAABBDesc*>(arg);

        _shapeDesc.shape = CollisionShape::AABB;
        _shapeDesc.center = aabbDesc.center;
        _shapeDesc.extents = aabbDesc.extents;
        _shapeDesc.radians = Vec3::Zero;
        _shapeDesc.radius = 0.5f;

        _bounding = Bounding_AABB::Create(
            _device,
            _context,
            aabbDesc);
        break;
    }

    case CollisionShape::OBB:
    {
        const auto& obbDesc = *static_cast<Bounding_OBB::BoundingOBBDesc*>(arg);

        _shapeDesc.shape = CollisionShape::OBB;
        _shapeDesc.center = obbDesc.center;
        _shapeDesc.extents = obbDesc.extents;
        _shapeDesc.radians = obbDesc.radians;
        _shapeDesc.radius = 0.5f;

        _bounding = Bounding_OBB::Create(
            _device,
            _context,
            obbDesc);
        break;
    }

    case CollisionShape::Sphere:
    {
        const auto& sphereDesc = *static_cast<Bounding_Sphere::FBoundingSphereDesc*>(arg);

        _shapeDesc.shape = CollisionShape::Sphere;
        _shapeDesc.center = sphereDesc.center;
        _shapeDesc.extents = Vec3::Zero;
        _shapeDesc.radians = Vec3::Zero;
        _shapeDesc.radius = sphereDesc.radius;

        _bounding = Bounding_Sphere::Create(
            _device,
            _context,
            sphereDesc);
        break;
    }

    default:
        return E_FAIL;
    }

    if (nullptr == _bounding)
        return E_FAIL;

    _overlapSet.clear();
    _isColl = false;

    _pxActor = nullptr;
    _pxShape = nullptr;
    _physxRegistered = false;
    _isTrigger = (_blockMask == 0); // blockMask가 하나도 없으면 overlap 전용

    return S_OK;
}

json ColliderCom::To_Json() const
{
    json root = __super::To_Json();

    root["shape"] = static_cast<int>(_shape);
    root["channel"] = static_cast<int>(_channel);
    root["overlap_mask"] = _overlapMask;
    root["block_mask"] = _blockMask;
    root["preset"] = static_cast<int>(_preset);
    root["is_active"] = _isActive;

    json shapeDesc = json::object();
    shapeDesc["shape"] = static_cast<int>(_shapeDesc.shape);
    shapeDesc["center"] = { _shapeDesc.center.x, _shapeDesc.center.y, _shapeDesc.center.z };
    shapeDesc["extents"] = { _shapeDesc.extents.x, _shapeDesc.extents.y, _shapeDesc.extents.z };
    shapeDesc["radians"] = { _shapeDesc.radians.x, _shapeDesc.radians.y, _shapeDesc.radians.z };
    shapeDesc["radius"] = _shapeDesc.radius;
    root["shape_desc"] = std::move(shapeDesc);

    return root;
}

void ColliderCom::From_Json(const json& data)
{
    __super::From_Json(data);

    if (data.contains("shape") && data["shape"].is_number_integer())
        _shape = static_cast<CollisionShape>(data["shape"].get<int>());

    if (data.contains("preset") && data["preset"].is_number_integer())
        _preset = static_cast<Collision_Preset>(data["preset"].get<int>());

    if (data.contains("channel") && data["channel"].is_number_integer())
        _channel = static_cast<Collision_Channel>(data["channel"].get<int>());

    if (data.contains("overlap_mask") && data["overlap_mask"].is_number_integer())
        _overlapMask = data["overlap_mask"].get<uint32>();

    if (data.contains("block_mask") && data["block_mask"].is_number_integer())
        _blockMask = data["block_mask"].get<uint32>();

    if (data.contains("is_active") && data["is_active"].is_boolean())
        _isActive = data["is_active"].get<bool>();

    ColliderShapeDesc loadedShapeDesc = _shapeDesc;
    loadedShapeDesc.shape = _shape;

    if (data.contains("shape_desc") && data["shape_desc"].is_object())
    {
        const json& shapeDescJson = data["shape_desc"];

        if (shapeDescJson.contains("shape") && shapeDescJson["shape"].is_number_integer())
            loadedShapeDesc.shape = static_cast<CollisionShape>(shapeDescJson["shape"].get<int>());

        if (shapeDescJson.contains("center") && shapeDescJson["center"].is_array() && shapeDescJson["center"].size() >= 3)
        {
            loadedShapeDesc.center.x = shapeDescJson["center"][0].get<float>();
            loadedShapeDesc.center.y = shapeDescJson["center"][1].get<float>();
            loadedShapeDesc.center.z = shapeDescJson["center"][2].get<float>();
        }

        if (shapeDescJson.contains("extents") && shapeDescJson["extents"].is_array() && shapeDescJson["extents"].size() >= 3)
        {
            loadedShapeDesc.extents.x = shapeDescJson["extents"][0].get<float>();
            loadedShapeDesc.extents.y = shapeDescJson["extents"][1].get<float>();
            loadedShapeDesc.extents.z = shapeDescJson["extents"][2].get<float>();
        }

        if (shapeDescJson.contains("radians") && shapeDescJson["radians"].is_array() && shapeDescJson["radians"].size() >= 3)
        {
            loadedShapeDesc.radians.x = shapeDescJson["radians"][0].get<float>();
            loadedShapeDesc.radians.y = shapeDescJson["radians"][1].get<float>();
            loadedShapeDesc.radians.z = shapeDescJson["radians"][2].get<float>();
        }

        if (shapeDescJson.contains("radius") && shapeDescJson["radius"].is_number())
            loadedShapeDesc.radius = shapeDescJson["radius"].get<float>();
    }

    Set_ShapeDesc(loadedShapeDesc);

    if (_preset != Collision_Preset::Custom)
        Set_CollisionPreset(_preset);
    else
    {
        _isTrigger = (_blockMask == 0);

        if (_pxShape != nullptr)
        {
            if (_isTrigger)
            {
                _pxShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                _pxShape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
            }
            else
            {
                _pxShape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
                _pxShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
            }

            Apply_PhysX_FilterData();
        }
    }
}

void ColliderCom::Update_Collider(const Matrix& worldMatrix)
{
    if (nullptr == _bounding)
        return;

    _bounding->Update(worldMatrix);
}

bool ColliderCom::Intersect(Shared<ColliderCom> target)
{
    if (nullptr == _bounding || nullptr == target)
        return false;

    Shared<Bounding> targetBounding = target->Get_Bounding();
    if (nullptr == targetBounding)
        return false;

    return _bounding->Intersect(targetBounding.get());
}

void ColliderCom::Add_Overlap(Shared<ColliderCom> other)
{
    if (nullptr == other)
        return;

    _overlapSet.emplace(other);
}

void ColliderCom::Remove_Overlap(Shared<ColliderCom> other)
{
    if (nullptr == other)
        return;

    auto iter = _overlapSet.begin();
    while (iter != _overlapSet.end())
    {
        Shared<ColliderCom> overlap = iter->lock();

        if (nullptr == overlap || overlap == other)
            iter = _overlapSet.erase(iter);
        else
            ++iter;
    }
}

bool ColliderCom::Is_Overlapping(Shared<ColliderCom> other) const
{
    if (nullptr == other)
        return false;

    for (const auto& weakCollider : _overlapSet)
    {
        Shared<ColliderCom> overlap = weakCollider.lock();

        if (overlap == other)
            return true;
    }

    return false;
}

void ColliderCom::Clear_Overlap()
{
    _overlapSet.clear();
}

void ColliderCom::Set_CollisionPreset(Collision_Preset preset)
{
    _preset = preset;

    const Collision_Preset_Data& data = Get_PresetData(preset);

    _channel = data.channel;
    _overlapMask = data.overlapMask;
    _blockMask = data.blockMask;

    _isTrigger = (_blockMask == 0);

    if (_pxShape != nullptr)
    {
        if (_isTrigger)
        {
            _pxShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
            _pxShape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
        }
        else
        {
            _pxShape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
            _pxShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
        }
        Apply_PhysX_FilterData();
    }
}

void ColliderCom::Set_ShapeDesc(const ColliderShapeDesc& shapeDesc)
{
    _shapeDesc = shapeDesc;
    _shapeDesc.shape = _shape;
    Invalidate_PhysXSyncCache();

    switch (_shape)
    {
    case CollisionShape::AABB:
    {
        Shared<Bounding_AABB> aabb = dynamic_pointer_cast<Bounding_AABB>(_bounding);
        if (aabb == nullptr)
            break;

        BoundingBox& origin = aabb->Get_OriginAABB();
        origin.Center = _shapeDesc.center;
        origin.Extents = _shapeDesc.extents;
        break;
    }

    case CollisionShape::OBB:
    {
        Shared<Bounding_OBB> obb = dynamic_pointer_cast<Bounding_OBB>(_bounding);
        if (obb == nullptr)
            break;

        BoundingOrientedBox& origin = obb->Get_OriginOBB();
        origin.Center = _shapeDesc.center;
        origin.Extents = _shapeDesc.extents;
        origin.Orientation = Quat::CreateFromYawPitchRoll(
            _shapeDesc.radians.y,
            _shapeDesc.radians.x,
            _shapeDesc.radians.z);
        break;
    }

    case CollisionShape::Sphere:
    {
        Shared<Bounding_Sphere> sphere = dynamic_pointer_cast<Bounding_Sphere>(_bounding);
        if (sphere == nullptr)
            break;

        BoundingSphere& origin = sphere->Get_OriginSphere();
        origin.Center = _shapeDesc.center;
        origin.Radius = _shapeDesc.radius;
        break;
    }

    default:
        break;
    }
}

HRESULT ColliderCom::Ready_DebugRenderResources()
{
    static constexpr D3D11_INPUT_ELEMENT_DESC kDebugInputElements[] =
    {
        { "SV_Position", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",       0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    try
    {
        _batch = make_shared<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>>(_context.Get());
        _effect = make_shared<DirectX::BasicEffect>(_device.Get());
    }
    catch (const exception& e)
    {
        const HRESULT removedReason = _device ? _device->GetDeviceRemovedReason() : E_FAIL;

        LOG_ERROR(
            "ColliderCom debug render resource creation failed. shape={}, deviceRemovedReason=0x{:08X}, exception={}",
            static_cast<int>(_shape),
            static_cast<unsigned int>(removedReason),
            e.what()
        );

        return E_FAIL;
    }

    if (nullptr == _batch || nullptr == _effect)
        return E_FAIL;

    _effect->SetVertexColorEnabled(true);

    const void* shaderByteCode = nullptr;
    size_t byteCodeLength = 0;

    _effect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

    if (FAILED(_device->CreateInputLayout(
        kDebugInputElements,
        static_cast<UINT>(_countof(kDebugInputElements)),
        shaderByteCode,
        byteCodeLength,
        &_inputLayout)))
    {
        return E_FAIL;
    }

    return S_OK;
}

void ColliderCom::Release_PhysX()
{
    auto Release_PX = [](auto& px)
    {
        if (px)
        {
            px->release();
            px = nullptr;
        }
    };

    Release_PX(_pxShape);
    Release_PX(_pxActor);

    _physxRegistered = false;
    Invalidate_PhysXSyncCache();
}

HRESULT ColliderCom::Build_PhysXActor(PxPhysics* physics)
{
    if (physics == nullptr)
        return E_FAIL;

    Release_PhysX();

    const PxGeometryHolder geometryHolder = Build_PxGeometry();
    const PxGeometry& geometry = geometryHolder.any();

    PxMaterial* material = physics->createMaterial(0.5f, 0.5f, 0.5f);
    if (material == nullptr)
        return E_FAIL;

    PxRigidDynamic* dynamicActor = physics->createRigidDynamic(PxTransform(PxIdentity));
    if (dynamicActor == nullptr)
    {
        material->release();
        return E_FAIL;
    }

    PxShape* shape = physics->createShape(geometry, *material, true);
    material->release();

    if (shape == nullptr)
    {
        dynamicActor->release();
        return E_FAIL;
    }

    if (_isTrigger)
    {
        shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
        shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
    }
    else
    {
        shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
        shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
    }

    dynamicActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);

    if (dynamicActor->attachShape(*shape) == false)
    {
        shape->release();
        dynamicActor->release();
        return E_FAIL;
    }

    _pxActor = dynamicActor;
    _pxShape = shape;

    _pxActor->userData = this;
    _pxShape->userData = this;

    Apply_PhysX_FilterData();

    _physxRegistered = false;

    return S_OK;
}

void ColliderCom::Sync_ToPhysX(const Matrix& worldMatrix)
{
    if (_pxActor == nullptr)
        return;

    if (Is_PhysXSyncCacheValid(worldMatrix))
        return;

    Update_PhysXGeometry(worldMatrix);

    const PxTransform pxTransform = Build_PXTransform(worldMatrix);

    if (PxRigidDynamic* dynamicActor = _pxActor->is<PxRigidDynamic>())
    {
        if (dynamicActor->getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC))
            dynamicActor->setKinematicTarget(pxTransform);
        else
            dynamicActor->setGlobalPose(pxTransform);
    }
    else
    {
        _pxActor->setGlobalPose(pxTransform);
    }

    Store_PhysXSyncCache(worldMatrix);
}

void ColliderCom::Sync_InitialPose_ToPhysX(const Matrix& worldMatrix)
{
    if (_pxActor == nullptr)
        return;

    Update_PhysXGeometry(worldMatrix);

    const PxTransform pxTransform = Build_PXTransform(worldMatrix);
    _pxActor->setGlobalPose(pxTransform);
    Store_PhysXSyncCache(worldMatrix);
}

bool ColliderCom::Is_PhysXSyncCacheValid(const Matrix& worldMatrix) const
{
    return _hasPhysXSyncCache && NearlyEqual(_lastPhysXSyncWorldMatrix, worldMatrix);
}

void ColliderCom::Store_PhysXSyncCache(const Matrix& worldMatrix)
{
    _lastPhysXSyncWorldMatrix = worldMatrix;
    _hasPhysXSyncCache = true;
}

void ColliderCom::Invalidate_PhysXSyncCache()
{
    _hasPhysXSyncCache = false;
}

PxTransform ColliderCom::Build_PXTransform(const Matrix& worldMatrix) const
{
    Matrix temp = worldMatrix;

    Vec3 scale;
    Quat rotation;
    Vec3 translation;

    temp.Decompose(scale, rotation, translation);

    return PxTransform(
        PxVec3(translation.x, translation.y, translation.z),
        PxQuat(rotation.x, rotation.y, rotation.z, rotation.w)
    );
}

PxTransform ColliderCom::Build_PxLocalPose(const Matrix& worldMatrix) const
{
    Matrix temp = worldMatrix;

    Vec3 scale;
    Quat rotation;
    Vec3 translation;
    temp.Decompose(scale, rotation, translation);

    const Vec3 scaledCenter(
        _shapeDesc.center.x * scale.x,
        _shapeDesc.center.y * scale.y,
        _shapeDesc.center.z * scale.z);

    const Quat localRotation = Quat::CreateFromYawPitchRoll(
        _shapeDesc.radians.y,
        _shapeDesc.radians.x,
        _shapeDesc.radians.z);

    return PxTransform(
        PxVec3(scaledCenter.x, scaledCenter.y, scaledCenter.z),
        PxQuat(localRotation.x, localRotation.y, localRotation.z, localRotation.w));
}

void ColliderCom::Apply_PhysX_FilterData()
{
    if (_pxShape == nullptr)
        return;

    const PxFilterData filterData = Build_FilterData(_channel, _overlapMask, _blockMask);

    _pxShape->setSimulationFilterData(filterData);
    _pxShape->setQueryFilterData(filterData);
}

PxTransform ColliderCom::Build_QueryTransform(const Matrix& worldMatrix) const
{
    return Build_PXTransform(worldMatrix) * Build_PxLocalPose(worldMatrix);
}

PxGeometryHolder ColliderCom::Build_QueryGeometry(const Matrix& worldMatrix) const
{
    Matrix tempMatrix = worldMatrix;

    Vec3 scale;
    Quat rotation;
    Vec3 translation;

    tempMatrix.Decompose(scale, rotation, translation);

    const float sx = abs(scale.x);
    const float sy = abs(scale.y);
    const float sz = abs(scale.z);

    switch (_shapeDesc.shape)
    {
    case CollisionShape::AABB:
    case CollisionShape::OBB:
        return PxBoxGeometry(
            _shapeDesc.extents.x * sx,
            _shapeDesc.extents.y * sy,
            _shapeDesc.extents.z * sz);

    case CollisionShape::Sphere:
    {
        const float maxScale = max(sx, max(sy, sz));
        return PxSphereGeometry(_shapeDesc.radius * maxScale);
    }

    default:
        return PxBoxGeometry(0.5f, 0.5f, 0.5f);
    }
}

PxGeometryHolder ColliderCom::Build_PxGeometry() const
{
    switch (_shapeDesc.shape)
    {
    case CollisionShape::AABB:
    case CollisionShape::OBB:
        return PxBoxGeometry(
            _shapeDesc.extents.x,
            _shapeDesc.extents.y,
            _shapeDesc.extents.z);

    case CollisionShape::Sphere:
        return PxSphereGeometry(_shapeDesc.radius);

    default:
        return PxBoxGeometry(0.5f, 0.5f, 0.5f);
    }
}

void ColliderCom::Update_PhysXGeometry(const Matrix& worldMatrix)
{
    Matrix tempMatrix = worldMatrix;

    if (_pxShape == nullptr)
        return;

    _pxShape->setLocalPose(Build_PxLocalPose(worldMatrix));

    Vec3 scale;
    Quat rotation;
    Vec3 translation;

    tempMatrix.Decompose(scale, rotation, translation);

    const float sx = abs(scale.x);
    const float sy = abs(scale.y);
    const float sz = abs(scale.z);

    switch (_shapeDesc.shape)
    {
    case CollisionShape::AABB:
    case CollisionShape::OBB:
    {
        PxBoxGeometry boxGeom(
            _shapeDesc.extents.x * sx,
            _shapeDesc.extents.y * sy,
            _shapeDesc.extents.z * sz);

        _pxShape->setGeometry(boxGeom);
        break;
    }

    case CollisionShape::Sphere:
    {
        const float maxScale = max(sx, max(sy, sz));
        PxSphereGeometry sphereGeom(_shapeDesc.radius * maxScale);
        _pxShape->setGeometry(sphereGeom);
        break;
    }

    default:
        break;
    }
}

HRESULT ColliderCom::Render_Debug()
{
    if (nullptr == _bounding || nullptr == _batch || nullptr == _effect || nullptr == _inputLayout)
        return S_OK;

    // 이펙트/파티클 렌더가 남긴 파이프라인 상태 정리하기
    // GS + AlphaBlend + SRV 때문에 문제생기네
    {
        ID3D11GeometryShader* nullGeometryShader = nullptr;
        _context->GSSetShader(nullGeometryShader, nullptr, 0);

        ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
        _context->PSSetShaderResources(0, 1, nullSRV);

        _context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
        _context->OMSetDepthStencilState(nullptr, 0);
        _context->RSSetState(nullptr);
    }

    _effect->SetWorld(Matrix::Identity);
    _effect->SetView(*GAME->Get_Transform(D3DTS::View));
    _effect->SetProjection(*GAME->Get_Transform(D3DTS::Proj));

    _context->IASetInputLayout(_inputLayout.Get());
    _effect->Apply(_context.Get());

    _batch->Begin();

    Color color = Resolve_CollisionDebugColor(_preset);

    if (_isActive == false)
        color = kInactiveCollisionDebugColor;
    else if (_isColl)
        color = Brighten_CollisionDebugColor(color);

    _bounding->Render_Debug(_batch.get(), color);

    _batch->End();

    return S_OK;
}

Shared<ColliderCom> ColliderCom::Create(
    const ComPtr<Device>& device,
    const ComPtr<Context>& context,
    CollisionShape shape)
{
    auto instance = make_shared<ColliderCom>(device, context);

    if (FAILED(instance->Initialize_Prototype(shape)))
    {
        LOG_WARN("Failed to create ColliderCom: shape={}", static_cast<int>(shape));
        return nullptr;
    }

    return instance;
}

Shared<Component> ColliderCom::Clone(void* arg)
{
    auto instance = make_shared<ColliderCom>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        MSG_BOX("Failed to Clone : ColliderCom");
        return nullptr;
    }

    return instance;
}

void ColliderCom::Free()
{
    if (_physxRegistered && GAME != nullptr)
        GAME->Unregister_PhysXCollider(this);

    Component::Free();

    Release_PhysX();

    _bounding = nullptr;
    _overlapSet.clear();

    _batch.reset();
    _effect.reset();
    _inputLayout.Reset();
}
