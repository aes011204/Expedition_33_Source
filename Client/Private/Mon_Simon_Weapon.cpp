#include "pch.h"
#include "Mon_Simon_Weapon.h"
#include "GameInstance.h"
#include "Material_Types.h"
#include "ModelCom.h"
#include "ShaderCom.h"

namespace
{
    constexpr uint32 kSimonWeaponEchoPassIndex = 15;
}

IMPLEMENT_REFLECTION(Mon_Simon_Weapon)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.category = "Weapon";
    info.displayName = "Simon_Weapon";

    PROPERTY_WSTRING("트레일 이펙트", _trailEffectName);

    return true;
}

Mon_Simon_Weapon::Mon_Simon_Weapon(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_Part(device, context)
{
}

Mon_Simon_Weapon::Mon_Simon_Weapon(const Mon_Simon_Weapon& prototype)
    : Monster_Part(prototype)
{
}

HRESULT Mon_Simon_Weapon::Initialize_Prototype()
{
    return Monster_Part::Initialize_Prototype();
}

HRESULT Mon_Simon_Weapon::Initialize(void* arg)
{
    auto* desc = static_cast<WEAPON_DESC*>(arg);

    _socketMatrix = (desc != nullptr) ? desc->socketMatrix : nullptr;
    _offsetMatrix = Matrix::CreateRotationX(XMConvertToRadians(-90.f));

    if (desc != nullptr)
    {
        if (FAILED(Monster_Part::Initialize(arg)))
            return E_FAIL;
    }
    else
    {
        if (FAILED(PartObject::Initialize(nullptr)))
            return E_FAIL;

        if (FAILED(Ready_DefaultComponents()))
            return E_FAIL;

        Set_ShadowCasterType(ShadowCasterType::Dynamic);
    }

    return S_OK;
}

void Mon_Simon_Weapon::Priority_Update(float timeDelta)
{
    Monster_Part::Priority_Update(timeDelta);
}

void Mon_Simon_Weapon::Update(float timeDelta)
{
    if (_isDetached)
    {
        _combinedWorldMatrix = _detachedWorldMatrix;
        Update_WeaponEchoPlayback(timeDelta);
        return;
    }

    Matrix worldMatrix = _offsetMatrix * _transformCom->Get_WorldMatrix();

    if (_useSocketBinding && _socketMatrix != nullptr)
        worldMatrix *= *_socketMatrix;

    Update_CombinedWorldMatrix(worldMatrix);
    Update_WeaponEchoPlayback(timeDelta);
}

void Mon_Simon_Weapon::Detach_PreserveWorld()
{
    _detachedWorldMatrix = Get_CombinedWorldMatrix();
    _useSocketBinding = false;
    _isDetached = true;
}

void Mon_Simon_Weapon::Attach_ToBindings()
{
    _isDetached = false;
    _useSocketBinding = true;
}

void Mon_Simon_Weapon::Late_Update(float timeDelta)
{
    Monster_Part::Late_Update(timeDelta);

    if (Get_Owner() == nullptr && _shader != nullptr && _model != nullptr && Is_Visible())
    {
        GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());

        if (Casts_DynamicShadow())
            GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());
    }

    Queue_WeaponEchoRender();
}

HRESULT Mon_Simon_Weapon::Render()
{
    if (GAME->Get_CurrentRenderGroup() == RenderGroup::Blend)
        return Render_WeaponEchoPlayback();

    return Monster_Part::Render();
}

HRESULT Mon_Simon_Weapon::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

void Mon_Simon_Weapon::From_Json(const json& data)
{
    if (FAILED(Ensure_RuntimeComponentsFromJson(data)))
        return;

    Monster_Part::From_Json(data);
}

uint32 Mon_Simon_Weapon::Decide_PassIndex(Shared<Material> mat)
{
    const int OPAQUE_PASS = 0;
    return OPAQUE_PASS;
}

HRESULT Mon_Simon_Weapon::Ready_DefaultComponents()
{
    WEAPON_DESC defaultDesc{};
    defaultDesc.modelType = ModelType::NonAnim;
    defaultDesc.modelTag = L"Model_Simon_Weapon";
    defaultDesc.socketMatrix = _socketMatrix;

    return Ready_Components(defaultDesc);
}

