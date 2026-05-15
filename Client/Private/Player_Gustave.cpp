#include "Player_Gustave.h"
#include "GameInstance.h"
#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "PlayableParts.h"

IMPLEMENT_REFLECTION(Player_Gustave)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "Player_Gustave";
    info.category = "Character";
    

    return true;
}

Player_Gustave::Player_Gustave(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: Player { device, context }
{
}

Player_Gustave::Player_Gustave(const Player_Gustave& prototype)
	: Player{ prototype }
{

}

HairChainPhysicsDesc Player_Gustave::Get_HairChainPhysicsDesc() const
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

Shared<Player_Gustave> Player_Gustave::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
	auto instance = make_shared<Player_Gustave>(device, context);

	if (FAILED(instance->Initialize_Prototype()))
	{
		MSG_BOX("Failed to Created : Player_Gustave");
		return nullptr;
	}
	return instance;
}

Shared<GameObject> Player_Gustave::Clone(void* arg)
{
	auto instance = make_shared<Player_Gustave>(*this);

	if (FAILED(instance->Initialize(arg)))
	{
		MSG_BOX("Failed to Cloned : Player_Gustave");
		return nullptr;
	}
	return instance;
}

void Player_Gustave::Free()
{
	__super::Free();

}



