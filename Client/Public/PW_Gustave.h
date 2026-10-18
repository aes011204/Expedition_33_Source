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
    void    BeginPlay() override;

protected:
    HRESULT Ready_BodyModifiers(Body_Player* body, const string& costumePresetId) override;
    HairChainPhysicsDesc Get_HairChainPhysicsDesc() const override;

    const wchar_t* Get_BodyModelTag() const override { return L"Model_GustaveBody"; }
    const wchar_t* Get_HeadModelTag() const override { return L"Model_GustaveHead"; }
    const wchar_t* Get_HairModelTag() const override { return L"Model_GustaveHair"; }

    // 구스타브 수염
    virtual const wchar_t* Get_BeardModelTag() const { return nullptr; /* return L"Model_GustaveBeard"; */ }
    virtual const wchar_t* Get_MustacheModelTag() const { return nullptr; /* return L"Model_GustaveMustache"; */ }
    const wchar_t* Get_GameMenuFacialClipPath() const override
    {
        return L"Animations/Facial/Metahuman_Common/MicroMovements/A_MH_RandomMovement_GameMenu_Gustave.facialclipbin";
    }

    HeroId Get_HeroId() const override { return HeroId::Gustave; }

public:
	static Shared<PW_Gustave> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* arg) override;
	virtual void Free() override;

};

NS_END

