# Engine ChainModifier 구현 및 적용 가이드

이 문서는 옷(Cloth) 및 머리카락(Hair)의 자연스러운 물리 시뮬레이션을 공통으로 처리하도록 구현한 전체 코드입니다. 공용 Math 헤더(`Helper_Math.h`)를 활용하도록 구조가 개선되었습니다.

## 0. 공용 Math 헤더 추가 (`Engine/Public/Helper_Math.h`)

기존 `Helper_Math.h`의 `namespace Engine::Math` 안에 아래의 공용 연산 함수들이 추가되어 있어야 합니다.

```cpp
/**
 * @brief 두 방향 벡터 사이의 회전을 나타내는 쿼터니언을 생성합니다. (DirectX SimpleMath 기반)
 * @param v1 정규화된 시작 벡터
 * @param v2 정규화된 목표 벡터
 * @return 두 벡터 사이의 최단 회전을 나타내는 쿼터니언
 */
inline Engine::Quat CreateRotationBetweenVectors(const Engine::Vec3& v1, const Engine::Vec3& v2)
{
    float dot = v1.Dot(v2);
    
    // 두 벡터가 거의 같은 방향일 때
    if (dot >= 1.0f - 1e-6f)
        return Engine::Quat::Identity;
        
    // 두 벡터가 거의 정반대 방향일 때 (180도 회전)
    if (dot <= -1.0f + 1e-6f)
    {
        Engine::Vec3 axis = Engine::Vec3::UnitX.Cross(v1);
        if (axis.LengthSquared() < 1e-6f)
            axis = Engine::Vec3::UnitY.Cross(v1);
        axis.Normalize();
        return Engine::Quat::CreateFromAxisAngle(axis, DirectX::XM_PI);
    }
    
    // 일반적인 경우 (최단 경로 회전)
    Engine::Quat q;
    Engine::Vec3 cross = v1.Cross(v2);
    q.x = cross.x;
    q.y = cross.y;
    q.z = cross.z;
    q.w = 1.0f + dot;
    q.Normalize();
    return q;
}

/**
 * @brief Verlet 적분을 이용한 다음 물리 위치 계산
 *
 * @param currentPos 현재 위치
 * @param prevPos    이전 위치
 * @param force      가해지는 총 힘 (중력 + 관성 + 복원력 등)
 * @param damping    감쇠 계수 (1.0에 가까울수록 감속 없음, 보통 0.5~0.9)
 * @param dt         델타 타임
 * @return Vec3      다음 프레임의 위치
 */
inline Engine::Vec3 SolveVerlet(const Engine::Vec3& currentPos, const Engine::Vec3& prevPos, const Engine::Vec3& force, float damping, float timeDelta, int pow = 2)
{
    Engine::Vec3 velocity = (currentPos - prevPos) * damping;
    
    // 최적화: 가장 많이 사용되는 2제곱(dt^2)은 단순 곱셈으로 처리하여 CPU 사이클을 크게 절약합니다.
    float timeWeight = (pow == 2) ? (timeDelta * timeDelta) : std::pow(timeDelta, static_cast<float>(pow));

    return currentPos + velocity + force * timeWeight;
}
```

---

## 1. 헤더 파일 적용 (`Engine/Public/ChainModifier.h`)

기존 `ChainModifier.h` 파일의 내용을 모두 지우고 아래 코드로 덮어씌웁니다.

