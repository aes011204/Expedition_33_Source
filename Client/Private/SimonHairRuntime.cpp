#include "pch.h"
#include "SimonHairRuntime.h"

#include "GameInstance.h"
#include "ModelCom.h"
#include "PhysX_Filter.h"
#include "PhysX_Manager.h"
#include "ModelPhysicsParser.h"
#include "Reflection_Types.h"

#include <algorithm>
#include <cmath>
#include <fstream>

namespace
{
    template<typename Member>
    size_t SimonHairOffset(Member Client::SimonHairRuntime::Desc::* member)
    {
        Client::SimonHairRuntime::Desc desc{};
        const auto* base = reinterpret_cast<const char*>(&desc);
        const auto* field = reinterpret_cast<const char*>(&(desc.*member));
        return static_cast<size_t>(field - base);
    }

    void AddRuntimeProperty(
        vector<Engine::PropertyInfo>& properties,
        Engine::PropertyType type,
        const string& name,
        const string& displayName,
        size_t offset,
        float dragSpeed = 0.01f,
        float minVal = 0.f,
        float maxVal = 0.f)
    {
        Engine::PropertyInfo property{};
        property.name = name;
        property.displayName = displayName;
        property.type = type;
        property.offset = offset;
        property.dragSpeed = dragSpeed;
        property.minVal = minVal;
        property.maxVal = maxVal;
        properties.push_back(std::move(property));
    }

    filesystem::path Resolve_PhysicsAssetPath_Local(const wstring& filePath)
    {
        filesystem::path resolvedPath = filePath;
        if (filesystem::exists(resolvedPath))
        {
            return resolvedPath;
        }

        const filesystem::path fileName = filesystem::path(filePath).filename();
        filesystem::path cursor = filesystem::current_path();
        for (int32 i = 0; i < 6 && !cursor.empty(); ++i)
        {
            const filesystem::path clientCandidate = cursor / L"Client" / L"Bin" / L"Resources" / L"ModelPhysics" / fileName;
            if (filesystem::exists(clientCandidate))
            {
                return clientCandidate;
            }

            const filesystem::path binCandidate = cursor / L"Bin" / L"Resources" / L"ModelPhysics" / fileName;
            if (filesystem::exists(binCandidate))
            {
                return binCandidate;
            }

            cursor = cursor.parent_path();
        }

        return resolvedPath;
    }

    Quaternion Parse_UePrimitiveRotation_Local(const nlohmann::json& rotationNode)
    {
        return Quaternion::CreateFromYawPitchRoll(
            XMConvertToRadians(rotationNode.value("yaw", 0.f)),
            XMConvertToRadians(rotationNode.value("pitch", 0.f)),
            XMConvertToRadians(rotationNode.value("roll", 0.f)));
    }

    vector<Engine::ChainModifier::CapsuleColliderDesc> LoadCapsulesFromJSON_RawBodyLocal(const wstring& filePath)
    {
        vector<Engine::ChainModifier::CapsuleColliderDesc> colliders{};
        const filesystem::path resolvedPath = Resolve_PhysicsAssetPath_Local(filePath);
        if (!filesystem::exists(resolvedPath))
        {
            return colliders;
        }

        std::ifstream file(resolvedPath);
        if (!file.is_open())
        {
            return colliders;
        }

        nlohmann::json data{};
        try
        {
            file >> data;
        }
        catch (...)
        {
            return colliders;
        }

        if (!data.is_array())
        {
            return colliders;
        }

        for (const auto& item : data)
        {
            Engine::ChainModifier::CapsuleColliderDesc desc{};
            desc.boneName = item.value("boneName", "");

            const auto& center = item["center"];
            desc.center = Vec3(
                center.value("x", 0.f),
                center.value("y", 0.f),
                center.value("z", 0.f));

            const auto& rotation = item["rotation"];
            desc.rotationQuat = Parse_UePrimitiveRotation_Local(rotation);

            desc.radius = item.value("radius", 0.f);
            desc.halfLength = std::max(0.f, item.value("length", 0.f) * 0.5f - desc.radius);
            colliders.push_back(desc);
        }

        return colliders;
    }

    PxVec3 To_PxVec3(const Vec3& value)
    {
        return PxVec3(value.x, value.y, value.z);
    }

    Vec3 From_PxVec3(const PxVec3& value)
    {
        return Vec3(value.x, value.y, value.z);
    }

    PxQuat To_PxQuat(const Quaternion& value)
    {
        return PxQuat(value.x, value.y, value.z, value.w);
    }

    Quaternion From_PxQuat(const PxQuat& value)
    {
        return Quaternion(value.x, value.y, value.z, value.w);
    }

    bool Build_SegmentWorldFrame(const Vec3& start, const Vec3& end, Matrix& outFrame)
    {
        Vec3 segmentAxis = end - start;
        const float segmentLength = segmentAxis.Length();
        if (segmentLength <= 0.0001f)
        {
            return false;
        }

        segmentAxis /= segmentLength;

        Vec3 from = Vec3::UnitX;
        Vec3 to = segmentAxis;
        float dot = std::clamp(from.Dot(to), -1.f, 1.f);
        Quaternion rotation = Quaternion::Identity;
        if (dot < 0.9999f)
        {
            if (dot <= -0.9999f)
            {
                rotation = Quaternion::CreateFromAxisAngle(Vec3::UnitY, XM_PI);
            }
            else
            {
                Vec3 axis = from.Cross(to);
                axis.Normalize();
                rotation = Quaternion::CreateFromAxisAngle(axis, acosf(dot));
            }
        }

        const Vec3 center = (start + end) * 0.5f;
        outFrame = Matrix::CreateFromQuaternion(rotation) * Matrix::CreateTranslation(center);
        return true;
    }

    PxTransform Build_CapsuleTransform(const Client::SimonHairRuntime::WorldCapsule& capsule)
    {
        const Vec3 center = (capsule.startPoint + capsule.endPoint) * 0.5f;
        Vec3 axis = capsule.startPoint - capsule.endPoint;
        if (axis.LengthSquared() <= 0.000001f)
        {
            axis = Vec3::UnitX;
        }
        else
        {
            axis.Normalize();
        }

        Vec3 from = Vec3::UnitX;
        Vec3 to = axis;
        float dot = std::clamp(from.Dot(to), -1.f, 1.f);
        Quaternion rotation = Quaternion::Identity;
        if (dot < 0.9999f)
        {
            if (dot <= -0.9999f)
            {
                rotation = Quaternion::CreateFromAxisAngle(Vec3::UnitY, XM_PI);
            }
            else
            {
                Vec3 axisVector = from.Cross(to);
                axisVector.Normalize();
                rotation = Quaternion::CreateFromAxisAngle(axisVector, acosf(dot));
            }
        }
        rotation.Normalize();

        return PxTransform(To_PxVec3(center), To_PxQuat(rotation));
    }

    PxQuat Build_AxisAlignmentQuat(PxVec3 axis)
    {
        if (axis.magnitudeSquared() <= 0.000001f)
        {
            axis = PxVec3(1.f, 0.f, 0.f);
        }
        else
        {
            axis.normalize();
        }

        const PxVec3 from(1.f, 0.f, 0.f);
        const float dot = std::clamp(from.dot(axis), -1.f, 1.f);
        if (dot >= 0.9999f)
        {
            return PxQuat(PxIdentity);
        }

        if (dot <= -0.9999f)
        {
            return PxQuat(PxPi, PxVec3(0.f, 1.f, 0.f));
        }

        PxVec3 rotAxis = from.cross(axis);
        if (rotAxis.magnitudeSquared() <= 0.000001f)
        {
            rotAxis = PxVec3(0.f, 1.f, 0.f);
        }
        rotAxis.normalize();
        return PxQuat(acosf(dot), rotAxis);
    }

