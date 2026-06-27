#pragma once
#include "GameObject.h"

NS_BEGIN(Client)

class EffectCom;

class MapEffect_TornadoPaint final : public GameObject
{
    GENERATED_GAMEOBJECT(MapEffect_TornadoPaint, ObjectType::Effect)

public:
    MapEffect_TornadoPaint(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MapEffect_TornadoPaint(const MapEffect_TornadoPaint& prototype);
    virtual ~MapEffect_TornadoPaint() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Update(float timeDelta) override;

private:
    HRESULT Ready_Components();
    void Play_TornadoEffect();

private:
    Shared<EffectCom> _effectCom;
    float _scale = 1.f;

public:
    void BeginPlay() override;

public:
    static Shared<MapEffect_TornadoPaint> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