```cpp
#pragma once
#include "AnimationModifier.h"

NS_BEGIN(Engine)

struct BoneChainGroupDesc
{
    string rootBoneName{};
    
    // 그룹별 추가 물성 보정치
    float stiffnessMul = 1.f;
    float dampingMul = 1.f;
    float gravityMul = 1.f;
    float movementInertiaMul = 1.f;
    float poseRestoreMul = 1.f;
    float maxBendAngleMul = 1.f;
};

struct BoneChainNode
{
    uint32 boneIndex = 0;
    int32 parentNodeIndex = -1;
    vector<int32> childNodeIndices{};

    Matrix originalWorld = Matrix::Identity;
    Vec3 position = Vec3::Zero;
    Vec3 prevPosition = Vec3::Zero;
    float length = 0.01f;
};

struct BoneChain
{
    vector<BoneChainNode> nodes{};
    string rootBoneName{};

    float stiffnessMul = 1.f;
    float dampingMul = 1.f;
    float gravityMul = 1.f;
    float movementInertiaMul = 1.f;
    float poseRestoreMul = 1.f;
    float maxBendAngleMul = 1.f;
};

class ENGINE_DLL ChainModifier final : public AnimationModifier
{
public:
    struct ChainModifierDesc : public AnimationModifierDesc
    {
        vector<BoneChainGroupDesc> groups{};

        // 공통 기본 물리 설정
        float stiffness = 0.04f;
        float damping = 0.55f;
        Vec3 gravity = Vec3(0.f, -0.25f, 0.f);

        float movementInertiaScale = 0.06f;
        float maxMovementInertia = 0.8f;
        float maxMovementOffset = 0.02f;

        float poseRestoreStrength = 0.35f;
        float maxBendAngleDegree = 14.f;

        float teleportDistance = 300.f;
        uint32 solverIterations = 2;

        // 콜백 대신 포인터로 1회 바인딩 (수명 주기가 보장된 변수의 주소여야 함)
        const Matrix* ownerWorldMatrix = nullptr;
        const Vec3* ownerVelocity = nullptr;
    };

public:
    ChainModifier() = default;
    virtual ~ChainModifier() override = default;

    virtual HRESULT Initialize(AnimationModifierDesc* desc) override;
    virtual void Evaluate_Modifier(ModelCom* model, float timeDelta) override;
    
    static Shared<ChainModifier> Create(const ChainModifierDesc* desc);
    virtual void Free() override {}

    void ResetPose(ModelCom* model);

private:
    void Initialize_Chains(ModelCom* model);
    void BuildChainRecursive(ModelCom* model, int32 currentBoneIndex, int32 parentNodeIndex, BoneChain& outChain);
    void Capture_CurrentPose(ModelCom* model, const Matrix& ownerWorld);
    void Solve_Verlet(ModelCom* model, float dt, const Matrix& ownerWorld, const Vec3& ownerVelocity);
    void Write_BackToModel(ModelCom* model, const Matrix& ownerWorld);

private:
    ChainModifierDesc _desc{};
    vector<BoneChain> _chains{};
    bool _isFirstUpdate = true;
    bool _chainsInitialized = false;

    Vec3 _prevOwnerPos = Vec3::Zero;
};

NS_END
```

---

## 2. 소스 파일 적용 (`Engine/Private/ChainModifier.cpp`)

비어있던 `ChainModifier.cpp` 파일에 아래 전체 코드를 복사하여 붙여넣습니다. `Helper_Math.h`를 인클루드하여 연산합니다.