    Quaternion Extract_Rotation(const Matrix& matrix)
    {
        Vec3 scale = Vec3::One;
        Quaternion rotation = Quaternion::Identity;
        Vec3 translation = Vec3::Zero;
        Matrix temp = matrix;
        temp.Decompose(scale, rotation, translation);
        rotation.Normalize();
        return rotation;
    }

    Vec3 Extract_Translation(const Matrix& matrix)
    {
        return Vec3(matrix._41, matrix._42, matrix._43);
    }

    Matrix Build_MatrixFromPxTransform(const PxTransform& transform)
    {
        const Quaternion rotation = From_PxQuat(transform.q);
        const Vec3 translation = From_PxVec3(transform.p);

        Matrix matrix = Matrix::CreateFromQuaternion(rotation);
        matrix._41 = translation.x;
        matrix._42 = translation.y;
        matrix._43 = translation.z;
        matrix._44 = 1.f;
        return matrix;
    }

    void Configure_ShapeFilter(PxShape* shape, Collision_Channel channel, uint32 overlapMask, uint32 blockMask)
    {
        if (shape == nullptr)
        {
            return;
        }

        PxFilterData filterData{};
        filterData.word0 = ETOI(channel);
        filterData.word1 = overlapMask;
        filterData.word2 = blockMask;
        filterData.word3 = 0;
        shape->setSimulationFilterData(filterData);
        shape->setQueryFilterData(filterData);
    }

    void Safe_Release(PxBase*& object)
    {
        if (object != nullptr)
        {
            object->release();
            object = nullptr;
        }
    }
}

NS_BEGIN(Client)

SimonHairRuntime::SimonHairRuntime(const SimonHairRuntime& other)
    : _desc(other._desc)
    , _colliders(other._colliders)
{
}

SimonHairRuntime& SimonHairRuntime::operator=(const SimonHairRuntime& other)
{
    if (this == &other)
    {
        return *this;
    }

    Release_PhysXResources();

    _desc = other._desc;
    _chains.clear();
    _colliders = other._colliders;
    _runtimeDebugCapsules.clear();
    _runtimeDebugSegments.clear();
    _blockers.clear();
    _prevOwnerWorld = Matrix::Identity;
    _prevOwnerPosition = Vec3::Zero;
    _hasPrevOwnerWorld = false;
    _built = false;
    _physxResourcesReady = false;
    _physics = nullptr;
    _scene = nullptr;
    _material = nullptr;

    return *this;
}

SimonHairRuntime::~SimonHairRuntime()
{
    Release_PhysXResources();
}

void SimonHairRuntime::Initialize(ModelCom* model, const Desc& desc)
{
    Release_PhysXResources();

    _desc = desc;
    _chains.clear();
    _colliders.clear();
    _runtimeDebugCapsules.clear();
    _runtimeDebugSegments.clear();
    _blockers.clear();
    _prevOwnerWorld = Matrix::Identity;
    _prevOwnerPosition = Vec3::Zero;
    _hasPrevOwnerWorld = false;
    _built = false;
    _physxResourcesReady = false;

    if (model == nullptr || !model->Is_Skeletal())
    {
        return;
    }

    if (!_desc.physicsAssetPath.empty())
    {
        _colliders = _desc.useRawBodyLocalCollisionAsset
            ? LoadCapsulesFromJSON_RawBodyLocal(_desc.physicsAssetPath)
            : ModelPhysicsParser::LoadCapsulesFromJSON(_desc.physicsAssetPath);
    }

    Initialize_Chains(model);
    _built = !_chains.empty();
}

void SimonHairRuntime::ResetPose(ModelCom* model, const Matrix& ownerWorld, const Matrix& motionDriverWorld)
{
    if (!_built || model == nullptr)
    {
        return;
    }

    Capture_CurrentPose(model, ownerWorld);

    if (_physxResourcesReady)
    {
        Reset_PhysXStateToPose(model, nullptr, ownerWorld, Matrix::Identity);
        Update_SolvedPositionsFromActors();
        Write_BackToModel(model, ownerWorld, false);
    }

    _prevOwnerWorld = motionDriverWorld;
    _prevOwnerPosition = Get_Position(motionDriverWorld);
    _hasPrevOwnerWorld = true;
}

void SimonHairRuntime::Simulate(
    ModelCom* model,
    const ModelCom* collisionModel,
    float timeDelta,
    const Matrix& ownerWorld,
    const Matrix& collisionOwnerWorld,
    const Matrix& motionDriverWorld)
{
    if (!_built || model == nullptr || timeDelta <= 0.f)
    {
        return;
    }

    Capture_CurrentPose(model, ownerWorld);

    if (!Ensure_PhysXResources(model, collisionModel, ownerWorld, collisionOwnerWorld))
    {
        return;
    }

    const Vec3 ownerPosition = Get_Position(motionDriverWorld);
    if (_hasPrevOwnerWorld)
    {
        const float teleportDistance = std::max(0.f, _desc.teleportDistance);
        if (teleportDistance > 0.f && Vec3::Distance(ownerPosition, _prevOwnerPosition) >= teleportDistance)
        {
            Reset_PhysXStateToPose(model, collisionModel, ownerWorld, collisionOwnerWorld);
        }
    }
    else
    {
        Reset_PhysXStateToPose(model, collisionModel, ownerWorld, collisionOwnerWorld);
    }

    const vector<WorldCapsule> worldCapsules = Build_WorldCapsules(collisionModel, collisionOwnerWorld);
    Sync_KinematicActors(model, collisionModel, ownerWorld, collisionOwnerWorld, motionDriverWorld);
    Update_SolvedPositionsFromActors();
    Apply_PostSimulationConstraints(model, ownerWorld, worldCapsules, timeDelta);

    const bool isOwnerIdle = !_hasPrevOwnerWorld
        || Vec3::Distance(ownerPosition, _prevOwnerPosition) <= std::max(0.f, _desc.idleMotionDeadzone);
    Write_BackToModel(model, ownerWorld, isOwnerIdle);
    Build_DebugSegmentsFromModel(model, ownerWorld);

    _prevOwnerWorld = motionDriverWorld;
    _prevOwnerPosition = ownerPosition;
    _hasPrevOwnerWorld = true;
}

