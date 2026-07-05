#pragma once
#include "GameObject.h"

NS_BEGIN(Client)

class Trigger_TeleportZone final : public GameObject
{
    GENERATED_GAMEOBJECT(Trigger_TeleportZone, ObjectType::Trigger)

public:
    Trigger_TeleportZone(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Trigger_TeleportZone(const Trigger_TeleportZone& prototype);
    virtual ~Trigger_TeleportZone() = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Late_Update(float timeDelta) override;
    void OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void BeginPlay() override;

private:
    HRESULT Ready_Components();
    bool Is_ActivePlayerCollider(const Shared<ColliderCom>& other) const;

private:
    Shared<ColliderCom> _colliderCom;
    Vec3 _teleportTargetPosition = Vec3(216.88f, -4000.38f, -2.66f);
    Vec3 _teleportTargetRotation = Vec3(0.f, 27.6f, 0.f);
    bool _hasTriggered = false;

public:
    static Shared<Trigger_TeleportZone> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
