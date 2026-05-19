#include "PW_Gustave.h"
#include "GameInstance.h"
#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "PlayableParts.h"
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
    CHECK_FAILED(__super::Initialize(arg), E_FAIL)

    auto skillTreeCom = Get_Component<SkillTreeCom>();

    skillTreeCom->TryUnlock("LumiereAssault");
    skillTreeCom->TryUnlock("UnleashedCharge");


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



