#include "PointParticlePreview.h"
#include "GameInstance.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "VIBufferCom_PointParticle.h"

IMPLEMENT_REFLECTION(PointParticlePreview)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "PointParticlePreview";
    info.category = "Effect";

    return true;
}

PointParticlePreview::PointParticlePreview(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

PointParticlePreview::PointParticlePreview(const PointParticlePreview& prototype)
    : GameObject{ prototype }
{
}

HRESULT PointParticlePreview::Initialize_Prototype()
{
    return S_OK;
}

HRESULT PointParticlePreview::Initialize(void* arg)
{
    if (nullptr != arg)
        _desc = *static_cast<PointParticlePreviewDesc*>(arg);

    CHECK_FAILED(__super::Initialize(&_desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    return S_OK;
}

void PointParticlePreview::Update(float timeDelta)
{
    if (nullptr != _viBuffer)
        _viBuffer->Spread(timeDelta);
}

void PointParticlePreview::Late_Update(float)
{
    GAME->Add_RenderGroup(RenderGroup::Blend, GetSharedPtr<GameObject>());
}

HRESULT PointParticlePreview::Render()
{
    CHECK_FAILED_ONCE(Bind_ShaderResources(), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Begin(0), E_FAIL);
    CHECK_FAILED_ONCE(_viBuffer->Bind_Resources(), E_FAIL);
    CHECK_FAILED_ONCE(_viBuffer->Render(), E_FAIL);

    return S_OK;
}

HRESULT PointParticlePreview::Ready_Components()
{
    string resolvedTextureId{};
    CHECK_FAILED(Ready_TextureComponent(resolvedTextureId), E_FAIL);
    CHECK_FAILED(Ready_ShaderComponent(), E_FAIL);
    CHECK_FAILED(Ready_NoiseTextureComponent(resolvedTextureId), E_FAIL);

    PointParticleDesc desc = Make_PointParticleDesc();
    const HRESULT hr = __super::Add_Component(
        ETOI(LevelType::Static),
        TEXT("Com_VIBuffer_PointParticle"),
        _viBuffer,
        &desc
    );
    CHECK_FAILED(hr, E_FAIL);

    return S_OK;
}

HRESULT PointParticlePreview::Ready_TextureComponent(string& resolvedTextureId)
{
    const string requestedTextureId = _desc.textureId.empty()
                                      ? string{ kFallbackTextureId }
                                      : _desc.textureId;
    const wstring requestedTextureTag = String::ToWString(requestedTextureId);

    if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), requestedTextureTag, _texture)))
    {
        if (requestedTextureId == kFallbackTextureId)
        {
            LOG_WARN("PointParticlePreview failed to resolve fallback textureId='{}'.", kFallbackTextureId);
            return E_FAIL;
        }

        LOG_WARN(
            "PointParticlePreview failed to resolve textureId='{}'. Falling back to '{}'.",
            requestedTextureId,
            kFallbackTextureId
        );

        if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), String::ToWString(kFallbackTextureId), _texture)))
        {
            LOG_WARN("PointParticlePreview fallback texture resolve failed. textureId='{}'", kFallbackTextureId);
            return E_FAIL;
        }

        resolvedTextureId = kFallbackTextureId;
        return S_OK;
    }

    if (_desc.textureId.empty())
    {
        LOG_WARN("PointParticlePreview textureId is empty. Falling back to '{}'.", kFallbackTextureId);
        resolvedTextureId = kFallbackTextureId;
        return S_OK;
    }

    resolvedTextureId = requestedTextureId;
    return S_OK;
}

HRESULT PointParticlePreview::Ready_ShaderComponent()
{
    if (FAILED(__super::Add_Component(ETOI(LevelType::Static), String::ToWString(kEffectSpriteShaderId), _shader)))
    {
        LOG_WARN(
            "PointParticlePreview failed to resolve shaderId='{}'. Add the DT_Shader row before running preview.",
            kEffectSpriteShaderId
        );
        return E_FAIL;
    }

    _opacitySourceIndex = Resolve_OpacitySourceIndex();
    return S_OK;
}

