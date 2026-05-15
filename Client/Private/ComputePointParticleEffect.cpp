#include "ComputePointParticleEffect.h"

#include "GameInstance.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "VIBufferCom_ComputePointParticle.h"

IMPLEMENT_REFLECTION(ComputePointParticleEffect)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "ComputePointParticleEffect";
    info.category = "Effect";

    return true;
}

ComputePointParticleEffect::ComputePointParticleEffect(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

ComputePointParticleEffect::ComputePointParticleEffect(const ComputePointParticleEffect& prototype)
    : GameObject{ prototype }
{
}

ComputePointParticleEffect::~ComputePointParticleEffect()
{
    Free();
}

HRESULT ComputePointParticleEffect::Initialize_Prototype()
{
    return S_OK;
}

HRESULT ComputePointParticleEffect::Initialize(void* arg)
{
    if (nullptr != arg)
        _desc = *static_cast<ComputePointParticleEffectDesc*>(arg);

    CHECK_FAILED(__super::Initialize(&_desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    return S_OK;
}

void ComputePointParticleEffect::Update(float timeDelta)
{
    if (nullptr != _viBuffer)
        _viBuffer->Dispatch_Compute(timeDelta);
}

void ComputePointParticleEffect::Late_Update(float timeDelta)
{
    GAME->Add_RenderGroup(RenderGroup::Blend, GetSharedPtr<GameObject>());
}

HRESULT ComputePointParticleEffect::Render()
{
    CHECK_FAILED_ONCE(Bind_ShaderResources(), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Begin(0), E_FAIL);
    CHECK_FAILED_ONCE(_viBuffer->Bind_Resources(), E_FAIL);
    CHECK_FAILED_ONCE(_viBuffer->Render(), E_FAIL);

    return S_OK;
}

HRESULT ComputePointParticleEffect::Ready_Components()
{
    string resolvedTextureId{};
    CHECK_FAILED(Ready_TextureComponent(resolvedTextureId), E_FAIL);
    CHECK_FAILED(Ready_ShaderComponent(), E_FAIL);
    CHECK_FAILED(Ready_NoiseTextureComponent(resolvedTextureId), E_FAIL);

    ComputePointParticleDesc desc = Make_ComputePointParticleDesc();
    CHECK_FAILED(__super::Add_Component(ETOI(LevelType::Static), TEXT("Com_Effect_ComputePointParticleBuffer"), _viBuffer, &desc), E_FAIL);

    LOG_INFO(
        "ComputePointParticleEffect ready: shaderId='{}', textureId='{}', noiseTextureId='{}', opacitySource='{}', noiseStrength={}",
        kEffectSpriteShaderId,
        _desc.textureId,
        _desc.noiseTextureId,
        _desc.opacitySource,
        _desc.noiseStrength
    );

    return S_OK;
}

HRESULT ComputePointParticleEffect::Ready_TextureComponent(string& outResolvedTextureId)
{
    const string requestedTextureId =
        !_desc.textureId.empty() ? _desc.textureId : string{ kFallbackTextureId };

    if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), String::ToWString(requestedTextureId), _texture)))
    {
        if (requestedTextureId == kFallbackTextureId)
        {
            LOG_WARN("ComputePointParticleEffect failed to resolve fallback textureId='{}'.", kFallbackTextureId);
            return E_FAIL;
        }

        CHECK_FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), String::ToWString(kFallbackTextureId), _texture), E_FAIL);

        outResolvedTextureId = kFallbackTextureId;
        return S_OK;
    }

    outResolvedTextureId = requestedTextureId;
    return S_OK;
}

HRESULT ComputePointParticleEffect::Ready_ShaderComponent()
{
    CHECK_FAILED(__super::Add_Component(ETOI(LevelType::Static), String::ToWString(kEffectSpriteShaderId), _shader), E_FAIL);
    _opacitySourceIndex = Resolve_OpacitySourceIndex();
    return S_OK;
}

