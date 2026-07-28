#pragma once
#include "AnimationModifier.h"
#include "TransformCom.h"
#include "ChainModifier.h"

namespace physx {
	class PxPhysics;
	class PxScene;
	class PxMaterial;
	class PxRigidDynamic;
	class PxJoint;
}
using namespace physx;

NS_BEGIN(Engine)

class ModelCom;

class ENGINE_DLL PhysicsModifier final : public AnimationModifier
{
public:
	struct ChainDesc
	{
		string rootBoneName{};
		float outputMultiplier{ 1.f };
		float smoothMultiplier{ 1.f };
		float maxAngleMul{ 1.f };
		float radiusMul{ 1.f };
		float dampingMul{ 1.f };
		float restoreMul{ 1.f };
	};

	struct PhysicsModifierDesc : public AnimationModifierDesc
	{
		vector<ChainDesc> chains{};
		TransformCom* ownerTransform{ nullptr };
		const Matrix* ownerWorldMatrix{ nullptr };

		wstring physicsAssetPath{};
		vector<string> includedCollisionBones{};
		vector<ChainModifier::CapsuleColliderDesc> overrideCapsules{};
		const ModelCom* collisionModel{ nullptr };
		const Matrix* collisionOwnerWorldMatrix{ nullptr };

		float outputAlpha{ 1.f };
		float rotationOutputAlpha{ 0.25f };
		bool enableTranslationOutput{ true };
		float rootRotationOutputScale{ 1.f };
		float rotationOutputDepthStart{ 0.f };
		float rotationOutputDepthEnd{ 0.f };
		float outputSmoothAlpha{ 0.35f };
		float idleOutputSmoothAlpha{ 0.45f };
		float idleMotionDeadzone{ 0.06f };
		float maxOutputAngleDegree{ 150.f };

		float freeRotationFollowAlpha{ 0.46f };
		float rotationFollowDepthScale{ 0.10f };

		float poseRestoreAlpha{ 0.08f };
		float poseRestoreMaxSpeed{ 2.50f };
		Vec3 maxLocalOffset{ Vec3::Zero };
		bool enableJointLimit{ true };

		float movementInertiaScale{ 0.f };
		float tipMovementInertiaScale{ 1.f };
		float maxMovementInertiaSpeed{ 2.f };
		float lateralInertiaScale{ 1.f };
		float verticalInertiaScale{ 1.f };
		float depthInertiaScale{ 1.f };
		float ownerRotationInertiaScale{ 0.f };
		bool includeDriverLateralInertia{ false };
		bool useSteadyInertia{ true };
		bool advectSimulationWithAnchor{ false };
		float anchorAdvectionAlpha{ 1.f };
		float accelerationInertiaScale{ 0.35f };
		float inertiaVelocitySmoothAlpha{ 0.25f };

		float poseSpringStrength{ 0.f };
		float poseSpringDamping{ 0.f };

		float lengthConstraintAlpha{ 0.f };
		float bendConstraintAlpha{ 0.f };
		uint32 projectionIterations{ 1 };

		float releaseInertiaDamping{ 0.12f };

		bool enableCollision{ true };
		float collisionPrimitiveScale{ 0.30f };
		float collisionRadiusPadding{ 0.12f };
		float collisionContactSkinWidth{ 0.08f };
		float collisionSphereRadius{ 0.24f };
		uint32 collisionProjectionIterations{ 3 };
		bool useHardCollisionProjection{ false };
		uint32 hardCollisionProjectionIterations{ 2 };
		float hardCollisionLengthAlpha{ 1.f };

		Vec3 windDirection{ Vec3::Zero };
		float windScale{ 0.f };
		float windWaveScale{ 0.f };
		float windWaveFrequency{ 0.f };
		float windWaveDepthPhase{ 0.f };
		float gravityScale{ 1.f };
		bool useHierarchicalDepthRatio{ false };

		float teleportDistance{ 300.f };
		uint32 solverIterations{ 5 };

		bool writeBackEnabled{ true };
		bool debugDrawCollisionCapsules{ false };
		bool debugTraceBoneMotion{ false };

