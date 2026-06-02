#include "ComputeSpriteEmitter.h"

#include "Battle_Enum.h"
#include "GameInstance.h"
#include "Renderer_Define.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "VIBufferCom_ComputePointParticle.h"

IMPLEMENT_REFLECTION(ComputeSpriteEmitter)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "ComputeSpriteEmitter";
    info.category = "EffectEmitter";

    return true;
}

void ComputeSpriteEmitter::Set_InfluenceOutlineDebugEnabled(bool enabled)
{
    _isInfluenceOutlineDebugEnabled = enabled;
}

bool ComputeSpriteEmitter::Is_InfluenceOutlineDebugEnabled()
{
    return _isInfluenceOutlineDebugEnabled;
}

namespace Client::Internal
{
wstring Resolve_TexturePathByGuid(const string& textureGuid)
{
    if (GAME == nullptr || textureGuid.empty())
        return {};

    const AssetMeta* assetMeta = GAME->Find_AssetByGUID(textureGuid);
    if (assetMeta == nullptr || assetMeta->type != "Texture")
        return {};

    const wstring resolvedPath = GAME->Resolve_AssetPath(textureGuid);
    return !resolvedPath.empty() && fs::exists(resolvedPath) ? resolvedPath : wstring{};
}

wstring Resolve_TexturePathByPath(const string& texturePath)
{
    if (!texturePath.empty())
    {
        fs::path candidatePath = String::ToWString(texturePath);
        if (candidatePath.is_relative() && GAME != nullptr)
            candidatePath = fs::path(GAME->Get_AssetRoot()) / candidatePath;

        candidatePath = candidatePath.lexically_normal();
        if (fs::exists(candidatePath))
            return candidatePath.wstring();
    }

    return {};
}

bool Is_SpriteTextureSamePath(const wstring& lhs, const wstring& rhs)
{
    if (lhs.empty() || rhs.empty())
        return false;

    return fs::path(lhs).lexically_normal() == fs::path(rhs).lexically_normal();
}

wstring Resolve_TexturePath(const string& textureGuid, const string& texturePath, bool& outUsedPathFallback, bool& outGuidPathMismatch)
{
    outUsedPathFallback = false;
    outGuidPathMismatch = false;

    wstring resolvedPath = Resolve_TexturePathByGuid(textureGuid);
    if (!resolvedPath.empty())
    {
        const wstring pathResolved = Resolve_TexturePathByPath(texturePath);
        outGuidPathMismatch = !pathResolved.empty() && !Is_SpriteTextureSamePath(resolvedPath, pathResolved);
        return resolvedPath;
    }

    resolvedPath = Resolve_TexturePathByPath(texturePath);
    if (!resolvedPath.empty())
    {
        outUsedPathFallback = true;
        return resolvedPath;
    }

    return {};
}
}

ComputeSpriteEmitter::ComputeSpriteEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : EffectEmitter{ device, context }
{
}

ComputeSpriteEmitter::ComputeSpriteEmitter(const ComputeSpriteEmitter& prototype)
    : EffectEmitter{ prototype }
{
}

ComputeSpriteEmitter::~ComputeSpriteEmitter()
{
    Free();
}

HRESULT ComputeSpriteEmitter::Initialize_Prototype()
{
    return S_OK;
}

HRESULT ComputeSpriteEmitter::Initialize(void* arg)
{
    if (nullptr != arg)
        _desc = *static_cast<ComputeSpriteEmitterDesc*>(arg);

    CHECK_FAILED(__super::Initialize(&_desc), E_FAIL);
    Reset_PlaybackRuntime();

    CHECK_FAILED(Ready_Components(), E_FAIL);

    return S_OK;
}

void ComputeSpriteEmitter::Update(float timeDelta)
{
    Sync_FromEffectOwner();

    if (nullptr != _viBuffer)
    {
        const PlaybackTick playbackTick = Advance_Playback(timeDelta);
        _viBuffer->Dispatch_Compute(
            playbackTick.computeDeltaTime,
            playbackTick.spawnRequest,
            playbackTick.killActiveParticles
        );
    }
}

void ComputeSpriteEmitter::Late_Update(float timeDelta)
{
    if (Can_SubmitRender())
        GAME->Add_RenderGroup(Resolve_RenderGroup(), GetSharedPtr<GameObject>());
}

HRESULT ComputeSpriteEmitter::Render()
{
    CHECK_FAILED_ONCE(Bind_ShaderResources(), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Begin(Resolve_ShaderPassIndex()), E_FAIL);
    CHECK_FAILED_ONCE(_viBuffer->Bind_Resources(), E_FAIL);
    CHECK_FAILED_ONCE(_viBuffer->Render(), E_FAIL);

    return S_OK;
}

HRESULT ComputeSpriteEmitter::Reset_ForEffectReplay()
{
    Reset_PlaybackRuntime();
    Sync_WorldCenterToComputeBuffer();
    CHECK_NULL(_viBuffer, E_FAIL);
    CHECK_FAILED(_viBuffer->Reset_ForEffectReplay(), E_FAIL);

    return S_OK;
}