HRESULT Mon_Simon_Weapon::Ensure_RuntimeComponentsFromJson(const json& data)
{
    if (!data.contains("components") || !data["components"].is_array())
        return S_OK;

    auto findComponentTag =
        [this](const Shared<Component>& targetComponent) -> wstring
        {
            if (targetComponent == nullptr)
                return {};

            wstring foundTag{};
            Visit_Components(
                [&foundTag, &targetComponent](const wstring& tag, const Shared<Component>& component)
                {
                    if (foundTag.empty() && component == targetComponent)
                        foundTag = tag;
                });

            return foundTag;
        };

    for (const json& componentJson : data["components"])
    {
        if (!componentJson.is_object())
            continue;

        const string prototypeTagString = componentJson.value("prototype_tag", string{});
        if (prototypeTagString.empty())
            continue;

        const wstring prototypeTag = String::ToWString(prototypeTagString);

        if (prototypeTag == L"Shader_VtxMesh")
        {
            if (_components.find(prototypeTag) != _components.end())
                continue;

            if (_shader != nullptr)
            {
                const wstring currentShaderTag = findComponentTag(static_pointer_cast<Component>(_shader));
                if (!currentShaderTag.empty())
                    CHECK_FAILED(Rename_ComponentTag(currentShaderTag, prototypeTag), E_FAIL);
            }
            else
            {
                CHECK_FAILED(Add_Component(ETOI(LevelType::Static), prototypeTag.c_str(), _shader), E_FAIL);
            }

            continue;
        }

        if (componentJson.contains("model_type"))
        {
            if (_components.find(prototypeTag) != _components.end())
                continue;

            if (_model != nullptr)
            {
                const wstring currentModelTag = findComponentTag(static_pointer_cast<Component>(_model));
                if (!currentModelTag.empty())
                    CHECK_FAILED(Rename_ComponentTag(currentModelTag, prototypeTag), E_FAIL);
            }
            else
            {
                CHECK_FAILED(Add_Component(ETOI(LevelType::Static), prototypeTag.c_str(), _model), E_FAIL);
            }

            continue;
        }
    }

    return S_OK;
}

void Mon_Simon_Weapon::Begin_WeaponEchoPlayback(float delaySec, float startAlpha, float endAlpha, const Vec4& tintColor)
{
    _weaponEchoSamples.clear();
    _weaponEchoActive = true;
    _weaponEchoRecordingEnded = false;
    _weaponEchoDelaySec = max(0.f, delaySec);
    _weaponEchoPlaybackSec = 0.f;
    _weaponEchoEndElapsedSec = 0.f;
    _weaponEchoStartAlpha = clamp(startAlpha, 0.f, 1.f);
    _weaponEchoEndAlpha = clamp(endAlpha, 0.f, 1.f);
    _weaponEchoTintColor = tintColor;
}

void Mon_Simon_Weapon::Record_WeaponEchoSample(float elapsedSec)
{
    if (!_weaponEchoActive)
        return;

    WeaponEchoSample sample{};
    sample.elapsedSec = max(0.f, elapsedSec);
    sample.worldMatrix = _combinedWorldMatrix;

    if (!_weaponEchoSamples.empty() && sample.elapsedSec + 1e-4f < _weaponEchoSamples.back().elapsedSec)
        _weaponEchoSamples.clear();

    if (!_weaponEchoSamples.empty() && fabsf(sample.elapsedSec - _weaponEchoSamples.back().elapsedSec) <= 1e-4f)
    {
        _weaponEchoSamples.back() = sample;
        _weaponEchoEndElapsedSec = sample.elapsedSec;
        return;
    }

    _weaponEchoSamples.push_back(sample);
    _weaponEchoEndElapsedSec = sample.elapsedSec;
}

void Mon_Simon_Weapon::End_WeaponEchoPlayback()
{
    if (!_weaponEchoActive)
        return;

    _weaponEchoRecordingEnded = true;
    if (!_weaponEchoSamples.empty())
        _weaponEchoEndElapsedSec = _weaponEchoSamples.back().elapsedSec;
}

bool Mon_Simon_Weapon::Has_WeaponEchoPlayback() const
{
    if (!_weaponEchoActive || _weaponEchoSamples.empty())
        return false;

    if (!_weaponEchoRecordingEnded)
        return _weaponEchoPlaybackSec >= _weaponEchoDelaySec;

    return _weaponEchoPlaybackSec <= _weaponEchoDelaySec + _weaponEchoEndElapsedSec;
}

void Mon_Simon_Weapon::Queue_WeaponEchoRender()
{
    if (!Has_WeaponEchoPlayback())
        return;

    if (_shader == nullptr || _model == nullptr)
        return;

    if (!Is_Visible())
        return;

    if (const Shared<GameObject> owner = Get_Owner())
    {
        if (!owner->Is_Visible())
            return;
    }

    GAME->Add_RenderGroup(RenderGroup::Blend, GetSharedPtr<GameObject>());
}

void Mon_Simon_Weapon::Update_WeaponEchoPlayback(float timeDelta)
{
    if (!_weaponEchoActive)
        return;

    _weaponEchoPlaybackSec += max(0.f, timeDelta);

    if (_weaponEchoRecordingEnded && _weaponEchoPlaybackSec > _weaponEchoDelaySec + _weaponEchoEndElapsedSec)
    {
        _weaponEchoActive = false;
        _weaponEchoRecordingEnded = false;
        _weaponEchoSamples.clear();
        _weaponEchoPlaybackSec = 0.f;
        _weaponEchoEndElapsedSec = 0.f;
    }
}

