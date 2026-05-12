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