bool ComputeSpriteEmitter::Is_EffectFinished() const
{
    return PlaybackState::Completed == _playbackState;
}

bool ComputeSpriteEmitter::Try_Get_BlendSortWorldPosition(Vec3& outWorldPosition) const
{
    if (_transformCom == nullptr)
        return false;

    const Vec4* cameraPosition4 = GAME != nullptr ? GAME->Get_CamPosition() : nullptr;
    if (cameraPosition4 == nullptr)
    {
        outWorldPosition = _transformCom->Get_WorldPosition();
        return true;
    }

    const auto max_abs = [](float a, float b) -> float
    {
        return max(fabsf(a), fabsf(b));
    };

    const auto max_curve_value = [](const Vec4& values, uint32 keyCount) -> float
    {
        float result = values.x;
        const uint32 safeKeyCount = min(keyCount, 4u);
        for (uint32 index = 1u; index < safeKeyCount; ++index)
        {
            switch (index)
            {
            case 1u: result = max(result, values.y);
                break;
            case 2u: result = max(result, values.z);
                break;
            case 3u: result = max(result, values.w);
                break;
            default: break;
            }
        }
        return result;
    };

    const Vec3 emitterWorldPosition = _transformCom->Get_WorldPosition();

    Vec3 localOffset{};
    if (_desc.initialLocation.enabled)
        localOffset += (_desc.initialLocation.minOffset + _desc.initialLocation.maxOffset) * 0.5f;

    if (_desc.sphereLocation.enabled)
        localOffset += _desc.sphereLocation.offset;

    const Vec3 worldOffset =
        _transformCom->Get_WorldRight() * localOffset.x +
        _transformCom->Get_WorldUp() * localOffset.y +
        _transformCom->Get_WorldForward() * localOffset.z;

    const Vec3 representativeCenter = emitterWorldPosition + worldOffset;

    outWorldPosition = representativeCenter;
    return true;
}

void ComputeSpriteEmitter::On_EffectTransformSynced()
{
    Sync_WorldCenterToComputeBuffer();
}

HRESULT ComputeSpriteEmitter::Ready_Components()
{
    string resolvedTexturePath{};
    CHECK_FAILED(Ready_TextureComponent(resolvedTexturePath), E_FAIL);
    CHECK_FAILED(Ready_ShaderComponent(), E_FAIL);
    CHECK_FAILED(Ready_NoiseTextureComponent(resolvedTexturePath), E_FAIL);
    CHECK_FAILED(Ready_MaskTextureComponent(resolvedTexturePath), E_FAIL);
    CHECK_FAILED(Ready_FlowTextureComponent(resolvedTexturePath), E_FAIL);

    ComputePointParticleDesc desc = Make_ComputePointParticleDesc();
    CHECK_FAILED(__super::Add_Component(ETOI(LevelType::Static), TEXT("Com_Effect_ComputePointParticleBuffer"), _viBuffer, &desc), E_FAIL);

    return S_OK;
}

HRESULT ComputeSpriteEmitter::Ready_TextureComponent(string& outResolvedTexturePath)
{
    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    wstring resolvedPath = Client::Internal::Resolve_TexturePath(
        _desc.required.material.mainTextureGuid,
        _desc.required.material.mainTexturePath,
        usedPathFallback,
        guidPathMismatch
    );
    bool usedFallback = false;
    if (guidPathMismatch)
    {
        LOG_WARN(
            "ComputeSpriteEmitter main texture GUID/path mismatch. mainTextureGuid='{}', mainTexturePath='{}', guidResolvedPath='{}'.",
            _desc.required.material.mainTextureGuid,
            _desc.required.material.mainTexturePath,
            String::ToString(resolvedPath)
        );
    }
    else if (usedPathFallback)
    {
        LOG_WARN(
            "ComputeSpriteEmitter main texture fell back to path. mainTextureGuid='{}', mainTexturePath='{}', resolvedPath='{}'.",
            _desc.required.material.mainTextureGuid,
            _desc.required.material.mainTexturePath,
            String::ToString(resolvedPath)
        );
    }

    if (resolvedPath.empty())
    {
        LOG_WARN(
            "ComputeSpriteEmitter main texture resolve failed. mainTextureGuid='{}', mainTexturePath='{}'. Falling back to fallbackPath='{}'.",
            _desc.required.material.mainTextureGuid,
            _desc.required.material.mainTexturePath,
            kFallbackTexturePath
        );

        resolvedPath = Client::Internal::Resolve_TexturePathByPath(kFallbackTexturePath);
        usedFallback = true;
    }

    if (resolvedPath.empty())
    {
        LOG_ERROR(
            "ComputeSpriteEmitter failed to resolve fallback texture. mainTextureGuid='{}', mainTexturePath='{}', fallbackPath='{}'.",
            _desc.required.material.mainTextureGuid,
            _desc.required.material.mainTexturePath,
            kFallbackTexturePath
        );
        return E_FAIL;
    }

    outResolvedTexturePath = String::ToString(resolvedPath);
    _texture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    if (_texture == nullptr)
    {
        LOG_ERROR(
            "ComputeSpriteEmitter texture create failed. mainTextureGuid='{}', mainTexturePath='{}', resolvedPath='{}', fallbackPath='{}', usedFallback={}.",
            _desc.required.material.mainTextureGuid,
            _desc.required.material.mainTexturePath,
            outResolvedTexturePath,
            kFallbackTexturePath,
            usedFallback
        );
        return E_FAIL;
    }

    return S_OK;
}

