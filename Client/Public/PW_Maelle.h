#pragma once

#include "Client_Defines.h"
#include "Player_WorldBase.h"

NS_BEGIN(Client)

class PW_Maelle final : public Player_WorldBase
{
    GENERATED_GAMEOBJECT(PW_Maelle, ObjectType::Player)

public:
	PW_Maelle(const ComPtr<Device>& device, const ComPtr<Context>& context);
	PW_Maelle(const PW_Maelle& prototype);
	virtual ~PW_Maelle() override = default;

public:
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;

protected:
    HairChainPhysicsDesc Get_HairChainPhysicsDesc() const override;

    const wchar_t* Get_BodyModelTag() const override { return L"Model_MaelleBody"; }
    const wchar_t* Get_HeadModelTag() const override { return L"Model_MaelleHead"; }
    const wchar_t* Get_HairModelTag() const override { return L"Model_MaelleHair"; }

public:
	static Shared<PW_Maelle> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* arg) override;
	virtual void Free() override;

};

NS_END

