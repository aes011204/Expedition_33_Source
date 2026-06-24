#pragma once

#include "ChainModifier.h"
#include "Reflection_Types.h"

NS_BEGIN(Engine)
class ModelCom;
NS_END

NS_BEGIN(Client)

class SimonHairRuntime final
{
public:
    struct ChainDesc
    {
        string rootBoneName{};
        float outputMultiplier{ 1.f };
        float smoothMultiplier{ 1.f };
    };

    struct Desc
    {
        wstring runtimeTag{ L"SimonHairRuntime" };
        vector<ChainDesc> chains{};

        wstring physicsAssetPath{};
        bool useRawBodyLocalCollisionAsset{ false };
        vector<string> includedCollisionBones{};

        float freeFollowAlpha{ 0.08f };
        float contactFollowAlpha{ 0.65f };
        float releaseFollowAlpha{ 0.04f };
        float releaseMemoryTime{ 0.18f };
        bool useAnimatedPoseTarget{ true };

        float animTranslationInertiaScale{ 0.85f };
        float animRotationInertiaScale{ 1.15f };
        float ownerTranslationInertiaScale{ 0.45f };
        float ownerRotationInertiaScale{ 0.85f };
        float inertiaMaxDistance{ 14.f };
        float inertiaDamping{ 0.82f };
        float contactInertiaDamping{ 0.18f };
        float releaseInertiaDamping{ 0.12f };
        float directSwingTranslationScale{ 1.f };
        float directSwingRotationScale{ 1.f };
        float directSwingDamping{ 0.82f };
        float directSwingMaxDistance{ 10.f };
        float directSwingWaveScale{ 1.f };
        float directSwingWaveFrequency{ 6.f };

        float outputAlpha{ 1.f };
        float rotationOutputAlpha{ 0.25f };
        float translationOutputAlpha{ 0.f };
        float translationOutputLimitScale{ 0.10f };
        float outputSmoothAlpha{ 0.35f };
        float idleOutputSmoothAlpha{ 0.45f };
        float idleMotionDeadzone{ 0.06f };
        float maxOutputAngleDegree{ 150.f };

        bool enableCollision{ true };
        bool debugDrawCollisionCapsules{ false };
        float collisionPrimitiveScale{ 0.30f };
        float collisionRadiusPadding{ 0.12f };
        float collisionContactSkinWidth{ 0.08f };
        float collisionSphereRadius{ 0.24f };
        float collisionPushOutScale{ 1.f };
        float contactSnapAlpha{ 0.90f };
        uint32 collisionProjectionIterations{ 3 };

        float teleportDistance{ 300.f };
        uint32 solverIterations{ 5 };
    };

public:
    struct RuntimeDebugCapsule
    {
        Vec3 startPoint{ Vec3::Zero };
        Vec3 endPoint{ Vec3::Zero };
        float sourceRadius{ 0.f };
        float collisionRadius{ 0.f };
    };

    struct RuntimeDebugSegment
    {
        Vec3 startPoint{ Vec3::Zero };
        Vec3 endPoint{ Vec3::Zero };
        bool hasContact{ false };
        bool hasPush{ false };
        bool isSolvedSegment{ false };
    };

    struct WorldCapsule
    {
        Vec3 startPoint{ Vec3::Zero };
        Vec3 endPoint{ Vec3::Zero };
        float radius{ 0.f };
    };

public:
    SimonHairRuntime() = default;
    SimonHairRuntime(const SimonHairRuntime& other);
    SimonHairRuntime& operator=(const SimonHairRuntime& other);
    ~SimonHairRuntime();

    void Initialize(Engine::ModelCom* model, const Desc& desc);
    void ResetPose(Engine::ModelCom* model, const Matrix& ownerWorld, const Matrix& motionDriverWorld);
    void Simulate(
        Engine::ModelCom* model,
        const Engine::ModelCom* collisionModel,
        float timeDelta,
        const Matrix& ownerWorld,
        const Matrix& collisionOwnerWorld,
        const Matrix& motionDriverWorld);
    const Engine::ClassReflectionInfo* Get_RuntimeReflectionInfo() const;
    void* Get_RuntimeReflectionTarget() { return &_desc; }
    void On_RuntimeReflectionChanged(
        Engine::ModelCom* model,
        const Engine::ModelCom* collisionModel,
        const Matrix& ownerWorld,
        const Matrix& collisionOwnerWorld,
        const Matrix& motionDriverWorld);
    void RuntimeRebuild(
        Engine::ModelCom* model,
        const Engine::ModelCom* collisionModel,
        const Matrix& ownerWorld,
        const Matrix& collisionOwnerWorld,
        const Matrix& motionDriverWorld);

    bool Is_Built() const { return _built; }
    void Build_RuntimeDebugCapsules(vector<RuntimeDebugCapsule>& outCapsules) const { outCapsules = _runtimeDebugCapsules; }
    void Build_RuntimeDebugSegments(vector<RuntimeDebugSegment>& outSegments) const { outSegments = _runtimeDebugSegments; }

private:
    struct Node
    {
        uint32 boneIndex{ 0 };
        int32 parentNodeIndex{ -1 };
        vector<int32> childNodeIndices{};

