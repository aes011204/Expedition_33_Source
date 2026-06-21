#include "pch.h"
#include "Hair_Player.h"
#include "GameInstance.h"
#include "Material.h"
#include "Material_Types.h"
#include "Player_WorldBase.h"
#include "ModelCom.h"
#include "Debug_Profile.h"
#include "AnimComputeRuntime.h"
#include "EffectCom.h"
#include "PoseFollowChainModifier.h"
#include "ChainModifier.h"
#include "ChainModifierPreset.h"

namespace
{
    Client::PlayerCharacterType ToPlayerCharacterType(Client::HairPhysicsProfile profile)
    {
        switch (profile)
        {
        case Client::HairPhysicsProfile::Gustave:
            return Client::PlayerCharacterType::Gustave;
        case Client::HairPhysicsProfile::Lune:
            return Client::PlayerCharacterType::Lune;
        case Client::HairPhysicsProfile::Maelle:
            return Client::PlayerCharacterType::Maelle;
        default:
            return Client::PlayerCharacterType::None;
        }
    }
}

Hair_Player::Hair_Player(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : PlayableParts{ device, context } {
}

Hair_Player::Hair_Player(const Hair_Player& prototype)
    : PlayableParts{ prototype } {
}

HRESULT Hair_Player::Initialize_Prototype()
{
    return PlayableParts::Initialize_Prototype();
}

HRESULT Hair_Player::Initialize(void* arg)
{
    if (FAILED(PlayableParts::Initialize(arg)))
    {
        LOG_ERROR("[Hair_Player] : Failed to Initialize");
        return S_OK;
    }

    if (const HAIR_PLAYER_DESC* desc = static_cast<const HAIR_PLAYER_DESC*>(arg))
    {
        // PW_* 캐릭터에서 넘긴 hair physics 값을 실제 파츠에 저장
        _hairPhysicsDesc = desc->hairPhysicsDesc;
        _hairCB = desc->hairCB;
        _usePoseFollowModifier = desc->usePoseFollowModifier;
    }

    _offsetMatrix = Matrix::Identity;

    //Vec3 hairOffset = Vec3(17.61f, 0.0f, -2.765f);
    //Vec3 hairOffset = Vec3(17.61f, -2.765f, 0.f);

    //_transformCom->Set_Position(hairOffset);

    return S_OK;
}

void Hair_Player::BeginPlay()
{
    PlayableParts::BeginPlay();

}

void Hair_Player::Priority_Update(float timeDelta)
{
}

void Hair_Player::Update(float timeDelta)
{
    PlayableParts::Update(timeDelta);

    ENGINE_PROFILE_SCOPE_DETAIL("Player::Hair::Update");

    if (_modelCom == nullptr)
        return;

    Update_CombinedWorldMatrix(_transformCom->Get_WorldMatrix());

    if (_usePoseFollowModifier)
    {
        if (!_hairPoseFollowModifierReady)
        {
            Shared<TransformCom> ownerTransform{};
            if (const Shared<GameObject> owner = Get_Owner())
                ownerTransform = owner->Get_Transform();

            const PlayerCharacterType characterType = ToPlayerCharacterType(_hairPhysicsDesc.profile);
            bool modifierAdded = false;
            auto addModifier = [&](const ChainModifier::ChainModifierDesc& desc)
            {
                if (desc.groups.empty())
                {
                    return;
                }

                if (auto modifier = ChainModifier::Create(&desc))
                {
                    _modelCom->Add_AnimModifier(modifier);
                    modifierAdded = true;
                }
            };

            if (characterType == PlayerCharacterType::Maelle)
            {
                ChainModifier::ChainModifierDesc backHairDesc{};
                ChainModifierPreset::BuildBindPoseHairDesc(
                    backHairDesc,
                    ownerTransform.get(),
                    characterType,
                    L"BindPose_Hair_Back");

                decltype(backHairDesc.groups) backGroups{};
                for (const auto& group : backHairDesc.groups)
                {
                    if (group.rootBoneName == "Strand_0_1")
                    {
                        backGroups.push_back(group);
                    }
                }
                backHairDesc.groups = std::move(backGroups);
                addModifier(backHairDesc);

                ChainModifier::ChainModifierDesc frontHairDesc{};
                ChainModifierPreset::BuildBindPoseHairDesc(
                    frontHairDesc,
                    ownerTransform.get(),
                    characterType,
                    L"BindPose_Hair_Front");

                decltype(frontHairDesc.groups) frontGroups{};
                for (const auto& group : frontHairDesc.groups)
                {
                    if (group.rootBoneName == "Strand_5_1" || group.rootBoneName == "Strand_6_1")
                    {
                        auto frontGroup = group;
                        frontGroup.stiffnessMul = 1.025f;
                        frontGroup.dampingMul = 1.010f;
                        frontGroup.movementInertiaMul = 1.25f;
                        frontGroup.poseRestoreMul = 1.05f;
                        frontGroup.maxBendAngleMul = 0.90f;
                        frontGroups.push_back(frontGroup);
                    }
                }
                frontHairDesc.groups = std::move(frontGroups);
                frontHairDesc.shape.stiffness = 0.805f;
                frontHairDesc.shape.tipStiffnessScale = 1.000f;
                frontHairDesc.shape.poseRestoreStrength = 0.020f;
                frontHairDesc.shape.tipPoseRestoreScale = 0.540f;
                frontHairDesc.shape.maxBendAngleDegree = 28.f;
                frontHairDesc.shape.tipMaxBendAngleScale = 1.000f;
                frontHairDesc.shape.surfaceConstraintStrength = 0.f;
                frontHairDesc.shape.rootOutputAlpha = 0.070f;
                frontHairDesc.shape.tipOutputAlpha = 1.f;
                frontHairDesc.shape.pinRoot = false;

                frontHairDesc.motion.damping = 0.830f;
                frontHairDesc.motion.tipDampingScale = 1.420f;
                frontHairDesc.motion.movementInertiaScale = 0.125f;
                frontHairDesc.motion.ownerMovementInertiaScale = 0.875f;
                frontHairDesc.motion.tipMovementInertiaScale = 1.220f;
                frontHairDesc.motion.maxMovementOffset = 0.018f;

                frontHairDesc.runtime.gravity = Vec3(0.f, -0.6f, 0.f);
                frontHairDesc.runtime.windScale = 0.f;
                frontHairDesc.runtime.windStartRatio = -1.f;
                frontHairDesc.runtime.windWeightScale = -1.f;
                frontHairDesc.runtime.constraintDeadzone = 0.010f;
                frontHairDesc.runtime.resetDistance = 3.f;
                frontHairDesc.runtime.targetFrameRate = 60.f;

                frontHairDesc.collision.enableCollision = false;
                frontHairDesc.collision.particleCollisionRadius = 0.f;
                addModifier(frontHairDesc);
            }
            else
            {
                ChainModifier::ChainModifierDesc hairDesc{};
                ChainModifierPreset::BuildBindPoseHairDesc(
                    hairDesc,
                    ownerTransform.get(),
                    characterType);

                addModifier(hairDesc);
            }

            _hairPoseFollowModifierReady = modifierAdded;
        }

        if (_hairPoseFollowModifierReady)
        {
            if (_animComputeRuntime != nullptr)
            {
                CHECK_FAILED(_animComputeRuntime->Update_Frame(timeDelta));
            }
            else
            {
                _modelCom->Play_Animation(timeDelta);
                CHECK_FAILED(_modelCom->Prepare_BoneMatrixBuffers());
            }
        }

        _wasPhysicsSkipped = false;
        return;
    }

    if (!_hairPhysicsReady)
    {
        _hairPhysics.Initialize(_modelCom.get(), _hairPhysicsDesc);
        _hairPhysicsReady = _hairPhysics.Is_Built();
    }

    Vec3 ownerVelocity = Vec3::Zero;
    if (const Shared<GameObject> owner = Get_Owner())
    {
        if (const Player_WorldBase* player = dynamic_cast<const Player_WorldBase*>(owner.get()))
            ownerVelocity = player->Get_MovementVelocity();
    }

    if (_hairPhysicsReady)
    {
        // 카메라 거리 기반 추가
        const Vec4* camPos = GAME->Get_CamPosition();
        float dist = 0.f;
        if (camPos)
        {
            const Vec3 hairPos(_combinedWorldMatrix._41, _combinedWorldMatrix._42, _combinedWorldMatrix._43);
            dist = Vec3::Distance(hairPos, Vec3(camPos->x, camPos->y, camPos->z));
        }

        bool doSimulate = false;


        if (dist <= 8.f)
        {
            // 매 프레임 simulate
            doSimulate = true;
        }
        else if (dist <= 18.f)
        {
            _physicsTickAccumulator += timeDelta;
            if (_physicsTickAccumulator >= _physicsTickIntervalSec)
            {
                _physicsTickAccumulator -= _physicsTickIntervalSec;
                doSimulate = true;
            }
        }
        else
        {
            // far: simulate 스킵, bone buffer는 유지
            _physicsTickAccumulator = 0.f;
        }
        if (doSimulate)
        {
            ENGINE_PROFILE_SCOPE_DETAIL("Player::Hair::Physics");
            _hairPhysics.Simulate(_modelCom.get(), timeDelta, _combinedWorldMatrix, ownerVelocity);
            _wasPhysicsSkipped = false;
        }
        else
        {
            _wasPhysicsSkipped = true;
        }
    }
    // physics를 실제로 실행한 프레임에만 bone buffer 재업로드
    if (!_wasPhysicsSkipped)
        CHECK_FAILED(_animComputeRuntime->Prepare_BoneMatrixBuffers());
}

void Hair_Player::Late_Update(float timeDelta)
{
    Shared<GameObject> owner = Get_Owner();
    if (!owner)
    {
        return;
    }

    if (Casts_DynamicShadow() && Is_Visible())
        GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());

    if (!owner->Is_Visible())
        return;

    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
    GAME->Add_RenderGroup(RenderGroup::Blend, GetSharedPtr<GameObject>());
}

