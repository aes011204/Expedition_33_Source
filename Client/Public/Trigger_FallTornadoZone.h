#pragma once
#include "GameObject.h"
#include "EffectInstancePool.h"

NS_BEGIN(Client)

class EffectCom;

class Trigger_FallTornadoZone final : public GameObject
{
    GENERATED_GAMEOBJECT(Trigger_FallTornadoZone, ObjectType::Trigger)

public:
    Trigger_FallTornadoZone(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Trigger_FallTornadoZone(const Trigger_FallTornadoZone& prototype);
    ~Trigger_FallTornadoZone() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Late_Update(float timeDelta) override;
    void    OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void    OnEndOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void    BeginPlay() override;

private:
    HRESULT Ready_Components();
    bool Is_ActivePlayerCollider(const Shared<ColliderCom>& other) const;
    void Play_FallTornadoEffect(const Shared<EffectCom>& effectCom, bool followTarget);
    void Stop_FallTornadoEffect();

private:
    Shared<ColliderCom> _colliderCom{};
    Shared<EffectCom> _effectCom;
    Vec3 _effectScale = Vec3(5.f, 5.f, 5.f);
    Vec3 _effectLocalOffset = Vec3(0.f, -23.f, 0.f);
    float _stopWorldY = -3000.f;
    float _followStartDropDistance = 23.f;
    bool _hasTriggered = false;
    bool _isFollowing = false;
    float _spawnWorldY = 0.f;
    Weak<GameObject> _trackedPlayer;
    Weak<EffectCom> _activeEffectCom;
    EffectInstancePool::AttachedHandle _fallTornadoHandle{};

public:
    static Shared<Trigger_FallTornadoZone> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
