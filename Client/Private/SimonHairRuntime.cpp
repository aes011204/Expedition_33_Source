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

    PxQuat Build_JointFrameQuat(const Vec3& tangentWorld, const Vec3& referenceUpWorld)
    {
        Vec3 tangent = tangentWorld;
        if (tangent.LengthSquared() <= 0.000001f)
        {
            tangent = Vec3::UnitX;
        }
        else
        {
            tangent.Normalize();
        }

        const Quaternion baseRotation = From_PxQuat(Build_AxisAlignmentQuat(To_PxVec3(tangent)));
        Vec3 currentUp = Vec3::TransformNormal(Vec3::UnitY, Matrix::CreateFromQuaternion(baseRotation));
        if (currentUp.LengthSquared() <= 0.000001f)
        {
            currentUp = Vec3::UnitY;
        }
        currentUp.Normalize();

        Vec3 desiredUp = referenceUpWorld - tangent * referenceUpWorld.Dot(tangent);
        if (desiredUp.LengthSquared() <= 0.000001f)
        {
            desiredUp = Vec3::UnitZ - tangent * Vec3::UnitZ.Dot(tangent);
            if (desiredUp.LengthSquared() <= 0.000001f)
            {
                desiredUp = Vec3::UnitY - tangent * Vec3::UnitY.Dot(tangent);
            }
        }
        if (desiredUp.LengthSquared() <= 0.000001f)
        {
            return To_PxQuat(baseRotation);
        }
        desiredUp.Normalize();

        const float dot = std::clamp(currentUp.Dot(desiredUp), -1.f, 1.f);
        float angle = acosf(dot);
        if (angle <= 0.0001f)
        {
            return To_PxQuat(baseRotation);
        }

        const float sign = (currentUp.Cross(desiredUp)).Dot(tangent) < 0.f ? -1.f : 1.f;
        const Quaternion twist = Quaternion::CreateFromAxisAngle(tangent, angle * sign);
        Quaternion finalRotation = twist * baseRotation;
        finalRotation.Normalize();
        return To_PxQuat(finalRotation);
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

    float SegmentSegmentClosestPoints(
        const Vec3& p1,
        const Vec3& q1,
        const Vec3& p2,
        const Vec3& q2,
        Vec3& outC1,
        Vec3& outC2,
        float& outT1,
        float& outT2)
    {
        const Vec3 d1 = q1 - p1;
        const Vec3 d2 = q2 - p2;
        const Vec3 r = p1 - p2;
        const float a = d1.Dot(d1);
        const float e = d2.Dot(d2);
        const float f = d2.Dot(r);
        float s = 0.f;
        float t = 0.f;

        if (a <= 0.000001f && e <= 0.000001f)
        {
            outC1 = p1;
            outC2 = p2;
            outT1 = 0.f;
            outT2 = 0.f;
            return (outC1 - outC2).LengthSquared();
        }

        if (a <= 0.000001f)
        {
            t = std::clamp(f / e, 0.f, 1.f);
        }
        else
        {
            const float c = d1.Dot(r);
            if (e <= 0.000001f)
            {
                s = std::clamp(-c / a, 0.f, 1.f);
            }
            else
            {
                const float b = d1.Dot(d2);
                const float denom = a * e - b * b;
                if (fabsf(denom) > 0.000001f)
                {
                    s = std::clamp((b * f - c * e) / denom, 0.f, 1.f);
                }

                t = (b * s + f) / e;
                if (t < 0.f)
                {
                    t = 0.f;
                    s = std::clamp(-c / a, 0.f, 1.f);
                }
                else if (t > 1.f)
                {
                    t = 1.f;
                    s = std::clamp((b - c) / a, 0.f, 1.f);
                }
            }
        }

        outC1 = p1 + d1 * s;
        outC2 = p2 + d2 * t;
        outT1 = s;
        outT2 = t;
        return (outC1 - outC2).LengthSquared();
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
    _runtimeDebugSpheres.clear();
    _runtimeDebugSegments.clear();
    _blockers.clear();
    _prevOwnerWorld = Matrix::Identity;
    _prevOwnerPosition = Vec3::Zero;
    _hasPrevOwnerWorld = false;
    _preparedThisFrame = false;
    _built = false;
    _physxResourcesReady = false;
    _rootAlignmentWarningLogged = false;
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
    _runtimeDebugSpheres.clear();
    _runtimeDebugSegments.clear();
    _blockers.clear();
    _prevOwnerWorld = Matrix::Identity;
    _prevOwnerPosition = Vec3::Zero;
    _hasPrevOwnerWorld = false;
    _preparedThisFrame = false;
    _built = false;
    _physxResourcesReady = false;
    _rootAlignmentWarningLogged = false;

    if (model == nullptr || !model->Is_Skeletal())
    {
        return;
    }

    if (!_desc.physicsAssetPath.empty())
    {
        _colliders = ModelPhysicsParser::LoadCapsulesFromJSON(_desc.physicsAssetPath);
    }

    Initialize_Chains(model);
    _built = !_chains.empty();
}

void SimonHairRuntime::ResetPose(
    ModelCom* model,
    const ModelCom* collisionModel,
    const Matrix& ownerWorld,
    const Matrix& collisionOwnerWorld,
    const Matrix& motionDriverWorld)
{
    if (!_built || model == nullptr)
    {
        return;
    }

    Capture_CurrentPose(model, ownerWorld);
    Ensure_PhysXResources(model, collisionModel, ownerWorld, collisionOwnerWorld);

    if (_physxResourcesReady)
    {
        Reset_PhysXStateToPose(model, collisionModel, ownerWorld, collisionOwnerWorld);
        Update_SolvedPositionsFromActors();
        Apply_PostSimulationConstraints(model, ownerWorld, Build_WorldCapsules(collisionModel, collisionOwnerWorld), 0.f);
        Write_BackToModel(model, ownerWorld, false);
        Build_DebugOutput(model, ownerWorld);
    }

    _prevOwnerWorld = motionDriverWorld;
    _prevOwnerPosition = Get_Position(motionDriverWorld);
    _hasPrevOwnerWorld = true;
    _preparedThisFrame = false;
}

void SimonHairRuntime::Prepare_Simulation(
    ModelCom* model,
    const ModelCom* collisionModel,
    float timeDelta,
    const Matrix& ownerWorld,
    const Matrix& collisionOwnerWorld,
    const Matrix& motionDriverWorld)
{
    if (!_built || model == nullptr || timeDelta <= 0.f)
    {
        _preparedThisFrame = false;
        return;
    }

    Capture_CurrentPose(model, ownerWorld);

    if (!Ensure_PhysXResources(model, collisionModel, ownerWorld, collisionOwnerWorld))
    {
        _preparedThisFrame = false;
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

    if (!Validate_RootAlignment(model, ownerWorld))
    {
        Reset_PhysXStateToPose(model, collisionModel, ownerWorld, collisionOwnerWorld);
    }

    Sync_KinematicActors(model, collisionModel, ownerWorld, collisionOwnerWorld, motionDriverWorld);
    Build_WorldCapsules(collisionModel, collisionOwnerWorld);
    _preparedThisFrame = true;
}

void SimonHairRuntime::Finalize_Simulation(
    ModelCom* model,
    const ModelCom* collisionModel,
    float timeDelta,
    const Matrix& ownerWorld,
    const Matrix& collisionOwnerWorld,
    const Matrix& motionDriverWorld)
{
    if (!_preparedThisFrame || !_built || model == nullptr)
    {
        return;
    }

    Update_SolvedPositionsFromActors();
    const vector<WorldCapsule> worldCapsules = Build_WorldCapsules(collisionModel, collisionOwnerWorld);
    Apply_PostSimulationConstraints(model, ownerWorld, worldCapsules, timeDelta);

    const Vec3 ownerPosition = Get_Position(motionDriverWorld);
    const bool isOwnerIdle = !_hasPrevOwnerWorld
        || Vec3::Distance(ownerPosition, _prevOwnerPosition) <= std::max(0.f, _desc.idleMotionDeadzone);
    Write_BackToModel(model, ownerWorld, isOwnerIdle);
    Build_DebugOutput(model, ownerWorld);

    _prevOwnerWorld = motionDriverWorld;
    _prevOwnerPosition = ownerPosition;
    _hasPrevOwnerWorld = true;
    _preparedThisFrame = false;
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

            AddRuntimeProperty(result.properties, Engine::PropertyType::Bool, "enableCollision", "Collision", SimonHairOffset(&Desc::enableCollision));
            AddRuntimeProperty(result.properties, Engine::PropertyType::Bool, "debugDrawCollisionCapsules", "Debug Col", SimonHairOffset(&Desc::debugDrawCollisionCapsules));

            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "contactInertiaDamping", "Hit Damp", SimonHairOffset(&Desc::contactInertiaDamping), 0.01f, 0.f, 1.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "releaseInertiaDamping", "Release Damp", SimonHairOffset(&Desc::releaseInertiaDamping), 0.01f, 0.f, 1.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "outputAlpha", "Out", SimonHairOffset(&Desc::outputAlpha), 0.01f, 0.f, 1.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "rotationOutputAlpha", "Rot Out", SimonHairOffset(&Desc::rotationOutputAlpha), 0.01f, 0.f, 1.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "outputSmoothAlpha", "Smooth", SimonHairOffset(&Desc::outputSmoothAlpha), 0.01f, 0.f, 1.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "idleOutputSmoothAlpha", "Idle Smooth", SimonHairOffset(&Desc::idleOutputSmoothAlpha), 0.01f, 0.f, 1.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "idleMotionDeadzone", "Idle Dead", SimonHairOffset(&Desc::idleMotionDeadzone), 0.001f, 0.f, 10.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "maxOutputAngleDegree", "Max Angle", SimonHairOffset(&Desc::maxOutputAngleDegree), 0.25f, 0.f, 180.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "contactRotationFollowAlpha", "Hit Follow", SimonHairOffset(&Desc::contactRotationFollowAlpha), 0.01f, 0.f, 1.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "freeRotationFollowAlpha", "Move Follow", SimonHairOffset(&Desc::freeRotationFollowAlpha), 0.01f, 0.f, 1.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "rotationFollowDepthScale", "Follow Depth", SimonHairOffset(&Desc::rotationFollowDepthScale), 0.01f, 0.f, 1.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "animatedPosePullAlpha", "Pose Pull", SimonHairOffset(&Desc::animatedPosePullAlpha), 0.01f, 0.f, 1.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "animatedPosePullMaxSpeed", "Pose Speed", SimonHairOffset(&Desc::animatedPosePullMaxSpeed), 0.05f, 0.f, 20.f);

            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "collisionPrimitiveScale", "Col Scale", SimonHairOffset(&Desc::collisionPrimitiveScale), 0.005f, 0.001f, 10.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "collisionRadiusPadding", "Col Pad", SimonHairOffset(&Desc::collisionRadiusPadding), 0.01f, 0.f, 10.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "collisionContactSkinWidth", "Col Skin", SimonHairOffset(&Desc::collisionContactSkinWidth), 0.005f, 0.f, 1.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "collisionSphereRadius", "Node Radius", SimonHairOffset(&Desc::collisionSphereRadius), 0.005f, 0.01f, 10.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "collisionPushOutScale", "Col Push", SimonHairOffset(&Desc::collisionPushOutScale), 0.01f, 0.f, 5.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "contactSnapAlpha", "Col Snap", SimonHairOffset(&Desc::contactSnapAlpha), 0.01f, 0.f, 1.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "collisionCorrectionSmoothAlpha", "Corr Lag", SimonHairOffset(&Desc::collisionCorrectionSmoothAlpha), 0.01f, 0.f, 1.f);
            AddRuntimeProperty(result.properties, Engine::PropertyType::Float, "collisionNormalSmoothAlpha", "Norm Lag", SimonHairOffset(&Desc::collisionNormalSmoothAlpha), 0.01f, 0.f, 1.f);
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
    ResetPose(model, collisionModel, ownerWorld, collisionOwnerWorld, motionDriverWorld);
    if (_physxResourcesReady)
    {
        Update_SolvedPositionsFromActors();
        Write_BackToModel(model, ownerWorld, true);
        Build_DebugOutput(model, ownerWorld);
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
            node.poseLocal = model->Get_BoneLocalMatrix(node.boneIndex);

            const Matrix* combined = model->Get_BoneMatrixPtr(node.boneIndex);
            if (combined == nullptr)
            {
                continue;
            }

            node.originalWorld = (*combined) * ownerWorld;
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

            node.isKinematic = node.parentNodeIndex < 0;

            PxShape* shape = nullptr;
            if (!node.isKinematic)
            {
                const float chainRadiusMul = std::max(0.10f, chain.desc.radiusMul);
                const float nodeRadius = std::max(
                    0.035f,
                    std::min(baseNodeRadius * chainRadiusMul, std::max(0.045f, node.length * 0.34f * chainRadiusMul)));
                node.debugSphereRadius = nodeRadius;
                shape = _physics->createShape(PxSphereGeometry(nodeRadius), *_material, true);
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

                node.actor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, false);
                node.actor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
                const float depthRatio = std::clamp(node.depthRatio, 0.f, 1.f);
                const float dampingMul = std::max(0.10f, chain.desc.dampingMul);
                node.actor->setLinearDamping((8.50f - depthRatio * 3.20f) * dampingMul);
                node.actor->setAngularDamping((9.50f - depthRatio * 3.60f) * dampingMul);
                node.actor->setSolverIterationCounts(static_cast<PxU32>(std::max(12u, _desc.solverIterations * 3u)), 4);
                node.actor->setSleepThreshold(0.0001f);
                node.actor->setMaxLinearVelocity(7.5f);
                node.actor->setMaxAngularVelocity(5.5f);
                PxRigidBodyExt::updateMassAndInertia(*node.actor, 0.08f + node.depthRatio * 0.03f);
            }
            else
            {
                node.debugSphereRadius = 0.f;
                node.actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
                node.actor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, true);
            }

            _scene->addActor(*node.actor);

            if (node.isKinematic)
            {
                continue;
            }

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

            Vec3 referenceUp = Vec3::TransformNormal(Vec3::UnitY, Matrix::CreateFromQuaternion(Extract_Rotation(node.originalWorld)));
            if (node.parentNodeIndex >= 0)
            {
                referenceUp = Vec3::TransformNormal(
                    Vec3::UnitY,
                    Matrix::CreateFromQuaternion(Extract_Rotation(chain.nodes[static_cast<size_t>(node.parentNodeIndex)].originalWorld)));
            }
            else
            {
                referenceUp = Vec3::TransformNormal(Vec3::UnitY, Matrix::CreateFromQuaternion(Extract_Rotation(anchorWorld)));
            }
            if (referenceUp.LengthSquared() <= 0.000001f)
            {
                referenceUp = Vec3::UnitY;
            }
            referenceUp.Normalize();

            const PxVec3 worldAnchor = parentPosition;
            const PxTransform jointWorldFrame(
                worldAnchor,
                Build_JointFrameQuat(From_PxVec3(jointAxis), referenceUp));
            const PxTransform parentLocalFrame = parentPose.transformInv(jointWorldFrame);
            const PxTransform childLocalFrame = childPose.transformInv(jointWorldFrame);

            PxSphericalJoint* sphericalJoint = PxSphericalJointCreate(
                *_physics,
                jointParentActor,
                parentLocalFrame,
                node.actor,
                childLocalFrame);
            if (sphericalJoint == nullptr)
            {
                Release_PhysXResources();
                return false;
            }
            node.joint = sphericalJoint;

            const float depthRatio = std::clamp(node.depthRatio, 0.f, 1.f);
            const float swingLimitDegree = std::clamp(
                std::min(_desc.maxOutputAngleDegree * std::max(0.10f, chain.desc.maxAngleMul), 90.f) * (0.32f + depthRatio * 0.48f),
                3.5f,
                75.f);
            const float swingLimit = XMConvertToRadians(swingLimitDegree);
            PxJointLimitCone coneLimit(swingLimit * 0.5f, swingLimit);
            coneLimit.restitution = 0.f;
            coneLimit.bounceThreshold = 0.f;
            coneLimit.stiffness = 0.f;
            coneLimit.damping = 0.f;
            sphericalJoint->setLimitCone(coneLimit);
            sphericalJoint->setSphericalJointFlag(PxSphericalJointFlag::eLIMIT_ENABLED, true);
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

            const float depthRatio = std::clamp(node.depthRatio, 0.f, 1.f);

            const Vec3 posePosition = Get_Position(node.originalWorld);
            const Quaternion poseRotation = Extract_Rotation(node.originalWorld);
            const PxTransform pose(To_PxVec3(posePosition), To_PxQuat(poseRotation));
            node.actor->setGlobalPose(pose);
            if (node.isKinematic)
            {
                node.actor->setKinematicTarget(pose);
            }
            else
            {
                node.actor->setLinearVelocity(PxVec3(0.f, 0.f, 0.f));
                node.actor->setAngularVelocity(PxVec3(0.f, 0.f, 0.f));
                node.actor->clearForce();
                node.actor->clearTorque();
            }

            node.solvedPosition = posePosition;
            node.prevSolvedPosition = posePosition;
            node.smoothedCollisionOffset = Vec3::Zero;
            node.smoothedContactNormal = Vec3::Zero;
            node.hasSmoothedCollisionOffset = false;
            node.hasSmoothedContactNormal = false;
            node.hadContactThisFrame = false;
            node.hadPushThisFrame = false;
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
                if (node.isKinematic)
                {
                    const PxTransform pose(To_PxVec3(Get_Position(node.originalWorld)), To_PxQuat(Extract_Rotation(node.originalWorld)));
                    node.actor->setKinematicTarget(pose);
                }
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

    if (!_physxResourcesReady)
    {
        return;
    }

    const float fallbackNodeRadius = std::max(0.02f, _desc.collisionSphereRadius);
    const float skinWidth = std::max(0.f, _desc.collisionContactSkinWidth);
    const float contactVelocityScale = std::clamp(1.f - std::clamp(_desc.contactInertiaDamping, 0.f, 1.f) * 0.45f, 0.55f, 0.92f);
    const float freeRestoreScale = std::clamp(_desc.releaseInertiaDamping, 0.f, 1.f);
    const float posePullAlpha = std::clamp(_desc.animatedPosePullAlpha, 0.f, 1.f);
    const float posePullMaxSpeed = std::max(0.f, _desc.animatedPosePullMaxSpeed);
    const float pushOutScale = std::max(0.f, _desc.collisionPushOutScale);
    const float snapAlpha = std::clamp(_desc.contactSnapAlpha, 0.f, 1.f);
    const uint32 projectionIterations = std::max(1u, std::min(_desc.collisionProjectionIterations, 4u));

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
            if (node.isKinematic)
            {
                node.hadContactThisFrame = false;
                node.hadPushThisFrame = false;
                continue;
            }

            Vec3 currentPosition = node.solvedPosition;
            if (node.actor != nullptr)
            {
                currentPosition = From_PxVec3(node.actor->getGlobalPose().p);
            }

            bool hadCollision = false;
            Vec3 resolvedPosition = currentPosition;
            Vec3 contactNormal = Vec3::Zero;
            uint32 contactCount = 0;
            float maxPenetration = 0.f;
            Vec3 parentPosition = hasAnchorPosition ? anchorPosition : currentPosition;
            if (node.parentNodeIndex >= 0)
            {
                const Node& parentNode = chain.nodes[static_cast<size_t>(node.parentNodeIndex)];
                parentPosition = parentNode.actor != nullptr
                    ? From_PxVec3(parentNode.actor->getGlobalPose().p)
                    : parentNode.solvedPosition;
            }
            const float nodeRadius = std::max(0.02f, node.debugSphereRadius > 0.f ? node.debugSphereRadius : fallbackNodeRadius);

            for (uint32 iteration = 0; iteration < projectionIterations; ++iteration)
            {
                bool hadIterationCollision = false;
                Vec3 iterationNormalSum = Vec3::Zero;
                uint32 iterationContactCount = 0;

                for (const WorldCapsule& capsule : worldCapsules)
                {
                    const Vec3 closestPoint = Closest_PointOnSegment(resolvedPosition, capsule.startPoint, capsule.endPoint);
                    Vec3 pushDirection = resolvedPosition - closestPoint;
                    float distance = pushDirection.Length();
                    const float combinedRadius = nodeRadius + capsule.radius + skinWidth;
                    if (distance >= combinedRadius)
                    {
                        continue;
                    }

                    if (distance <= 0.0001f)
                    {
                        pushDirection = resolvedPosition - parentPosition;
                        if (pushDirection.LengthSquared() <= 0.0001f)
                        {
                            pushDirection = Get_Position(node.originalWorld) - closestPoint;
                        }
                        if (pushDirection.LengthSquared() <= 0.0001f)
                        {
                            pushDirection = Vec3::UnitY;
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

                    const float depenetration = penetration * (0.82f + pushOutScale * 0.12f) + 0.0005f;
                    resolvedPosition += pushDirection * depenetration;
                    hadCollision = true;
                    hadIterationCollision = true;
                    iterationNormalSum += pushDirection;
                    ++iterationContactCount;
                    maxPenetration = std::max(maxPenetration, penetration);
                }

                if (!hadIterationCollision)
                {
                    break;
                }

                if (iterationContactCount > 0)
                {
                    iterationNormalSum /= static_cast<float>(iterationContactCount);
                    if (iterationNormalSum.LengthSquared() > 0.0001f)
                    {
                        iterationNormalSum.Normalize();
                        contactNormal += iterationNormalSum;
                        ++contactCount;
                    }
                }
            }

            for (uint32 iteration = 0; iteration < projectionIterations; ++iteration)
            {
                bool hadSegmentCollision = false;
                for (const WorldCapsule& capsule : worldCapsules)
                {
                    Vec3 hairClosest = Vec3::Zero;
                    Vec3 capsuleClosest = Vec3::Zero;
                    float hairT = 0.f;
                    float capsuleT = 0.f;
                    const float distanceSq = SegmentSegmentClosestPoints(
                        parentPosition,
                        resolvedPosition,
                        capsule.startPoint,
                        capsule.endPoint,
                        hairClosest,
                        capsuleClosest,
                        hairT,
                        capsuleT);
                    const float combinedRadius = nodeRadius + capsule.radius + skinWidth;
                    if (distanceSq >= combinedRadius * combinedRadius)
                    {
                        continue;
                    }

                    Vec3 pushDirection = hairClosest - capsuleClosest;
                    float distance = sqrtf(std::max(0.f, distanceSq));
                    if (distance <= 0.0001f)
                    {
                        pushDirection = resolvedPosition - parentPosition;
                        if (pushDirection.LengthSquared() <= 0.0001f)
                        {
                            pushDirection = resolvedPosition - capsuleClosest;
                        }
                        if (pushDirection.LengthSquared() <= 0.0001f)
                        {
                            pushDirection = Vec3::UnitY;
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

                    const float endpointWeight = 1.f / std::max(hairT, 0.42f);
                    const float maxCorrection = std::max(0.001f, node.length * (0.14f + node.depthRatio * 0.12f));
                    Vec3 correction = pushDirection * std::min(
                        penetration * endpointWeight * (0.68f + pushOutScale * 0.18f) + 0.0005f,
                        maxCorrection);
                    resolvedPosition += correction;
                    hadCollision = true;
                    hadSegmentCollision = true;
                    contactNormal += pushDirection;
                    ++contactCount;
                    maxPenetration = std::max(maxPenetration, penetration);
                }

                if (!hadSegmentCollision)
                {
                    break;
                }
            }

            if (timeDelta > 0.f)
            {
                for (const WorldCapsule& capsule : worldCapsules)
                {
                    Vec3 sweepClosest = Vec3::Zero;
                    Vec3 capsuleClosest = Vec3::Zero;
                    float sweepT = 0.f;
                    float capsuleT = 0.f;
                    const float distanceSq = SegmentSegmentClosestPoints(
                        node.prevSolvedPosition,
                        resolvedPosition,
                        capsule.startPoint,
                        capsule.endPoint,
                        sweepClosest,
                        capsuleClosest,
                        sweepT,
                        capsuleT);
                    const float combinedRadius = nodeRadius + capsule.radius + skinWidth;
                    if (distanceSq >= combinedRadius * combinedRadius)
                    {
                        continue;
                    }

                    Vec3 pushDirection = sweepClosest - capsuleClosest;
                    const float distance = sqrtf(std::max(0.f, distanceSq));
                    if (distance > 0.0001f)
                    {
                        pushDirection /= distance;
                    }
                    else
                    {
                        pushDirection = resolvedPosition - capsuleClosest;
                        if (pushDirection.LengthSquared() <= 0.0001f)
                        {
                            pushDirection = resolvedPosition - parentPosition;
                        }
                        if (pushDirection.LengthSquared() <= 0.0001f)
                        {
                            pushDirection = Vec3::UnitY;
                        }
                        pushDirection.Normalize();
                    }

                    const Vec3 capsuleSurface = capsuleClosest + pushDirection * combinedRadius;
                    resolvedPosition = Vec3::Lerp(resolvedPosition, capsuleSurface, std::clamp(0.22f + sweepT * 0.34f, 0.f, 0.68f));
                    hadCollision = true;
                    contactNormal += pushDirection;
                    ++contactCount;
                    maxPenetration = std::max(maxPenetration, combinedRadius - distance);
                }
            }

            const float depthRatio = std::clamp(node.depthRatio, 0.f, 1.f);
            const float rootStabilization = 1.f - depthRatio;

            if (node.length > 0.0001f)
            {
                Vec3 segment = resolvedPosition - parentPosition;
                float currentLength = segment.Length();
                if (currentLength <= 0.0001f)
                {
                    segment = Get_Position(node.originalWorld) - parentPosition;
                    currentLength = segment.Length();
                }

                if (currentLength > 0.0001f)
                {
                    segment /= currentLength;

                    const float minLength = node.length * (0.90f - rootStabilization * 0.03f);
                    const float maxLength = node.length * (hadCollision
                        ? (1.02f + depthRatio * 0.04f)
                        : (1.05f + depthRatio * 0.06f));
                    const float clampedLength = std::clamp(currentLength, minLength, maxLength);
                    const float recoveryAlpha = hadCollision
                        ? (0.58f + rootStabilization * 0.12f)
                        : (0.42f + rootStabilization * 0.18f);
                    resolvedPosition = parentPosition + segment * std::lerp(currentLength, clampedLength, std::clamp(recoveryAlpha, 0.f, 1.f));
                }
            }

            if (hadCollision && snapAlpha > 0.f)
            {
                const float appliedSnapAlpha = std::clamp(0.42f + snapAlpha * 0.32f, 0.f, 0.82f);
                resolvedPosition = Vec3::Lerp(currentPosition, resolvedPosition, appliedSnapAlpha);
            }

            if (hadCollision)
            {
                Vec3 correctionOffset = resolvedPosition - currentPosition;
                if (node.hasSmoothedCollisionOffset)
                {
                    const float correctionAlpha = std::clamp(
                        _desc.collisionCorrectionSmoothAlpha + depthRatio * 0.12f,
                        0.05f,
                        1.f);
                    node.smoothedCollisionOffset = Vec3::Lerp(
                        node.smoothedCollisionOffset,
                        correctionOffset,
                        correctionAlpha);

                    const float targetLength = correctionOffset.Length();
                    const float smoothedLength = node.smoothedCollisionOffset.Length();
                    const float maxSmoothedLength = targetLength * 1.20f + 0.001f;
                    if (smoothedLength > maxSmoothedLength && smoothedLength > 0.0001f)
                    {
                        node.smoothedCollisionOffset *= maxSmoothedLength / smoothedLength;
                    }
                }
                else
                {
                    node.smoothedCollisionOffset = correctionOffset;
                    node.hasSmoothedCollisionOffset = true;
                }

                const float penetrationAlpha = std::clamp(
                    maxPenetration / std::max(0.001f, nodeRadius + skinWidth),
                    0.f,
                    1.f);
                const float hardCorrectionBlend = 0.18f + penetrationAlpha * 0.34f;
                resolvedPosition = Vec3::Lerp(
                    currentPosition + node.smoothedCollisionOffset,
                    resolvedPosition,
                    hardCorrectionBlend);
            }
            else if (node.hasSmoothedCollisionOffset)
            {
                node.smoothedCollisionOffset *= 0.55f;
                if (node.smoothedCollisionOffset.LengthSquared() <= 0.000001f)
                {
                    node.smoothedCollisionOffset = Vec3::Zero;
                    node.hasSmoothedCollisionOffset = false;
                }
            }

            if (hadCollision)
            {
                for (const WorldCapsule& capsule : worldCapsules)
                {
                    const float combinedRadius = nodeRadius + capsule.radius + skinWidth;

                    const Vec3 closestPoint = Closest_PointOnSegment(resolvedPosition, capsule.startPoint, capsule.endPoint);
                    Vec3 pushDirection = resolvedPosition - closestPoint;
                    float distance = pushDirection.Length();
                    if (distance < combinedRadius)
                    {
                        if (distance <= 0.0001f)
                        {
                            pushDirection = resolvedPosition - parentPosition;
                            if (pushDirection.LengthSquared() <= 0.0001f)
                            {
                                pushDirection = Vec3::UnitY;
                            }
                            pushDirection.Normalize();
                            distance = 0.f;
                        }
                        else
                        {
                            pushDirection /= distance;
                        }

                        resolvedPosition += pushDirection * (combinedRadius - distance + 0.0005f);
                        contactNormal += pushDirection;
                        ++contactCount;
                    }

                    Vec3 hairClosest = Vec3::Zero;
                    Vec3 capsuleClosest = Vec3::Zero;
                    float hairT = 0.f;
                    float capsuleT = 0.f;
                    const float distanceSq = SegmentSegmentClosestPoints(
                        parentPosition,
                        resolvedPosition,
                        capsule.startPoint,
                        capsule.endPoint,
                        hairClosest,
                        capsuleClosest,
                        hairT,
                        capsuleT);
                    if (distanceSq < combinedRadius * combinedRadius)
                    {
                        Vec3 segmentPushDirection = hairClosest - capsuleClosest;
                        float segmentDistance = sqrtf(std::max(0.f, distanceSq));
                        if (segmentDistance <= 0.0001f)
                        {
                            segmentPushDirection = resolvedPosition - parentPosition;
                            if (segmentPushDirection.LengthSquared() <= 0.0001f)
                            {
                                segmentPushDirection = Vec3::UnitY;
                            }
                            segmentPushDirection.Normalize();
                            segmentDistance = 0.f;
                        }
                        else
                        {
                            segmentPushDirection /= segmentDistance;
                        }

                        const float childWeight = std::clamp(hairT, 0.25f, 1.f);
                        resolvedPosition += segmentPushDirection * ((combinedRadius - segmentDistance + 0.0005f) / childWeight);
                        contactNormal += segmentPushDirection;
                        ++contactCount;
                    }
                }
            }

            if (node.length > 0.0001f)
            {
                Vec3 boneSegment = resolvedPosition - parentPosition;
                if (boneSegment.LengthSquared() <= 0.0001f)
                {
                    boneSegment = Get_Position(node.originalWorld) - parentPosition;
                }
                if (boneSegment.LengthSquared() > 0.0001f)
                {
                    boneSegment.Normalize();
                    resolvedPosition = parentPosition + boneSegment * node.length;
                }

                if (hadCollision)
                {
                    for (uint32 iteration = 0; iteration < 2; ++iteration)
                    {
                        bool adjustedBoneLengthSolution = false;
                        for (const WorldCapsule& capsule : worldCapsules)
                        {
                            const float combinedRadius = nodeRadius + capsule.radius + skinWidth;

                            Vec3 hairClosest = Vec3::Zero;
                            Vec3 capsuleClosest = Vec3::Zero;
                            float hairT = 0.f;
                            float capsuleT = 0.f;
                            const float distanceSq = SegmentSegmentClosestPoints(
                                parentPosition,
                                resolvedPosition,
                                capsule.startPoint,
                                capsule.endPoint,
                                hairClosest,
                                capsuleClosest,
                                hairT,
                                capsuleT);
                            if (distanceSq >= combinedRadius * combinedRadius)
                            {
                                continue;
                            }

                            Vec3 pushDirection = hairClosest - capsuleClosest;
                            float distance = sqrtf(std::max(0.f, distanceSq));
                            if (distance <= 0.0001f)
                            {
                                pushDirection = resolvedPosition - parentPosition;
                                if (pushDirection.LengthSquared() <= 0.0001f)
                                {
                                    pushDirection = Vec3::UnitY;
                                }
                                pushDirection.Normalize();
                                distance = 0.f;
                            }
                            else
                            {
                                pushDirection /= distance;
                            }

                            Vec3 adjustedSegment = (resolvedPosition - parentPosition)
                                + pushDirection * ((combinedRadius - distance + 0.0005f) / std::clamp(hairT, 0.25f, 1.f));
                            if (adjustedSegment.LengthSquared() > 0.0001f)
                            {
                                adjustedSegment.Normalize();
                                resolvedPosition = parentPosition + adjustedSegment * node.length;
                                contactNormal += pushDirection;
                                ++contactCount;
                                adjustedBoneLengthSolution = true;
                            }
                        }

                        if (!adjustedBoneLengthSolution)
                        {
                            break;
                        }
                    }
                }
            }

            node.solvedPosition = resolvedPosition;

            if (node.actor != nullptr && hadCollision)
            {
                PxTransform correctedPose = node.actor->getGlobalPose();
                correctedPose.p = To_PxVec3(resolvedPosition);
                node.actor->setGlobalPose(correctedPose);

                Vec3 linearVelocity = From_PxVec3(node.actor->getLinearVelocity());
                if (contactCount > 0)
                {
                    contactNormal /= static_cast<float>(contactCount);
                    if (contactNormal.LengthSquared() > 0.0001f)
                    {
                        contactNormal.Normalize();
                        if (node.hasSmoothedContactNormal && node.smoothedContactNormal.Dot(contactNormal) > -0.15f)
                        {
                            contactNormal = Vec3::Lerp(
                                node.smoothedContactNormal,
                                contactNormal,
                                std::clamp(_desc.collisionNormalSmoothAlpha, 0.f, 1.f));
                            if (contactNormal.LengthSquared() > 0.0001f)
                            {
                                contactNormal.Normalize();
                            }
                        }
                        node.smoothedContactNormal = contactNormal;
                        node.hasSmoothedContactNormal = true;

                        const float normalSpeed = linearVelocity.Dot(contactNormal);
                        Vec3 tangentVelocity = linearVelocity - contactNormal * normalSpeed;
                        Vec3 contactSegment = resolvedPosition - parentPosition;
                        Vec3 laydownDirection = contactSegment - contactNormal * contactSegment.Dot(contactNormal);
                        if (laydownDirection.LengthSquared() > 0.0001f)
                        {
                            laydownDirection.Normalize();

                            const float tangentSpeed = tangentVelocity.Length();
                            if (tangentSpeed > 0.0001f)
                            {
                                Vec3 tangentDir = tangentVelocity / tangentSpeed;
                                tangentVelocity = Vec3::Lerp(
                                    tangentDir,
                                    laydownDirection,
                                    0.18f + rootStabilization * 0.18f) * tangentSpeed;
                            }
                            else
                            {
                                tangentVelocity = laydownDirection * std::min(0.85f, node.length * 8.f);
                            }
                        }

                        if (normalSpeed < 0.f)
                        {
                            linearVelocity = tangentVelocity;
                        }
                        else
                        {
                            linearVelocity = tangentVelocity + contactNormal * (normalSpeed * (0.08f + depthRatio * 0.12f));
                        }
                    }
                }

                linearVelocity *= contactVelocityScale;
                if (posePullAlpha > 0.f && posePullMaxSpeed > 0.f && timeDelta > 0.f)
                {
                    Vec3 poseDelta = Get_Position(node.originalWorld) - resolvedPosition;
                    if (contactCount > 0 && contactNormal.LengthSquared() > 0.0001f)
                    {
                        poseDelta -= contactNormal * poseDelta.Dot(contactNormal);
                    }

                    if (poseDelta.LengthSquared() > 0.000001f)
                    {
                        Vec3 poseDirection = poseDelta;
                        poseDirection.Normalize();
                        const Vec3 poseVelocity = poseDirection * std::min(poseDelta.Length() / timeDelta, posePullMaxSpeed);
                        linearVelocity = Vec3::Lerp(
                            linearVelocity,
                            linearVelocity + poseVelocity,
                            std::clamp(posePullAlpha * 0.35f, 0.f, 0.20f));
                    }
                }
                node.actor->setLinearVelocity(To_PxVec3(linearVelocity));
                const float angularContactScale = 0.42f + depthRatio * 0.16f;
                node.actor->setAngularVelocity(node.actor->getAngularVelocity() * angularContactScale);
                node.actor->wakeUp();
            }
            else if (node.actor != nullptr && timeDelta > 0.f)
            {
                if (node.hasSmoothedContactNormal)
                {
                    node.smoothedContactNormal *= 0.70f;
                    if (node.smoothedContactNormal.LengthSquared() <= 0.0001f)
                    {
                        node.smoothedContactNormal = Vec3::Zero;
                        node.hasSmoothedContactNormal = false;
                    }
                }

                const Vec3 restTarget = Get_Position(node.originalWorld);
                const Vec3 restDelta = restTarget - currentPosition;
                Vec3 linearVelocity = From_PxVec3(node.actor->getLinearVelocity());

                const float positionRestoreAlpha =
                    (0.015f + rootStabilization * 0.030f)
                    * (0.20f + freeRestoreScale * 0.45f)
                    * std::max(0.10f, chain.desc.restoreMul);
                if (restDelta.LengthSquared() > 0.000001f)
                {
                    const float maxRestoreSpeed = std::max(
                        std::max(0.10f, node.length * (2.0f + rootStabilization * 1.5f)),
                        posePullMaxSpeed);
                    Vec3 desiredDirection = restDelta;
                    desiredDirection.Normalize();
                    const Vec3 desiredVelocity = desiredDirection * std::min(restDelta.Length() / timeDelta, maxRestoreSpeed);
                    const float restoreAlpha = std::clamp(positionRestoreAlpha + posePullAlpha * 0.18f, 0.f, 0.22f);
                    linearVelocity = Vec3::Lerp(
                        linearVelocity,
                        desiredVelocity,
                        restoreAlpha);
                }

                const float freeVelocityScale =
                    0.88f - rootStabilization * 0.14f + depthRatio * 0.03f;
                linearVelocity *= std::clamp(freeVelocityScale, 0.70f, 0.94f);
                node.actor->setLinearVelocity(To_PxVec3(linearVelocity));

                Vec3 angularVelocity = From_PxVec3(node.actor->getAngularVelocity());
                const float angularFreeScale =
                    0.78f - rootStabilization * 0.16f + depthRatio * 0.05f;
                angularVelocity *= std::clamp(angularFreeScale, 0.58f, 0.90f);
                node.actor->setAngularVelocity(To_PxVec3(angularVelocity));
                node.actor->wakeUp();
            }

            node.hadContactThisFrame = node.hadContactThisFrame || hadCollision;
            node.hadPushThisFrame = node.hadPushThisFrame || hadCollision;
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
            if (node.actor == nullptr || node.bindLocalDir.LengthSquared() <= 0.0001f)
            {
                continue;
            }

            Vec3 poseScale = node.bindScale;
            Quaternion poseRotation = Quaternion::Identity;
            Vec3 poseTranslation = Vec3::Zero;
            Matrix poseLocal = node.poseLocal;
            poseLocal.Decompose(poseScale, poseRotation, poseTranslation);
            poseRotation.Normalize();

            Matrix parentWorld = ownerWorld;
            Vec3 parentPosition = Vec3::Zero;
            bool hasParentPosition = false;
            if (node.parentNodeIndex >= 0)
            {
                const Node& parentNode = chain.nodes[static_cast<size_t>(node.parentNodeIndex)];
                parentPosition = parentNode.actor != nullptr
                    ? From_PxVec3(parentNode.actor->getGlobalPose().p)
                    : parentNode.solvedPosition;
                hasParentPosition = true;

                if (const Matrix* parentCombined = model->Get_BoneMatrixPtr(parentNode.boneIndex))
                {
                    parentWorld = (*parentCombined) * ownerWorld;
                }
            }
            else if (chain.anchorBoneIndex >= 0)
            {
                if (const Matrix* anchorCombined = model->Get_BoneMatrixPtr(static_cast<uint32>(chain.anchorBoneIndex)))
                {
                    parentWorld = (*anchorCombined) * ownerWorld;
                    parentPosition = chain.anchorActor != nullptr
                        ? From_PxVec3(chain.anchorActor->getGlobalPose().p)
                        : Get_Position(parentWorld);
                    hasParentPosition = true;
                }
            }

            const Vec3 nodeWorldPosition = From_PxVec3(node.actor->getGlobalPose().p);
            Vec3 segmentWorld = hasParentPosition
                ? (nodeWorldPosition - parentPosition)
                : (nodeWorldPosition - Get_Position(node.originalWorld));
            if (segmentWorld.LengthSquared() <= 0.0001f)
            {
                continue;
            }
            segmentWorld.Normalize();

            const Vec3 restLocalDir =
                node.poseLocalDir.LengthSquared() > 0.0001f
                ? node.poseLocalDir
                : node.bindLocalDir;
            if (restLocalDir.LengthSquared() <= 0.0001f)
            {
                continue;
            }

            Vec3 targetLocalDir = Get_LocalDirection(parentWorld, segmentWorld);
            const float nodeAngleLimit = std::clamp(
                _desc.maxOutputAngleDegree
                * std::max(0.10f, chain.desc.maxAngleMul)
                * (0.45f + std::clamp(node.depthRatio, 0.f, 1.f) * 0.55f),
                0.f,
                180.f);
            targetLocalDir = Limit_LocalDirectionByAngle(restLocalDir, targetLocalDir, nodeAngleLimit);

            Quaternion solvedRotation = Build_RotationDelta(restLocalDir, targetLocalDir) * poseRotation;
            solvedRotation.Normalize();

            const float nodeRotationOutputAlpha = std::clamp(
                overallOutputAlpha * std::max(0.f, chain.desc.outputMultiplier) * std::clamp(_desc.rotationOutputAlpha, 0.f, 1.f),
                0.f,
                1.f);
            const Quaternion blendedRotation = Quaternion::Slerp(poseRotation, solvedRotation, nodeRotationOutputAlpha);
            const float depthRatio = std::clamp(node.depthRatio, 0.f, 1.f);
            const float minimumFollowAlpha = node.hadContactThisFrame
                ? (_desc.contactRotationFollowAlpha - depthRatio * _desc.rotationFollowDepthScale)
                : (_desc.freeRotationFollowAlpha - depthRatio * (_desc.rotationFollowDepthScale + 0.02f));
            const float rotationFollowAlpha = std::max(
                std::clamp(minimumFollowAlpha, 0.f, 1.f),
                std::clamp(smoothAlpha * std::max(0.f, chain.desc.smoothMultiplier), 0.f, 1.f));
            node.smoothedLocalRotation = Quaternion::Slerp(
                node.smoothedLocalRotation,
                blendedRotation,
                rotationFollowAlpha);
            node.smoothedLocalRotation.Normalize();

            node.smoothedLocalTranslation = poseTranslation;

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

        const float nodeRadius = std::max(0.02f, _desc.collisionSphereRadius);
        const float skinWidth = std::max(0.f, _desc.collisionContactSkinWidth);
        const float effectiveCollisionRadius = capsule.radius + nodeRadius + skinWidth;

        RuntimeDebugCapsule debugCapsule{};
        debugCapsule.startPoint = capsule.startPoint;
        debugCapsule.endPoint = capsule.endPoint;
        debugCapsule.sourceRadius = collider.radius * primitiveScale * radiusScale;
        debugCapsule.collisionRadius = effectiveCollisionRadius;
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

            DebugCapsuleDesc drawEffectiveCapsule{};
            drawEffectiveCapsule.start = capsule.startPoint;
            drawEffectiveCapsule.end = capsule.endPoint;
            drawEffectiveCapsule.radius = effectiveCollisionRadius;
            drawEffectiveCapsule.style.color = Color(1.f, 0.35f, 0.05f, 1.f);
            drawEffectiveCapsule.style.depthEnabled = false;
            GAME->Draw_DebugCapsule(drawEffectiveCapsule);

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

bool SimonHairRuntime::Validate_RootAlignment(ModelCom* model, const Matrix& ownerWorld)
{
    if (model == nullptr || _chains.empty() || _chains.front().nodes.empty())
    {
        return true;
    }

    const Chain& chain = _chains.front();
    const Node& rootNode = chain.nodes.front();
    const Matrix* rootCombined = model->Get_BoneMatrixPtr(rootNode.boneIndex);
    if (rootCombined == nullptr)
    {
        return true;
    }

    const Vec3 ownerPosition = Get_Position(ownerWorld);
    const Vec3 rootPosition = Get_Position((*rootCombined) * ownerWorld);
    Vec3 anchorPosition = ownerPosition;
    if (chain.anchorBoneIndex >= 0)
    {
        if (const Matrix* anchorCombined = model->Get_BoneMatrixPtr(static_cast<uint32>(chain.anchorBoneIndex)))
        {
            anchorPosition = Get_Position((*anchorCombined) * ownerWorld);
        }
    }

    const float ownerToRoot = Vec3::Distance(ownerPosition, rootPosition);
    const float anchorToRoot = Vec3::Distance(anchorPosition, rootPosition);
    constexpr float kSevereRootMismatch = 150.f;
    if (ownerToRoot <= kSevereRootMismatch && anchorToRoot <= kSevereRootMismatch)
    {
        return true;
    }

    if (!_rootAlignmentWarningLogged)
    {
        LOG_WARN(
            "SimonHairRuntime root alignment mismatch. ownerToRoot={}, anchorToRoot={}, runtime={}",
            ownerToRoot,
            anchorToRoot,
            String::ToString(_desc.runtimeTag));
        _rootAlignmentWarningLogged = true;
    }
    return false;
}

void SimonHairRuntime::Build_DebugOutput(ModelCom* model, const Matrix& ownerWorld)
{
    _runtimeDebugSegments.clear();
    _runtimeDebugSpheres.clear();

    if (!_desc.debugDrawCollisionCapsules)
    {
        _runtimeDebugCapsules.clear();
        return;
    }

    if (!_chains.empty() && !_chains.front().nodes.empty())
    {
        const Chain& firstChain = _chains.front();
        const Node& rootNode = firstChain.nodes.front();
        const Vec3 ownerPosition = Get_Position(ownerWorld);

        DebugSphereDesc ownerSphere{};
        ownerSphere.center = ownerPosition;
        ownerSphere.radius = 1.2f;
        ownerSphere.style.color = Color(0.1f, 0.6f, 1.f, 1.f);
        ownerSphere.style.depthEnabled = false;
        GAME->Draw_DebugSphere(ownerSphere);

        if (firstChain.anchorActor != nullptr)
        {
            DebugSphereDesc anchorSphere{};
            anchorSphere.center = From_PxVec3(firstChain.anchorActor->getGlobalPose().p);
            anchorSphere.radius = 1.6f;
            anchorSphere.style.color = Color(0.1f, 1.f, 0.25f, 1.f);
            anchorSphere.style.depthEnabled = false;
            GAME->Draw_DebugSphere(anchorSphere);
        }

        if (model != nullptr)
        {
            if (const Matrix* rootCombined = model->Get_BoneMatrixPtr(rootNode.boneIndex))
            {
                DebugSphereDesc rootSphere{};
                rootSphere.center = Get_Position((*rootCombined) * ownerWorld);
                rootSphere.radius = 1.4f;
                rootSphere.style.color = Color(1.f, 0.2f, 0.9f, 1.f);
                rootSphere.style.depthEnabled = false;
                GAME->Draw_DebugSphere(rootSphere);
            }
        }
    }

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
            Vec3 actorPosition = node.solvedPosition;
            if (node.actor != nullptr)
            {
                actorPosition = From_PxVec3(node.actor->getGlobalPose().p);
            }

            if (node.debugSphereRadius > 0.f)
            {
                RuntimeDebugSphere sphere{};
                sphere.center = actorPosition;
                sphere.radius = node.debugSphereRadius;
                sphere.hasContact = node.hadContactThisFrame;
                _runtimeDebugSpheres.push_back(sphere);
            }

            RuntimeDebugSegment actorSegment{};
            if (node.parentNodeIndex >= 0)
            {
                const Node& parentNode = chain.nodes[static_cast<size_t>(node.parentNodeIndex)];
                actorSegment.startPoint = parentNode.actor != nullptr
                    ? From_PxVec3(parentNode.actor->getGlobalPose().p)
                    : parentNode.solvedPosition;
            }
            else if (hasAnchorPosition)
            {
                actorSegment.startPoint = anchorPosition;
            }
            else
            {
                continue;
            }

            actorSegment.endPoint = actorPosition;
            actorSegment.hasContact = node.hadContactThisFrame;
            actorSegment.hasPush = node.hadPushThisFrame;
            actorSegment.type = RuntimeDebugSegmentType::ActorChain;
            _runtimeDebugSegments.push_back(actorSegment);
        }
    }

    if (model == nullptr)
    {
        return;
    }

    for (const Chain& chain : _chains)
    {
        Vec3 anchorPosition = Vec3::Zero;
        bool hasAnchorPosition = false;
        if (chain.anchorBoneIndex >= 0)
        {
            if (const Matrix* anchorCombined = model->Get_BoneMatrixPtr(static_cast<uint32>(chain.anchorBoneIndex)))
            {
                anchorPosition = Get_Position((*anchorCombined) * ownerWorld);
                hasAnchorPosition = true;
            }
        }

        for (const Node& node : chain.nodes)
        {
            RuntimeDebugSegment boneSegment{};
            if (node.parentNodeIndex >= 0)
            {
                if (const Matrix* parentCombined = model->Get_BoneMatrixPtr(chain.nodes[static_cast<size_t>(node.parentNodeIndex)].boneIndex))
                {
                    boneSegment.startPoint = Get_Position((*parentCombined) * ownerWorld);
                }
                else
                {
                    continue;
                }
            }
            else if (hasAnchorPosition)
            {
                boneSegment.startPoint = anchorPosition;
            }
            else
            {
                continue;
            }

            const Matrix* boneCombined = model->Get_BoneMatrixPtr(node.boneIndex);
            if (boneCombined == nullptr)
            {
                continue;
            }

            boneSegment.endPoint = Get_Position((*boneCombined) * ownerWorld);
            boneSegment.hasContact = node.hadContactThisFrame;
            boneSegment.hasPush = node.hadPushThisFrame;
            boneSegment.type = RuntimeDebugSegmentType::BoneChain;
            _runtimeDebugSegments.push_back(boneSegment);
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