const Engine::ClassReflectionInfo* SimonHairRuntime::Get_RuntimeReflectionInfo() const
{
    static Engine::ClassReflectionInfo info = []()
        {
            Engine::ClassReflectionInfo result{};
            result.className = "SimonHairRuntime";
            result.displayName = "Simon Hair Runtime";
            result.category = "Animation";
            result.type = Engine::RegisteredType::None;

            AddRuntimeProperty(result.properties, Engine::PropertyType::Bool, "useAnimatedPoseTarget", "Use Anim", SimonHairOffset(&Desc::useAnimatedPoseTarget));
            AddRuntimeProperty(result.properties, Engine::PropertyType::Bool, "enableCollision", "Collision", SimonHairOffset(&Desc::enableCollision));
            AddRuntimeProperty(result.properties, Engine::PropertyType::Bool, "debugDrawCollisionCapsules", "Debug Col", SimonHairOffset(&Desc::debugDrawCollisionCapsules));

            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "contactInertiaDamping", "Hit Damp", SimonHairOffset(&Desc::contactInertiaDamping), 0.01f, 0.f, 1.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "releaseInertiaDamping", "Release Damp", SimonHairOffset(&Desc::releaseInertiaDamping), 0.01f, 0.f, 1.f);

            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "outputAlpha", "Out", SimonHairOffset(&Desc::outputAlpha), 0.01f, 0.f, 1.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "rotationOutputAlpha", "Rot Out", SimonHairOffset(&Desc::rotationOutputAlpha), 0.01f, 0.f, 1.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "translationOutputAlpha", "Trans Out", SimonHairOffset(&Desc::translationOutputAlpha), 0.01f, 0.f, 1.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "translationOutputLimitScale", "Trans Limit", SimonHairOffset(&Desc::translationOutputLimitScale), 0.01f, 0.f, 1.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "outputSmoothAlpha", "Smooth", SimonHairOffset(&Desc::outputSmoothAlpha), 0.01f, 0.f, 1.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "idleOutputSmoothAlpha", "Idle Smooth", SimonHairOffset(&Desc::idleOutputSmoothAlpha), 0.01f, 0.f, 1.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "idleMotionDeadzone", "Idle Dead", SimonHairOffset(&Desc::idleMotionDeadzone), 0.001f, 0.f, 10.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "maxOutputAngleDegree", "Max Angle", SimonHairOffset(&Desc::maxOutputAngleDegree), 0.25f, 0.f, 180.f);

            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "collisionPrimitiveScale", "Col Scale", SimonHairOffset(&Desc::collisionPrimitiveScale), 0.005f, 0.001f, 10.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "collisionRadiusPadding", "Col Pad", SimonHairOffset(&Desc::collisionRadiusPadding), 0.01f, 0.f, 10.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "collisionContactSkinWidth", "Col Skin", SimonHairOffset(&Desc::collisionContactSkinWidth), 0.005f, 0.f, 1.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "collisionSphereRadius", "Node Radius", SimonHairOffset(&Desc::collisionSphereRadius), 0.005f, 0.01f, 10.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "collisionPushOutScale", "Col Push", SimonHairOffset(&Desc::collisionPushOutScale), 0.01f, 0.f, 5.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "contactSnapAlpha", "Col Snap", SimonHairOffset(&Desc::contactSnapAlpha), 0.01f, 0.f, 1.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Int, "collisionProjectionIterations", "Col Iter", SimonHairOffset(&Desc::collisionProjectionIterations), 1.f, 1.f, 16.f);

            AddRuntimeProperty(result.properties, Engine::PropertyType::Int, "solverIterations", "Solver Iter", SimonHairOffset(&Desc::solverIterations), 1.f, 1.f, 32.f);
            return result;
        }();

    return &info;
}

void SimonHairRuntime::On_RuntimeReflectionChanged(
    ModelCom* model,
    const ModelCom* collisionModel,
    const Matrix& ownerWorld,
    const Matrix& collisionOwnerWorld,
    const Matrix& motionDriverWorld)
{
    RuntimeRebuild(model, collisionModel, ownerWorld, collisionOwnerWorld, motionDriverWorld);
}

void SimonHairRuntime::RuntimeRebuild(
    ModelCom* model,
    const ModelCom* collisionModel,
    const Matrix& ownerWorld,
    const Matrix& collisionOwnerWorld,
    const Matrix& motionDriverWorld)
{
    if (model == nullptr)
    {
        return;
    }

    Initialize(model, _desc);
    if (!_built)
    {
        return;
    }

    Capture_CurrentPose(model, ownerWorld);
    Ensure_PhysXResources(model, collisionModel, ownerWorld, collisionOwnerWorld);
    ResetPose(model, ownerWorld, motionDriverWorld);
    if (_physxResourcesReady)
    {
        Update_SolvedPositionsFromActors();
        Write_BackToModel(model, ownerWorld, true);
        Build_DebugSegmentsFromModel(model, ownerWorld);
        model->Rebuild_BoneCombinedMatrices();
    }
}

void SimonHairRuntime::Release_PhysXResources()
{
    if (_scene != nullptr)
    {
        for (Blocker& blocker : _blockers)
        {
            if (blocker.actor != nullptr)
            {
                _scene->removeActor(*blocker.actor);
            }
        }

        for (Chain& chain : _chains)
        {
            for (Node& node : chain.nodes)
            {
                if (node.actor != nullptr)
                {
                    _scene->removeActor(*node.actor);
                }
            }

            if (chain.anchorActor != nullptr)
            {
                _scene->removeActor(*chain.anchorActor);
            }
        }
    }

    for (Blocker& blocker : _blockers)
    {
        PxBase* blockerActor = blocker.actor;
        Safe_Release(blockerActor);
        blocker.actor = nullptr;
    }
    _blockers.clear();

    for (Chain& chain : _chains)
    {
        for (Node& node : chain.nodes)
        {
            PxBase* joint = node.joint;
            Safe_Release(joint);
            node.joint = nullptr;

            PxBase* actor = node.actor;
            Safe_Release(actor);
            node.actor = nullptr;
        }

        PxBase* anchorActor = chain.anchorActor;
        Safe_Release(anchorActor);
        chain.anchorActor = nullptr;
    }

    PxBase* material = _material;
    Safe_Release(material);
    _material = nullptr;

    _physics = nullptr;
    _scene = nullptr;
    _physxResourcesReady = false;
}

void SimonHairRuntime::Initialize_Chains(ModelCom* model)
{
    _chains.clear();

    if (model == nullptr)
    {
        return;
    }

    for (const ChainDesc& chainDesc : _desc.chains)
    {
        const int32 rootBoneIndex = model->Get_BoneIndex(chainDesc.rootBoneName);
        if (rootBoneIndex < 0)
        {
            continue;
        }

        const int32 anchorBoneIndex = model->Get_BoneParentIndex(static_cast<uint32>(rootBoneIndex));
        if (anchorBoneIndex < 0)
        {
            continue;
        }

        Chain chain{};
        chain.desc = chainDesc;
        chain.anchorBoneIndex = anchorBoneIndex;
        BuildChainRecursive(model, rootBoneIndex, -1, chain);
        if (chain.nodes.empty())
        {
            continue;
        }

        const float depthDivisor = std::max(1.f, static_cast<float>(chain.nodes.size() - 1));
        for (size_t i = 0; i < chain.nodes.size(); ++i)
        {
            chain.nodes[i].depthRatio = static_cast<float>(i) / depthDivisor;
        }

        _chains.push_back(std::move(chain));
    }
}

