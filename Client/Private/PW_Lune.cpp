#include "PW_Lune.h"

#include <ChainModifier.h>
#include <OffsetAnimationModifier.h>

#include "Body_Player.h"
#include "GameInstance.h"
#include "Bounding_OBB.h"
#include "ChainModifierPreset.h"
#include "ColliderCom.h"
#include "PlayableParts.h"
#include "SkillTreeCom.h"
#include "PlayerStatCom.h"
#include "MovementCom.h"
#include "TransformCom.h"

IMPLEMENT_REFLECTION(PW_Lune)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "PW_Lune";
    info.category = "Character";
    

    return true;
}

PW_Lune::PW_Lune(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: Player_WorldBase { device, context }
{
}

PW_Lune::PW_Lune(const PW_Lune& prototype)
	: Player_WorldBase{ prototype }
{

}

HRESULT PW_Lune::Initialize(void* arg)
{
    CHECK_FAILED(__super::Initialize(arg), E_FAIL);

    auto skilltreeCom = Get_Component<SkillTreeCom>();

    skilltreeCom->TryUnlock("Immolation");
    skilltreeCom->TryUnlock("IceGust");
    skilltreeCom->TryUnlock("Thunderfall");

    skilltreeCom->ForceUnlock("TerraQuake");
    skilltreeCom->ForceUnlock("CripplingTsunami");

     // 그래디언트
    skilltreeCom->ForceUnlock("SkyBreak");

    CHECK_FAILED(Ready_Modifier(), E_FAIL);

    return S_OK;
}

void PW_Lune::BeginPlay()
{
    Player_WorldBase::BeginPlay();

}

HRESULT PW_Lune::Ready_Modifier()
{
    auto* body = dynamic_cast<Body_Player*>(Find_PartObject(TEXT("Part_Body")));
    CHECK_NULL(body, E_FAIL);

    return Ready_BodyModifiers(body);
}

HRESULT PW_Lune::Ready_BodyModifiers(Body_Player* body)
{
    CHECK_NULL(body, E_FAIL);

        OffsetAnimationModifier::OffsetModifierDesc aimOffsetDesc{};
        aimOffsetDesc.baseModelCom = body->Get_Model().get();
        aimOffsetDesc.animName = "Lune_IdleFreeAim_LoopingWorld_Sweep";
        aimOffsetDesc.offsetBoneRanges = { { "spine_01", "hand_r" } };
        aimOffsetDesc.blendMode = AnimationModifier::ModifierBlendMode::ModifyAddictive;
        aimOffsetDesc.blendInTime = 0.25f;
        aimOffsetDesc.blendOutTime = 0.25f;
        aimOffsetDesc.responseCurve = 1.5f;
        aimOffsetDesc.modifierTag = L"AimOffset";
        if (auto modifier = OffsetAnimationModifier::Create(&aimOffsetDesc))
        {
            modifier->Set_Active(false);
            aimOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
        }
        else return E_FAIL;

        OffsetAnimationModifier::OffsetModifierDesc wOffsetDesc{};
        wOffsetDesc.baseModelCom = body->Get_Model().get();
        wOffsetDesc.animName = "Lune_Walk_F";
        wOffsetDesc.offsetBoneRanges = {
            {"pelvis", "ball_l"},  
            {"pelvis", "ball_r"},
        };
        wOffsetDesc.translationOnlyBones = { "pelvis" };
        wOffsetDesc.blendMode = AnimationModifier::ModifierBlendMode::ModifyReplace;
        wOffsetDesc.blendInTime = 0.25f;
        wOffsetDesc.blendOutTime = 0.25f;
        wOffsetDesc.responseCurve = 1.5f;
        wOffsetDesc.isDynamicPlay = true;
        wOffsetDesc.modifierTag = L"Walk_F";
        if (auto modifier = OffsetAnimationModifier::Create(&wOffsetDesc))
        {
            modifier->Set_Active(false);
            aimOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
        }
        else return E_FAIL;

        wOffsetDesc.animName = "Lune_Walk_B";
        wOffsetDesc.modifierTag = L"Walk_B";
        if (auto modifier = OffsetAnimationModifier::Create(&wOffsetDesc))
        {
            modifier->Set_Active(false);
            aimOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
        }
        else return E_FAIL;

        wOffsetDesc.animName = "Lune_Walk_LF";
        wOffsetDesc.modifierTag = L"Walk_L";
        if (auto modifier = OffsetAnimationModifier::Create(&wOffsetDesc))
        {
            modifier->Set_Active(false);
            aimOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
        }
        else return E_FAIL;

        wOffsetDesc.animName = "Lune_Walk_RF";
        wOffsetDesc.modifierTag = L"Walk_R";
        if (auto modifier = OffsetAnimationModifier::Create(&wOffsetDesc))
        {
            modifier->Set_Active(false);
            aimOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
        }
        else return E_FAIL;
        
        wOffsetDesc.animName = "Lune_Walk_LB";
        wOffsetDesc.modifierTag = L"Walk_LB";
        if (auto modifier = OffsetAnimationModifier::Create(&wOffsetDesc))
        {
            modifier->Set_Active(false);
            aimOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
        }
        else return E_FAIL;

        wOffsetDesc.animName = "Lune_Walk_RB";
        wOffsetDesc.modifierTag = L"Walk_RB";
        if (auto modifier = OffsetAnimationModifier::Create(&wOffsetDesc))
        {
            modifier->Set_Active(false);
            aimOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
        }
        else return E_FAIL;

        PoseFollowChainModifier::PoseFollowChainModifierDesc scarfDesc{};
        ChainModifierPreset::BuildPoseFollowScarfDesc(
            scarfDesc,
            Get_Transform().get(),
            PlayerCharacterType::Lune);

        if (auto modifier = PoseFollowChainModifier::Create(&scarfDesc))
        {
            body->Get_Model()->Add_AnimModifier(modifier);
        }

        ChainModifier::ChainModifierDesc armbandDesc{};
        ChainModifierPreset::BuildArmbandDesc(
            armbandDesc,
            Get_Transform().get(),
            PlayerCharacterType::Lune);

        if (auto modifier = ChainModifier::Create(&armbandDesc))
        {
            body->Get_Model()->Add_AnimModifier(modifier);
        }

        PoseFollowChainModifier::PoseFollowChainModifierDesc coatDesc{};
        ChainModifierPreset::BuildPoseFollowCoatDesc(
            coatDesc,
            Get_Transform().get(),
            PlayerCharacterType::Lune);

        if (auto modifier = PoseFollowChainModifier::Create(&coatDesc))
        {
            body->Get_Model()->Add_AnimModifier(modifier);
        }

    return S_OK;
}


HairChainPhysicsDesc PW_Lune::Get_HairChainPhysicsDesc() const
{
    HairChainPhysicsDesc desc{};

    desc.profile = HairPhysicsProfile::Lune;
    desc.bonePrefix = "Strand_";

    // Lune: 긴 뒷머리 + 앞머리 체인이 많아서 현재 기준값.
    desc.stiffness = 0.04f;
    desc.damping = 0.42f;
    desc.windScale = 0.08f;
    desc.gravity = Vec3(0.f, -0.45f, 0.f);

    desc.movementInertiaScale = 0.12f;
    desc.maxMovementInertia = 1.6f;
    desc.maxMovementOffset = 0.035f;

    desc.poseRestoreStrength = 0.18f;
    desc.maxBendAngleDegree = 28.f;

    desc.teleportDistance = 300.f;
    desc.solverIterations = 2;

    return desc;
}

Shared<PW_Lune> PW_Lune::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	auto instance = make_shared<PW_Lune>(device, context);

	if (FAILED(instance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : PW_Lune");
		return nullptr;
	}
	return instance;
}

Shared<GameObject> PW_Lune::Clone(void* arg)
{
	auto instance = make_shared<PW_Lune>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Cloned : PW_Lune");
		return nullptr;
	}
	return instance;
}

void PW_Lune::Free()
{
	__super::Free();

}
