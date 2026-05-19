#pragma once

#include "Client_Defines.h"
#include "Player_WorldBase.h"

NS_BEGIN(Client)

class PW_Gustave final : public Player_WorldBase
{
    GENERATED_GAMEOBJECT(PW_Gustave, ObjectType::Player)

public:
	PW_Gustave(const ComPtr<Device>& device, const ComPtr<Context>& context);
	PW_Gustave(const PW_Gustave& prototype);
	virtual ~PW_Gustave() override = default;

public:
    HRESULT Initialize(void* arg) override;

protected:
    HairChainPhysicsDesc Get_HairChainPhysicsDesc() const override;

    const wchar_t* Get_BodyModelTag() const override { return L"Model_GustaveBody"; }
    const wchar_t* Get_HeadModelTag() const override { return L"Model_GustaveHead"; }
    const wchar_t* Get_HairModelTag() const override { return L"Model_GustaveHair"; }

public:
	static Shared<PW_Gustave> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* arg) override;
	virtual void Free() override;

};

NS_END