void SimonHairRuntime::BuildChainRecursive(ModelCom* model, int32 boneIndex, int32 parentNodeIndex, Chain& outChain)
{
    if (model == nullptr || boneIndex < 0)
    {
        return;
    }

    Node node{};
    node.boneIndex = static_cast<uint32>(boneIndex);
    node.parentNodeIndex = parentNodeIndex;
    node.bindLocal = model->Get_BoneLocalMatrix(static_cast<uint32>(boneIndex));
    node.poseLocal = node.bindLocal;

    const int32 nodeIndex = static_cast<int32>(outChain.nodes.size());
    outChain.nodes.push_back(node);

    if (parentNodeIndex >= 0)
    {
        outChain.nodes[static_cast<size_t>(parentNodeIndex)].childNodeIndices.push_back(nodeIndex);
    }

    const uint32 numBones = static_cast<uint32>(model->Get_NumBones());
    for (uint32 childBoneIndex = 0; childBoneIndex < numBones; ++childBoneIndex)
    {
        if (model->Get_BoneParentIndex(childBoneIndex) == boneIndex)
        {
            BuildChainRecursive(model, static_cast<int32>(childBoneIndex), nodeIndex, outChain);
        }
    }
}

void SimonHairRuntime::Capture_CurrentPose(ModelCom* model, const Matrix& ownerWorld)
{
    if (model == nullptr)
    {
        return;
    }

    if (!_desc.useAnimatedPoseTarget)
    {
        for (const Chain& chain : _chains)
        {
            for (const Node& node : chain.nodes)
            {
                model->Set_BoneLocalMatrix(node.boneIndex, node.bindLocal);
            }
        }
    }

    model->Rebuild_BoneCombinedMatrices();

    for (Chain& chain : _chains)
    {
        for (Node& node : chain.nodes)
        {
            const Matrix previousOriginalWorld = node.originalWorld;
            const bool hadPreviousOriginalWorld = node.hasPrevOriginalWorld;
            node.poseLocal = model->Get_BoneLocalMatrix(node.boneIndex);

            const Matrix* combined = model->Get_BoneMatrixPtr(node.boneIndex);
            if (combined == nullptr)
            {
                continue;
            }

            node.originalWorld = (*combined) * ownerWorld;
            node.prevOriginalWorld = hadPreviousOriginalWorld ? previousOriginalWorld : node.originalWorld;
            node.hasPrevOriginalWorld = true;
            const Vec3 nodeWorldPosition = Get_Position(node.originalWorld);

            Matrix parentWorld = ownerWorld;
            if (node.parentNodeIndex >= 0)
            {
                parentWorld = chain.nodes[static_cast<size_t>(node.parentNodeIndex)].originalWorld;
            }
            else if (chain.anchorBoneIndex >= 0)
            {
                if (const Matrix* anchorCombined = model->Get_BoneMatrixPtr(static_cast<uint32>(chain.anchorBoneIndex)))
                {
                    parentWorld = (*anchorCombined) * ownerWorld;
                }
            }

            const Vec3 parentWorldPosition = Get_Position(parentWorld);
            node.length = Vec3::Distance(nodeWorldPosition, parentWorldPosition);

            Vec3 poseWorldDir = nodeWorldPosition - parentWorldPosition;
            if (poseWorldDir.LengthSquared() > 0.0001f)
            {
                poseWorldDir.Normalize();
                node.poseLocalDir = Get_LocalDirection(parentWorld, poseWorldDir);
                if (node.bindLocalDir.LengthSquared() <= 0.0001f)
                {
                    node.bindLocalDir = node.poseLocalDir;
                }
            }
            else
            {
                node.poseLocalDir = node.bindLocalDir;
            }

            if (!node.hasSmoothedLocalTransform)
            {
                Vec3 poseScale = Vec3::One;
                Quaternion poseRotation = Quaternion::Identity;
                Vec3 poseTranslation = Vec3::Zero;
                Matrix temp = node.poseLocal;
                temp.Decompose(poseScale, poseRotation, poseTranslation);
                poseRotation.Normalize();

                node.smoothedLocalRotation = poseRotation;
                node.smoothedLocalTranslation = poseTranslation;
                node.bindScale = poseScale;
                node.hasSmoothedLocalTransform = true;
            }

            if (!node.hasSmoothedLocalDir && node.poseLocalDir.LengthSquared() > 0.0001f)
            {
                node.smoothedLocalDir = node.poseLocalDir;
                node.hasSmoothedLocalDir = true;
            }
        }
    }
}

