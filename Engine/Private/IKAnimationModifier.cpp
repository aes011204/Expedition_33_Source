#include "pch.h"
#include "IKAnimationModifier.h"

#include "ModelCom.h"

HRESULT IKAnimationModifier::Initialize(const IKModifierDesc& IKModifierDesc)
{
    if (!IKModifierDesc.baseModelCom || _endBoneName.empty() || _rootBoneName.empty())
    {
        return E_FAIL;
    }

    int32 currentBoneIndex = IKModifierDesc.baseModelCom->Get_BoneIndex(_endBoneName);
    int32 InvRootBoneIndex = IKModifierDesc.baseModelCom->Get_BoneIndex(_rootBoneName);

    vector<int32> boneChain;
    while (currentBoneIndex != -1)
    {
        boneChain.push_back(currentBoneIndex);
        if (currentBoneIndex == InvRootBoneIndex)
        {   
            break;
        }
    }

    currentBoneIndex = IKModifierDesc.baseModelCom->Get_BoneParentIndex(currentBoneIndex);
    auto chainSize = boneChain.size();
    if (chainSize == 0)
    {
        return E_FAIL;
    }

    float totalWeight{ 0.f };
    for (uint32 i = 0; i < chainSize; ++i)
    {
        ModelBoneNode boneNode{};
        boneNode.boneIndex = boneChain[i];
        boneNode.boneWeight = static_cast<float>(chainSize - i) / totalWeight;
        _boneChain.push_back(boneNode);
    }

    return S_OK;
}

void IKAnimationModifier::Evaluate_Modifier(ModelCom* model, float timeDelta, float alpha)
{
    if (!_isActive || alpha <= 0.f || !model)
    {
        return;
    }

    float clampedYaw = clamp(_targetYaw, _limits.minYaw, _limits.maxYaw);
    float clampedPitch = clamp(_targetPitch, _limits.minPitch, _limits.maxPitch);

    for (const auto& bone : _boneChain)
    {
        float applyYaw = clampedYaw * bone.boneWeight * alpha;
        float applyPitch = clampedPitch * bone.boneWeight * alpha;

        Matrix ikMatrix = Matrix::CreateFromYawPitchRoll(applyYaw, applyPitch, 0.f);
        Matrix localMatrix = model->Get_BoneLocalMatrix(bone.boneIndex);
        localMatrix = ikMatrix * localMatrix;

        model->Set_BoneLocalMatrix(bone.boneIndex, localMatrix);
    }
}

void IKAnimationModifier::Set_AimAngles(float yaw, float pitch)
{
    _targetYaw = yaw;
    _targetPitch = pitch;
}

void IKAnimationModifier::Setup_BoneChain(const string& endBoneName, const string& InvRootBoneName)
{
    _endBoneName = endBoneName;
    _rootBoneName = InvRootBoneName;
}

void IKAnimationModifier::Set_Limits(const ModelBoneRotationLimits& limits)
{
    _limits = limits;
}

Shared<IKAnimationModifier> IKAnimationModifier::Create(const IKModifierDesc& IKModifierDesc)
{
    auto instance = make_shared<IKAnimationModifier>();

    CHECK_FAILED(instance->Initialize(IKModifierDesc), nullptr);

    return instance;
}
