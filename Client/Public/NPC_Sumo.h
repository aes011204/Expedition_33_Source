#pragma once
#include "NPC_Base.h"

NS_BEGIN(Client)

class NPC_Body;
class NPC_Hair;

class NPC_Sumo : public NPC_Base
{
    GENERATED_GAMEOBJECT(NPC_Sumo, ObjectType::NPC)

public:
    NPC_Sumo(const ComPtr<Device>& device, const ComPtr<Context>& context);
    NPC_Sumo(const NPC_Sumo& prototype);
    virtual ~NPC_Sumo() override = default;

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
    Shared<NPC_Hair> _hair = {};

public:
    static Shared<NPC_Sumo> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
