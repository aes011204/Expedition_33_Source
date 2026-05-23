#include "PW_Maelle.h"
#include "GameInstance.h"
#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "PlayableParts.h"
#include "PlayerStatCom.h"
#include "SkillTreeCom.h"

IMPLEMENT_REFLECTION(PW_Maelle)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "PW_Maelle";
    info.category = "Character";
    

    return true;
}

PW_Maelle::PW_Maelle(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: Player_WorldBase { device, context }
{
}

PW_Maelle::PW_Maelle(const PW_Maelle& prototype)
	: Player_WorldBase{ prototype }
{

}

HRESULT PW_Maelle::Initialize(void* arg)
{
    CHECK_FAILED(__super::Initialize(arg), E_FAIL)

    auto skilltreeCom = Get_Component<SkillTreeCom>();

    skilltreeCom->TryUnlock("OffensiveSwitch");
    skilltreeCom->TryUnlock("Percee");
    skilltreeCom->TryUnlock("Spark");

    return S_OK;
}

void PW_Maelle::BeginPlay()
{
    Player_WorldBase::BeginPlay();

    _combatStatCom->Get_StatInfo().maxAp = 9;   
    _combatStatCom->Get_PlayerRuntimeStat().CurMaxAp = 9;
    _combatStatCom->Get_PlayerRuntimeStat().CurAp = 4;
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
