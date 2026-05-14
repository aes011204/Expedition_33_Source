#pragma once

NS_BEGIN(Engine)
class ModelCom;
NS_END

NS_BEGIN(Client)

// 일단 Lune 전용 기본값으로 시작
struct HairChainPhysicsDesc
{
    string bonePrefix = "Strand_";

    // ABP_PostProcess_Hair_LUNE_Rebelious 에서 추출한 값
    float stiffness = 0.05f;
    float damping = 0.1f;
    float radius = 3.f;

    float windScale = 0.35f;

    Vec3 gravity = Vec3(0.f, -2.5f, 0.f);

    // 큰 이동/순간이동 시 이전 위치를 버리고 현재 pose로 리셋하기 위한 기준.
    float teleportDistance = 300.f;

    // Verlet constraint 반복 횟수
    uint32 solverIterations = 3;
};

// ModelCom의 bone 이름/parent 관계를 읽어서 Strand 체인 생성
class HairChainPhysics final
{
public:
    void Initialize(ModelCom* model, const HairChainPhysicsDesc& desc);
    void ResetPose(ModelCom* model);
    void Simulate(ModelCom* model, float timeDelta, const Matrix& ownerWorld);

    bool Is_Built() const { return _built; }

private:
    struct Node
    {
        uint32 boneIndex = 0;
        int32 parentNodeIndex = -1;

        // 원래 애니메이션 local matrix.
        // ResetPose나 초기 방향 보정에 사용.
        Matrix bindLocal = Matrix::Identity;

        // parent와의 bind pose 거리.
        float length = 0.f;

        // Verlet 상태. world space 기준.
        Vec3 position = Vec3::Zero;
        Vec3 prevPosition = Vec3::Zero;
    };

    struct Chain
    {
        vector<Node> nodes;
    };

private:
    void Initialize_Chains(ModelCom* model);
    void Capture_CurrentPose(ModelCom* model);
    void Solve_Verlet(float dt);
    void Write_BackToModel(ModelCom* model);

    static bool Is_HairBoneName(const string& name, const string& prefix);
    static string Get_ChainKey(const string& boneName);
    static Vec3 Get_WorldPosition(const Matrix& m);
    static Matrix Build_LocalMatrix_FromDirection(
        const Matrix& bindLocal,
        const Matrix& parentWorld,
        const Vec3& worldPosition,
        const Vec3& childDirection);

private:
    HairChainPhysicsDesc _desc{};
    vector<Chain> _chains{};

    Vec3 _lastOwnerPosition = Vec3::Zero;
    bool _built = false;
    bool _hasLastOwnerPosition = false;
};

NS_END