HRESULT ComputeSpriteEmitter::Ready_ShaderComponent()
{
    CHECK_FAILED(__super::Add_Component(ETOI(LevelType::Static), String::ToWString(Resolve_ShaderId()), _shader), E_FAIL);
    _opacitySourceIndex = Resolve_OpacitySourceIndex();
    return S_OK;
}

HRESULT ComputeSpriteEmitter::Ready_NoiseTextureComponent(const string& resolvedTexturePath)
{
    if (_desc.required.material.noiseTextureGuid.empty() && _desc.required.material.noiseTexturePath.empty())
        return S_OK;

    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    const wstring resolvedPath = Client::Internal::Resolve_TexturePath(
        _desc.required.material.noiseTextureGuid,
        _desc.required.material.noiseTexturePath,
        usedPathFallback,
        guidPathMismatch
    );
    if (resolvedPath.empty())
    {
        _noiseTexture = nullptr;
        LOG_WARN(
            "ComputeSpriteEmitter noise texture resolve failed. noiseTextureGuid='{}', noiseTexturePath='{}'.",
            _desc.required.material.noiseTextureGuid,
            _desc.required.material.noiseTexturePath
        );
        return S_OK;
    }
    if (guidPathMismatch)
    {
        LOG_WARN(
            "ComputeSpriteEmitter noise texture GUID/path mismatch. noiseTextureGuid='{}', noiseTexturePath='{}', guidResolvedPath='{}'.",
            _desc.required.material.noiseTextureGuid,
            _desc.required.material.noiseTexturePath,
            String::ToString(resolvedPath)
        );
    }
    else if (usedPathFallback)
    {
        LOG_WARN(
            "ComputeSpriteEmitter noise texture fell back to path. noiseTextureGuid='{}', noiseTexturePath='{}', resolvedPath='{}'.",
            _desc.required.material.noiseTextureGuid,
            _desc.required.material.noiseTexturePath,
            String::ToString(resolvedPath)
        );
    }

    const string noiseTexturePath = String::ToString(resolvedPath);
    if (noiseTexturePath == resolvedTexturePath && nullptr != _texture)
    {
        _noiseTexture = _texture;
        return S_OK;
    }

    _noiseTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    if (_noiseTexture == nullptr)
    {
        LOG_WARN(
            "ComputeSpriteEmitter noise texture create failed. noiseTextureGuid='{}', noiseTexturePath='{}', resolvedPath='{}'.",
            _desc.required.material.noiseTextureGuid,
            _desc.required.material.noiseTexturePath,
            noiseTexturePath
        );
    }

    return S_OK;
}

HRESULT ComputeSpriteEmitter::Ready_MaskTextureComponent(const string& resolvedTexturePath)
{
    if (_desc.required.material.maskTextureGuid.empty() && _desc.required.material.maskTexturePath.empty())
        return S_OK;

    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    const wstring resolvedPath = Client::Internal::Resolve_TexturePath(
        _desc.required.material.maskTextureGuid,
        _desc.required.material.maskTexturePath,
        usedPathFallback,
        guidPathMismatch
    );
    if (resolvedPath.empty())
    {
        _maskTexture = nullptr;
        LOG_WARN(
            "ComputeSpriteEmitter mask texture resolve failed. maskTextureGuid='{}', maskTexturePath='{}'.",
            _desc.required.material.maskTextureGuid,
            _desc.required.material.maskTexturePath
        );
        return S_OK;
    }
    if (guidPathMismatch)
    {
        LOG_WARN(
            "ComputeSpriteEmitter mask texture GUID/path mismatch. maskTextureGuid='{}', maskTexturePath='{}', guidResolvedPath='{}'.",
            _desc.required.material.maskTextureGuid,
            _desc.required.material.maskTexturePath,
            String::ToString(resolvedPath)
        );
    }
    else if (usedPathFallback)
    {
        LOG_WARN(
            "ComputeSpriteEmitter mask texture fell back to path. maskTextureGuid='{}', maskTexturePath='{}', resolvedPath='{}'.",
            _desc.required.material.maskTextureGuid,
            _desc.required.material.maskTexturePath,
            String::ToString(resolvedPath)
        );
    }

    const string maskTexturePath = String::ToString(resolvedPath);
    if (maskTexturePath == resolvedTexturePath && nullptr != _texture)
    {
        _maskTexture = _texture;
        return S_OK;
    }

    _maskTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    if (_maskTexture == nullptr)
    {
        LOG_WARN(
            "ComputeSpriteEmitter mask texture create failed. maskTextureGuid='{}', maskTexturePath='{}', resolvedPath='{}'.",
            _desc.required.material.maskTextureGuid,
            _desc.required.material.maskTexturePath,
            maskTexturePath
        );
    }

    return S_OK;
}