		mutable int32 debugActiveChainCount{ 0 };
		mutable int32 debugActiveNodeCount{ 0 };
		mutable int32 debugLoadedCapsuleCount{ 0 };
		mutable int32 debugActiveCapsuleCount{ 0 };
		mutable int32 debugCollisionHitCount{ 0 };
		mutable int32 debugConstraintProjectionCount{ 0 };
		mutable int32 debugLocalClampCount{ 0 };
		mutable int32 debugLocalClampXCount{ 0 };
		mutable int32 debugLocalClampYCount{ 0 };
		mutable int32 debugLocalClampZCount{ 0 };
		mutable float debugInertiaMagnitude{ 0.f };
		mutable int32 debugTraceFrameCount{ 0 };
		mutable float debugTraceMaxNodeMotion{ 0.f };
		mutable float debugTraceMaxOutputAngle{ 0.f };
	};

	struct RuntimeDebugCapsule
	{
		Vec3 startPoint{ Vec3::Zero };
		Vec3 endPoint{ Vec3::Zero };
		float sourceRadius{ 0.f };
		float collisionRadius{ 0.f };
	};

	struct RuntimeDebugSphere
	{
		Vec3 center{ Vec3::Zero };
		float radius{ 0.f };
		bool hasContact{ false };
	};

	struct RuntimeDebugSegment
	{
		Vec3 startPoint{ Vec3::Zero };
		Vec3 endPoint{ Vec3::Zero };
		bool hasContact{ false };
		bool hasPush{ false };
	};

	struct WorldCapsule
	{
		Vec3 startPoint{ Vec3::Zero };
		Vec3 endPoint{ Vec3::Zero };
		float radius{ 0.f };
	};

private:
	struct Node
	{
		uint32 boneIndex{ 0 };
		int32 parentNodeIndex{ -1 };
		vector<int32> childNodeIndices{};

		Matrix bindLocal{ Matrix::Identity };
		Matrix poseLocal{ Matrix::Identity };
		Matrix originalWorld{ Matrix::Identity };

		Vec3 bindLocalDir{ Vec3::Zero };
		Vec3 bindScale{ Vec3::One };
		Vec3 poseLocalDir{ Vec3::Zero };
		Vec3 smoothedLocalDirection{ Vec3::Zero };
		Vec3 smoothedLocalTranslation{ Vec3::Zero };
		Quat smoothedLocalRotation{ Quat::Identity };

		float length{ 0.f };
		float depthRatio{ 0.f };
		float debugSphereRadius{ 0.f };

		Vec3 solvedPosition{ Vec3::Zero };
		Vec3 prevSolvedPosition{ Vec3::Zero };
		Vec3 collisionOutputPosition{ Vec3::Zero };
		Vec3 prevPosePosition{ Vec3::Zero };
		Vec3 poseMotionDelta{ Vec3::Zero };
		Vec3 smoothedInertiaVelocity{ Vec3::Zero };

		bool hasPrevPosePosition{ false };
		bool hasSmoothedInertiaVelocity{ false };
		bool hasSmoothedLocalDirection{ false };
		bool hasSmoothedLocalTransform{ false };
		bool hasCollisionOutputPosition{ false };
		bool hadContactThisFrame{ false };
		bool hadPushThisFrame{ false };
		bool isKinematic{ false };

		physx::PxRigidDynamic* actor{ nullptr };
		PxJoint* joint{ nullptr };
	};

	struct Chain
	{
		ChainDesc desc{};
		int32 anchorBoneIndex{ -1 };
		PxRigidDynamic* anchorActor{ nullptr };
		Matrix anchorWorld{ Matrix::Identity };
		Matrix prevAnchorWorld{ Matrix::Identity };
		Vec3 prevDriverPosition{ Vec3::Zero };
		Vec3 driverMotionDelta{ Vec3::Zero };
		Vec3 prevDriverVelocity{ Vec3::Zero };
		Vec3 smoothedLateralAxis{ Vec3::UnitX };
		bool hasPrevAnchorWorld{ false };
		bool hasPrevDriverPosition{ false };
		bool hasPrevDriverVelocity{ false };
		bool hasSmoothedLateralAxis{ false };
		vector<Node> nodes{};
	};

	struct Blocker
	{
		PxRigidDynamic* actor{ nullptr };
	};

protected:
	HRESULT Initialize(AnimationModifierDesc* desc) override;
	void On_ActiveChanged(ModelCom* model, bool isActive) override;

public:
	void Evaluate_Modifier(ModelCom* model, float timeDelta) override;
	const ClassReflectionInfo* Get_RuntimeReflectionInfo() const override;
	void* Get_RuntimeReflectionTarget() override { return &_desc; }
	void On_RuntimeReflectionChanged(ModelCom* model, const PropertyInfo& property) override;
	bool Can_RuntimeResetPose() const override { return true; }
	void RuntimeResetPose(ModelCom* model) override;
	bool Can_RuntimeRebuild() const override { return true; }
	void RuntimeRebuild(ModelCom* model) override;
	void ResetPose(ModelCom* model);
	void Build_RuntimeDebugCapsules(vector<RuntimeDebugCapsule>& outCapsules) const { outCapsules = _runtimeDebugCapsules; }
	void Build_RuntimeDebugSpheres(vector<RuntimeDebugSphere>& outSpheres) const { outSpheres = _runtimeDebugSpheres; }
	void Build_RuntimeDebugSegments(vector<RuntimeDebugSegment>& outSegments) const { outSegments = _runtimeDebugSegments; }

