#pragma once
#include "AnimationModifier.h"
#include "TransformCom.h"
#include "ChainModifier.h"

NS_BEGIN(Engine)

class ENGINE_DLL PoseFollowChainModifier final : public AnimationModifier
{
public:
	struct BoneChainGroupDesc
	{
		string rootBoneName{};
		float windMul{ 1.f };
		float motionMul{ 1.f };
		float followMul{ 1.f };
		float rootOutputMul{ 1.f };
		float tipOutputMul{ 1.f };
	};

	struct PoseFollowChainModifierDesc : public AnimationModifierDesc
	{
		vector<BoneChainGroupDesc> groups{};
		TransformCom* ownerTransform{ nullptr };
		const Matrix* ownerWorldMatrix{ nullptr };

		wstring physicsAssetPath{};
		vector<string> includedCollisionBones{};
		vector<ChainModifier::CapsuleColliderDesc> overrideCapsules{};
		const ModelCom* collisionModel{ nullptr };
		const Matrix* collisionOwnerWorldMatrix{ nullptr };
		bool enableCollision{ true };
		bool debugDrawCapsules{ false };
		mutable int32 debugLoadedCapsuleCount{ 0 };
		mutable int32 debugActiveCapsuleCount{ 0 };
		mutable int32 debugCollisionSource{ 0 };
		mutable int32 debugCollisionHitCount{ 0 };
		mutable int32 debugFinalCollisionHitCount{ 0 };
		mutable float debugMinCollisionMargin{ 0.f };
		float particleCollisionRadius{ 2.5f };
		float collisionRadiusPadding{ 0.08f };
		float collisionPrimitiveScale{ 1.f };
		float collisionOutputAlpha{ 0.f };

		Vec3 windDirection{ Vec3(1.f, 0.f, 1.f) };
		float windScale{ 0.016f };
		float ownerMotionScale{ 0.012f };
		float animatedMotionScale{ 0.2f };
		float ownerMotionDeadzone{ 0.02f };
		float ownerRotationDeadzoneDegree{ 0.4f };
		float animatedMotionDeadzone{ 0.04f };
		float maxExternalOffset{ 0.012f };

		float damping{ 0.72f };
		float tipDampingScale{ 1.05f };
		float poseFollowStrength{ 0.44f };
		float tipFollowScale{ 0.48f };
		float maxBendAngleDegree{ 18.f };
		float tipBendAngleScale{ 1.35f };
		float bendLimitStrength{ 0.86f };
		float surfaceConstraintStrength{ 0.f };

		float rootOutputAlpha{ 0.28f };
		float tipOutputAlpha{ 0.68f };
		float idleOutputAlphaScale{ 0.25f };
		float outputSmoothAlpha{ 0.36f };
		float idleOutputSmoothAlpha{ 0.72f };
		float outputDeadzoneDegree{ 0.85f };
		float idleSettleStrength{ 0.85f };
		float sleepVelocityThreshold{ 0.01f };
		float sleepPoseDistance{ 0.08f };

		uint32 solverIterations{ 4 };
		float targetFrameRate{ 60.f };
		float resetDistance{ 3.f };
	};

private:
	struct BoneChainNode
	{
		uint32 boneIndex{ 0 };
		int32 parentNodeIndex{ -1 };
		vector<int32> childNodeIndices{};

		Matrix poseLocal{ Matrix::Identity };
		Matrix originalWorld{ Matrix::Identity };
		Matrix prevOriginalWorld{ Matrix::Identity };
		Vec3 poseLocalDir{ Vec3::Zero };
		Vec3 smoothedLocalDir{ Vec3::Zero };
		Vec3 position{ Vec3::Zero };
		Vec3 prevPosition{ Vec3::Zero };
		float length{ 0.01f };
		float debugWriteAlpha{ 0.f };
		bool debugCollisionContact{ false };
		bool isVirtualAnchor{ false };
		bool hasSmoothedLocalDir{ false };
		bool debugIsRootNode{ false };
	};