HRESULT ComputeSpriteEmitter::Ready_FlowTextureComponent(const string& resolvedTexturePath)
{
    if (!Is_DistortionFamily())
        return S_OK;

    if (_desc.required.material.flowTextureGuid.empty() && _desc.required.material.flowTexturePath.empty())
        return S_OK;

    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    const wstring resolvedPath = Internal::Resolve_TexturePath(
        _desc.required.material.flowTextureGuid,
        _desc.required.material.flowTexturePath,
        usedPathFallback,
        guidPathMismatch
    );

    if (resolvedPath.empty())
    {
        LOG_WARN(
            "ComputeSpriteEmitter flow texture resolve failed. flowTextureGuid='{}', flowTexturePath='{}'.",
            _desc.required.material.flowTextureGuid,
            _desc.required.material.flowTexturePath
        );
        return S_OK;
    }

    const string flowTexturePath = String::ToString(resolvedPath);
    if (flowTexturePath == resolvedTexturePath && nullptr != _texture)
    {
        _flowTexture = _texture;
        return S_OK;
    }

    _flowTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    if (nullptr == _flowTexture)
    {
        LOG_WARN(
            "ComputeSpriteEmitter flow texture create failed. flowTextureGuid='{}', flowTexturePath='{}', resolvedPath='{}'.",
            _desc.required.material.flowTextureGuid,
            _desc.required.material.flowTexturePath,
            flowTexturePath
        );
    }

    return S_OK;
}

HRESULT ComputeSpriteEmitter::Bind_ShaderResources()
{
    CHECK_FAILED_ONCE(GAME->Bind_CameraCB(_shader), E_FAIL);
    const int screenAlignmentMode = static_cast<int>(_desc.required.spriteRender.screenAlignment);
    const int directionalAlignmentMode = static_cast<int>(_desc.required.spriteRender.directionalAlignmentMode);
    const int spriteTextureAxis = static_cast<int>(_desc.required.spriteRender.spriteTextureAxis);
    const float spriteRollOffsetRadians = XMConvertToRadians(_desc.required.spriteRender.spriteRollOffsetDegrees);
    const Vec4 emitterCenter = nullptr != _transformCom
                               ? Vec4{ _transformCom->Get_WorldPosition().x, _transformCom->Get_WorldPosition().y, _transformCom->Get_WorldPosition().z, 1.f }
                               : Vec4{ _desc.center.x, _desc.center.y, _desc.center.z, 1.f };
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_ScreenAlignmentMode", &screenAlignmentMode, sizeof(screenAlignmentMode)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_DirectionalAlignmentMode", &directionalAlignmentMode, sizeof(directionalAlignmentMode)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_SpriteTextureAxis", &spriteTextureAxis, sizeof(spriteTextureAxis)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_SpriteRollOffsetRadians", &spriteRollOffsetRadians, sizeof(spriteRollOffsetRadians)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteEmitterCenter", &emitterCenter, sizeof(emitterCenter)), E_FAIL);
    // 현재 sprite effect 경로에서는 주 텍스처를 lit base color가 아니라 emissive-like color + opacity source로 해석한다.
    CHECK_FAILED_ONCE(_texture->Bind_ShaderResourceView(_shader.get(), "g_Texture", 0), E_FAIL);
    CHECK_FAILED_ONCE(Bind_MaterialResources(), E_FAIL);
    if (Is_DistortionFamily())
        CHECK_FAILED_ONCE(Bind_DistortionResources(), E_FAIL);

    return S_OK;
}