	void Apply_WindForce(const Vec3& direction, float scale);

private:
	void Release_PhysXResources();
	void Initialize_Chains(ModelCom* model);
	void BuildChainRecursive(ModelCom* model, int32 boneIndex, int32 parentNodeIndex, Chain& outChain);
	void Capture_CurrentPose(ModelCom* model, const Matrix& ownerWorld);
	bool Ensure_PhysXResources(
		ModelCom* model,
		const ModelCom* collisionModel,
		const Matrix& ownerWorld,
		const Matrix& collisionOwnerWorld);
	void Reset_PhysXStateToPose(
		ModelCom* model,
		const ModelCom* collisionModel,
		const Matrix& ownerWorld,
		const Matrix& collisionOwnerWorld);
	void Sync_KinematicActors(
		ModelCom* model,
		const ModelCom* collisionModel,
		const Matrix& ownerWorld,
		const Matrix& collisionOwnerWorld);
	void Apply_AnchorFrameAdvection();
	void Update_SolvedPositionsFromActors();
	void Apply_OwnerInertiaForce(const Vec3& ownerMotionDelta, float timeDelta, bool isOwnerIdle);
	void Apply_PoseSpringForces(float timeDelta, bool isOwnerIdle);
	void Apply_LengthAndBendConstraints();
	void Apply_PostSimulationConstraints(const vector<WorldCapsule>& worldCapsules);
	void Apply_PoseRestoreAndDamping(float timeDelta, bool isOwnerIdle);
	void Apply_LocalOffsetConstraints(float timeDelta, bool isOwnerIdle);
	void Write_BackToModel(ModelCom* model, const Matrix& ownerWorld, bool isOwnerIdle);
	void Trace_BoneMotion(ModelCom* model, const Matrix& ownerWorld);
	vector<WorldCapsule> Build_WorldCapsules(const ModelCom* collisionModel, const Matrix& collisionOwnerWorld) const;
	void Build_DebugOutput(ModelCom* model, const Matrix& ownerWorld);
	bool Validate_RootAlignment(ModelCom* model, const Matrix& ownerWorld);
	void Restore_PoseLocals(ModelCom* model);

	static Vec3 Get_Position(const Matrix& matrix);
	static Vec3 Get_LocalDirection(const Matrix& parentWorld, const Vec3& worldDirection);
	static Vec3 Limit_LocalDirectionByAngle(const Vec3& baseLocalDirection, const Vec3& targetLocalDirection, float maxAngleDegree);
	static Quat Build_RotationDelta(const Vec3& fromDirection, const Vec3& toDirection);
	static Matrix Build_LocalMatrix_FromSRT(const Vec3& scale, const Quat& rotation, const Vec3& translation);
	static Vec3 Closest_PointOnSegment(const Vec3& point, const Vec3& start, const Vec3& end);
	float Compute_NodeAngleLimitDegree(const Chain& chain, float depthRatio) const;

private:
	PhysicsModifierDesc _desc{};
	vector<Chain> _chains{};
	vector<ChainModifier::CapsuleColliderDesc> _colliders{};
	vector<Blocker> _blockers{};
	vector<RuntimeDebugCapsule> _runtimeDebugCapsules{};
	vector<RuntimeDebugSphere> _runtimeDebugSpheres{};
	vector<RuntimeDebugSegment> _runtimeDebugSegments{};
	bool _chainsInitialized{ false };
	bool _physxResourcesReady{ false };
	Vec3 _prevOwnerPos{ Vec3::Zero };
	bool _hasPrevOwnerWorld{ false };
	bool _hasCapturedPose{ false };
	float _windTime{ 0.f };

	PxPhysics* _physics{ nullptr };
	PxScene* _scene{ nullptr };
	PxMaterial* _material{ nullptr };

public:
	static Shared<PhysicsModifier> Create(const PhysicsModifierDesc* desc);
	void Free() override;
};

NS_END