HRESULT PointParticlePreview::Ready_NoiseTextureComponent(const string& resolvedTextureId)
{
    if (_desc.noiseTextureId.empty())
        return S_OK;

    if (_desc.noiseTextureId == resolvedTextureId && nullptr != _texture)
    {
        _noiseTexture = _texture;
        return S_OK;
    }

    if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), String::ToWString(_desc.noiseTextureId), _noiseTexture)))
    {
        LOG_WARN(
            "PointParticlePreview failed to resolve noiseTextureId='{}'. Noise modulation is disabled.",
            _desc.noiseTextureId
        );
        _noiseTexture = nullptr;
    }

    return S_OK;
}

HRESULT PointParticlePreview::Bind_ShaderResources()
{
    CHECK_FAILED_ONCE(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix"), E_FAIL);
    CHECK_FAILED_ONCE(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix"), E_FAIL);
    CHECK_FAILED_ONCE(GAME->Bind_CamPosition(_shader.get(), "g_vBillboardTarget"), E_FAIL);
    CHECK_FAILED_ONCE(_texture->Bind_ShaderResourceView(_shader.get(), "g_Texture", 0), E_FAIL);
    CHECK_FAILED_ONCE(Bind_MaterialResources(), E_FAIL);

    return S_OK;
}

HRESULT PointParticlePreview::Bind_MaterialResources()
{
    if (nullptr != _noiseTexture)
        CHECK_FAILED_ONCE(_noiseTexture->Bind_ShaderResourceView(_shader.get(), "g_NoiseTexture", 0), E_FAIL);

    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_vTint", &_desc.tint, sizeof(_desc.tint)), E_FAIL);

    const Vec4 effectSpriteParams{ _desc.intensity, _desc.opacityPower, _desc.noiseStrength, nullptr != _noiseTexture ? 1.f : 0.f };
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_vEffectSpriteParams", &effectSpriteParams, sizeof(effectSpriteParams)), E_FAIL);

    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_iOpacitySource", &_opacitySourceIndex, sizeof(_opacitySourceIndex)), E_FAIL);

    return S_OK;
}

int PointParticlePreview::Resolve_OpacitySourceIndex() const
{
    if (_desc.opacitySource == "Alpha" || _desc.opacitySource == "alpha")
        return kOpacitySourceAlpha;

    if (_desc.opacitySource == "Red" || _desc.opacitySource == "red")
        return kOpacitySourceRed;

    if (_desc.opacitySource == "Luminance" || _desc.opacitySource == "luminance")
        return kOpacitySourceLuminance;

    LOG_WARN(
        "PointParticlePreview unknown opacitySource='{}'. Falling back to Alpha.",
        _desc.opacitySource
    );

    return kOpacitySourceAlpha;
}

PointParticleDesc PointParticlePreview::Make_PointParticleDesc() const
{
    PointParticleDesc desc{};
    desc.instanceCount = max(1u, _desc.instanceCount);
    desc.center = _desc.center;
    desc.range = Vec3{ 2.f, 2.f, 2.f };
    desc.scale = _desc.scale;
    desc.pivot = _desc.pivot;
    desc.speed = _desc.speed;
    desc.lifeTime = _desc.lifeTime;
    desc.startColorMin = _desc.startColorMin;
    desc.startColorMax = _desc.startColorMax;
    desc.endColorMin = _desc.endColorMin;
    desc.endColorMax = _desc.endColorMax;
    desc.loop = _desc.loop;
    desc.drawMode = _desc.drawMode;

    return desc;
}

Shared<PointParticlePreview> PointParticlePreview::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<PointParticlePreview>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : PointParticlePreview");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> PointParticlePreview::Clone(void* arg)
{
    auto instance = make_shared<PointParticlePreview>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : PointParticlePreview");
        MSG_BOX("Failed to Clone : PointParticlePreview");
        return nullptr;
    }

    return instance;
}

void PointParticlePreview::Free()
{
    __super::Free();
}