HRESULT ComputeSpriteEmitter::Bind_MaterialResources()
{
    const auto& material = _desc.required.material;

    if (nullptr != _noiseTexture)
        CHECK_FAILED_ONCE(_noiseTexture->Bind_ShaderResourceView(_shader.get(), "g_NoiseTexture", 0), E_FAIL);
    else
        CHECK_FAILED_ONCE(_shader->Bind_SRV("g_NoiseTexture", nullptr), E_FAIL);

    if (nullptr != _maskTexture)
        CHECK_FAILED_ONCE(_maskTexture->Bind_ShaderResourceView(_shader.get(), "g_MaskTexture", 0), E_FAIL);
    else
        CHECK_FAILED_ONCE(_shader->Bind_SRV("g_MaskTexture", nullptr), E_FAIL);

    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_Tint", &material.tint, sizeof(material.tint)), E_FAIL);

    const Vec4 effectSpriteParams = Vec4(
        material.intensity,
        material.opacityPower,
        material.noiseStrength,
        nullptr != _noiseTexture ? 1.f : 0.f
    );
    const Vec4 effectSpriteAlphaParams = Vec4(
        clamp(material.alphaCutoff, 0.f, 1.f),
        clamp(material.alphaErosion, 0.f, 1.f),
        nullptr != _maskTexture ? 1.f : 0.f,
        0.f
    );
    const Vec4 effectSpriteMainUVParams = Vec4(material.mainUVScale.x, material.mainUVScale.y, material.mainUVScrollSpeed.x, material.mainUVScrollSpeed.y);
    const Vec4 effectSpriteNoiseUVParams = Vec4(material.noiseUVScale.x, material.noiseUVScale.y, material.noiseUVScrollSpeed.x, material.noiseUVScrollSpeed.y);
    const Vec4 effectSpriteMaskUVParams = Vec4(material.maskUVScale.x, material.maskUVScale.y, material.maskUVScrollSpeed.x, material.maskUVScrollSpeed.y);
    const Vec4 effectSpriteUVOffsetParams = Vec4(material.mainUVOffset.x, material.mainUVOffset.y, material.noiseUVOffset.x, material.noiseUVOffset.y);
    const Vec4 effectSpriteMaskUVOffsetParams = Vec4(material.maskUVOffset.x, material.maskUVOffset.y, 0.f, 0.f);
    const Vec4 effectSpriteUVModeParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.mainUVTilingMode)),
        static_cast<float>(static_cast<uint32>(material.noiseUVTilingMode)),
        static_cast<float>(static_cast<uint32>(material.maskUVTilingMode)),
        0.f
    );

    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteParams", &effectSpriteParams, sizeof(effectSpriteParams)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteAlphaParams", &effectSpriteAlphaParams, sizeof(effectSpriteAlphaParams)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteMainUVParams", &effectSpriteMainUVParams, sizeof(effectSpriteMainUVParams)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteNoiseUVParams", &effectSpriteNoiseUVParams, sizeof(effectSpriteNoiseUVParams)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteMaskUVParams", &effectSpriteMaskUVParams, sizeof(effectSpriteMaskUVParams)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteUVOffsetParams", &effectSpriteUVOffsetParams, sizeof(effectSpriteUVOffsetParams)), E_FAIL);
    CHECK_FAILED_ONCE(
        _shader->Bind_RawValue("g_EffectSpriteMaskUVOffsetParams", &effectSpriteMaskUVOffsetParams, sizeof(effectSpriteMaskUVOffsetParams)),
        E_FAIL
    );
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteUVModeParams", &effectSpriteUVModeParams, sizeof(effectSpriteUVModeParams)), E_FAIL);
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteMaterialTime", &_emitterElapsedTime, sizeof(_emitterElapsedTime)), E_FAIL);

    const Vec4 effectInfluenceDebugParams = Vec4(
        _isInfluenceOutlineDebugEnabled ? 1.f : 0.f,
        0.5f,
        kInfluenceOutlinePixelWidthScale,
        0.f
    );
    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectInfluenceDebugParams", &effectInfluenceDebugParams, sizeof(effectInfluenceDebugParams)), E_FAIL);

    if (!Is_DistortionFamily())
    {
        const EffectMaterialAdditiveContributionData& additive = material.additive;
        const EffectMaterialCoreEmissiveData& coreEmissive = material.coreEmissive;
        const Vec4 effectSpriteAdditiveParams = Vec4(
            static_cast<float>(static_cast<uint32>(additive.colorSource)),
            static_cast<float>(static_cast<uint32>(additive.amountSource)),
            static_cast<float>(static_cast<uint32>(additive.coveragePolicy)),
            additive.intensityScale
        );
        const Vec4 effectSpriteAdditiveFlags = Vec4(additive.blackNeutral ? 1.f : 0.f, 0.f, 0.f, 0.f);
        const Vec4 effectSpriteCoreEmissiveParams = Vec4(
            coreEmissive.enabled ? 1.f : 0.f,
            coreEmissive.corePower,
            coreEmissive.coreIntensity,
            coreEmissive.outerPower
        );
        const Vec4 effectSpriteCoreEmissiveColor = Vec4(
            coreEmissive.coreColor.x,
            coreEmissive.coreColor.y,
            coreEmissive.coreColor.z,
            coreEmissive.outerIntensity
        );
        CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteAdditiveParams", &effectSpriteAdditiveParams, sizeof(effectSpriteAdditiveParams)), E_FAIL);
        CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteAdditiveEmissiveColor", &additive.emissiveColor, sizeof(additive.emissiveColor)), E_FAIL);
        CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteAdditiveConstantColor", &additive.constantColor, sizeof(additive.constantColor)), E_FAIL);
        CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_EffectSpriteAdditiveFlags", &effectSpriteAdditiveFlags, sizeof(effectSpriteAdditiveFlags)), E_FAIL);
        CHECK_FAILED_ONCE(
            _shader->Bind_RawValue("g_EffectSpriteCoreEmissiveParams", &effectSpriteCoreEmissiveParams, sizeof(effectSpriteCoreEmissiveParams)),
            E_FAIL
        );
        CHECK_FAILED_ONCE(
            _shader->Bind_RawValue("g_EffectSpriteCoreEmissiveColor", &effectSpriteCoreEmissiveColor, sizeof(effectSpriteCoreEmissiveColor)),
            E_FAIL
        );
    }

    CHECK_FAILED_ONCE(_shader->Bind_RawValue("g_OpacitySource", &_opacitySourceIndex, sizeof(_opacitySourceIndex)), E_FAIL);

    return S_OK;
}

