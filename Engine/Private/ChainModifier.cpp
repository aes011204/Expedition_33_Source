#include "ChainModifier.h"
#include "ModelCom.h"
#include "Bone.h"
#include "GameObject.h"
#include "Helper_Math.h"
#include "TransformCom.h"
#include "MovementCom.h"

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
    Matrix ownerWorld = _desc.ownerTransform ? _desc.ownerTransform->Get_WorldMatrix() : Matrix::Identity;

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
    movementInertia.y *= _desc.inertiaY;

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

void ChainModifier::On_ActiveChanged(ModelCom* model, bool isActive)
{
    if (isActive && model)
    {
        _isFirstUpdate = true;
        if (_chainsInitialized)
        {
            ResetPose(model);
        }
    }
}

void ChainModifier::Evaluate_Modifier(ModelCom* model, float timeDelta)
{
    if (!_isActive || _strengthAlpha <= 0.f || !model)
        return;

    if (!_chainsInitialized)
        Initialize_Chains(model);

    // 컴포넌트 포인터로 가져옴
    Matrix ownerWorld = _desc.ownerTransform ? _desc.ownerTransform->Get_WorldMatrix() : Matrix::Identity;
    Vec3 ownerVelocity = _desc.ownerMovement ? _desc.ownerMovement->Get_Velocity() : Vec3::Zero;
    
    Vec3 currentOwnerPos = ownerWorld.Translation();

    if (_isFirstUpdate)
    {
        _prevOwnerPos = currentOwnerPos;
        ResetPose(model);
        _isFirstUpdate = false;
        return;
    }

    if (Vec3::Distance(_prevOwnerPos, currentOwnerPos) > _desc.resetDistance)
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
    Shared<ChainModifier> instance = make_shared<ChainModifier>();

    if (FAILED(instance->Initialize(const_cast<ChainModifierDesc*>(desc))))
    {
        assert(false && "Failed to Create ChainModifier");
        return nullptr;
    }

    return instance;
}

void ChainModifier::Free()
{
    AnimationModifier::Free();
}

NS_END
