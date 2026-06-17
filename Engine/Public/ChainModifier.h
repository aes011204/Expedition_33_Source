#pragma once
#include "AnimationModifier.h"
#include "TransformCom.h"

NS_BEGIN(Engine)

class ENGINE_DLL ChainModifier final : public AnimationModifier
{
public:
	struct BoneChainGroupDesc
	{
		string rootBoneName{};              // 이 체인 그룹의 시뮬레이션을 시작할 첫 번째 본의 이름
		float stiffnessMul{ 1.f };          // 체인 그룹별 형태 유지(stiffness) 강도 배율
		float dampingMul{ 1.f };            // 체인 그룹별 속도 유지(damping) 강도 배율
		float gravityMul{ 1.f };            // 체인 그룹별 중력(gravity) 강도 배율
		float movementInertiaMul{ 1.f };    // 체인 그룹별 관성(inertia) 적용 배율 (캐릭터 움직임에 의한 흔들림)
		float poseRestoreMul{ 1.f };        // 체인 그룹별 기본 포즈로 되돌아가려는 복원력 배율
		float maxBendAngleMul{ 1.f };       // 체인 그룹별 최대 꺾임 허용 각도 배율
		uint32 maxDepth{ 0xffffffffu };     // (선택) 시뮬레이션을 적용할 본의 최대 깊이 제한
		float windMul{ 1.f };               // 체인 그룹별 바람 영향 배율
		float dummyEndLength{ 0.f };        // (선택) Kawaii DummyBoneLength와 비슷하게 끝단 뒤에 둘 가상 팁 길이
		Vec3 rootRotationOffsetDegrees{ Vec3::Zero }; // 이 그룹 첫 본에 적용할 로컬 회전 오프셋 (도 단위)
	};

	struct BoneChainNode
	{
		uint32 boneIndex{ 0 };
		int32 parentNodeIndex{ -1 };
		vector<int32> childNodeIndices{};

		Matrix originalWorld{ Matrix::Identity };
		Matrix prevOriginalWorld{ Matrix::Identity };
		Vec3 prevOriginalPosition{ Vec3::Zero };
		Matrix bindLocal{ Matrix::Identity };
		Vec3 bindLocalDir{ Vec3::Zero };
		Matrix poseLocal{ Matrix::Identity };
		Vec3 poseLocalDir{ Vec3::Zero };
		Vec3 position{ Vec3::Zero };
		Vec3 prevPosition{ Vec3::Zero };
		float length{ 0.01f };
		bool isVirtualAnchor{ false };
		bool isVirtualTip{ false };
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
		float windMul{ 1.f };
		float dummyEndLength{ 0.f };
		Vec3 rootRotationOffsetDegrees{ Vec3::Zero };
	};

	struct CapsuleColliderDesc
	{
		string boneName{};
		Vec3 center{ Vec3::Zero };
		Quaternion rotationQuat{ Quaternion::Identity };
		float radius{ 0.f };
		float halfLength{ 0.f };
	};

	struct ChainModifierDesc : public AnimationModifierDesc
	{
		vector<BoneChainGroupDesc> groups{};

		struct ShapeSettings
		{
			float stiffness{ 0.04f }; 	                    // 전체 형태 유지 강도 (복원력, 꺾임 제한 등에 영향을 줌)
			float tipStiffnessScale{ 1.f };                 // 끝단으로 갈수록 적용할 형태 유지 강도 배율
			float poseRestoreStrength{ 0.35f };             // 바인드 포즈(기본 방향)로 되돌아가려는 복원력 강도
			float tipPoseRestoreScale{ 1.f };               // 끝단으로 갈수록 적용할 포즈 복원력 배율
			float maxBendAngleDegree{ 13.f };               // 바인드 포즈를 기준으로 허용되는 최대 꺾임 각도 (도 단위)
			float tipMaxBendAngleScale{ 1.f };              // 끝단으로 갈수록 적용할 최대 꺾임 각도 배율
			float surfaceConstraintStrength{ 0.f };         // 인접한 옷 체인끼리 원래 폭을 유지하는 가로 제약 강도
			bool pinRoot{ true };                           // 첫 번째 시뮬레이션 본의 위치를 부모 본에 핀(Pin)처럼 고정할지 여부
		} shape{};

		struct MotionSettings
		{
			float damping{ 0.55f };                         // 속도 유지 비율 (Verlet 적분에서 사용, 1에 가까울수록 속도 유지, 0에 가까울수록 감쇠)
			float tipDampingScale{ 1.f };                   // 끝단으로 갈수록 적용할 속도 유지 비율 배율
			float movementInertiaScale{ 0.06f };            // 캐릭터의 이동 속도가 체인의 움직임(관성)에 미치는 영향력
			float tipMovementInertiaScale{ 1.f };           // 끝단으로 갈수록 적용할 이동 관성 배율
			float maxMovementOffset{ 0.02f };               // 외부 요인에 의해 한 번에 이동할 수 있는 최대 거리 제한
			uint32 solverIterations{ 2 };                   // 프레임당 물리 시뮬레이션 반복 계산 횟수 (높을수록 정밀해짐)
		} motion{};