HRESULT ComputeSpriteEmitter::Bind_DistortionResources()
{
    const auto& material = _desc.required.material;

    if (nullptr != _flowTexture)
        CHECK_FAILED_ONCE(_flowTexture->Bind_ShaderResourceView(_shader.get(), "g_FlowTexture", 0), E_FAIL);
    else
        CHECK_FAILED_ONCE(_shader->Bind_SRV("g_FlowTexture", nullptr), E_FAIL);

    D3D11_VIEWPORT viewport{};
    uint32 viewportCount = 1u;
    _context->RSGetViewports(&viewportCount, &viewport);

    const Vec4 distortionParams(
        max(0.f, material.refractionIntensity),
        max(0.f, material.refractionPresence),
        nullptr != _flowTexture ? 1.f : 0.f,
        _emitterElapsedTime
    );
    const Vec4 distortionShapeParams(
        static_cast<float>(static_cast<uint32>(material.distortionShapeMode)),
        max(0.f, material.distortionShapeRadius),
        max(0.f, material.distortionShapeThickness),
        max(0.f, material.distortionShapeSoftness)
    );
    const Vec4 distortionScreenSize(max(1.f, viewport.Width), max(1.f, viewport.Height), 0.f, 0.f);
    const float flowUVTilingMode = static_cast<float>(static_cast<uint32>(material.flowUVTilingMode));

    CHECK_FAILED_ONCE(
        _shader->Bind_RawValue("g_EffectDistortionParams", &distortionParams, sizeof(distortionParams)),
        E_FAIL
    );
    CHECK_FAILED_ONCE(
        _shader->Bind_RawValue("g_EffectDistortionShapeParams", &distortionShapeParams, sizeof(distortionShapeParams)),
        E_FAIL
    );
    CHECK_FAILED_ONCE(
        _shader->Bind_RawValue("g_DistortionScreenSize", &distortionScreenSize, sizeof(distortionScreenSize)),
        E_FAIL
    );
    CHECK_FAILED_ONCE(
        _shader->Bind_RawValue("g_FlowUVScale", &material.flowUVScale, sizeof(material.flowUVScale)),
        E_FAIL
    );
    CHECK_FAILED_ONCE(
        _shader->Bind_RawValue("g_FlowUVOffset", &material.flowUVOffset, sizeof(material.flowUVOffset)),
        E_FAIL
    );
    CHECK_FAILED_ONCE(
        _shader->Bind_RawValue("g_FlowUVScrollSpeed", &material.flowUVScrollSpeed, sizeof(material.flowUVScrollSpeed)),
        E_FAIL
    );
    CHECK_FAILED_ONCE(
        _shader->Bind_RawValue("g_FlowUVTilingMode", &flowUVTilingMode, sizeof(flowUVTilingMode)),
        E_FAIL
    );

    return S_OK;
}

bool ComputeSpriteEmitter::Is_DistortionFamily() const
{
    return _desc.required.material.materialFamily == EffectMaterialFamily::SpriteDistortion;
}

RenderGroup ComputeSpriteEmitter::Resolve_RenderGroup() const
{
    if (_desc.renderLayerOverride == EffectEmitterRenderLayerOverride::UIEffect)
        return RenderGroup::UIEffect;

    return Is_DistortionFamily() ? RenderGroup::Distortion : RenderGroup::Blend;
}

const char* ComputeSpriteEmitter::Resolve_ShaderId() const
{
    return Is_DistortionFamily() ? kEffectDistortionSpriteShaderId : kEffectSpriteShaderId;
}

uint32 ComputeSpriteEmitter::Resolve_ShaderPassIndex() const
{
    return Is_DistortionFamily() ? 0u : Resolve_BlendPassIndex();
}

int ComputeSpriteEmitter::Resolve_OpacitySourceIndex() const
{
    if (_desc.required.material.opacitySource == "Alpha" || _desc.required.material.opacitySource == "alpha")
        return kOpacitySourceAlpha;

    if (_desc.required.material.opacitySource == "Red" || _desc.required.material.opacitySource == "red")
        return kOpacitySourceRed;

    if (_desc.required.material.opacitySource == "Luminance" || _desc.required.material.opacitySource == "luminance")
        return kOpacitySourceLuminance;

    return kOpacitySourceAlpha;
}

