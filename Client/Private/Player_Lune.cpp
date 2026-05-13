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
