#include "PointParticleEffect.h"
#include "GameInstance.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "VIBufferCom_PointParticle.h"

IMPLEMENT_REFLECTION(PointParticleEffect)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "PointParticleEffect";
    info.category = "Effect";

    return true;
}

PointParticleEffect::PointParticleEffect(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

PointParticleEffect::PointParticleEffect(const PointParticleEffect& prototype)
    : GameObject{ prototype }
{
}

HRESULT PointParticleEffect::Initialize_Prototype()
{
    return S_OK;
}

HRESULT PointParticleEffect::Initialize(void* arg)
{
    if (nullptr != arg)
        _desc = *static_cast<PointParticleEffectDesc*>(arg);

    CHECK_FAILED(__super::Initialize(&_desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    return S_OK;
}

void PointParticleEffect::Update(float timeDelta)
{
    if (nullptr != _viBuffer)
        _viBuffer->Spread(timeDelta);
}

void PointParticleEffect::Late_Update(float)
{
    GAME->Add_RenderGroup(RenderGroup::Blend, GetSharedPtr<GameObject>());
}

HRESULT PointParticleEffect::Render()
{
    CHECK_FAILED_ONCE(Bind_ShaderResources(), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Begin(0), E_FAIL);
    CHECK_FAILED_ONCE(_viBuffer->Bind_Resources(), E_FAIL);
    CHECK_FAILED_ONCE(_viBuffer->Render(), E_FAIL);

    return S_OK;
}

HRESULT PointParticleEffect::Ready_Components()
{
    string resolvedTextureId{};
    CHECK_FAILED(Ready_TextureComponent(resolvedTextureId), E_FAIL);
    CHECK_FAILED(Ready_ShaderComponent(), E_FAIL);
    CHECK_FAILED(Ready_NoiseTextureComponent(resolvedTextureId), E_FAIL);

    PointParticleDesc desc = Make_PointParticleDesc();
    const HRESULT hr = __super::Add_Component(
        ETOI(LevelType::Static),
        TEXT("Com_Effect_PointParticleBuffer"),
        _viBuffer,
        &desc
    );
    CHECK_FAILED(hr, E_FAIL);

    return S_OK;
}

HRESULT PointParticleEffect::Ready_TextureComponent(string& resolvedTextureId)
{
    const string requestedTextureId = _desc.textureId.empty()
                                      ? string{ kFallbackTextureId }
                                      : _desc.textureId;
    const wstring requestedTextureTag = String::ToWString(requestedTextureId);

    if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), requestedTextureTag, _texture)))
    {
        if (requestedTextureId == kFallbackTextureId)
        {
            LOG_WARN("PointParticleEffect failed to resolve fallback textureId='{}'.", kFallbackTextureId);
            return E_FAIL;
        }

        LOG_WARN(
            "PointParticleEffect failed to resolve textureId='{}'. Falling back to '{}'.",
            requestedTextureId,
            kFallbackTextureId
        );

        if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), String::ToWString(kFallbackTextureId), _texture)))
        {
            LOG_WARN("PointParticleEffect fallback texture resolve failed. textureId='{}'", kFallbackTextureId);
            return E_FAIL;
        }

        resolvedTextureId = kFallbackTextureId;
        return S_OK;
    }

    if (_desc.textureId.empty())
    {
        LOG_WARN("PointParticleEffect textureId is empty. Falling back to '{}'.", kFallbackTextureId);
        resolvedTextureId = kFallbackTextureId;
        return S_OK;
    }

    resolvedTextureId = requestedTextureId;
    return S_OK;
}

HRESULT PointParticleEffect::Ready_ShaderComponent()
{
    if (FAILED(__super::Add_Component(ETOI(LevelType::Static), String::ToWString(kEffectSpriteShaderId), _shader)))
    {
        LOG_WARN(
            "PointParticleEffect failed to resolve shaderId='{}'. Add the DT_Shader row before running effect.",
            kEffectSpriteShaderId
        );
        return E_FAIL;
    }

    _opacitySourceIndex = Resolve_OpacitySourceIndex();
    return S_OK;
}

HRESULT PointParticleEffect::Ready_NoiseTextureComponent(const string& resolvedTextureId)
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
            "PointParticleEffect failed to resolve noiseTextureId='{}'. Noise modulation is disabled.",
            _desc.noiseTextureId
        );
        _noiseTexture = nullptr;
    }

    return S_OK;
}

HRESULT PointParticleEffect::Bind_ShaderResources()
{
    CHECK_FAILED_ONCE(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix"), E_FAIL);
    CHECK_FAILED_ONCE(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix"), E_FAIL);
    CHECK_FAILED_ONCE(GAME->Bind_CamPosition(_shader.get(), "g_BillboardTarget"), E_FAIL);
    CHECK_FAILED_ONCE(_texture->Bind_ShaderResourceView(_shader.get(), "g_Texture", 0), E_FAIL);
    CHECK_FAILED_ONCE(Bind_MaterialResources(), E_FAIL);

    return S_OK;
}

HRESULT PointParticleEffect::Bind_MaterialResources()
{
    if (nullptr != _noiseTexture)
        CHECK_FAILED_ONCE(_noiseTexture->Bind_ShaderResourceView(_shader.get(), "g_NoiseTexture", 0), E_FAIL);

    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_Tint", &_desc.tint, sizeof(_desc.tint)), E_FAIL);

    const Vec4 effectSpriteParams{ _desc.intensity, _desc.opacityPower, _desc.noiseStrength, nullptr != _noiseTexture ? 1.f : 0.f };
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteParams", &effectSpriteParams, sizeof(effectSpriteParams)), E_FAIL);

    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_OpacitySource", &_opacitySourceIndex, sizeof(_opacitySourceIndex)), E_FAIL);

    return S_OK;
}

int PointParticleEffect::Resolve_OpacitySourceIndex() const
{
    if (_desc.opacitySource == "Alpha" || _desc.opacitySource == "alpha")
        return kOpacitySourceAlpha;

    if (_desc.opacitySource == "Red" || _desc.opacitySource == "red")
        return kOpacitySourceRed;

    if (_desc.opacitySource == "Luminance" || _desc.opacitySource == "luminance")
        return kOpacitySourceLuminance;

    LOG_WARN(
        "PointParticleEffect unknown opacitySource='{}'. Falling back to Alpha.",
        _desc.opacitySource
    );

    return kOpacitySourceAlpha;
}

PointParticleDesc PointParticleEffect::Make_PointParticleDesc() const
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
    desc.subUVRows = max(1u, _desc.subUVRows);
    desc.subUVCols = max(1u, _desc.subUVCols);
    desc.subUvFrameOverLife = _desc.subUVFrameOverLife;
    desc.sizeByLife = _desc.sizeByLife;
    desc.loop = _desc.loop;
    desc.drawMode = _desc.drawMode;
    return desc;
}

Shared<PointParticleEffect> PointParticleEffect::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<PointParticleEffect>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : PointParticleEffect");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> PointParticleEffect::Clone(void* arg)
{
    auto instance = make_shared<PointParticleEffect>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : PointParticleEffect");
        MSG_BOX("Failed to Clone : PointParticleEffect");
        return nullptr;
    }

    return instance;
}

void PointParticleEffect::Free()
{
}