```cpp
#include "ChainModifier.h"
#include "ModelCom.h"
#include "Bone.h"
#include "GameObject.h"
#include "Helper_Math.h"

NS_BEGIN(Engine)

HRESULT ChainModifier::Initialize(AnimationModifierDesc* desc)
{
    if (FAILED(__super::Initialize(desc)))
        return E_FAIL;

    auto chainDesc = static_cast<ChainModifierDesc*>(desc);
    if (!chainDesc)
        return E_FAIL;

    _desc = *chainDesc;
    return S_OK;
}

void ChainModifier::BuildChainRecursive(ModelCom* model, int32 currentBoneIndex, int32 parentNodeIndex, BoneChain& outChain)
{
    BoneChainNode node;
    node.boneIndex = currentBoneIndex;
    node.parentNodeIndex = parentNodeIndex;

    // 부모가 있다면, 부모의 로컬 오프셋 기준 거리를 Length로 설정
    if (parentNodeIndex != -1)
    {
        Matrix localMat = model->Get_BoneLocalMatrix(currentBoneIndex);
        node.length = localMat.Translation().Length();
        if (node.length < 0.01f) // 최소 길이 임계값
            node.length = 0.01f;
    }

    int32 thisNodeIndex = static_cast<int32>(outChain.nodes.size());
    outChain.nodes.push_back(node);

    if (parentNodeIndex != -1)
    {
        outChain.nodes[parentNodeIndex].childNodeIndices.push_back(thisNodeIndex);
    }

    // 자식 뼈 검색 (엔진의 모든 뼈를 순회하며 ParentIndex가 현재 뼈인 것을 찾음)
    uint32 numBones = static_cast<uint32>(model->Get_NumBones());
    for (uint32 i = 0; i < numBones; ++i)
    {
        if (model->Get_BoneParentIndex(i) == currentBoneIndex)
        {
            BuildChainRecursive(model, static_cast<int32>(i), thisNodeIndex, outChain);
        }
    }
}

void ChainModifier::Initialize_Chains(ModelCom* model)
{
    _chains.clear();

    for (const auto& groupDesc : _desc.groups)
    {
        int32 rootBoneIndex = model->Get_BoneIndex(groupDesc.rootBoneName);
        if (rootBoneIndex == -1)
            continue;

        BoneChain newChain;
        newChain.rootBoneName = groupDesc.rootBoneName;
        newChain.stiffnessMul = groupDesc.stiffnessMul;
        newChain.dampingMul = groupDesc.dampingMul;
        newChain.gravityMul = groupDesc.gravityMul;
        newChain.movementInertiaMul = groupDesc.movementInertiaMul;
        newChain.poseRestoreMul = groupDesc.poseRestoreMul;
        newChain.maxBendAngleMul = groupDesc.maxBendAngleMul;

        // DFS로 루트부터 말단 뼈까지 트리 형태로 단일 체인에 담음
        BuildChainRecursive(model, rootBoneIndex, -1, newChain);

        if (!newChain.nodes.empty())
        {
            _chains.push_back(std::move(newChain));
        }
    }

    _chainsInitialized = true;
}

void ChainModifier::ResetPose(ModelCom* model)
{
    Matrix ownerWorld = _desc.ownerWorldMatrix ? *(_desc.ownerWorldMatrix) : Matrix::Identity;

    Capture_CurrentPose(model, ownerWorld);

    for (auto& chain : _chains)
    {
        for (auto& node : chain.nodes)
        {
            node.prevPosition = node.originalWorld.Translation();
            node.position = node.prevPosition;
        }
    }
}

void ChainModifier::Capture_CurrentPose(ModelCom* model, const Matrix& ownerWorld)
{
    for (auto& chain : _chains)
    {
        for (auto& node : chain.nodes)
        {
            const Matrix* combinedMat = model->Get_BoneMatrixPtr(node.boneIndex);
            if (combinedMat)
            {
                node.originalWorld = (*combinedMat) * ownerWorld;
            }
            else
            {
                node.originalWorld = ownerWorld;
            }
        }
    }
}

void ChainModifier::Solve_Verlet(ModelCom* model, float dt, const Matrix& ownerWorld, const Vec3& ownerVelocity)
{
    Vec3 movementInertia = -ownerVelocity * _desc.movementInertiaScale;
    
    // Y축 이동 관성 감소 (옷 튀어오름 방지)
    movementInertia.y *= 0.3f;

    if (movementInertia.Length() > _desc.maxMovementOffset)
    {
        movementInertia.Normalize();
        movementInertia *= _desc.maxMovementOffset;
    }

    for (auto& chain : _chains)
    {
        float curDamping = _desc.damping * chain.dampingMul;
        Vec3 curGravity = _desc.gravity * chain.gravityMul;
        Vec3 curInertia = movementInertia * chain.movementInertiaMul;

        // 1. 공용 Math 헤더의 Verlet 적분 함수 사용
        for (size_t i = 0; i < chain.nodes.size(); ++i)
        {
            auto& node = chain.nodes[i];
            
            // 루트 노드는 애니메이션 핀(Pin) 고정
            if (node.parentNodeIndex == -1)
            {
                node.position = node.originalWorld.Translation();
                node.prevPosition = node.position;
                continue;
            }
            
            // Pose Restore 힘 적용
            Vec3 restoreForce = (node.originalWorld.Translation() - node.position) * _desc.poseRestoreStrength * chain.poseRestoreMul;
            Vec3 force = curGravity + curInertia + restoreForce; 

            // 공통 함수로 다음 위치 계산 (기본값 pow = 2)
            Vec3 nextPosition = Engine::Math::SolveVerlet(node.position, node.prevPosition, force, curDamping, dt);

            node.prevPosition = node.position;
            node.position = nextPosition;
        }

        // 2. Length Distance Constraint (부모와의 간격 유지)
        for (size_t i = 0; i < chain.nodes.size(); ++i)
        {
            auto& node = chain.nodes[i];
            if (node.parentNodeIndex == -1)
                continue;

            auto& parentNode = chain.nodes[node.parentNodeIndex];
            Vec3 dirToParent = parentNode.position - node.position;
            float currentDist = dirToParent.Length();

            if (currentDist > 0.0001f)
            {
                // 강제 거리 보정 (스프링 대신 하드 제약 적용)
                Vec3 correction = dirToParent * (1.0f - node.length / currentDist);
                node.position += correction; // 부모는 고정이고 자식만 당겨짐
            }
        }
    }
}

void ChainModifier::Write_BackToModel(ModelCom* model, const Matrix& ownerWorld)
{
    Matrix invOwnerWorld = ownerWorld.Invert();

    for (auto& chain : _chains)
    {
        for (size_t i = 0; i < chain.nodes.size(); ++i)
        {
            auto& node = chain.nodes[i];
            
            // 자식이 없으면 방향을 계산할 수 없으므로 무시 (말단 노드)
            if (node.childNodeIndices.empty())
                continue;

            // 여러 자식이 있을 경우(갈래길) 첫 번째 자식 기준으로 방향 산출
            auto& childNode = chain.nodes[node.childNodeIndices[0]];

            Vec3 originalDir = childNode.originalWorld.Translation() - node.originalWorld.Translation();
            Vec3 simDir = childNode.position - node.position;

            originalDir.Normalize();
            simDir.Normalize();

            // 기존 애니메이션 포즈 대비 물리적으로 회전한 변화량(Delta) 계산
            Quaternion deltaRot = Engine::Math::CreateRotationBetweenVectors(originalDir, simDir);

            // 해당 노드의 Original World Matrix에 Delta 회전을 곱하여 새로운 World Transform 산출
            Matrix newWorldMat = node.originalWorld;
            newWorldMat.Translation(Vec3::Zero);
            newWorldMat = Matrix::CreateFromQuaternion(deltaRot) * newWorldMat;
            newWorldMat.Translation(node.originalWorld.Translation());

            // 새로운 World Transform을 부모의 World Transform 역행렬과 곱하여 Local Transform으로 변환
            Matrix parentWorldInv = Matrix::Identity;
            if (node.parentNodeIndex != -1)
            {
                parentWorldInv = chain.nodes[node.parentNodeIndex].originalWorld.Invert();
            }
            else
            {
                int32 realParentBoneIndex = model->Get_BoneParentIndex(node.boneIndex);
                if (realParentBoneIndex != -1)
                {
                    const Matrix* parentCombined = model->Get_BoneMatrixPtr(realParentBoneIndex);
                    if (parentCombined)
                        parentWorldInv = ((*parentCombined) * ownerWorld).Invert();
                }
                else
                {
                    parentWorldInv = invOwnerWorld;
                }
            }

            Matrix newLocalMat = newWorldMat * parentWorldInv;
            
            // ModelCom의 로컬 매트릭스 덮어쓰기
            model->Set_BoneLocalMatrix(node.boneIndex, newLocalMat);
        }
    }
}

void ChainModifier::Evaluate_Modifier(ModelCom* model, float timeDelta)
{
    if (!_isActive || _strengthAlpha <= 0.f || !model)
        return;

    if (!_chainsInitialized)
        Initialize_Chains(model);

    // 포인터로 제공된 데이터를 역참조하여 가져옴 (가장 가볍고 빠름)
    Matrix ownerWorld = _desc.ownerWorldMatrix ? *(_desc.ownerWorldMatrix) : Matrix::Identity;
    Vec3 ownerVelocity = _desc.ownerVelocity ? *(_desc.ownerVelocity) : Vec3::Zero;
    
    Vec3 currentOwnerPos = ownerWorld.Translation();

    if (_isFirstUpdate)
    {
        _prevOwnerPos = currentOwnerPos;
        ResetPose(model);
        _isFirstUpdate = false;
        return;
    }

    if (Vec3::Distance(_prevOwnerPos, currentOwnerPos) > _desc.teleportDistance)
    {
        ResetPose(model);
    }
    _prevOwnerPos = currentOwnerPos;

    Capture_CurrentPose(model, ownerWorld);

    for (uint32 i = 0; i < _desc.solverIterations; ++i)
    {
        Solve_Verlet(model, timeDelta, ownerWorld, ownerVelocity);
    }

    Write_BackToModel(model, ownerWorld);
    
    // 변경된 모든 Local Matrix를 바탕으로 CombinedMatrix를 최상단부터 다시 재귀 계산
    model->Rebuild_BoneCombinedMatrices();
}

Shared<ChainModifier> ChainModifier::Create(const ChainModifierDesc* desc)
{
    Shared<ChainModifier> instance = MakeShared<ChainModifier>();
    if (FAILED(instance->Initialize(const_cast<ChainModifierDesc*>(desc))))
    {
        assert(false && "Failed to Create ChainModifier");
        return nullptr;
    }
    return instance;
}

NS_END
```

