#pragma once
#include "NPC_Base.h"

NS_BEGIN(Client)

class NPC_Body;

class NPC_Merchant : public NPC_Base
{
    GENERATED_GAMEOBJECT(NPC_Merchant, ObjectType::NPC)

public:
    NPC_Merchant(const ComPtr<Device>& device, const ComPtr<Context>& context);
    NPC_Merchant(const NPC_Merchant& prototype);
    virtual ~NPC_Merchant() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Priority_Update(float timeDelta) override;
    void    Update(float timeDelta) override;
    HRESULT Render() override;
    HRESULT Render_Shadow() override;

public:
    void    OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;

protected:
    HRESULT Ready_Components() override;
    HRESULT Ready_PartObjects();

private:
    Shared<NPC_Body> _body = {};

public:
    static Shared<NPC_Merchant> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
