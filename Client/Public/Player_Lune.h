#pragma once

#include "Client_Defines.h"
#include "Player.h"

NS_BEGIN(Client)

class Player_Lune final : public Player
{
    GENERATED_GAMEOBJECT(Player_Lune, ObjectType::Player)

public:
	Player_Lune(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Player_Lune(const Player_Lune& prototype);
	virtual ~Player_Lune() override = default;

protected:
    const wchar_t* Get_BodyModelTag() const override { return L"Model_LuneBody"; }
    const wchar_t* Get_HeadModelTag() const override { return L"Model_LuneHead"; }
    const wchar_t* Get_HairModelTag() const override { return L"Model_LuneHair"; }

public:
	static Shared<Player_Lune> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* arg) override;
	virtual void Free() override;

};

NS_END