HRESULT ComputePointParticleEffect::Ready_NoiseTextureComponent(const string& outResolvedTextureId)
{
    if (_desc.noiseTextureId.empty())
        return S_OK;

    if (_desc.noiseTextureId == outResolvedTextureId && nullptr != _texture)
    {
        _noiseTexture = _texture;
        return S_OK;
    }

    if (FAILED(__super::Add_Component(ETOI(LevelType::GamePlay), String::ToWString(_desc.noiseTextureId), _noiseTexture)))
        _noiseTexture = nullptr;

    return S_OK;
}

HRESULT ComputePointParticleEffect::Bind_ShaderResources()
{
    CHECK_FAILED_ONCE(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix"), E_FAIL);
    CHECK_FAILED_ONCE(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix"), E_FAIL);
    CHECK_FAILED_ONCE(GAME->Bind_CamPosition(_shader.get(), "g_BillboardTarget"), E_FAIL);
    CHECK_FAILED_ONCE(_texture->Bind_ShaderResourceView(_shader.get(), "g_Texture", 0), E_FAIL);
    CHECK_FAILED_ONCE(Bind_MaterialResources(), E_FAIL);

    return S_OK;
}

HRESULT ComputePointParticleEffect::Bind_MaterialResources()
{
    if (nullptr != _noiseTexture)
        CHECK_FAILED_ONCE(_noiseTexture->Bind_ShaderResourceView(_shader.get(), "g_NoiseTexture", 0), E_FAIL);

    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_Tint", &_desc.tint, sizeof(_desc.tint)), E_FAIL);

    const Vec4 effectSpriteParams = Vec4(
        _desc.intensity,
        _desc.opacityPower,
        _desc.noiseStrength,
        nullptr != _noiseTexture ? 1.f : 0.f
    );
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteParams", &effectSpriteParams, sizeof(effectSpriteParams)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_OpacitySource", &_opacitySourceIndex, sizeof(_opacitySourceIndex)), E_FAIL);

    return S_OK;
}

int ComputePointParticleEffect::Resolve_OpacitySourceIndex() const
{
    if (_desc.opacitySource == "Alpha" || _desc.opacitySource == "alpha")
        return kOpacitySourceAlpha;

    if (_desc.opacitySource == "Red" || _desc.opacitySource == "red")
        return kOpacitySourceRed;

    if (_desc.opacitySource == "Luminance" || _desc.opacitySource == "luminance")
        return kOpacitySourceLuminance;

    return kOpacitySourceAlpha;
}

ComputePointParticleDesc ComputePointParticleEffect::Make_ComputePointParticleDesc() const
{
    ComputePointParticleDesc desc{};
    desc.instanceCount = max(1u, _desc.instanceCount);
    desc.center = _desc.center;
    desc.range = Vec3{ 3.f, 1.f, 3.f };
    desc.scale = _desc.scale;
    desc.lifeTime = _desc.lifeTime;
    desc.startColorMin = _desc.startColorMin;
    desc.startColorMax = _desc.startColorMax;
    desc.endColorMin = _desc.endColorMin;
    desc.endColorMax = _desc.endColorMax;
    desc.subUVRows = max(1u, _desc.subUVRows);
    desc.subUVCols = max(1u, _desc.subUVCols);
    desc.subUvFrameOverLife = _desc.subUVFrameOverLife;
    desc.sizeByLife = _desc.sizeByLife;
    desc.radius = Vec2{ 1.2f, 2.2f };
    desc.drawMode = PointParticleDrawMode::DrawIndexedInstancedIndirect;
    desc.computeShaderLevelIndex = ETOI(LevelType::Static);
    desc.computeShaderPrototypeTag = L"ComputeShader_PointParticle";

    return desc;
}

Shared<ComputePointParticleEffect> ComputePointParticleEffect::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<ComputePointParticleEffect>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : ComputePointParticleEffect");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> ComputePointParticleEffect::Clone(void* arg)
{
    auto instance = make_shared<ComputePointParticleEffect>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : ComputePointParticleEffect");
        MSG_BOX("Failed to Clone : ComputePointParticleEffect");
        return nullptr;
    }

    return instance;
}

void ComputePointParticleEffect::Free()
{
}
