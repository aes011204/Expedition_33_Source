#pragma once
#include "AnimationModifier.h"


NS_BEGIN(Engine)

class ChainModifier final : public AnimationModifier
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
    };

    struct BoneChainNode
    {
        uint32 boneIndex{ 0 };
        int32 parentNodeIndex{ -1 };
        vector<int32> childNodeIndices{};

        Matrix originalWorld{ Matrix::Identity };
        Vec3 position{ Vec3::Zero };
        Vec3 prevPosition{ Vec3::Zero };
        float length{ 0.01f };
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

public:
    struct ChainModifierDesc : public AnimationModifierDesc
    {
        vector<BoneChainGroupDesc> groups{};

        // 공통 기본 물리 설정
        float stiffness{ 0.04f };
        float damping{ 0.55f };
        float inertiaY{ 0.3f };
        Vec3 gravity{ 0.f, -0.25f, 0.f };

        float movementInertiaScale{ 0.06f };
        float maxMovementInertia{ 0.8f };
        float maxMovementOffset{ 0.02f };

        float poseRestoreStrength{ 0.35f };
        float maxBendAngleDegree{ 14.f };

        float resetDistance{ 300.f };
        uint32 solverIterations{ 2 };

        class TransformCom* ownerTransform{ nullptr };
        class MovementCom*  ownerMovement{ nullptr };
    };

public:
    ~ChainModifier() override = default;

private:
    HRESULT Initialize(AnimationModifierDesc* desc) override;
    void On_ActiveChanged(ModelCom* model, bool isActive) override;

private:
    void Initialize_Chains(ModelCom* model);
    void BuildChainRecursive(ModelCom* model, int32 currentBoneIndex, int32 parentNodeIndex, BoneChain& outChain);
    void Capture_CurrentPose(ModelCom* model, const Matrix& ownerWorld);
    void Solve_Verlet(ModelCom* model, float timeDelta, const Matrix& ownerWorld, const Vec3& ownerVelocity);
    void Write_BackToModel(ModelCom* model, const Matrix& ownerWorld);


public:
    void Evaluate_Modifier(ModelCom* model, float timeDelta) override;

public:
    void ResetPose(ModelCom* model);

private:
    ChainModifierDesc _desc{};
    vector<BoneChain> _chains{};
    bool _isFirstUpdate{ true };
    bool _chainsInitialized{ false };
    Vec3 _prevOwnerPos{ Vec3::Zero };

public:
    static Shared<ChainModifier> Create(const ChainModifierDesc* desc);
    void Free() override;
};

NS_END
