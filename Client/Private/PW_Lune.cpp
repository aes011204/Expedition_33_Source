#include "PW_Lune.h"
#include "GameInstance.h"
#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "PlayableParts.h"
#include "SkillTreeCom.h"
#include "PlayerStatCom.h"

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
    CHECK_FAILED(__super::Initialize(arg), E_FAIL)

    auto skilltreeCom = Get_Component<SkillTreeCom>();

    skilltreeCom->TryUnlock("Immolation");
    skilltreeCom->TryUnlock("IceGust");

    return S_OK;
}

void PW_Lune::BeginPlay()
{
    Player_WorldBase::BeginPlay();

    // PW에서 세팅해줘야, 전투 시작 시 PB한테 전달된다.
    _combatStatCom->Get_StatInfo().maxAp = 9;
    _combatStatCom->Get_PlayerRuntimeStat().CurMaxAp = 9;
    _combatStatCom->Get_PlayerRuntimeStat().CurAp = 3;
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
