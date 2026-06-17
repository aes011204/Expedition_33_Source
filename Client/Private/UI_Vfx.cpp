#include "pch.h"
#include "UI_Vfx.h"

#include "ClientInstance.h"
#include "EffectInstance.h"
#include "GameInstance.h"

NS_BEGIN(Client)

UI_Vfx::UI_Vfx()
{
}

UI_Vfx::UI_Vfx(const UI_Vfx& prototype)
    : UI_Base{ prototype }
    , _asset{ prototype._asset }
    , _assetPath{ prototype._assetPath }
    , _playOnOpen{ prototype._playOnOpen }
    , _loop{ prototype._loop }
    , _stopOnClose{ prototype._stopOnClose }
{
}

HRESULT UI_Vfx::Initialize_Prototype()
{
    return __super::Initialize_Prototype();
}

HRESULT UI_Vfx::Initialize(void* arg)
{
    UI_VFX_DESC desc{};
    if (nullptr != arg)
        desc = *static_cast<UI_VFX_DESC*>(arg);

    if (FAILED(__super::Initialize(&desc)))
        return E_FAIL;

    _asset = desc.asset;
    _asset.loop = desc.loop || _asset.loop;
    _assetPath = desc.assetPath;
    _playOnOpen = desc.playOnOpen;
    _loop = desc.loop || _asset.loop;
    _stopOnClose = desc.stopOnClose;
    _definition = UISpriteVfxRuntimeBuilder::Build_EffectDefinition(_asset, _assetPath.empty() ? _asset.name : _assetPath);

    return S_OK;
}

void UI_Vfx::Open()
{
    __super::Open();

    _playedForCurrentOpen = false;
    if (_playOnOpen)
        Play_Vfx();
}

void UI_Vfx::Close()
{
    if (_stopOnClose)
        Stop_Vfx();

    _playedForCurrentOpen = false;
    __super::Close();
}

void UI_Vfx::Update(float timeDelta)
{
    __super::Update(timeDelta);

    if (_effectHandle.effectObject != nullptr && Is_VfxFinished())
    {
        _effectHandle = {};
        _playedForCurrentOpen = !_loop;
    }

    if (_playOnOpen && !_playedForCurrentOpen)
        Play_Vfx();

    Update_VfxTransform();
}

HRESULT UI_Vfx::Render()
{
    return S_OK;
}

void UI_Vfx::On_EffectiveOpenChanged(bool isEffectivelyOpen)
{
    if (isEffectivelyOpen)
    {
        _playedForCurrentOpen = false;
        if (_playOnOpen)
            Play_Vfx();
    }
    else if (_stopOnClose)
    {
        Stop_Vfx();
        _playedForCurrentOpen = false;
    }
}

void UI_Vfx::Play_Vfx()
{
    if (_effectHandle.effectObject != nullptr)
    {
        Update_VfxTransform();
        _playedForCurrentOpen = true;
        return;
    }

    EffectInstancePool* effectPool = CLIENT->Get_EffectInstancePool();
    if (nullptr == effectPool || nullptr == _definition)
        return;

    EffectInstancePool::AcquireDesc acquireDesc{};
    acquireDesc.effectName = String::ToWString(_assetPath.empty() ? _asset.name : _assetPath);
    acquireDesc.definition = _definition;
    acquireDesc.layerLevelIndex = GAME->Current_LevelIndex();
    acquireDesc.layerTag = L"Layer_UI_Effect";
    acquireDesc.worldPosition = Resolve_EffectWorldPosition();
    acquireDesc.worldRotation = Quat::CreateFromYawPitchRoll(
        0.f,
        0.f,
        XMConvertToRadians(Resolve_EffectWorldRotation().z));
    acquireDesc.scale = Resolve_EffectWorldScale();

    if (FAILED(effectPool->Acquire(acquireDesc, _effectHandle)))
    {
        _effectHandle = {};
        return;
    }

    if (Shared<EffectInstance> effectInstance = dynamic_pointer_cast<EffectInstance>(_effectHandle.effectObject))
        effectInstance->Set_Loop(_loop);

    _playedForCurrentOpen = true;
}

void UI_Vfx::Stop_Vfx()
{
    if (_effectHandle.effectObject == nullptr)
        return;

    if (EffectInstancePool* effectPool = CLIENT->Get_EffectInstancePool())
        effectPool->Release(_effectHandle);

    _effectHandle = {};
}

void UI_Vfx::Update_VfxTransform()
{
    if (_effectHandle.effectObject == nullptr)
        return;

    const Shared<TransformCom> transform = _effectHandle.effectObject->Get_Transform();
    if (nullptr == transform)
        return;

    transform->Set_WorldPosition(Resolve_EffectWorldPosition());
    transform->Set_WorldRotationQuaternion(Quat::CreateFromYawPitchRoll(
        0.f,
        0.f,
        XMConvertToRadians(Resolve_EffectWorldRotation().z)));
    transform->Set_Scale(Resolve_EffectWorldScale());
}

bool UI_Vfx::Is_VfxFinished() const
{
    if (_effectHandle.effectObject == nullptr)
        return false;

    if (_loop)
        return false;

    if (!_effectHandle.effectObject->Is_Visible())
        return true;

    const Shared<EffectInstance> effectInstance = dynamic_pointer_cast<EffectInstance>(_effectHandle.effectObject);
    return effectInstance != nullptr && effectInstance->Is_Finished();
}

Vec3 UI_Vfx::Resolve_EffectWorldPosition() const
{
    if (nullptr == _transform)
        return Vec3::Zero;

    const Vec2 center = Transform_LocalPointToWorld(Get_ScaledLocalCenter(_transform->Get_Scale()));
    const float viewportWidth = static_cast<float>(max(1u, GAME->Get_ViewportWidth()));
    const float viewportHeight = static_cast<float>(max(1u, GAME->Get_ViewportHeight()));

    return Vec3(
        center.x - viewportWidth * 0.5f,
        -center.y + viewportHeight * 0.5f,
        0.f);
}

Vec3 UI_Vfx::Resolve_EffectWorldScale() const
{
    const Vec2 scale = Get_WorldScale();
    return Vec3(max(0.0001f, scale.x), max(0.0001f, scale.y), 1.f);
}

Vec3 UI_Vfx::Resolve_EffectWorldRotation() const
{
    return Vec3(0.f, 0.f, -XMConvertToDegrees(Get_WorldRotationRadians()));
}

Shared<UI_Vfx> UI_Vfx::Create()
{
    const Shared<UI_Vfx> instance = make_shared<UI_Vfx>();

    if (nullptr == instance)
    {
        LOG_CRITICAL("Failed to Create : UI_Vfx");
        return nullptr;
    }

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Initialize Prototype : UI_Vfx");
        return nullptr;
    }

    return instance;
}

Shared<UI_Base> UI_Vfx::Clone(void* arg)
{
    const Shared<UI_Vfx> instance = make_shared<UI_Vfx>(*this);

    if (nullptr == instance)
    {
        LOG_CRITICAL("Failed to Clone : UI_Vfx");
        return nullptr;
    }

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Initialize : UI_Vfx");
        return nullptr;
    }

    return instance;
}

void UI_Vfx::Free()
{
    Stop_Vfx();
    _definition.reset();

    __super::Free();
}

NS_END
