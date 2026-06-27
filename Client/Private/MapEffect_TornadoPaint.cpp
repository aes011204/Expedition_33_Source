#include "MapEffect_TornadoPaint.h"
#include "GameInstance.h"
#include "EffectCom.h"

namespace
{
    constexpr wchar_t kEffectName[] = L"Effects/Assets/Map/FX_TornadoPaint.effect.json";
}

IMPLEMENT_REFLECTION(MapEffect_TornadoPaint)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.className = "MapEffect_TornadoPaint";
    info.displayName = "MapEffect_TornadoPaint";
    info.category = "GameObject";

    PROPERTY_FLOAT("Scale", _scale, 0.f, 100.f, 0.1f);

    return true;
}

MapEffect_TornadoPaint::MapEffect_TornadoPaint(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject(device, context)
{
}

MapEffect_TornadoPaint::MapEffect_TornadoPaint(const MapEffect_TornadoPaint& prototype)
    : GameObject(prototype)
{
}

HRESULT MapEffect_TornadoPaint::Initialize_Prototype()
{
    return __super::Initialize_Prototype();
}

HRESULT MapEffect_TornadoPaint::Initialize(void* arg)
{
    if (FAILED(__super::Initialize(arg)))
    {
        return E_FAIL;
    }

    CHECK_FAILED(Ready_Components(), E_FAIL);

    return S_OK;
}

void MapEffect_TornadoPaint::Update(float timeDelta)
{
    if (_effectCom)
    {
        _effectCom->Update(timeDelta);
    }
}

HRESULT MapEffect_TornadoPaint::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Effect"), _effectCom), E_FAIL);

    return S_OK;
}

void MapEffect_TornadoPaint::Play_TornadoEffect()
{
    if (_effectCom == nullptr)
    {
        return;
    }

    _effectCom->Stop_AllAttachedEffects();

    EffectCom::EffectPlayDesc desc{};
    desc.levelIndex = GAME->Current_LevelIndex();
    desc.effectName = kEffectName;
    desc.target = EffectCom::EffectPlayTarget::OwnerRoot;
    desc.layerTag = L"Layer_Effect";
    desc.followTarget = true;
    desc.scale = Vec3(_scale);
    desc.requiresManualRelease = true;

    _effectCom->Play_Effect(desc);
}

void MapEffect_TornadoPaint::BeginPlay()
{
    GameObject::BeginPlay();
    Play_TornadoEffect();
}

Shared<MapEffect_TornadoPaint> MapEffect_TornadoPaint::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MapEffect_TornadoPaint>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> MapEffect_TornadoPaint::Clone(void* arg)
{
    auto clone = make_shared<MapEffect_TornadoPaint>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void MapEffect_TornadoPaint::Free()
{
    if (_effectCom)
    {
        _effectCom->Stop_AllAttachedEffects();
    }

    GameObject::Free();
}