bool SimonHairRuntime::Ensure_PhysXResources(
    ModelCom* model,
    const ModelCom* collisionModel,
    const Matrix& ownerWorld,
    const Matrix& collisionOwnerWorld)
{
    if (_physxResourcesReady)
    {
        return true;
    }

    _physics = GAME->Get_PhysXPhysics();
    _scene = GAME->Get_PhysXScene();
    if (_physics == nullptr || _scene == nullptr)
    {
        return false;
    }

    _material = _physics->createMaterial(0.2f, 0.2f, 0.0f);
    if (_material == nullptr)
    {
        return false;
    }

    const float baseNodeRadius = std::max(0.05f, _desc.collisionSphereRadius * 0.65f);

    for (Chain& chain : _chains)
    {
        Matrix anchorWorld = ownerWorld;
        if (chain.anchorBoneIndex >= 0)
        {
            if (const Matrix* anchorCombined = model->Get_BoneMatrixPtr(static_cast<uint32>(chain.anchorBoneIndex)))
            {
                anchorWorld = (*anchorCombined) * ownerWorld;
            }
        }

        chain.anchorActor = _physics->createRigidDynamic(PxTransform(To_PxVec3(Get_Position(anchorWorld)), To_PxQuat(Extract_Rotation(anchorWorld))));
        if (chain.anchorActor == nullptr)
        {
            Release_PhysXResources();
            return false;
        }

        chain.anchorActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
        chain.anchorActor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, true);
        _scene->addActor(*chain.anchorActor);

        for (Node& node : chain.nodes)
        {
            const Vec3 nodePosition = Get_Position(node.originalWorld);
            const Quaternion nodeRotation = Extract_Rotation(node.originalWorld);
            node.actor = _physics->createRigidDynamic(PxTransform(To_PxVec3(nodePosition), To_PxQuat(nodeRotation)));
            if (node.actor == nullptr)
            {
                Release_PhysXResources();
                return false;
            }

            const float nodeRadius = std::max(0.04f, std::min(baseNodeRadius, std::max(0.05f, node.length * 0.42f)));
            PxShape* shape = _physics->createShape(PxSphereGeometry(nodeRadius), *_material, true);
            if (shape == nullptr)
            {
                Release_PhysXResources();
                return false;
            }

            Configure_ShapeFilter(
                shape,
                Collision_Channel::Monster_Attack,
                0,
                ETOI(Collision_Channel::Monster_Body));

            node.actor->attachShape(*shape);
            shape->release();

            node.actor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, true);
            node.actor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
            node.actor->setLinearDamping(2.40f + node.depthRatio * 0.45f);
            node.actor->setAngularDamping(4.50f);
            node.actor->setSolverIterationCounts(static_cast<PxU32>(std::max(4u, _desc.solverIterations)), 2);
            node.actor->setSleepThreshold(0.0001f);
            node.actor->setMaxLinearVelocity(80.f);
            node.actor->setMaxAngularVelocity(25.f);
            PxRigidBodyExt::updateMassAndInertia(*node.actor, 0.08f + node.depthRatio * 0.05f);

            _scene->addActor(*node.actor);

            PxRigidActor* jointParentActor = node.parentNodeIndex >= 0
                ? static_cast<PxRigidActor*>(chain.nodes[static_cast<size_t>(node.parentNodeIndex)].actor)
                : static_cast<PxRigidActor*>(chain.anchorActor);
            if (jointParentActor == nullptr)
            {
                Release_PhysXResources();
                return false;
            }

            const PxTransform parentPose = jointParentActor->getGlobalPose();
            const PxTransform childPose = node.actor->getGlobalPose();
            PxVec3 parentPosition = parentPose.p;
            if (node.parentNodeIndex < 0)
            {
                parentPosition = chain.anchorActor->getGlobalPose().p;
            }

            PxVec3 childPosition = childPose.p;
            PxVec3 jointAxis = childPosition - parentPosition;
            if (jointAxis.magnitudeSquared() <= 0.000001f)
            {
                jointAxis = PxVec3(1.f, 0.f, 0.f);
            }
            else
            {
                jointAxis.normalize();
            }

            const PxVec3 worldAnchor = (parentPosition + childPosition) * 0.5f;
            const PxTransform jointWorldFrame(worldAnchor, Build_AxisAlignmentQuat(jointAxis));
            const PxTransform parentLocalFrame = parentPose.transformInv(jointWorldFrame);
            const PxTransform childLocalFrame = childPose.transformInv(jointWorldFrame);

            node.joint = PxD6JointCreate(
                *_physics,
                jointParentActor,
                parentLocalFrame,
                node.actor,
                childLocalFrame);
            if (node.joint == nullptr)
            {
                Release_PhysXResources();
                return false;
            }

            node.joint->setMotion(PxD6Axis::eX, PxD6Motion::eLOCKED);
            node.joint->setMotion(PxD6Axis::eY, PxD6Motion::eLOCKED);
            node.joint->setMotion(PxD6Axis::eZ, PxD6Motion::eLOCKED);
            node.joint->setMotion(PxD6Axis::eTWIST, PxD6Motion::eLIMITED);
            node.joint->setMotion(PxD6Axis::eSWING1, PxD6Motion::eLIMITED);
            node.joint->setMotion(PxD6Axis::eSWING2, PxD6Motion::eLIMITED);

            const float twistLimit = XMConvertToRadians(15.f);
            node.joint->setTwistLimit(PxJointAngularLimitPair(-twistLimit, twistLimit));

            const float swingLimit = XMConvertToRadians(std::clamp(_desc.maxOutputAngleDegree, 5.f, 60.f));
            PxJointLimitCone coneLimit(swingLimit * 0.5f, swingLimit);
            coneLimit.restitution = 0.f;
            coneLimit.bounceThreshold = 0.f;
            coneLimit.stiffness = 0.f;
            coneLimit.damping = 0.f;
            node.joint->setSwingLimit(coneLimit);

            node.joint->setDrive(
                PxD6Drive::eSLERP,
                PxD6JointDrive(
                    150.f + node.depthRatio * 40.f,
                    18.f + node.depthRatio * 6.f,
                    80.f,
                    false));
            node.joint->setDrivePosition(PxTransform(PxIdentity));
            node.joint->setDriveVelocity(PxVec3(0.f), PxVec3(0.f));
            node.joint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, false);
            node.joint->setInvMassScale0(0.f);
            node.joint->setInvInertiaScale0(0.f);
            node.joint->setInvMassScale1(1.f);
            node.joint->setInvInertiaScale1(1.f);
        }
    }

    const vector<WorldCapsule> worldCapsules = Build_WorldCapsules(collisionModel, collisionOwnerWorld);
    for (size_t i = 0; i < worldCapsules.size(); ++i)
    {
        const WorldCapsule& capsule = worldCapsules[i];
        const float halfHeight = std::max(0.f, Vec3::Distance(capsule.startPoint, capsule.endPoint) * 0.5f);
        const bool isSphere = halfHeight <= 0.0001f;

        Blocker blocker{};
        blocker.radius = std::max(0.001f, capsule.radius);
        blocker.halfHeight = halfHeight;
        blocker.isSphere = isSphere;
        blocker.boneName = i < _colliders.size() ? _colliders[i].boneName : string{};

        blocker.actor = _physics->createRigidDynamic(Build_CapsuleTransform(capsule));
        if (blocker.actor == nullptr)
        {
            Release_PhysXResources();
            return false;
        }

        blocker.actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
        blocker.actor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, true);

        PxShape* shape = nullptr;
        if (isSphere)
        {
            shape = _physics->createShape(PxSphereGeometry(blocker.radius), *_material, true);
        }
        else
        {
            shape = _physics->createShape(PxCapsuleGeometry(blocker.radius, blocker.halfHeight), *_material, true);
        }

        if (shape == nullptr)
        {
            Release_PhysXResources();
            return false;
        }

        Configure_ShapeFilter(
            shape,
            Collision_Channel::Monster_Body,
            0,
            ETOI(Collision_Channel::Monster_Attack));

        blocker.actor->attachShape(*shape);
        shape->release();

        _scene->addActor(*blocker.actor);
        _blockers.push_back(blocker);
    }

    _physxResourcesReady = true;
    Reset_PhysXStateToPose(model, collisionModel, ownerWorld, collisionOwnerWorld);
    return true;
}

void SimonHairRuntime::Reset_PhysXStateToPose(
    ModelCom* model,
    const ModelCom* collisionModel,
    const Matrix& ownerWorld,
    const Matrix& collisionOwnerWorld)
{
    UNREFERENCED_PARAMETER(collisionModel);

    if (!_physxResourcesReady || model == nullptr)
    {
        return;
    }

    for (Chain& chain : _chains)
    {
        Matrix anchorWorld = ownerWorld;
        if (chain.anchorBoneIndex >= 0)
        {
            if (const Matrix* anchorCombined = model->Get_BoneMatrixPtr(static_cast<uint32>(chain.anchorBoneIndex)))
            {
                anchorWorld = (*anchorCombined) * ownerWorld;
            }
        }

        if (chain.anchorActor != nullptr)
        {
            const PxTransform anchorPose(To_PxVec3(Get_Position(anchorWorld)), To_PxQuat(Extract_Rotation(anchorWorld)));
            chain.anchorActor->setGlobalPose(anchorPose);
            chain.anchorActor->setKinematicTarget(anchorPose);
        }

        for (Node& node : chain.nodes)
        {
            if (node.actor == nullptr)
            {
                continue;
            }

            const Vec3 posePosition = Get_Position(node.originalWorld);
            const Quaternion poseRotation = Extract_Rotation(node.originalWorld);
            const PxTransform pose(To_PxVec3(posePosition), To_PxQuat(poseRotation));
            node.actor->setGlobalPose(pose);
            node.actor->setLinearVelocity(PxVec3(0.f, 0.f, 0.f));
            node.actor->setAngularVelocity(PxVec3(0.f, 0.f, 0.f));
            node.actor->clearForce();
            node.actor->clearTorque();

            node.solvedPosition = posePosition;
            node.prevSolvedPosition = posePosition;
            node.hadContactThisFrame = false;
            node.hadPushThisFrame = false;
            node.actorToBoneWorld = Build_MatrixFromPxTransform(pose).Invert() * node.originalWorld;
        }
    }

    const vector<WorldCapsule> worldCapsules = Build_WorldCapsules(collisionModel, collisionOwnerWorld);
    const size_t blockerCount = std::min(_blockers.size(), worldCapsules.size());
    for (size_t i = 0; i < blockerCount; ++i)
    {
        if (_blockers[i].actor == nullptr)
        {
            continue;
        }

        const PxTransform pose = Build_CapsuleTransform(worldCapsules[i]);
        _blockers[i].actor->setGlobalPose(pose);
        _blockers[i].actor->setKinematicTarget(pose);
    }
}