---

## 3. 클라이언트(캐릭터) 적용 예시 (`Client/Private/PW_Lune.cpp` 등)

`Ready_Modifier()` 함수 마지막에 아래와 같이 추가하시면 됩니다. 
(컨테이너 기반일 경우 `PartObject`가 가진 `_combinedWorldMatrix`의 주소값을 1회 넘겨주면 끝입니다.)

```cpp
#include "ChainModifier.h"

// PW_Lune::Ready_Modifier() 함수 내부 마지막:

ChainModifier::ChainModifierDesc modifierDesc{};
modifierDesc.modifierTag = L"ClothPhysics";

// 물성치 설정
modifierDesc.stiffness = 0.04f;
modifierDesc.damping = 0.55f;
modifierDesc.gravity = Vec3(0.f, -0.25f, 0.f);
modifierDesc.movementInertiaScale = 0.06f;

// 룬(Lune)의 옷 뼈대 그룹 등록
modifierDesc.groups = {
    { "Lune_V2_Scarf_01_01" },
    { "Lune_FrontBands_Root" },
    { "Lune_V3_Coat_Root" }
};

// =================================================================================
// 핵심: 람다 콜백 대신, 수명이 보장된 멤버 변수의 "메모리 주소"를 직접 연결 (가장 효율적)
// _bodyPlayer가 PartObject를 상속받는다면, 그 내부의 Combined 행렬 주소를 던져줍니다.
modifierDesc.ownerWorldMatrix = &_bodyPlayer->Get_CombinedWorldMatrix(); 

// 속도(바람 저항용)가 필요하다면 아래처럼 추가로 바인딩합니다. 필수는 아닙니다.
// modifierDesc.ownerVelocity = &_bodyPlayer->Get_Velocity_Or_Something();
// =================================================================================

if (auto modifier = ChainModifier::Create(&modifierDesc))
{
    _bodyPlayer->Get_Model()->Add_AnimModifier(modifier);
}
```