	struct BoneChain
	{
		vector<BoneChainNode> nodes{};
		string rootBoneName{};
		float windMul{ 1.f };
		float motionMul{ 1.f };
		float followMul{ 1.f };
		float rootOutputMul{ 1.f };
		float tipOutputMul{ 1.f };
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
	struct RuntimeDebugNode
	{
		size_t groupIndex{ 0 };
		size_t nodeIndex{ 0 };
		int32 parentNodeIndex{ -1 };
		uint32 boneIndex{ 0 };
		string boneName{};
		Vec3 posePosition{ Vec3::Zero };
		Vec3 simulatedPosition{ Vec3::Zero };
		float writeAlpha{ 0.f };
		bool isVirtualAnchor{ false };
		bool isRootNode{ false };
	};

	struct RuntimeDebugCapsule
	{
		Vec3 startPoint{ Vec3::Zero };
		Vec3 endPoint{ Vec3::Zero };
		float sourceRadius{ 0.f };
		float collisionRadius{ 0.f };
	};

	void Evaluate_Modifier(ModelCom* model, float timeDelta) override;
	const ClassReflectionInfo* Get_RuntimeReflectionInfo() const override;
	void* Get_RuntimeReflectionTarget() override { return &_desc; }
	void On_RuntimeReflectionChanged(ModelCom* model, const PropertyInfo& property) override;
	bool Can_RuntimeResetPose() const override { return true; }
	void RuntimeResetPose(ModelCom* model) override { ResetPose(model); }
	bool Can_RuntimeRebuild() const override { return true; }
	void RuntimeRebuild(ModelCom* model) override;
	void ResetPose(ModelCom* model);
	size_t Get_RuntimeGroupCount() const { return _chains.size(); }
	string Get_RuntimeGroupRootBoneName(size_t groupIndex) const;
	Vec3 Get_RuntimeGroupMultipliers(size_t groupIndex) const;
	void Set_RuntimeGroupMultipliers(size_t groupIndex, const Vec3& multipliers);
	void Set_RuntimeTestOwnerMotion(bool enabled, const Vec3& delta);
	void Build_RuntimeDebugNodes(ModelCom* model, vector<RuntimeDebugNode>& outNodes) const;
	void Build_RuntimeDebugCapsules(ModelCom* model, vector<RuntimeDebugCapsule>& outCapsules) const;

private:
	void Initialize_Chains(ModelCom* model);
	void BuildChainRecursive(ModelCom* model, int32 currentBoneIndex, int32 parentNodeIndex, BoneChain& outChain);
	void Capture_CurrentPose(ModelCom* model, const Matrix& ownerWorld);
	void Solve(ModelCom* model, float timeDelta, const Matrix& ownerWorld, const Vec3& ownerMotionDelta, float movementWeight, bool isOwnerIdle);
	void Solve_SurfaceConstraints(const vector<WorldCapsule>& worldCapsules);
	void Write_BackToModel(ModelCom* model, const Matrix& ownerWorld, bool isOwnerIdle);
	void Write_BackChain(ModelCom* model, BoneChain& chain, const Matrix& ownerWorld, bool isOwnerIdle);

	vector<WorldCapsule> Update_WorldCapsules(const ModelCom* model, const Matrix& ownerWorld) const;
	Vec3 Resolve_Collision(BoneChainNode& node, const vector<WorldCapsule>& capsules, const Vec3& position, bool movePrevPosition);
	float Compute_CollisionMargin(const vector<WorldCapsule>& capsules, const Vec3& position) const;

private:
	PoseFollowChainModifierDesc _desc{};
	vector<BoneChain> _chains{};
	vector<int32> _configuredRootBoneIndices{};
	vector<ChainModifier::CapsuleColliderDesc> _colliders{};
	bool _isFirstUpdate{ true };
	bool _chainsInitialized{ false };
	Vec3 _prevOwnerPos{ Vec3::Zero };
	Vec3 _prevOwnerForward{ Vec3::UnitZ };
	bool _runtimeTestOwnerMotionEnabled{ false };
	Vec3 _runtimeTestOwnerMotionDelta{ Vec3::Zero };

public:
	static Shared<PoseFollowChainModifier> Create(const PoseFollowChainModifierDesc* desc);
	void Free() override;
};

NS_END