void SimonHairRuntime::Sync_KinematicActors(
    ModelCom* model,
    const ModelCom* collisionModel,
    const Matrix& ownerWorld,
    const Matrix& collisionOwnerWorld,
    const Matrix& motionDriverWorld)
{
    UNREFERENCED_PARAMETER(collisionModel);
    UNREFERENCED_PARAMETER(motionDriverWorld);

    if (!_physxResourcesReady || model == nullptr)
    {
        return;
    }

    for (Chain& chain : _chains)
    {
        Matrix anchorWorld = ownerWorld;
        if (chain.anchorBoneIndex >= 0)
        {
            if (const Matrix* anchorCombined = model->Get_BoneMatrixPtr(static_cast<uint32>(chain.anchorBoneIndex)))
            {
                anchorWorld = (*anchorCombined) * ownerWorld;
            }
        }

        if (chain.anchorActor != nullptr)
        {
            const PxTransform anchorPose(To_PxVec3(Get_Position(anchorWorld)), To_PxQuat(Extract_Rotation(anchorWorld)));
            chain.anchorActor->setKinematicTarget(anchorPose);
            chain.anchorActor->wakeUp();
        }

        for (Node& node : chain.nodes)
        {
            node.hadContactThisFrame = false;
            node.hadPushThisFrame = false;
            if (node.actor != nullptr)
            {
                node.actor->wakeUp();
            }
        }
    }

    const vector<WorldCapsule> worldCapsules = Build_WorldCapsules(collisionModel, collisionOwnerWorld);
    const size_t blockerCount = std::min(_blockers.size(), worldCapsules.size());
    for (size_t i = 0; i < blockerCount; ++i)
    {
        if (_blockers[i].actor == nullptr)
        {
            continue;
        }

        const PxTransform pose = Build_CapsuleTransform(worldCapsules[i]);
        _blockers[i].actor->setKinematicTarget(pose);
        _blockers[i].actor->wakeUp();
    }
}

void SimonHairRuntime::Update_SolvedPositionsFromActors()
{
    for (Chain& chain : _chains)
    {
        for (Node& node : chain.nodes)
        {
            node.prevSolvedPosition = node.solvedPosition;

            if (node.actor == nullptr)
            {
                continue;
            }

            const PxTransform actorPose = node.actor->getGlobalPose();
            node.solvedPosition = From_PxVec3(actorPose.p);
        }
    }
}

void SimonHairRuntime::Apply_PostSimulationConstraints(
    ModelCom* model,
    const Matrix& ownerWorld,
    const vector<WorldCapsule>& worldCapsules,
    float timeDelta)
{
    UNREFERENCED_PARAMETER(model);
    UNREFERENCED_PARAMETER(ownerWorld);
    UNREFERENCED_PARAMETER(timeDelta);

    if (!_physxResourcesReady)
    {
        return;
    }

    const float nodeRadius = std::max(0.02f, _desc.collisionSphereRadius);
    const float skinWidth = std::max(0.f, _desc.collisionContactSkinWidth);
    const float pushOutScale = std::max(0.f, _desc.collisionPushOutScale);
    const float contactSnapAlpha = std::clamp(_desc.contactSnapAlpha, 0.f, 1.f);
    const float contactVelocityScale = std::clamp(1.f - std::clamp(_desc.contactInertiaDamping, 0.f, 1.f) * 0.60f, 0.12f, 1.f);
    const float releaseVelocityScale = std::clamp(1.f - std::clamp(_desc.releaseInertiaDamping, 0.f, 1.f) * 0.40f, 0.45f, 1.f);
    const uint32 projectionIterations = std::max(1u, _desc.collisionProjectionIterations);

    for (uint32 iteration = 0; iteration < projectionIterations; ++iteration)
    {
        for (Chain& chain : _chains)
        {
            Vec3 anchorPosition = Vec3::Zero;
            bool hasAnchorPosition = false;
            if (chain.anchorActor != nullptr)
            {
                anchorPosition = From_PxVec3(chain.anchorActor->getGlobalPose().p);
                hasAnchorPosition = true;
            }

            for (Node& node : chain.nodes)
            {
                const Vec3 beforeCollision = node.solvedPosition;
                bool hadCollision = false;
                bool hadPush = false;

                for (const WorldCapsule& capsule : worldCapsules)
                {
                    const Vec3 closestPoint = Closest_PointOnSegment(node.solvedPosition, capsule.startPoint, capsule.endPoint);
                    Vec3 pushDirection = node.solvedPosition - closestPoint;
                    float distance = pushDirection.Length();
                    const float combinedRadius = nodeRadius + capsule.radius + skinWidth;
                    if (distance >= combinedRadius)
                    {
                        continue;
                    }

                    if (distance <= 0.0001f)
                    {
                        Vec3 capsuleAxis = capsule.endPoint - capsule.startPoint;
                        if (capsuleAxis.LengthSquared() <= 0.0001f)
                        {
                            pushDirection = Vec3::UnitY;
                        }
                        else
                        {
                            capsuleAxis.Normalize();
                            pushDirection = capsuleAxis.Cross(Vec3::UnitY);
                            if (pushDirection.LengthSquared() <= 0.0001f)
                            {
                                pushDirection = capsuleAxis.Cross(Vec3::UnitZ);
                            }
                        }
                        pushDirection.Normalize();
                        distance = 0.f;
                    }
                    else
                    {
                        pushDirection /= distance;
                    }

                    const float penetration = combinedRadius - distance;
                    if (penetration <= 0.f)
                    {
                        continue;
                    }

                    node.solvedPosition += pushDirection * penetration * std::max(1.f, pushOutScale);
                    hadCollision = true;
                    hadPush = true;
                }

                Vec3 parentPosition = Vec3::Zero;
                bool hasParentPosition = false;
                if (node.parentNodeIndex >= 0)
                {
                    parentPosition = chain.nodes[static_cast<size_t>(node.parentNodeIndex)].solvedPosition;
                    hasParentPosition = true;
                }
                else if (hasAnchorPosition)
                {
                    parentPosition = anchorPosition;
                    hasParentPosition = true;
                }

                if (hasParentPosition)
                {
                    Vec3 segment = node.solvedPosition - parentPosition;
                    if (segment.LengthSquared() <= 0.0001f)
                    {
                        segment = Get_Position(node.originalWorld) - parentPosition;
                    }
                    if (segment.LengthSquared() > 0.0001f)
                    {
                        segment.Normalize();
                        node.solvedPosition = parentPosition + segment * std::max(0.01f, node.length);
                    }
                }

                if (node.actor != nullptr)
                {
                    if (hadCollision && contactSnapAlpha > 0.f)
                    {
                        node.solvedPosition = Vec3::Lerp(beforeCollision, node.solvedPosition, contactSnapAlpha);
                    }

                    PxTransform pose = node.actor->getGlobalPose();
                    pose.p = To_PxVec3(node.solvedPosition);
                    node.actor->setGlobalPose(pose);
                    node.actor->setLinearVelocity(node.actor->getLinearVelocity() * (hadCollision ? contactVelocityScale : releaseVelocityScale));
                    node.actor->setAngularVelocity(node.actor->getAngularVelocity() * (hadCollision ? contactVelocityScale : releaseVelocityScale));
                    node.actor->wakeUp();
                }

                node.hadContactThisFrame = node.hadContactThisFrame || hadCollision;
                node.hadPushThisFrame = node.hadPushThisFrame || hadPush
                    || (beforeCollision - node.solvedPosition).LengthSquared() > 0.0001f;
            }
        }
    }
}

