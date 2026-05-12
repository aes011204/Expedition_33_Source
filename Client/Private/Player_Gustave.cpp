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