		struct CollisionSettings
		{
			bool enableCollision{ true };                   // 지정된 Physics Asset 기반의 캡슐 충돌 처리 활성화 여부
			float particleCollisionRadius{ 0.f };           // 각 체인 particle이 갖는 추가 충돌 반지름 (Kawaii Physics Radius 대응)
			float collisionRadiusPadding{ 1.f };            // 콜라이더 반지름에 추가로 적용할 여유 패딩 값
			wstring physicsAssetPath{ L"" };            // 충돌 캡슐 정보를 불러올 Physics Asset 데이터 경로 (JSON 파일)
			vector<string> includedBones{};                 // 비어 있지 않으면 이 본들의 충돌체만 사용
			vector<CapsuleColliderDesc> overrideCapsules{}; // 비어 있지 않으면 Physics Asset 대신 이 캡슐 데이터를 직접 사용
			bool debugDrawCapsules{ false };                // true면 계산된 월드 캡슐을 DebugDraw로 표시
		} collision{};

		struct RuntimeSettings
		{
			float resetDistance{ 3.f };                     // 1프레임 내에 이 거리 이상 이동(텔레포트)하면 시뮬레이션을 초기화함
			TransformCom *ownerTransform{ nullptr };        // 월드 공간 물리 시뮬레이션의 기준이 되는 소유자의 Transform 정보 (보통 캐릭터 Transform)
			float targetFrameRate{ 0.f };                   // 0보다 크면 이 FPS 기준으로 최대 서브스텝 델타를 맞추도록 solver iteration을 보정
			Vec3 gravity{ 0.f, -0.25f, 0.f };               // 매 서브스텝마다 적용되는 월드 공간 기준 중력
			Vec3 windDirection{ Vec3::Zero };
			float windScale{ 1.f };
			float windStartRatio{ -1.f };                  // 0 이상이면 체인 앞쪽 몇 %부터 바람을 적용할지 직접 지정
			float windWeightScale{ -1.f };                 // 0 이상이면 바람 가중치 세기를 직접 지정
			float constraintDeadzone{ 0.f };               // constraint 보정 미세 떨림을 무시할 거리
		} runtime{};
	};

	struct WorldCapsule
	{
		Vec3 startPoint;
		Vec3 endPoint;
		float radius;
	};

protected:
	HRESULT Initialize(AnimationModifierDesc *desc) override;
	void On_ActiveChanged(ModelCom *model, bool isActive) override;

private:
	void Initialize_Chains(ModelCom *model);
	void BuildChainRecursive(ModelCom *model, int32 currentBoneIndex, int32 parentNodeIndex, BoneChain &outChain, uint32 currentDepth, uint32 maxDepth);
	void BuildBindLocalDirections(ModelCom *model, BoneChain &chain, const Matrix &ownerWorld);

private:
	void Capture_CurrentPose(ModelCom *model, const Matrix &ownerWorld);
	void Capture_ReplacePose(ModelCom *model, BoneChain &chain, const Matrix &ownerWorld);
	void Capture_AdditivePose(ModelCom *model, BoneChain &chain, const Matrix &ownerWorld);
	void Update_PoseLocalDirections(BoneChain &chain);
	void Solve_Verlet(ModelCom *model, float timeDelta, const Matrix &ownerWorld, float movementWeight);
	void Solve_SurfaceConstraints(const vector<WorldCapsule> &worldCapsules);
	void Write_BackToModel(ModelCom *model, const Matrix &ownerWorld);
	void Write_BackChain(ModelCom *model, BoneChain &chain);

	vector<WorldCapsule> Update_WorldCapsule(const ModelCom *model, const Matrix *ownerWorld);
	Vec3 Compute_WorldCapsule(BoneChainNode &currentNode, const vector<WorldCapsule> &capsules, const Vec3 &nextPosition);

	static Vec3 Get_LocalDirection(const Matrix &parentWorld, const Vec3 &worldDirection);
	static Vec3 Get_WorldDirection(const Matrix &parentWorld, const Vec3 &localDirection);
	static Vec3 Limit_LocalDirectionByAngle(const Vec3 &bindLocalDirection, const Vec3 &simulatedLocalDirection, float maxAngleDegree);
	static Quaternion Build_RotationDelta(const Vec3 &fromDirection, const Vec3 &toDirection);
	static Matrix Build_LocalMatrix_RotationOnly(const Matrix &bindLocal, const Quaternion &deltaRotation);

public:
	void Evaluate_Modifier(ModelCom *model, float timeDelta) override;
	const ClassReflectionInfo* Get_RuntimeReflectionInfo() const override;
	void* Get_RuntimeReflectionTarget() override { return &_desc; }
	void On_RuntimeReflectionChanged(ModelCom* model, const PropertyInfo& property) override;
	bool Can_RuntimeResetPose() const override { return true; }
	void RuntimeResetPose(ModelCom* model) override { ResetPose(model); }
	bool Can_RuntimeRebuild() const override { return true; }
	void RuntimeRebuild(ModelCom* model) override;
	size_t Get_RuntimeGroupCount() const { return _desc.groups.size(); }
	string Get_RuntimeGroupRootBoneName(size_t groupIndex) const;
	Vec3 Get_RuntimeGroupRootRotationOffset(size_t groupIndex) const;
	void Set_RuntimeGroupRootRotationOffset(size_t groupIndex, const Vec3& offsetDegrees);

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
