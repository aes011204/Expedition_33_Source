#pragma once
#include "AnimationModifier.h"
#include "TransformCom.h"

NS_BEGIN(Engine)

class ENGINE_DLL ChainModifier final : public AnimationModifier
{
public:
	struct BoneChainGroupDesc
	{
		string rootBoneName{};
		float stiffnessMul{ 1.f };
		float dampingMul{ 1.f };
		float gravityMul{ 1.f };
		float movementInertiaMul{ 1.f };
		float poseRestoreMul{ 1.f };
		float maxBendAngleMul{ 1.f };
		uint32 maxDepth{ 0xffffffffu };
	};

	struct BoneChainNode
	{
		uint32 boneIndex{ 0 };
		int32 parentNodeIndex{ -1 };
		vector<int32> childNodeIndices{};

		Matrix originalWorld{ Matrix::Identity };
		Vec3 prevOriginalPosition{ Vec3::Zero };
		Matrix bindLocal{ Matrix::Identity };
		Vec3 bindLocalDir{ Vec3::Zero };
		Vec3 position{ Vec3::Zero };
		Vec3 prevPosition{ Vec3::Zero };
		float length{ 0.01f };
		bool isVirtualAnchor{ false };
	};

	struct BoneChain
	{
		vector<BoneChainNode> nodes{};
		string rootBoneName{};
		float stiffnessMul{ 1.f };
		float dampingMul{ 1.f };
		float gravityMul{ 1.f };
		float movementInertiaMul{ 1.f };
		float poseRestoreMul{ 1.f };
		float maxBendAngleMul{ 1.f };
	};

	struct CapsuleColliderDesc
	{
		string boneName{};
		Vec3 center{ Vec3::Zero };
		Quaternion rotationQuat{ Quaternion::Identity };
		float radius{ 0.f };
		float halfLength{ 0.f };
	};

public:
	struct ChainModifierDesc : public AnimationModifierDesc
	{
		vector<BoneChainGroupDesc> groups{};
		wstring physicsAssetPath{ L"" };

		float stiffness{ 0.04f };
		float damping{ 0.55f };
		Vec3 gravity{ 0.f, -0.25f, 0.f };

		float movementInertiaScale{ 0.06f };
		float maxMovementOffset{ 0.02f };

		float poseRestoreStrength{ 0.35f };
		float maxBendAngleDegree{ 13.f };
		float collisionRadiusPadding{ 1.f };
		bool enableCollision{ true };
		bool pinRoot{ true };

		float resetDistance{ 3.f };
		uint32 solverIterations{ 2 };

		TransformCom *ownerTransform{ nullptr };
	};

	struct WorldCapsule
	{
		Vec3 startPoint;
		Vec3 endPoint;
		float radius;
	};

public:
	HRESULT Initialize(AnimationModifierDesc *desc) override;
	void On_ActiveChanged(ModelCom *model, bool isActive) override;

private:
	void Initialize_Chains(ModelCom *model);
	void BuildChainRecursive(ModelCom *model, int32 currentBoneIndex, int32 parentNodeIndex, BoneChain &outChain, uint32 currentDepth, uint32 maxDepth);

private:
	void Capture_CurrentPose(ModelCom *model, const Matrix &ownerWorld);
	void Solve_Verlet(ModelCom *model, float timeDelta, const Matrix &ownerWorld, float movementWeight);
	void Write_BackToModel(ModelCom *model, const Matrix &ownerWorld);

	vector<WorldCapsule> Update_WorldCapsule(const ModelCom *model, const Matrix *ownerWorld);
	Vec3 Compute_WorldCapsule(BoneChainNode &currentNode, const vector<WorldCapsule> &capsules, const Vec3 &nextPosition);

	static Vec3 Get_LocalDirection(const Matrix &parentWorld, const Vec3 &worldDirection);
	static Vec3 Limit_LocalDirectionByAngle(const Vec3 &bindLocalDirection, const Vec3 &simulatedLocalDirection, float maxAngleDegree);
	static Quaternion Build_RotationDelta(const Vec3 &fromDirection, const Vec3 &toDirection);
	static Matrix Build_LocalMatrix_RotationOnly(const Matrix &bindLocal, const Quaternion &deltaRotation);

public:
	void Evaluate_Modifier(ModelCom *model, float timeDelta) override;

public:
	void ResetPose(ModelCom *model);

private:
	ChainModifierDesc _desc{};
	vector<BoneChain> _chains{};
	vector<CapsuleColliderDesc> _colliders{};

	bool _isFirstUpdate{ true };
	bool _chainsInitialized{ false };
	Vec3 _prevOwnerPos{ Vec3::Zero };

public:
	static Shared<ChainModifier> Create(const ChainModifierDesc *desc);
	void Free() override;
};

NS_END
