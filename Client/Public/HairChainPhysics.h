#pragma once

NS_BEGIN(Engine)
class ModelCom;
NS_END

NS_BEGIN(Client)

// ModelCom의 bone 이름/parent 관계를 읽어서 Strand 체인 생성
class HairChainPhysics final
{
public:
    void Initialize(ModelCom* model, const HairChainPhysicsDesc& desc);
    void ResetPose(ModelCom* model);
    void Simulate(ModelCom* model, float timeDelta, const Matrix& ownerWorld, const Vec3& ownerVelocity);

    bool Is_Built() const { return _built; }

private:
    struct Node
    {
        uint32 boneIndex = 0;
        int32 parentNodeIndex = -1;

        // 원래 애니메이션 local matrix.
        // ResetPose나 초기 방향 보정에 사용.
        Matrix bindLocal = Matrix::Identity;

        // parent 기준 bind pose 방향.
        // 매 프레임 current pose에 delta를 누적하지 않기 위한 기준 방향.
        Vec3 bindLocalDir = Vec3::Zero;

        // parent와의 bind pose 거리.
        float length = 0.f;

        // Verlet 상태. world space 기준.
        Vec3 position = Vec3::Zero;
        Vec3 prevPosition = Vec3::Zero;
    };

    struct Chain
    {
        vector<Node> nodes;

        // chain root bone 이름. Strand_1_1, Strand_5_1 같은 대표 bone으로 튜닝 그룹을 나눈다.
        string rootBoneName;

        // 전체 desc에 곱하는 chain별 보정값.
        float stiffnessMul = 1.f;
        float dampingMul = 1.f;
        float windMul = 1.f;
        float gravityMul = 1.f;
        float movementInertiaMul = 1.f;
        float poseRestoreMul = 1.f;
        float maxBendAngleMul = 1.f;
    };

private:
    void Initialize_Chains(ModelCom* model);
    void Capture_CurrentPose(ModelCom* model);
    void Solve_Verlet(ModelCom* model, float dt, const Vec3& ownerVelocity);
    void Write_BackToModel(ModelCom* model);

    static bool Is_HairBoneName(const string& name, const string& prefix);
    static uint32 Get_StrandGroupNumber(const string& boneName);

    static void Apply_LuneChainTuning(Chain& chain, uint32 strandGroup);
    static void Apply_MaelleChainTuning(Chain& chain, uint32 strandGroup);
    static void Apply_GustaveChainTuning(Chain& chain, uint32 strandGroup);

    static Vec3 Get_WorldPosition(const Matrix& m);

    static Vec3 Get_LocalDirection(const Matrix& parentWorld, const Vec3& worldDirection);

    static Vec3 Get_WorldDirection(const Matrix& parentWorld, const Vec3& localDirection);
    static Vec3 Limit_LocalDirectionByAngle(
        const Vec3& bindLocalDirection,
        const Vec3& simulatedLocalDirection,
        float maxAngleDegree);

    static Quaternion Build_RotationDelta(const Vec3& fromDirection, const Vec3& toDirection);
    static Matrix Build_LocalMatrix_RotationOnly(
        const Matrix& bindLocal,
        const Quaternion& deltaRotation);

private:
    HairChainPhysicsDesc _desc{};
    vector<Chain> _chains{};

    Vec3 _lastOwnerPosition = Vec3::Zero;
    bool _built = false;
    bool _hasLastOwnerPosition = false;
};

NS_END