void SimonHairRuntime::Write_BackToModel(ModelCom* model, const Matrix& ownerWorld, bool isOwnerIdle)
{
    if (model == nullptr)
    {
        return;
    }

    const float smoothAlpha = isOwnerIdle
        ? std::clamp(_desc.idleOutputSmoothAlpha, 0.f, 1.f)
        : std::clamp(_desc.outputSmoothAlpha, 0.f, 1.f);
    const float overallOutputAlpha = std::clamp(_desc.outputAlpha, 0.f, 1.f);

    for (Chain& chain : _chains)
    {
        for (Node& node : chain.nodes)
        {
            if (node.actor == nullptr)
            {
                continue;
            }

            Vec3 poseScale = node.bindScale;
            Quaternion poseRotation = Quaternion::Identity;
            Vec3 poseTranslation = Vec3::Zero;
            Matrix poseLocal = node.poseLocal;
            poseLocal.Decompose(poseScale, poseRotation, poseTranslation);
            poseRotation.Normalize();

            const Matrix actorWorld = Build_MatrixFromPxTransform(node.actor->getGlobalPose());
            const Matrix targetBoneWorld = actorWorld * node.actorToBoneWorld;

            Matrix parentWorld = ownerWorld;
            const int32 parentBoneIndex = model->Get_BoneParentIndex(node.boneIndex);
            if (parentBoneIndex >= 0)
            {
                if (const Matrix* parentCombined = model->Get_BoneMatrixPtr(static_cast<uint32>(parentBoneIndex)))
                {
                    parentWorld = (*parentCombined) * ownerWorld;
                }
            }

            Matrix targetLocal = targetBoneWorld * parentWorld.Invert();

            Vec3 targetScale = Vec3::One;
            Quaternion targetRotation = Quaternion::Identity;
            Vec3 targetTranslation = Vec3::Zero;
            targetLocal.Decompose(targetScale, targetRotation, targetTranslation);
            targetRotation.Normalize();

            const float nodeRotationOutputAlpha = std::clamp(
                overallOutputAlpha * std::max(0.f, chain.desc.outputMultiplier) * std::clamp(_desc.rotationOutputAlpha, 0.f, 1.f),
                0.f,
                1.f);
            const Quaternion blendedRotation = Quaternion::Slerp(poseRotation, targetRotation, nodeRotationOutputAlpha);
            node.smoothedLocalRotation = Quaternion::Slerp(
                node.smoothedLocalRotation,
                blendedRotation,
                std::clamp(smoothAlpha * std::max(0.f, chain.desc.smoothMultiplier), 0.f, 1.f));
            node.smoothedLocalRotation.Normalize();

            const float translationAlpha = std::clamp(overallOutputAlpha * _desc.translationOutputAlpha, 0.f, 1.f);
            const float maxTranslationDelta = std::max(0.f, std::max(0.01f, node.length) * std::max(0.f, _desc.translationOutputLimitScale));
            const Vec3 limitedTranslation = Clamp_LocalTranslationFromPose(
                poseTranslation,
                targetTranslation,
                maxTranslationDelta);
            const Vec3 blendedTranslation = Vec3::Lerp(poseTranslation, limitedTranslation, translationAlpha);
            node.smoothedLocalTranslation = Vec3::Lerp(
                node.smoothedLocalTranslation,
                blendedTranslation,
                std::clamp(smoothAlpha * std::max(0.f, chain.desc.smoothMultiplier), 0.f, 1.f));

            model->Set_BoneLocalMatrix(
                node.boneIndex,
                Build_LocalMatrix_FromSRT(
                    poseScale,
                    node.smoothedLocalRotation,
                    node.smoothedLocalTranslation));
            model->Rebuild_BoneCombinedMatrices();
        }
    }
}

vector<SimonHairRuntime::WorldCapsule> SimonHairRuntime::Build_WorldCapsules(
    const ModelCom* collisionModel,
    const Matrix& collisionOwnerWorld) const
{
    vector<WorldCapsule> worldCapsules{};
    auto* self = const_cast<SimonHairRuntime*>(this);
    self->_runtimeDebugCapsules.clear();

    if (!_desc.enableCollision || collisionModel == nullptr)
    {
        return worldCapsules;
    }

    const float primitiveScale = std::max(0.0001f, _desc.collisionPrimitiveScale);
    const float radiusPadding = std::max(0.f, _desc.collisionRadiusPadding);

    for (const ChainModifier::CapsuleColliderDesc& collider : _colliders)
    {
        if (!_desc.includedCollisionBones.empty()
            && std::find(_desc.includedCollisionBones.begin(), _desc.includedCollisionBones.end(), collider.boneName) == _desc.includedCollisionBones.end())
        {
            continue;
        }

        const int32 boneIndex = collisionModel->Get_BoneIndex(collider.boneName);
        if (boneIndex < 0)
        {
            continue;
        }

        const Matrix* boneCombined = collisionModel->Get_BoneMatrixPtr(static_cast<uint32>(boneIndex));
        if (boneCombined == nullptr)
        {
            continue;
        }

        const Matrix boneWorld = (*boneCombined) * collisionOwnerWorld;
        Vec3 localAxis = Vec3::Transform(Vec3::UnitY, collider.rotationQuat);
        if (localAxis.LengthSquared() <= 0.0001f)
        {
            localAxis = Vec3::UnitY;
        }
        localAxis.Normalize();

        Vec3 worldScale = Vec3::One;
        Quaternion worldRotation = Quaternion::Identity;
        Vec3 worldTranslation = Vec3::Zero;
        Matrix decomposed = boneWorld;
        decomposed.Decompose(worldScale, worldRotation, worldTranslation);

        const Matrix boneRotationWorld = Matrix::CreateFromQuaternion(worldRotation);
        Vec3 worldAxis = Vec3::TransformNormal(localAxis, boneRotationWorld);
        if (worldAxis.LengthSquared() <= 0.0001f)
        {
            worldAxis = Vec3::UnitY;
        }
        worldAxis.Normalize();

        Vec3 scaledLocalAxis{
            localAxis.x * fabsf(worldScale.x),
            localAxis.y * fabsf(worldScale.y),
            localAxis.z * fabsf(worldScale.z)
        };
        float lengthScale = scaledLocalAxis.Length();
        if (lengthScale <= 0.0001f)
        {
            lengthScale = 1.f;
        }

        const Vec3 worldCenter = Vec3::Transform(collider.center, boneWorld);
        const float radiusScale = std::max(std::max(fabsf(worldScale.x), fabsf(worldScale.y)), fabsf(worldScale.z));

        WorldCapsule capsule{};
        capsule.radius = collider.radius * primitiveScale * radiusScale + radiusPadding;
        const float scaledHalfLength = collider.halfLength * primitiveScale * lengthScale;
        capsule.startPoint = worldCenter + worldAxis * scaledHalfLength;
        capsule.endPoint = worldCenter - worldAxis * scaledHalfLength;
        worldCapsules.push_back(capsule);

        RuntimeDebugCapsule debugCapsule{};
        debugCapsule.startPoint = capsule.startPoint;
        debugCapsule.endPoint = capsule.endPoint;
        debugCapsule.sourceRadius = collider.radius * primitiveScale * radiusScale;
        debugCapsule.collisionRadius = capsule.radius;
        self->_runtimeDebugCapsules.push_back(debugCapsule);

        if (_desc.debugDrawCollisionCapsules)
        {
            DebugCapsuleDesc drawCapsule{};
            drawCapsule.start = capsule.startPoint;
            drawCapsule.end = capsule.endPoint;
            drawCapsule.radius = capsule.radius;
            drawCapsule.style.color = Color(1.f, 0.85f, 0.05f, 1.f);
            drawCapsule.style.depthEnabled = false;
            GAME->Draw_DebugCapsule(drawCapsule);

            DebugSphereDesc drawCenter{};
            drawCenter.center = worldCenter;
            drawCenter.radius = std::max(0.4f, capsule.radius * 0.2f);
            drawCenter.style.color = Color(1.f, 0.1f, 0.1f, 1.f);
            drawCenter.style.depthEnabled = false;
            GAME->Draw_DebugSphere(drawCenter);
        }
    }

    return worldCapsules;
}

