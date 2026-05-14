#pragma once

#include "Client_Defines.h"
#include "Player.h"

NS_BEGIN(Client)

class Player_Gustave final : public Player
{
    GENERATED_GAMEOBJECT(Player_Gustave, ObjectType::Player)

public:
	Player_Gustave(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Player_Gustave(const Player_Gustave& prototype);
	virtual ~Player_Gustave() override = default;

protected:
    HairChainPhysicsDesc Get_HairChainPhysicsDesc() const override;

    const wchar_t* Get_BodyModelTag() const override { return L"Model_GustaveBody"; }
    const wchar_t* Get_HeadModelTag() const override { return L"Model_GustaveHead"; }
    const wchar_t* Get_HairModelTag() const override { return L"Model_GustaveHair"; }

public:
	static Shared<Player_Gustave> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* arg) override;
	virtual void Free() override;

};

NS_END

