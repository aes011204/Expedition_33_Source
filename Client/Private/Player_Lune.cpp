#include "Player_Lune.h"
#include "GameInstance.h"
#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "PlayableParts.h"

IMPLEMENT_REFLECTION(Player_Lune)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "Player_Lune";
    info.category = "Character";
    

    return true;
}

Player_Lune::Player_Lune(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: Player { device, context }
{
}

Player_Lune::Player_Lune(const Player_Lune& prototype)
	: Player{ prototype }
{

}

HairChainPhysicsDesc Player_Lune::Get_HairChainPhysicsDesc() const
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

Shared<Player_Lune> Player_Lune::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	auto instance = make_shared<Player_Lune>(device, context);

	if (FAILED(instance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Player_Lune");
		return nullptr;
	}
	return instance;
}

Shared<GameObject> Player_Lune::Clone(void* arg)
{
	auto instance = make_shared<Player_Lune>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Cloned : Player_Lune");
		return nullptr;
	}
	return instance;
}

void Player_Lune::Free()
{
	__super::Free();

}