uint32 ComputeSpriteEmitter::Resolve_BlendPassIndex() const
{
    return _desc.required.material.blendMode == EffectMaterialBlendMode::Additive ? 1u : 0u;
}

ComputePointParticleDesc ComputeSpriteEmitter::Make_ComputePointParticleDesc() const
{
    ComputePointParticleDesc desc{};
    desc.instanceCount = max(1u, _desc.spawn.instanceCount);
    desc.center = _desc.center;
    desc.range = _desc.spawn.spawnRange;
    desc.sizeMin = _desc.initialSize.sizeMin;
    desc.sizeMax = _desc.initialSize.sizeMax;
    desc.lifeTime = _desc.lifetime.lifeTime;
    desc.startColorMin = _desc.initialColor.startColorMin;
    desc.startColorMax = _desc.initialColor.startColorMax;
    desc.endColorMin = _desc.colorOverLife.endColorMin;
    desc.endColorMax = _desc.colorOverLife.endColorMax;
    desc.colorOverLifeCurve = _desc.colorOverLife.curve;
    desc.subUVRows = max(1u, _desc.required.material.subUVRows);
    desc.subUVCols = max(1u, _desc.required.material.subUVCols);
    desc.subUvFrameOverLife = _desc.subUVFrameOverLife;
    desc.sizeByLife = _desc.sizeByLife;
    desc.motion = _desc.motion;
    desc.rotation = _desc.rotation;
    desc.spawn = _desc.spawn.particleSpawn;
    desc.initialLocation = _desc.initialLocation;
    desc.sphereLocation = _desc.sphereLocation;
    if (nullptr != _transformCom)
    {
        desc.emitterRight = _transformCom->Get_WorldRight();
        desc.emitterUp = _transformCom->Get_WorldUp();
        desc.emitterLook = _transformCom->Get_WorldForward();
    }
    desc.spawn.maxActiveCount = min(max(1u, desc.spawn.maxActiveCount), desc.instanceCount);
    desc.useMaxDrawCount = _desc.required.drawLimit.useMaxDrawCount;
    desc.maxDrawCount = _desc.required.drawLimit.useMaxDrawCount
                        ? min(desc.instanceCount, max(1u, _desc.required.drawLimit.maxDrawCount))
                        : desc.instanceCount;
    desc.drawMode = PointParticleDrawMode::DrawIndexedInstancedIndirect;
    desc.computeShaderLevelIndex = ETOI(LevelType::Static);
    desc.computeShaderPrototypeTag = L"ComputeShader_PointParticle";

    return desc;
}

void ComputeSpriteEmitter::Sync_WorldCenterToComputeBuffer()
{
    CHECK_NULL_THROTTLED(_transformCom, 120);
    CHECK_NULL_THROTTLED(_viBuffer, 120);

    _viBuffer->Set_Center(_transformCom->Get_WorldPosition());
    _viBuffer->Set_EmitterBasis(
        _transformCom->Get_WorldRight(),
        _transformCom->Get_WorldUp(),
        _transformCom->Get_WorldForward()
    );
}

uint32 ComputeSpriteEmitter::Consume_SpawnRequest(float timeDelta)
{
    const float safeDeltaTime = max(0.f, timeDelta);
    const float previousTime = _emitterElapsedTime;
    _emitterElapsedTime += safeDeltaTime;

    uint32 spawnRequest = 0u;

    if (_desc.spawn.particleSpawn.processSpawnRate)
    {
        const float spawnRate = max(0.f, _desc.spawn.particleSpawn.spawnRate * _desc.spawn.particleSpawn.spawnRateScale);
        _spawnAccumulator += spawnRate * safeDeltaTime;

        const float wholeSpawn = floorf(_spawnAccumulator + 0.0001f);
        if (wholeSpawn > 0.f)
        {
            spawnRequest += static_cast<uint32>(wholeSpawn);
            _spawnAccumulator -= wholeSpawn;
        }
    }

    if (_desc.spawn.particleSpawn.processBurstList)
    {
        if (_burstExecuted.size() != _desc.spawn.particleSpawn.bursts.size())
            _burstExecuted.assign(_desc.spawn.particleSpawn.bursts.size(), false);

        for (size_t index = 0; index < _desc.spawn.particleSpawn.bursts.size(); ++index)
        {
            if (_burstExecuted[index])
                continue;

            const PointParticleBurstDesc& burst = _desc.spawn.particleSpawn.bursts[index];
            if (previousTime > burst.time || _emitterElapsedTime < burst.time)
                continue;

            const float scaledCount = static_cast<float>(burst.count) * max(0.f, _desc.spawn.particleSpawn.burstScale);
            spawnRequest += static_cast<uint32>(floorf(scaledCount));
            _burstExecuted[index] = true;
        }
    }

    return min(spawnRequest, max(1u, _desc.spawn.particleSpawn.maxActiveCount));
}

