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
		uint32 maxDepth{ 0xffffffffu };
		float windMul{ 1.f };
		float motionMul{ 1.f };
		float followMul{ 1.f };
	};

	struct PoseFollowChainModifierDesc : public AnimationModifierDesc
	{
		vector<BoneChainGroupDesc> groups{};
		TransformCom* ownerTransform{ nullptr };

		wstring physicsAssetPath{};
		vector<string> includedCollisionBones{};
		vector<ChainModifier::CapsuleColliderDesc> overrideCapsules{};
		bool enableCollision{ true };
		bool debugDrawCapsules{ false };
		float particleCollisionRadius{ 2.5f };
		float collisionRadiusPadding{ 0.08f };

		Vec3 windDirection{ Vec3(1.f, 0.f, 1.f) };
		float windScale{ 0.016f };
		float ownerMotionScale{ 0.012f };
		float animatedMotionScale{ 0.2f };
		float ownerMotionDeadzone{ 0.02f };
		float animatedMotionDeadzone{ 0.04f };
		float maxExternalOffset{ 0.012f };

		float damping{ 0.72f };
		float tipDampingScale{ 1.05f };
		float poseFollowStrength{ 0.44f };
		float tipFollowScale{ 0.48f };
		float maxBendAngleDegree{ 18.f };
		float tipBendAngleScale{ 1.35f };
		float bendLimitStrength{ 0.86f };
		float surfaceConstraintStrength{ 0.32f };

		float rootOutputAlpha{ 0.28f };
		float tipOutputAlpha{ 0.68f };
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
		bool isVirtualAnchor{ false };
		bool hasSmoothedLocalDir{ false };
	};

	struct BoneChain
	{
		vector<BoneChainNode> nodes{};
		string rootBoneName{};
		float windMul{ 1.f };
		float motionMul{ 1.f };
		float followMul{ 1.f };
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
	void ResetPose(ModelCom* model);

private:
	void Initialize_Chains(ModelCom* model);
	void BuildChainRecursive(ModelCom* model, int32 currentBoneIndex, int32 parentNodeIndex, BoneChain& outChain, uint32 currentDepth, uint32 maxDepth);
	void Capture_CurrentPose(ModelCom* model, const Matrix& ownerWorld);
	void Solve(ModelCom* model, float timeDelta, const Matrix& ownerWorld, const Vec3& ownerMotionDelta, float movementWeight);
	void Solve_SurfaceConstraints(const vector<WorldCapsule>& worldCapsules);
	void Write_BackToModel(ModelCom* model, const Matrix& ownerWorld);
	void Write_BackChain(ModelCom* model, BoneChain& chain, const Matrix& ownerWorld);

	vector<WorldCapsule> Update_WorldCapsules(const ModelCom* model, const Matrix& ownerWorld) const;
	Vec3 Resolve_Collision(BoneChainNode& node, const vector<WorldCapsule>& capsules, const Vec3& position, bool movePrevPosition);

	static float Get_NodeRatio(size_t nodeIndex, size_t nodeCount);
	static float Get_TipRatio(size_t nodeIndex, size_t nodeCount);
	static Vec3 Get_LocalDirection(const Matrix& parentWorld, const Vec3& worldDirection);
	static Vec3 Get_WorldDirection(const Matrix& parentWorld, const Vec3& localDirection);
	static Vec3 Limit_LocalDirectionByAngle(const Vec3& poseLocalDirection, const Vec3& simulatedLocalDirection, float maxAngleDegree);
	static Quaternion Build_RotationDelta(const Vec3& fromDirection, const Vec3& toDirection);
	static Matrix Build_LocalMatrix_RotationOnly(const Matrix& baseLocal, const Quaternion& deltaRotation);

private:
	PoseFollowChainModifierDesc _desc{};
	vector<BoneChain> _chains{};
	vector<ChainModifier::CapsuleColliderDesc> _colliders{};
	bool _isFirstUpdate{ true };
	bool _chainsInitialized{ false };
	Vec3 _prevOwnerPos{ Vec3::Zero };

public:
	static Shared<PoseFollowChainModifier> Create(const PoseFollowChainModifierDesc* desc);
	void Free() override;
};

NS_END