HRESULT Hair_Player::Render()
{
    ENGINE_PROFILE_SCOPE_DETAIL("Player::Hair::Render");

    if (FAILED(Bind_ShaderResources()))
        return E_FAIL;

    CHECK_FAILED_THROTTLED(_animComputeRuntime->Prepare_BoneMatrixBuffers(), 60, E_FAIL);

    const uint32 iNumMeshes = static_cast<uint32>(_modelCom->Get_NumMeshes());
    const uint32 supportFeatureMask = Build_DrawFeatureSupportMask(*this);

    for (uint32 i = 0; i < iNumMeshes; i++)
    {
        const uint32 materialIndex = _modelCom->Get_MeshMaterialIndex(i);
        const Shared<Material> material = _modelCom->Get_Material(materialIndex);

        if (FAILED(_shaderCom->Bind_MaterialTextures(*material)))
            continue;

        if (FAILED(_shaderCom->Bind_CBufferData(material->Get_CB())))
            return E_FAIL;

        CHECK_FAILED_THROTTLED(_animComputeRuntime->Bind_PreparedBoneMatrices(_shaderCom.get(), "g_BoneMatricesBuffer", i), 60, E_FAIL);

        CHECK_FAILED_THROTTLED(EffectCom::Bind_DrawFeatures(Get_Owner(), _shaderCom.get(), supportFeatureMask), 60, E_FAIL);

        uint32 passIndex = 0;
        if (GAME->Get_CurrentRenderGroup() == RenderGroup::Blend)
            passIndex = static_cast<uint32>(VtxAnimMeshPass::HairBlend);
        else 
            passIndex = material->Get_PassIndex();

        if (FAILED(_shaderCom->Begin(passIndex)))
            return E_FAIL;

        // 플레이어 파츠가 그려진 픽셀에 stencil=1을 남겨 Background UI가 이 영역을 피하게 한다.
        const bool depthWrite = passIndex != static_cast<uint32>(VtxAnimMeshPass::HairBlend);
        CHECK_FAILED_THROTTLED(GAME->Bind_PlayerStencilWriteState(depthWrite), 60, E_FAIL);

        _modelCom->Render(i);
    }

    return S_OK;
}

HRESULT Hair_Player::Render_Shadow()
{
    return PlayableParts::Render_Shadow();
}

Shared<Hair_Player> Hair_Player::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Hair_Player>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("[Hair_Player] : Failed to Initialize Prototype");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> Hair_Player::Clone(void* arg)
{
    auto clone = make_shared<Hair_Player>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("[Hair_Player] : Failed to Initialize");
        return nullptr;
    }

    return clone;
}

void Hair_Player::Free()
{
    __super::Free();
}