void ComputeSpriteEmitter::Reset_PlaybackRuntime()
{
    _burstExecuted.assign(_desc.spawn.particleSpawn.bursts.size(), false);
    _emitterElapsedTime = 0.f;
    _spawnAccumulator = 0.f;
    _loopElapsedTime = 0.f;
    _loopIndex = 0u;
    _completedKillDispatched = false;
    _playbackState = Resolve_CurrentLoopDelay() > 0.f ? PlaybackState::Delayed : PlaybackState::Playing;
}

ComputeSpriteEmitter::PlaybackTick ComputeSpriteEmitter::Advance_Playback(float timeDelta)
{
    PlaybackTick tick{};
    const float safeDeltaTime = max(0.f, timeDelta);

    if (PlaybackState::Completed == _playbackState)
    {
        tick.computeDeltaTime = _desc.required.playback.killOnCompleted ? 0.f : safeDeltaTime;
        if (_desc.required.playback.killOnCompleted && !_completedKillDispatched)
        {
            tick.killActiveParticles = true;
            _completedKillDispatched = true;
        }
        return tick;
    }

    float remainingTime = safeDeltaTime;
    while (remainingTime > 0.f && PlaybackState::Completed != _playbackState)
    {
        const float delay = Resolve_CurrentLoopDelay();
        if (_loopElapsedTime < delay)
        {
            _playbackState = PlaybackState::Delayed;
            const float delayStep = min(remainingTime, delay - _loopElapsedTime);
            _loopElapsedTime += delayStep;
            remainingTime -= delayStep;

            if (_loopElapsedTime < delay || remainingTime <= 0.f)
                break;
        }

        _playbackState = PlaybackState::Playing;

        const float duration = Resolve_Duration();
        const float currentPlaybackTime = max(0.f, _loopElapsedTime - delay);
        const float remainingPlaybackTime = max(0.f, duration - currentPlaybackTime);
        const float playbackStep = min(remainingTime, remainingPlaybackTime);

        if (playbackStep > 0.f)
        {
            tick.computeDeltaTime += playbackStep;
            tick.spawnRequest += Consume_SpawnRequest(playbackStep);
            _loopElapsedTime += playbackStep;
            remainingTime -= playbackStep;
        }

        if (_loopElapsedTime - delay < duration)
            break;

        if (Has_NextLoop())
        {
            Start_NextLoop();
            continue;
        }

        _playbackState = PlaybackState::Completed;
        if (_desc.required.playback.killOnCompleted && !_completedKillDispatched)
        {
            tick.killActiveParticles = true;
            _completedKillDispatched = true;
        }
        break;
    }

    if (PlaybackState::Completed == _playbackState && !_desc.required.playback.killOnCompleted)
        tick.computeDeltaTime += remainingTime;

    return tick;
}

void ComputeSpriteEmitter::Start_NextLoop()
{
    ++_loopIndex;
    _loopElapsedTime = 0.f;
    _emitterElapsedTime = 0.f;
    _burstExecuted.assign(_desc.spawn.particleSpawn.bursts.size(), false);
    _playbackState = Resolve_CurrentLoopDelay() > 0.f ? PlaybackState::Delayed : PlaybackState::Playing;
}

float ComputeSpriteEmitter::Resolve_CurrentLoopDelay() const
{
    if (_desc.required.playback.delayFirstLoopOnly && _loopIndex > 0u)
        return 0.f;

    return max(0.f, _desc.required.playback.delay);
}

float ComputeSpriteEmitter::Resolve_Duration() const
{
    return max(0.0001f, _desc.required.playback.duration);
}

bool ComputeSpriteEmitter::Has_NextLoop() const
{
    return _desc.required.playback.loopCount == 0u || _loopIndex + 1u < _desc.required.playback.loopCount;
}

bool ComputeSpriteEmitter::Can_SubmitRender() const
{
    if (PlaybackState::Delayed == _playbackState)
        return false;

    if (PlaybackState::Completed == _playbackState && _desc.required.playback.killOnCompleted)
        return false;

    return nullptr != _viBuffer;
}

Shared<ComputeSpriteEmitter> ComputeSpriteEmitter::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<ComputeSpriteEmitter>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : ComputeSpriteEmitter");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> ComputeSpriteEmitter::Clone(void* arg)
{
    auto instance = make_shared<ComputeSpriteEmitter>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : ComputeSpriteEmitter");
        MSG_BOX("Failed to Clone : ComputeSpriteEmitter");
        return nullptr;
    }

    return instance;
}

void ComputeSpriteEmitter::Free()
{
    _shader.reset();
    _texture.reset();
    _noiseTexture.reset();
    _maskTexture.reset();
    _viBuffer.reset();

    _desc = {};
    _opacitySourceIndex = kOpacitySourceAlpha;
    _burstExecuted.clear();
    _emitterElapsedTime = 0.f;
    _spawnAccumulator = 0.f;
    _playbackState = PlaybackState::Delayed;
    _loopElapsedTime = 0.f;
    _loopIndex = 0u;
    _completedKillDispatched = false;

    __super::Free();
}