void SimonHairRuntime::Build_DebugSegmentsFromModel(ModelCom* model, const Matrix& ownerWorld)
{
    UNREFERENCED_PARAMETER(model);
    UNREFERENCED_PARAMETER(ownerWorld);

    _runtimeDebugSegments.clear();

    for (const Chain& chain : _chains)
    {
        Vec3 anchorPosition = Vec3::Zero;
        bool hasAnchorPosition = false;
        if (chain.anchorActor != nullptr)
        {
            anchorPosition = From_PxVec3(chain.anchorActor->getGlobalPose().p);
            hasAnchorPosition = true;
        }

        for (const Node& node : chain.nodes)
        {
            RuntimeDebugSegment segment{};
            if (node.parentNodeIndex >= 0)
            {
                segment.startPoint = chain.nodes[static_cast<size_t>(node.parentNodeIndex)].solvedPosition;
            }
            else if (hasAnchorPosition)
            {
                segment.startPoint = anchorPosition;
            }
            else
            {
                continue;
            }

            segment.endPoint = node.solvedPosition;
            segment.hasContact = node.hadContactThisFrame;
            segment.hasPush = node.hadPushThisFrame;
            segment.isSolvedSegment = true;
            _runtimeDebugSegments.push_back(segment);
        }
    }
}

Vec3 SimonHairRuntime::Get_Position(const Matrix& matrix)
{
    return Extract_Translation(matrix);
}

Vec3 SimonHairRuntime::Get_LocalDirection(const Matrix& parentWorld, const Vec3& worldDirection)
{
    const Matrix parentInv = parentWorld.Invert();
    Vec3 localDirection = Vec3::TransformNormal(worldDirection, parentInv);
    if (localDirection.LengthSquared() > 0.0001f)
    {
        localDirection.Normalize();
    }

    return localDirection;
}

Vec3 SimonHairRuntime::Limit_LocalDirectionByAngle(const Vec3& baseLocalDirection, const Vec3& targetLocalDirection, float maxAngleDegree)
{
    if (maxAngleDegree <= 0.f)
    {
        return targetLocalDirection;
    }

    Vec3 baseDirection = baseLocalDirection;
    Vec3 targetDirection = targetLocalDirection;
    if (baseDirection.LengthSquared() <= 0.0001f || targetDirection.LengthSquared() <= 0.0001f)
    {
        return baseLocalDirection;
    }

    baseDirection.Normalize();
    targetDirection.Normalize();

    const float maxAngle = XMConvertToRadians(maxAngleDegree);
    const float dot = std::clamp(baseDirection.Dot(targetDirection), -1.f, 1.f);
    const float angle = acosf(dot);
    if (angle <= maxAngle)
    {
        return targetDirection;
    }

    Vec3 axis = baseDirection.Cross(targetDirection);
    if (axis.LengthSquared() <= 0.0001f)
    {
        return baseDirection;
    }

    axis.Normalize();
    Vec3 limitedDirection = Vec3::Transform(baseDirection, Matrix::CreateFromQuaternion(Quaternion::CreateFromAxisAngle(axis, maxAngle)));
    if (limitedDirection.LengthSquared() > 0.0001f)
    {
        limitedDirection.Normalize();
    }

    return limitedDirection;
}

Quaternion SimonHairRuntime::Build_RotationDelta(const Vec3& fromDirection, const Vec3& toDirection)
{
    Vec3 from = fromDirection;
    Vec3 to = toDirection;
    if (from.LengthSquared() <= 0.0001f || to.LengthSquared() <= 0.0001f)
    {
        return Quaternion::Identity;
    }

    from.Normalize();
    to.Normalize();

    float dot = std::clamp(from.Dot(to), -1.f, 1.f);
    if (dot > 0.9995f)
    {
        return Quaternion::Identity;
    }

    if (dot < -0.9995f)
    {
        Vec3 axis = Vec3::UnitX.Cross(from);
        if (axis.LengthSquared() <= 0.0001f)
        {
            axis = Vec3::UnitY.Cross(from);
        }
        if (axis.LengthSquared() <= 0.0001f)
        {
            return Quaternion::Identity;
        }

        axis.Normalize();
        return Quaternion::CreateFromAxisAngle(axis, XM_PI);
    }

    Vec3 axis = from.Cross(to);
    if (axis.LengthSquared() <= 0.0001f)
    {
        return Quaternion::Identity;
    }

    axis.Normalize();
    Quaternion delta = Quaternion::CreateFromAxisAngle(axis, acosf(dot));
    delta.Normalize();
    return delta;
}

Matrix SimonHairRuntime::Build_LocalMatrix_FromSRT(const Vec3& scale, const Quaternion& rotation, const Vec3& translation)
{
    Quaternion normalizedRotation = rotation;
    normalizedRotation.Normalize();

    Matrix local = Matrix::CreateScale(scale) * Matrix::CreateFromQuaternion(normalizedRotation);
    local._41 = translation.x;
    local._42 = translation.y;
    local._43 = translation.z;
    local._44 = 1.f;
    return local;
}

Vec3 SimonHairRuntime::Clamp_LocalTranslationFromPose(
    const Vec3& poseTranslation,
    const Vec3& targetTranslation,
    float maxDelta)
{
    Vec3 delta = targetTranslation - poseTranslation;
    const float deltaLength = delta.Length();
    if (deltaLength <= 0.000001f)
    {
        return poseTranslation;
    }

    const float limitedDelta = std::max(0.f, maxDelta);
    if (limitedDelta <= 0.f)
    {
        return poseTranslation;
    }

    if (deltaLength > limitedDelta)
    {
        delta *= limitedDelta / deltaLength;
    }

    return poseTranslation + delta;
}

Vec3 SimonHairRuntime::Closest_PointOnSegment(const Vec3& point, const Vec3& start, const Vec3& end)
{
    const Vec3 segment = end - start;
    const float segmentLengthSq = segment.LengthSquared();
    if (segmentLengthSq <= 0.000001f)
    {
        return start;
    }

    const float t = std::clamp((point - start).Dot(segment) / segmentLengthSq, 0.f, 1.f);
    return start + segment * t;
}

NS_END
