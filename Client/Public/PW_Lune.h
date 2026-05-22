#pragma once

#include "Client_Defines.h"
#include "Player_WorldBase.h"

NS_BEGIN(Client)

class PW_Lune final : public Player_WorldBase
{
    GENERATED_GAMEOBJECT(PW_Lune, ObjectType::Player)

public:
	PW_Lune(const ComPtr<Device>& device, const ComPtr<Context>& context);
	PW_Lune(const PW_Lune& prototype);
	virtual ~PW_Lune() override = default;

public:
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;

protected:
    HairChainPhysicsDesc Get_HairChainPhysicsDesc() const override;

    const wchar_t* Get_BodyModelTag() const override { return L"Model_LuneBody"; }
    const wchar_t* Get_HeadModelTag() const override { return L"Model_LuneHead"; }
    const wchar_t* Get_HairModelTag() const override { return L"Model_LuneHair"; }

public:
	static Shared<PW_Lune> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* arg) override;
	virtual void Free() override;

};

NS_END