        Matrix bindLocal{ Matrix::Identity };
        Matrix poseLocal{ Matrix::Identity };
        Matrix originalWorld{ Matrix::Identity };
        Matrix prevOriginalWorld{ Matrix::Identity };
        Matrix actorToBoneWorld{ Matrix::Identity };

        Vec3 bindLocalDir{ Vec3::Zero };
        Vec3 bindScale{ Vec3::One };
        Vec3 poseLocalDir{ Vec3::Zero };
        Vec3 smoothedLocalDir{ Vec3::Zero };
        Vec3 smoothedLocalTranslation{ Vec3::Zero };
        Quaternion smoothedLocalRotation{ Quaternion::Identity };

        float length{ 0.f };
        float depthRatio{ 0.f };

        Vec3 solvedPosition{ Vec3::Zero };
        Vec3 prevSolvedPosition{ Vec3::Zero };

        bool hasSmoothedLocalDir{ false };
        bool hasSmoothedLocalTransform{ false };
        bool hasPrevOriginalWorld{ false };
        bool hadContactThisFrame{ false };
        bool hadPushThisFrame{ false };

        PxRigidDynamic* actor{ nullptr };
        PxD6Joint* joint{ nullptr };
    };

    struct Chain
    {
        ChainDesc desc{};
        int32 anchorBoneIndex{ -1 };
        PxRigidDynamic* anchorActor{ nullptr };
        vector<Node> nodes{};
    };

    struct Blocker
    {
        string boneName{};
        PxRigidDynamic* actor{ nullptr };
        float radius{ 0.f };
        float halfHeight{ 0.f };
        bool isSphere{ false };
    };

private:
    void Release_PhysXResources();
    void Initialize_Chains(Engine::ModelCom* model);
    void BuildChainRecursive(Engine::ModelCom* model, int32 boneIndex, int32 parentNodeIndex, Chain& outChain);
    void Capture_CurrentPose(Engine::ModelCom* model, const Matrix& ownerWorld);
    bool Ensure_PhysXResources(
        Engine::ModelCom* model,
        const Engine::ModelCom* collisionModel,
        const Matrix& ownerWorld,
        const Matrix& collisionOwnerWorld);
    void Reset_PhysXStateToPose(
        Engine::ModelCom* model,
        const Engine::ModelCom* collisionModel,
        const Matrix& ownerWorld,
        const Matrix& collisionOwnerWorld);
    void Sync_KinematicActors(
        Engine::ModelCom* model,
        const Engine::ModelCom* collisionModel,
        const Matrix& ownerWorld,
        const Matrix& collisionOwnerWorld,
        const Matrix& motionDriverWorld);
    void Update_SolvedPositionsFromActors();
    void Apply_PostSimulationConstraints(
        Engine::ModelCom* model,
        const Matrix& ownerWorld,
        const vector<WorldCapsule>& worldCapsules,
        float timeDelta);
    void Write_BackToModel(Engine::ModelCom* model, const Matrix& ownerWorld, bool isOwnerIdle);
    vector<WorldCapsule> Build_WorldCapsules(const Engine::ModelCom* collisionModel, const Matrix& collisionOwnerWorld) const;
    void Build_DebugSegmentsFromModel(Engine::ModelCom* model, const Matrix& ownerWorld);

    static Vec3 Get_Position(const Matrix& matrix);
    static Vec3 Get_LocalDirection(const Matrix& parentWorld, const Vec3& worldDirection);
    static Vec3 Limit_LocalDirectionByAngle(const Vec3& baseLocalDirection, const Vec3& targetLocalDirection, float maxAngleDegree);
    static Quaternion Build_RotationDelta(const Vec3& fromDirection, const Vec3& toDirection);
    static Matrix Build_LocalMatrix_FromSRT(const Vec3& scale, const Quaternion& rotation, const Vec3& translation);
    static Vec3 Clamp_LocalTranslationFromPose(const Vec3& poseTranslation, const Vec3& targetTranslation, float maxDelta);
    static Vec3 Closest_PointOnSegment(const Vec3& point, const Vec3& start, const Vec3& end);

private:
    Desc _desc{};
    vector<Chain> _chains{};
    vector<Engine::ChainModifier::CapsuleColliderDesc> _colliders{};
    vector<RuntimeDebugCapsule> _runtimeDebugCapsules{};
    vector<RuntimeDebugSegment> _runtimeDebugSegments{};
    vector<Blocker> _blockers{};

    PxPhysics* _physics{ nullptr };
    PxScene* _scene{ nullptr };
    PxMaterial* _material{ nullptr };

    Matrix _prevOwnerWorld{ Matrix::Identity };
    Vec3 _prevOwnerPosition{ Vec3::Zero };
    bool _hasPrevOwnerWorld{ false };
    bool _built{ false };
    bool _physxResourcesReady{ false };
};

NS_END
