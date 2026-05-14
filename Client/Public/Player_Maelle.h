#pragma once

#include "Client_Defines.h"
#include "Player.h"

NS_BEGIN(Client)

class Player_Maelle final : public Player
{
    GENERATED_GAMEOBJECT(Player_Maelle, ObjectType::Player)

public:
	Player_Maelle(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Player_Maelle(const Player_Maelle& prototype);
	virtual ~Player_Maelle() override = default;

protected:
    HairChainPhysicsDesc Get_HairChainPhysicsDesc() const override;

    const wchar_t* Get_BodyModelTag() const override { return L"Model_MaelleBody"; }
    const wchar_t* Get_HeadModelTag() const override { return L"Model_MaelleHead"; }
    const wchar_t* Get_HairModelTag() const override { return L"Model_MaelleHair"; }

public:
	static Shared<Player_Maelle> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* arg) override;
	virtual void Free() override;

};

NS_END

