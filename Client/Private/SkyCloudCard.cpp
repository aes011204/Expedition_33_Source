#include "SkyCloudCard.h"
#include "GameInstance.h"
#include "ModelCom.h"
#include "ShaderCom.h"
#include "Texture.h"

IMPLEMENT_REFLECTION(SkyCloudCard)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "SkyCloudCard";
    info.category = "World";

    PROPERTY_VEC4("Tint", _tint, 0.01f);
    PROPERTY_VEC4("Sky Scatter Color", _skyScatterColor, 0.01f);
    PROPERTY_VEC4("Ground Scatter Color", _groundScatterColor, 0.01f);
    PROPERTY_VEC4("Light Offset", _lightOffset, 0.01f);
    PROPERTY_FLOAT("Scatter Blend", _scatterBlend, 0.f, 0.f, 0.001f);
    PROPERTY_FLOAT("Shadow Blur", _shadowBlur, 0.f, 0.f, 0.001f);
    PROPERTY_FLOAT("Glow", _glow, 0.f, 0.f, 0.1f);
    PROPERTY_FLOAT("Opacity", _opacity, 0.f, 1.f, 0.01f);

    return true;
}

SkyCloudCard::SkyCloudCard(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

SkyCloudCard::SkyCloudCard(const SkyCloudCard& prototype)
    : GameObject{ prototype }
{
}

HRESULT SkyCloudCard::Initialize_Prototype()
{
    return S_OK;
}

HRESULT SkyCloudCard::Initialize(void* pArg)
{
    CHECK_FAILED(GameObject::Initialize(pArg), E_FAIL);

    SKYCLOUDCARD_DESC desc{};
    if (pArg != nullptr)
        desc = *static_cast<SKYCLOUDCARD_DESC*>(pArg);

    _tint = desc.tint;
    _skyScatterColor = desc.skyScatterColor;
    _groundScatterColor = desc.groundScatterColor;
    _lightOffset = desc.lightOffset;
    _scatterBlend = desc.scatterBlend;
    _shadowBlur = desc.shadowBlur;
    _glow = desc.glow;
    _opacity = desc.opacity;

    CHECK_FAILED(Ready_Components(desc), E_FAIL);

    _transformCom->Set_Position(desc.position);
    _transformCom->Set_RotationEuler(desc.rotation);
    _transformCom->Set_Scale(desc.scale);

    return S_OK;
}

void SkyCloudCard::Priority_Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void SkyCloudCard::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void SkyCloudCard::Late_Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
    GAME->Add_RenderGroup(RenderGroup::Blend, GetSharedPtr<GameObject>());
}

HRESULT SkyCloudCard::Render()
{
    CHECK_FAILED(Bind_ShaderResources(), E_FAIL);
    CHECK_NULL(_model, E_FAIL);

    const uint32 meshCount = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 i = 0; i < meshCount; ++i)
    {
        CHECK_FAILED(_shader->Begin(0), E_FAIL);
        CHECK_FAILED(_model->Render(i), E_FAIL);
    }

    return S_OK;
}

HRESULT SkyCloudCard::Ready_Components(const SKYCLOUDCARD_DESC& desc)
{
    CHECK_FAILED(__super::Add_Component(ETOI(LevelType::Static), desc.modelTag.c_str(), _model), E_FAIL);
    CHECK_FAILED(__super::Add_Component(ETOI(LevelType::Static), desc.shaderTag.c_str(), _shader), E_FAIL);
    CHECK_FAILED(__super::Add_Component(ETOI(LevelType::Static), desc.cloudTextureTag.c_str(), _cloudTexture), E_FAIL);

    return S_OK;
}

HRESULT SkyCloudCard::Bind_ShaderResources()
{
    CHECK_FAILED(_shader->Bind_ObjectCB(_transformCom->Get_WorldMatrix()), E_FAIL);
    CHECK_FAILED(GAME->Bind_CameraCB(_shader), E_FAIL);
    CHECK_FAILED(_cloudTexture->Bind_ShaderResourceView(_shader.get(), "g_CloudAlphaTexture", 0), E_FAIL);

    DrawFeatureCB cb{};
    cb.param0 = _tint;
    cb.param1 = _skyScatterColor;
    cb.param2 = _groundScatterColor;
    cb.param3 = Vec4(_scatterBlend, _shadowBlur, _glow, _opacity);

    CHECK_FAILED(_shader->Bind_CBufferData(cb), E_FAIL);

    return S_OK;
}

Shared<SkyCloudCard> SkyCloudCard::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<SkyCloudCard>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> SkyCloudCard::Clone(void* pArg)
{
    auto instance = make_shared<SkyCloudCard>(*this);

    if (FAILED(instance->Initialize(pArg)))
        return nullptr;

    return instance;
}

void SkyCloudCard::Free()
{
    __super::Free();
}
