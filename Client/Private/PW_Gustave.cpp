#include "PW_Gustave.h"

#include <OffsetAnimationModifier.h>

#include "Body_Player.h"
#include "GameInstance.h"
#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "PlayableParts.h"
#include "PlayerStatCom.h"
#include "SkillTreeCom.h"

IMPLEMENT_REFLECTION(PW_Gustave)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "PW_Gustave";
    info.category = "Character";
    

    return true;
}

PW_Gustave::PW_Gustave(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: Player_WorldBase { device, context }
{
}

PW_Gustave::PW_Gustave(const PW_Gustave& prototype)
	: Player_WorldBase{ prototype }
{

}

HRESULT PW_Gustave::Initialize(void* arg)
{
    CHECK_FAILED(__super::Initialize(arg), E_FAIL);

    auto skillTreeCom = Get_Component<SkillTreeCom>();

    skillTreeCom->TryUnlock("LumiereAssault");
    skillTreeCom->TryUnlock("UnleashedCharge");
    skillTreeCom->TryUnlock("MarkingShot");
    skillTreeCom->ForceUnlock("Shatter");
    skillTreeCom->ForceUnlock("FromFire");
    skillTreeCom->ForceUnlock("StrikeStorm");
    skillTreeCom->ForceUnlock("PerfectRecovery");

    CHECK_FAILED(Ready_Modifier(), E_FAIL);

    return S_OK; 
}

void PW_Gustave::BeginPlay()
{
    Player_WorldBase::BeginPlay();

}

HRESULT PW_Gustave::Ready_Modifier()
{
    if (auto body = dynamic_cast<Body_Player*>(Find_PartObject(TEXT("Part_Body"))))
    {
        OffsetAnimationModifier::OffsetModifierDesc aimOffsetDesc{};
        aimOffsetDesc.baseModelCom = body->Get_Model().get();
        aimOffsetDesc.animName = "Gustave_IdleFreeAim_LoopingWorld_Sweep";
        aimOffsetDesc.offsetBoneRanges = { { "spine_01", "hand_r" } };
        aimOffsetDesc.blendMode = AnimationModifier::ModifierBlendMode::ModifyAddictive;
        aimOffsetDesc.blendInTime = 0.75f;
        aimOffsetDesc.blendOutTime = 0.75f;
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
        wOffsetDesc.animName = "Gustave_Walk_F";
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

        wOffsetDesc.animName = "Gustave_Walk_B";
        wOffsetDesc.modifierTag = L"Walk_B";
        if (auto modifier = OffsetAnimationModifier::Create(&wOffsetDesc))
        {
            modifier->Set_Active(false);
            aimOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
        }
        else return E_FAIL;

        wOffsetDesc.animName = "Gustave_Walk_LF";
        wOffsetDesc.modifierTag = L"Walk_L";
        if (auto modifier = OffsetAnimationModifier::Create(&wOffsetDesc))
        {
            modifier->Set_Active(false);
            wOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
        }
        else return E_FAIL;

        wOffsetDesc.animName = "Gustave_Walk_RF";
        wOffsetDesc.modifierTag = L"Walk_R";
        if (auto modifier = OffsetAnimationModifier::Create(&wOffsetDesc))
        {
            modifier->Set_Active(false);
            wOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
        }
        else return E_FAIL;
        
        wOffsetDesc.animName = "Gustave_Walk_LB";
        wOffsetDesc.modifierTag = L"Walk_LB";
        if (auto modifier = OffsetAnimationModifier::Create(&wOffsetDesc))
        {
            modifier->Set_Active(false);
            wOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
        }
        else return E_FAIL;

        wOffsetDesc.animName = "Gustave_Walk_RB";
        wOffsetDesc.modifierTag = L"Walk_RB";
        if (auto modifier = OffsetAnimationModifier::Create(&wOffsetDesc))
        {
            modifier->Set_Active(false);
            wOffsetDesc.baseModelCom->Add_AnimModifier(modifier);
        }
        else return E_FAIL;
    }
    else
        return E_FAIL;

    return S_OK;
}

HairChainPhysicsDesc PW_Gustave::Get_HairChainPhysicsDesc() const
{
    HairChainPhysicsDesc desc{};

    desc.profile = HairPhysicsProfile::Gustave;
    desc.bonePrefix = "Strand_";

    // Gustave: 짧은 머리 체인이 많아서 과하게 찰랑이면 어색함.
    desc.stiffness = 0.065f;
    desc.damping = 0.55f;
    desc.windScale = 0.04f;
    desc.gravity = Vec3(0.f, -0.5f, 0.f);

    desc.movementInertiaScale = 0.065f;
    desc.maxMovementInertia = 0.9f;
    desc.maxMovementOffset = 0.02f;

    desc.poseRestoreStrength = 0.26f;
    desc.maxBendAngleDegree = 18.f;

    desc.teleportDistance = 300.f;
    desc.solverIterations = 2;

    return desc;
}

Shared<PW_Gustave> PW_Gustave::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	auto instance = make_shared<PW_Gustave>(device, context);

	if (FAILED(instance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : PW_Gustave");
		return nullptr;
	}
	return instance;
}

Shared<GameObject> PW_Gustave::Clone(void* arg)
{
	auto instance = make_shared<PW_Gustave>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Cloned : PW_Gustave");
		return nullptr;
	}
	return instance;
}

void PW_Gustave::Free()
{
	__super::Free();
        
}



