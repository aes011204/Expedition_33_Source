#pragma once
#include "AnimationModifier.h"
#include "TransformCom.h"
#include "ChainModifier.h"
#include <unordered_set>

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
		float outputSmoothAlpha{ 0.18f };
		float idleOutputSmoothAlpha{ 0.35f };
		float tipOutputSmoothScale{ 1.f };
		float idleMotionDeadzone{ 0.001f };
		float teleportDistance{ 300.f };
		float poseFollowAlpha{ 0.25f };
		float inertiaDamping{ 0.82f };
		float contactInertiaDamping{ 0.35f };
		float maxInertialStepScale{ 0.25f };
		uint32 solverIterations{ 2 };
		float targetFrameRate{ 60.f };
		bool enableCollision{ false };
		float collisionPrimitiveScale{ 1.f };
		float collisionRadiusPadding{ 0.f };
		float collisionPushOutScale{ 0.9f };
		float collisionPushOutMaxScale{ 0.35f };
		float collisionContactSkinWidth{ 0.03f };
		float contactOutputAlpha{ 0.85f };
		float contactOutputSmoothAlpha{ 0.45f };
		float contactOutputLimitDegree{ 120.f };
		float contactPoseFollowScale{ 0.15f };
		float contactSolveLimitDegree{ 75.f };
		bool segmentFrameOutput{ false };
		float rotationOutputAlpha{ 1.f };
		float translationOutputAlpha{ 0.f };
		float translationOutputLimitScale{ 0.10f };
		float contactCorrectionSmoothAlpha{ 0.55f };
		float contactCorrectionReleaseAlpha{ 0.35f };
		float childCollisionPushDamping{ 0.35f };
		float tipCollisionPushDamping{ 0.65f };
		float contactBendPropagationAlpha{ 0.f };
		float contactBendPropagationDecay{ 0.75f };
		float finalCollisionProjectionScale{ 1.f };
		float finalCollisionProjectionMaxScale{ 0.08f };
		int32 finalCollisionProjectionIterations{ 1 };
		float collisionDirectionBlockAlpha{ 1.f };
		mutable int32 debugActiveChainCount{ 0 };
		mutable int32 debugActiveNodeCount{ 0 };
		mutable int32 debugLoadedCapsuleCount{ 0 };
		mutable int32 debugActiveCapsuleCount{ 0 };
		mutable int32 debugCollisionSource{ 0 };
		mutable int32 debugCollisionHitCount{ 0 };
		mutable int32 debugSegmentCollisionHitCount{ 0 };
		mutable float debugMinCollisionMargin{ 0.f };
		mutable int32 debugCollisionPushOutCount{ 0 };
		mutable float debugMaxCollisionPushOut{ 0.f };
		mutable int32 debugSegmentCount{ 0 };
		bool writeBackEnabled{ true };
		float hairCollisionClearance{ 0.f };
		bool debugDrawChains{ false };
		bool debugDrawNodes{ false };
		bool debugDrawSegments{ false };
		bool debugDrawCollisions{ false };
	};

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

