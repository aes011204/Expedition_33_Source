#pragma once
#include "GameObject.h"

NS_BEGIN(Client)
class NPC_Base;
class Trigger_Interaction : public GameObject
{
    GENERATED_GAMEOBJECT(Trigger_Interaction, ObjectType::Trigger_Interaction)

public:
    struct TRIGGER_INTERACTION_DESC
    {
        Shared<NPC_Base> ownerNpc = nullptr;
        Vec3 localOffset = Vec3(0.f, 0.8f, 1.2f);
    };

public:
    Trigger_Interaction(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Trigger_Interaction(const Trigger_Interaction& prototype);
    virtual ~Trigger_Interaction() = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Update(float timeDelta) override;
    void    Late_Update(float timeDelta) override;
    void    OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void    OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void    OnStayOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;

private:
    HRESULT Ready_Components();
    void    Sync_ToOwnerTransform();
    bool    Has_ActiveHeroOverlap() const;
    bool    Refresh_ActiveHeroOverlapRegistration();

private:
    Shared<ColliderCom> _colliderCom {};
    Shared<NPC_Base>    _ownerNPC {};
    Vec3                _localOffset = Vec3(0.f, 0.8f, 1.2f);
    uset<ColliderCom*>  _playerOverlapColliders {};
    bool                _activeHeroOverlapRegistered = false;
public:
    static Shared<Trigger_Interaction> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
