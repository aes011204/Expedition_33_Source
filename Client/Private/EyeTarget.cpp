#include "EyeTarget.h"
#include "ColliderCom.h"
#include "Bounding_Sphere.h"
#include "EffectCom.h"
#include "ClientInstance.h"
#include "GameInstance.h"

NS_BEGIN(Client)

namespace
{
    constexpr wchar_t kDefaultEffectName[] = L"Effects\\Assets\\Monster\\Monster_Weak.effect.json";
    constexpr wchar_t kBreakEffectName[] = L"Effects\\Assets\\Monster\\WeakAtt_Blue.effect.effect.json";
}

IMPLEMENT_REFLECTION(EyeTarget)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.className = "EyeTarget";
    info.displayName = "EyeTarget";
    info.category = "GameObject";

    PROPERTY_FLOAT("Effect Scale", _effectScale, 0.f, 10.f, 0.01f );
    return true;
}

EyeTarget::EyeTarget(const ComPtr<Device>& device, const ComPtr<Context>& context) : GameObject { device, context }
{
}

EyeTarget::EyeTarget(const EyeTarget& prototype) : GameObject { prototype }
{
}

HRESULT EyeTarget::Initialize_Prototype()
{
    return __super::Initialize_Prototype();
}

HRESULT EyeTarget::Initialize(void* arg)
{
    if(FAILED(GameObject::Initialize(arg)))
        return E_FAIL;

    CHECK_FAILED(Ready_EyeTargetCollider(), E_FAIL);
    CHECK_FAILED(Ready_EyeTargetEffectSetting(), E_FAIL);

    return S_OK;
}

void EyeTarget::BeginPlay()
{
    __super::BeginPlay();

    //리셋
    Play_EyeTargetEffect();

    _isEnable = true;
}

void EyeTarget::Priority_Update(float timeDelta)
{
    __super::Priority_Update(timeDelta);
}

void EyeTarget::Update(float timeDelta)
{
    __super::Update(timeDelta);

    if(!_isEnable)
        return;

    if(_effectCom)
        _effectCom->Update(timeDelta);
}

void EyeTarget::Late_Update(float timeDelta)
{
    __super::Late_Update(timeDelta);

    if(!_isEnable)
        return;

    if(_colliderCom == nullptr || !_colliderCom->Get_IsActive() || !Is_Visible())
        return;

    Matrix worldMatrix = _transformCom->Get_WorldMatrix();

    _colliderCom->Update_Collider(worldMatrix);

    GAME->Add_Collider(_colliderCom,worldMatrix);
}

HRESULT EyeTarget::Render()
{
    return __super::Render();
}

HRESULT EyeTarget::Render_Shadow()
{
    return __super::Render_Shadow();
}

void EyeTarget::From_Json(const json& data)
{
    __super::From_Json(data);

    if(_colliderCom)
        _colliderCom->Set_CollisionPreset(Collision_Preset::Monster_WeakPoint);

    Apply_EyeTargetEffectSettings();
}

bool EyeTarget::Try_Consume_EyeTargetHit()
{
    if(!_isEnable)
        return false;

    const Matrix worldMatrix = _transformCom->Get_WorldMatrix();

    PlayEffectDesc effectDesc {};
    effectDesc.effectName = kBreakEffectName;
    effectDesc.worldPosition = Vec3(worldMatrix._41, worldMatrix._42, worldMatrix._43);
    effectDesc.scale = Vec3(0.3f, 0.3f, 0.3f);

    CLIENT->Play_Effect(effectDesc);

    _isEnable = false;
    Set_Visible(false);

    if(_colliderCom)
    {
        _colliderCom->Set_IsActive(false);
        _colliderCom->Clear_Overlap();
        GAME->Unregister_PhysXCollider(_colliderCom.get());
    }

    if(_effectCom)
        _effectCom->Stop_AllAttachedEffects();
    
    OnDestroyed.Broadcast();

    return true;
}

void EyeTarget::Reset_Target()
{
    _isEnable = true;
    Set_Visible(true);
    _colliderCom->Set_IsActive(true);

    Play_EyeTargetEffect();
}

HRESULT EyeTarget::Ready_EyeTargetCollider()
{
    Bounding_Sphere::FBoundingSphereDesc sphereDesc {};

    sphereDesc.center = Vec3::Zero;
    sphereDesc.radius = 0.32f;

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_Sphere"), _colliderCom, &sphereDesc),  E_FAIL);
    _colliderCom->Set_CollisionPreset(Collision_Preset::Monster_WeakPoint);
    _colliderCom->Set_IsActive(true);

    return S_OK;
}

HRESULT EyeTarget::Ready_EyeTargetEffectSetting()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Effect"), _effectCom), E_FAIL);

    if(_effectCom == nullptr)
        return E_FAIL;

    Apply_EyeTargetEffectSettings();

    return S_OK;
}

void EyeTarget::Apply_EyeTargetEffectSettings()
{
    if(_effectCom == nullptr)
        return;

    EffectCom::InspectorEffectSettings settings = _effectCom->Get_InspectorEffectSettings();
    settings.autoPlay = false;
    if(settings.effectName.empty())
        settings.effectName = kDefaultEffectName;
    settings.target = EffectCom::EffectPlayTarget::OwnerRoot;
    settings.partTag.clear();
    settings.layerTag = L"Layer_Effect";
    settings.localOffset = Vec3::Zero;
    settings.localRotation = Vec3::Zero;
    settings.followTarget = true;
    settings.scale = Vec3(_effectScale);
    settings.requiresManualRelease = true;

    _effectCom->Set_InspectorEffectSettings(settings);
}

void EyeTarget::Play_EyeTargetEffect()
{
    if(_effectCom == nullptr)
        return;

    _effectCom->Stop_AllAttachedEffects();

    const EffectCom::InspectorEffectSettings settings = _effectCom->Get_InspectorEffectSettings();

    EffectCom::EffectPlayDesc desc {};
    desc.levelIndex = GAME->Current_LevelIndex();
    desc.effectName = settings.effectName;
    if(desc.effectName.empty())
        desc.effectName = kDefaultEffectName;
    desc.target = EffectCom::EffectPlayTarget::OwnerRoot;
    desc.layerTag = L"Layer_Effect";
    desc.followTarget = true;
    desc.scale = Vec3(_effectScale);
    desc.requiresManualRelease = true;

    _effectCom->Play_Effect(desc);
}

Shared<EyeTarget> EyeTarget::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<EyeTarget>(device, context);

    if(FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> EyeTarget::Clone(void* arg)
{
    auto clone = make_shared<EyeTarget>(*this);

    if(FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void EyeTarget::Free()
{
    __super::Free();
}

NS_END