private:
	struct Node
	{
		uint32 boneIndex{ 0 };
		int32 parentNodeIndex{ -1 };
		vector<int32> childNodeIndices{};
		Matrix bindLocal{ Matrix::Identity };
		Matrix poseLocal{ Matrix::Identity };
		Matrix originalWorld{ Matrix::Identity };
		Vec3 poseLocalDir{ Vec3::Zero };
		Vec3 smoothedLocalDir{ Vec3::Zero };
		Vec3 smoothedLocalTranslation{ Vec3::Zero };
		Quat smoothedLocalRotation{ Quat::Identity };
		Vec3 position{ Vec3::Zero };
		Vec3 solvedPosition{ Vec3::Zero };
		Vec3 prevSolvedPosition{ Vec3::Zero };
		Vec3 contactCorrection{ Vec3::Zero };
		Vec3 smoothedContactCorrection{ Vec3::Zero };
		Vec3 smoothedCollisionNormal{ Vec3::Zero };
		float length{ 0.f };
		float depthRatio{ 0.f };
		float collisionCorrectionDistance{ 0.f };
		bool hasSmoothedLocalDir{ false };
		bool hasSmoothedLocalTransform{ false };
		bool hasSolvedState{ false };
		bool hasCollisionContact{ false };
		bool hasCollisionPushApplied{ false };
		bool hasSmoothedCollisionNormal{ false };
	};

	struct Chain
	{
		ChainDesc desc{};
		vector<Node> nodes{};
		int32 anchorBoneIndex{ -1 };
		Matrix anchorWorld{ Matrix::Identity };
	};

	struct WorldCapsule
	{
		Vec3 startPoint{ Vec3::Zero };
		Vec3 endPoint{ Vec3::Zero };
		float radius{ 0.f };
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
	void RuntimeResetPose(ModelCom* model) override { ResetPose(model); }
	bool Can_RuntimeRebuild() const override { return true; }
	void RuntimeRebuild(ModelCom* model) override;
	void Set_RuntimeTestOwnerMotion(bool enabled, const Vec3& delta);
	void ResetPose(ModelCom* model);
	void Build_RuntimeDebugCapsules(ModelCom* model, vector<RuntimeDebugCapsule>& outCapsules) const;
	void Build_RuntimeDebugSegments(vector<RuntimeDebugSegment>& outSegments) const;

private:
	void Initialize_Chains(ModelCom* model);
	void BuildChainRecursive(ModelCom* model, int32 boneIndex, int32 parentNodeIndex, Chain& chain);
	void Restore_PoseLocals(ModelCom* model);
	void Capture_CurrentPose(ModelCom* model, const Matrix& ownerWorld);
	void Reset_SolvedStateToPose();
	void Prepare_SolvedPoseStart();
	void Evaluate_SegmentCollisionContacts(const ModelCom* model, const Matrix& ownerWorld);
	void Solve_ContactPoseChains(const ModelCom* model, const Matrix& ownerWorld);
	void Finalize_CollisionProjection(const ModelCom* model, const Matrix& ownerWorld);
	void Commit_SolvedState();
	void Enforce_ChainLengthConstraints(Chain& chain);
	void Project_ChainOutOfColliders(Chain& chain, const vector<WorldCapsule>& worldCapsules, float scale, float maxScale, bool markContact);
	void HardProject_SegmentsOutOfColliders(Chain& chain, const vector<WorldCapsule>& worldCapsules, bool markContact);
	void HardProject_NodePointsOutOfColliders(Chain& chain, const vector<WorldCapsule>& worldCapsules, bool markContact);
	float GetCollisionQueryRadius(const WorldCapsule& capsule) const;
	float GetCollisionContactDistance(const WorldCapsule& capsule) const;
	void Apply_NodeCorrection(Chain& chain, size_t nodeIndex, const Vec3& correction, float penetration, bool propagateToChildren);
	void Apply_CorrectionToDescendants(Chain& chain, size_t nodeIndex, const Vec3& correction);
	void Write_BackSolvedPoseToModel(ModelCom* model, const Matrix& ownerWorld, bool isOwnerIdle);
	vector<WorldCapsule> Update_WorldCapsules(const ModelCom* model, const Matrix& ownerWorld) const;
	void Draw_DebugChains() const;

private:
	PhysicsModifierDesc _desc{};
	vector<Chain> _chains{};
	vector<ChainModifier::CapsuleColliderDesc> _colliders{};
	unordered_set<string> _configuredRoots{};
	bool _chainsInitialized{ false };
	bool _isFirstUpdate{ true };
	Vec3 _prevOwnerPos{ Vec3::Zero };
	Matrix _prevOwnerWorld{ Matrix::Identity };
	float _elapsedTime{ 0.f };
	bool _runtimeTestOwnerMotionEnabled{ false };
	Vec3 _runtimeTestOwnerMotionDelta{ Vec3::Zero };

public:
	static Shared<PhysicsModifier> Create(const PhysicsModifierDesc* desc);
	void Free() override;
};

NS_END
