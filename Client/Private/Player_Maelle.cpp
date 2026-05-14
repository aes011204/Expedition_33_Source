#include "Player_Maelle.h"
#include "GameInstance.h"
#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "PlayableParts.h"

IMPLEMENT_REFLECTION(Player_Maelle)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "Player_Maelle";
    info.category = "Character";
    

    return true;
}

Player_Maelle::Player_Maelle(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: Player { device, context }
{
}

Player_Maelle::Player_Maelle(const Player_Maelle& prototype)
	: Player{ prototype }
{

}

HairChainPhysicsDesc Player_Maelle::Get_HairChainPhysicsDesc() const
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

Shared<Player_Maelle> Player_Maelle::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	auto instance = make_shared<Player_Maelle>(device, context);

	if (FAILED(instance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Player_Maelle");
		return nullptr;
	}
	return instance;
}

Shared<GameObject> Player_Maelle::Clone(void* arg)
{
	auto instance = make_shared<Player_Maelle>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Cloned : Player_Maelle");
		return nullptr;
	}
	return instance;
}

void Player_Maelle::Free()
{
	__super::Free();

}