bool Mon_Simon_Weapon::Try_SampleWeaponEchoMatrix(float elapsedSec, Matrix& outWorldMatrix) const
{
    if (_weaponEchoSamples.empty())
        return false;

    if (elapsedSec <= _weaponEchoSamples.front().elapsedSec)
    {
        outWorldMatrix = _weaponEchoSamples.front().worldMatrix;
        return true;
    }

    if (elapsedSec >= _weaponEchoSamples.back().elapsedSec)
    {
        outWorldMatrix = _weaponEchoSamples.back().worldMatrix;
        return true;
    }

    for (size_t i = 1; i < _weaponEchoSamples.size(); ++i)
    {
        const WeaponEchoSample& right = _weaponEchoSamples[i];
        if (elapsedSec > right.elapsedSec)
            continue;

        const WeaponEchoSample& left = _weaponEchoSamples[i - 1];
        const float duration = right.elapsedSec - left.elapsedSec;
        const float ratio = duration > 0.0001f
            ? clamp((elapsedSec - left.elapsedSec) / duration, 0.f, 1.f)
            : 1.f;

        Vec3 leftScale = Vec3::One;
        Quat leftRotation = Quat::Identity;
        Vec3 leftTranslation = Vec3::Zero;
        Vec3 rightScale = Vec3::One;
        Quat rightRotation = Quat::Identity;
        Vec3 rightTranslation = Vec3::Zero;
        Matrix leftWorldMatrix = left.worldMatrix;
        Matrix rightWorldMatrix = right.worldMatrix;

        if (!leftWorldMatrix.Decompose(leftScale, leftRotation, leftTranslation) ||
            !rightWorldMatrix.Decompose(rightScale, rightRotation, rightTranslation))
        {
            outWorldMatrix = left.worldMatrix;
            return true;
        }

        const Vec3 blendedScale = Vec3::Lerp(leftScale, rightScale, ratio);
        const Quat blendedRotation = Quat::Slerp(leftRotation, rightRotation, ratio);
        const Vec3 blendedTranslation = Vec3::Lerp(leftTranslation, rightTranslation, ratio);

        outWorldMatrix =
            Matrix::CreateScale(blendedScale) *
            Matrix::CreateFromQuaternion(blendedRotation) *
            Matrix::CreateTranslation(blendedTranslation);

        return true;
    }

    return false;
}

HRESULT Mon_Simon_Weapon::Render_WeaponEchoPlayback()
{
    if (!Has_WeaponEchoPlayback())
        return S_OK;

    const float echoElapsedSec = _weaponEchoPlaybackSec - _weaponEchoDelaySec;
    if (echoElapsedSec < 0.f)
        return S_OK;

    Matrix echoWorldMatrix = Matrix::Identity;
    if (!Try_SampleWeaponEchoMatrix(echoElapsedSec, echoWorldMatrix))
        return S_OK;

    if (FAILED(GAME->Bind_CameraCB(_shader)))
        return E_FAIL;

    if (FAILED(_shader->Reset_DrawFeatures()))
        return E_FAIL;

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    const float duration = max(_weaponEchoEndElapsedSec, 0.0001f);
    const float ratio = clamp(echoElapsedSec / duration, 0.f, 1.f);

    Vec4 ghostColor = _weaponEchoTintColor;
    ghostColor.w = std::lerp(_weaponEchoStartAlpha, _weaponEchoEndAlpha, ratio);

    if (ghostColor.w <= 0.001f)
        return S_OK;

    if (FAILED(_shader->Bind_ObjectCB(echoWorldMatrix)))
        return E_FAIL;

    if (FAILED(_shader->Bind_RawValue("g_WeaponGhostColor", &ghostColor, sizeof(Vec4))))
        return E_FAIL;

    for (uint32 i = 0; i < numMeshes; ++i)
    {
        const uint32 materialIndex = _model->Get_MeshMaterialIndex(i);
        const Shared<Material> material = _model->Get_Material(materialIndex);
        if (material == nullptr)
            continue;

        if (FAILED(_shader->Bind_MaterialTextures(*material)))
            continue;

        if (FAILED(_shader->Bind_CBufferData(material->Get_CB())))
            return E_FAIL;

        if (FAILED(_shader->Begin(kSimonWeaponEchoPassIndex)))
            return E_FAIL;

        _model->Render(i);
    }

    return S_OK;
}

Shared<Mon_Simon_Weapon> Mon_Simon_Weapon::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Mon_Simon_Weapon>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> Mon_Simon_Weapon::Clone(void* arg)
{
    auto clone = make_shared<Mon_Simon_Weapon>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void Mon_Simon_Weapon::Free()
{
    Monster_Part::Free();
}
