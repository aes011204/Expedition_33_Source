#include "PW_Maelle.h"

#include <OffsetAnimationModifier.h>

#include "Body_Player.h"
#include "GameInstance.h"
#include "Bounding_OBB.h"
#include "ChainModifierPreset.h"
#include "ColliderCom.h"
#include "PlayableParts.h"
#include "PlayerStatCom.h"
#include "SkillTreeCom.h"
#include "MovementCom.h"
#include "TransformCom.h"

IMPLEMENT_REFLECTION(PW_Maelle)
{
	auto& info = GetStaticReflectionInfo();
	info.properties.clear();

	info.displayName = "PW_Maelle";
	info.category = "Character";

	PROPERTY_FLOAT("Parkour Low Probe Distance", _parkourLowProbeDistance, 0.01f, 1.f, 0.005f);
	PROPERTY_FLOAT("Parkour High Probe Distance", _parkourHighProbeDistance, 0.01f, 1.f, 0.005f);
	PROPERTY_VEC3("Parkour Low Probe Center", _parkourLowProbeCenter, 0.01f);
	PROPERTY_VEC3("Parkour High Probe Center", _parkourHighProbeCenter, 0.01f);

	return true;
}

PW_Maelle::PW_Maelle(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: Player_WorldBase{ device, context }
{
}

PW_Maelle::PW_Maelle(const PW_Maelle& prototype)
	: Player_WorldBase{ prototype }
{

}

HRESULT PW_Maelle::Initialize(void* arg)
{
    CHECK_FAILED(__super::Initialize(arg), E_FAIL);

	auto skilltreeCom = Get_Component<SkillTreeCom>();

    // 공격전환 -> 불꽃으로 변경
	skilltreeCom->ForceUnlock("Spark");
	skilltreeCom->ForceUnlock("Percee");
    skilltreeCom->ForceUnlock("FleureFury");

	//skilltreeCom->ForceUnlock("FireRise");
    //skilltreeCom->ForceUnlock("SwordBallet");
    //skilltreeCom->ForceUnlock("Stendhal");

    // 열분해는 그래디언트로
    //skilltreeCom->ForceUnlock("Pyrolyse");


	CHECK_FAILED(Ready_Modifier(), E_FAIL);

	return S_OK;
}

void PW_Maelle::BeginPlay()
{
	Player_WorldBase::BeginPlay();
}

HRESULT PW_Maelle::Ready_Modifier()
{
	auto* body = dynamic_cast<Body_Player*>(Find_PartObject(TEXT("Part_Body")));
	CHECK_NULL(body, E_FAIL);

	return Ready_BodyModifiers(body);
}

HRESULT PW_Maelle::Ready_BodyModifiers(Body_Player* body)
{
	CHECK_NULL(body, E_FAIL);

		OffsetAnimationModifier::OffsetModifierDesc aimOffsetDesc{};
		aimOffsetDesc.baseModelCom = body->Get_Model().get();
		aimOffsetDesc.animName = "Maelle_IdleFreeAim_LoopingWorld_Sweep";
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
		wOffsetDesc.animName = "Maelle_Walk_F";
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
		wOffsetDesc.playRate = 1.f;
		wOffsetDesc.modifierTag = L"Walk_F";
		if (auto modifier = OffsetAnimationModifier::Create(&wOffsetDesc))
		{
			modifier->Set_Active(false);
			aimOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
		}
		else return E_FAIL;

		wOffsetDesc.animName = "Maelle_Walk_B";
		wOffsetDesc.modifierTag = L"Walk_B";
		if (auto modifier = OffsetAnimationModifier::Create(&wOffsetDesc))
		{
			modifier->Set_Active(false);
			aimOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
		}
		else return E_FAIL;

		wOffsetDesc.animName = "Maelle_Walk_LF";
		wOffsetDesc.modifierTag = L"Walk_L";
		if (auto modifier = OffsetAnimationModifier::Create(&wOffsetDesc))
		{
			modifier->Set_Active(false);
			aimOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
		}
		else return E_FAIL;

		wOffsetDesc.animName = "Maelle_Walk_RF";
		wOffsetDesc.modifierTag = L"Walk_R";
		if (auto modifier = OffsetAnimationModifier::Create(&wOffsetDesc))
		{
			modifier->Set_Active(false);
			aimOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
		}
		else return E_FAIL;

		wOffsetDesc.animName = "Maelle_Walk_LB";
		wOffsetDesc.modifierTag = L"Walk_LB";
		if (auto modifier = OffsetAnimationModifier::Create(&wOffsetDesc))
		{
			modifier->Set_Active(false);
			aimOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
		}
		else return E_FAIL;

		wOffsetDesc.animName = "Maelle_Walk_RB";
		wOffsetDesc.modifierTag = L"Walk_RB";
		if (auto modifier = OffsetAnimationModifier::Create(&wOffsetDesc))
		{
			modifier->Set_Active(false);
			aimOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
		}
		else return E_FAIL;

		// --- PoseFollowChainModifier for Scarf ---
		PoseFollowChainModifier::PoseFollowChainModifierDesc scarfDesc{};
		ChainModifierPreset::BuildPoseFollowScarfDesc(
			scarfDesc,
			Get_Transform().get(),
			PlayerCharacterType::Maelle);

		if (auto modifier = PoseFollowChainModifier::Create(&scarfDesc))
		{
			body->Get_Model()->Add_AnimModifier(modifier);
		}

		// --- ChainModifier for Armband ---
		ChainModifier::ChainModifierDesc armbandDesc{};
		ChainModifierPreset::BuildArmbandDesc(
			armbandDesc,
			Get_Transform().get(),
			PlayerCharacterType::Maelle);

		if (auto armbandModifier = ChainModifier::Create(&armbandDesc))
		{
			body->Get_Model()->Add_AnimModifier(armbandModifier);
		}

		PoseFollowChainModifier::PoseFollowChainModifierDesc coatDesc{};
		ChainModifierPreset::BuildPoseFollowCoatDesc(
			coatDesc,
			Get_Transform().get(),
			PlayerCharacterType::Maelle);

		if (auto coatModifier = PoseFollowChainModifier::Create(&coatDesc))
		{
			body->Get_Model()->Add_AnimModifier(coatModifier);
		}

	return S_OK;
}

HairChainPhysicsDesc PW_Maelle::Get_HairChainPhysicsDesc() const
{
	HairChainPhysicsDesc desc{};

	desc.profile = HairPhysicsProfile::Maelle;
	desc.bonePrefix = "Strand_";

	// Maelle: Strand_0_1 total 46, Strand_5/6도 길어서 Lune보다 더 흐르게.
	desc.stiffness = 0.035f;
	desc.damping = 0.38f;
	desc.windScale = 0.12f;
	desc.gravity = Vec3(0.f, -0.35f, 0.f);

	desc.movementInertiaScale = 0.12f;
	desc.maxMovementInertia = 1.6f;
	desc.maxMovementOffset = 0.045f;

	desc.poseRestoreStrength = 0.15f;
	desc.maxBendAngleDegree = 34.f;

	desc.teleportDistance = 300.f;
	desc.solverIterations = 3;

	return desc;
}

Shared<PW_Maelle> PW_Maelle::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	auto instance = make_shared<PW_Maelle>(device, context);

	if (FAILED(instance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : PW_Maelle");
		return nullptr;
	}
	return instance;
}

Shared<GameObject> PW_Maelle::Clone(void* arg)
{
	auto instance = make_shared<PW_Maelle>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Cloned : PW_Maelle");
		return nullptr;
	}
	return instance;
}

void PW_Maelle::Free()
{
	__super::Free();

}
