#include "ComputeRibbonEmitter.h"

#include "ComputeShaderCom.h"
#include "ComputeStructuredBuffer.h"
#include "EffectMaterialScalarModulationRuntime.h"
#include "GameInstance.h"
#include "Renderer_Define.h"
#include "ShaderCom.h"
#include "Texture.h"

namespace Client::Internal
{
static int Resolve_MaterialSourceIndex(const string& source)
{
    constexpr int sourceAlpha = 0;
    constexpr int sourceRed = 1;
    constexpr int sourceLuminance = 2;

    if (source == "Red" || source == "red")
        return sourceRed;

    if (source == "Luminance" || source == "luminance")
        return sourceLuminance;

    return sourceAlpha;
}

static HRESULT Bind_MaterialScalarModulationPayload(
    ShaderCom* shader,
    const EffectMaterialScalarModulationRuntimeDesc& modulation,
    float emitterPhase)
{
    CHECK_NULL(shader, E_FAIL);

    const EffectMaterialScalarModulationShaderPayload payload =
        Build_MaterialScalarModulationShaderPayload(modulation, emitterPhase);

    CHECK_FAILED(shader->Bind_RawValue("g_EffectMaterialScalarModulationParams", &payload.params, sizeof(payload.params)), E_FAIL);
    CHECK_FAILED(shader->Bind_RawValue("g_EffectMaterialScalarModulationMeta", payload.meta.data(), sizeof(payload.meta)), E_FAIL);
    CHECK_FAILED(shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyTimes", payload.keyTimes.data(), sizeof(payload.keyTimes)), E_FAIL);
    CHECK_FAILED(
        shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyTimesBlock1", payload.keyTimesBlock1.data(), sizeof(payload.keyTimesBlock1)),
        E_FAIL
    );
    CHECK_FAILED(shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyValues", payload.keyValues.data(), sizeof(payload.keyValues)), E_FAIL);
    CHECK_FAILED(
        shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyValuesBlock1", payload.keyValuesBlock1.data(), sizeof(payload.keyValuesBlock1)),
        E_FAIL
    );
    CHECK_FAILED(
        shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyArriveTangents", payload.keyArriveTangents.data(), sizeof(payload.keyArriveTangents)),
        E_FAIL
    );
    CHECK_FAILED(
        shader->Bind_RawValue(
            "g_EffectMaterialScalarModulationKeyArriveTangentsBlock1",
            payload.keyArriveTangentsBlock1.data(),
            sizeof(payload.keyArriveTangentsBlock1)
        ),
        E_FAIL
    );
    CHECK_FAILED(
        shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyLeaveTangents", payload.keyLeaveTangents.data(), sizeof(payload.keyLeaveTangents)),
        E_FAIL
    );
    CHECK_FAILED(
        shader->Bind_RawValue(
            "g_EffectMaterialScalarModulationKeyLeaveTangentsBlock1",
            payload.keyLeaveTangentsBlock1.data(),
            sizeof(payload.keyLeaveTangentsBlock1)
        ),
        E_FAIL
    );
    CHECK_FAILED(shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyModes", payload.keyModes.data(), sizeof(payload.keyModes)), E_FAIL);
    CHECK_FAILED(
        shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyModesBlock1", payload.keyModesBlock1.data(), sizeof(payload.keyModesBlock1)),
        E_FAIL
    );

    return S_OK;
}

static EffectRequiredMaterialRuntimeDesc Resolve_EmitterTimeCoreColorRgbModulation(
    const EffectRequiredMaterialRuntimeDesc& baseMaterial,
    float emitterPhase)
{
    auto material = baseMaterial;
    const auto& modulation = baseMaterial.coreColorRgbModulation;
    const uint32 count = min(EffectMaterialCoreColorRgbModulationRuntimeDesc::kMaxModulators, modulation.count);

    for (uint32 index = 0; index < count; ++index)
    {
        const auto& modulator = modulation.modulators[index];
        if (!modulator.enabled || modulator.timeSource != EffectMaterialScalarModulationTimeSource::EmitterTime)
            continue;

        const Vec3 fallbackValue{
            material.coreEmissive.coreColor.x,
            material.coreEmissive.coreColor.y,
            material.coreEmissive.coreColor.z
        };
        const Vec3 coreColorRgb = Evaluate_CoreColorRgbModulationCurve(modulator.curve, emitterPhase, fallbackValue);
        material.coreEmissive.coreColor.x = coreColorRgb.x;
        material.coreEmissive.coreColor.y = coreColorRgb.y;
        material.coreEmissive.coreColor.z = coreColorRgb.z;
    }

    return material;
}

constexpr float kPlaneRadialEpsilon = 1e-4f;

static float Read_Vec4Component(const Vec4& values, uint32 index)
{
    switch (index)
    {
    case 0: return values.x;
    case 1: return values.y;
    case 2: return values.z;
    case 3: return values.w;
    default: return values.x;
    }
}

static float Read_Vec4Component(const Vec4& valuesBlock0, const Vec4& valuesBlock1, uint32 index)
{
    if (index < 4u)
        return Read_Vec4Component(valuesBlock0, index);

    return Read_Vec4Component(valuesBlock1, index - 4u);
}

static Vec4 Lerp_Vector4(const Vec4& from, const Vec4& to, float ratio)
{
    return from + (to - from) * ratio;
}

static uint32 Resolve_SeedSalt(const PointParticleRandomSeedRuntimeDesc& seed, uint32 effectPlaybackSeed)
{
    uint32 salt = seed.manualSeedEnabled ? seed.seed : 0u;
    if (seed.useInstanceSeed)
        salt += effectPlaybackSeed;
    return salt;
}

static float Hash01(uint32 seed)
{
    seed ^= 2747636419u;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    seed *= 2654435769u;
    seed ^= seed >> 16;
    return static_cast<float>(seed & 0x00FFFFFFu) / 16777215.f;
}

static float Resolve_EvenByParticleIndexAngleRatio(uint32 placementIndex, uint32 placementCount, float angleMinDegrees, float angleMaxDegrees)
{
    const uint32 clampedCount = max(1u, placementCount);
    if (clampedCount <= 1u)
        return 0.f;

    const float angleSpanDegrees = fabsf(angleMaxDegrees - angleMinDegrees);
    const bool isFullCircle = fabsf(angleSpanDegrees - 360.f) <= kPlaneRadialEpsilon;
    if (isFullCircle)
        return static_cast<float>(placementIndex) / static_cast<float>(clampedCount);

    return static_cast<float>(placementIndex) / static_cast<float>(clampedCount - 1u);
}

static Vec3 Resolve_EvenByParticleIndexSphereDirection(uint32 placementIndex, uint32 placementCount)
{
    const uint32 clampedCount = max(1u, placementCount);
    if (clampedCount <= 1u)
        return Vec3{ 0.f, 0.f, 1.f };

    const uint32 clampedIndex = placementIndex % clampedCount;
    const float ratio = (static_cast<float>(clampedIndex) + 0.5f) / static_cast<float>(clampedCount);
    const float z = 1.f - 2.f * ratio;
    const float radiusOnPlane = sqrtf(max(0.f, 1.f - z * z));
    const float goldenAngle = XM_PI * (3.f - sqrtf(5.f));
    const float angle = static_cast<float>(clampedIndex) * goldenAngle;

    return Vec3{
        cosf(angle) * radiusOnPlane,
        sinf(angle) * radiusOnPlane,
        z
    };
}

static Vec3 Normalize_OrFallback(const Vec3& value, const Vec3& fallback)
{
    if (value.LengthSquared() <= kPlaneRadialEpsilon * kPlaneRadialEpsilon)
        return fallback;

    Vec3 result = value;
    result.Normalize();
    return result;
}

static void Resolve_PlaneRadialBasis(PointParticlePlaneRadialLocationPlane plane, Vec3& axisU, Vec3& axisV, Vec3& planeNormal)
{
    axisU = Vec3{ 1.f, 0.f, 0.f };
    axisV = Vec3{ 0.f, 1.f, 0.f };
    planeNormal = Vec3{ 0.f, 0.f, 1.f };

    if (plane == PointParticlePlaneRadialLocationPlane::XZ)
    {
        axisV = Vec3{ 0.f, 0.f, 1.f };
        planeNormal = Vec3{ 0.f, 1.f, 0.f };
    }
    else if (plane == PointParticlePlaneRadialLocationPlane::YZ)
    {
        axisU = Vec3{ 0.f, 1.f, 0.f };
        axisV = Vec3{ 0.f, 0.f, 1.f };
        planeNormal = Vec3{ 1.f, 0.f, 0.f };
    }
}

static wstring Resolve_RibbonTexturePathByGuid(const string& textureGuid)
{
    if (GAME == nullptr || textureGuid.empty())
        return {};

    const AssetMeta* assetMeta = GAME->Find_AssetByGUID(textureGuid);
    if (assetMeta == nullptr || assetMeta->type != "Texture")
        return {};

    const wstring resolvedPath = GAME->Resolve_AssetPath(textureGuid);
    return !resolvedPath.empty() && fs::exists(resolvedPath) ? resolvedPath : wstring{};
}

static wstring Resolve_RibbonTexturePathByPath(const string& texturePath)
{
    if (texturePath.empty())
        return {};

    fs::path candidatePath = String::ToWString(texturePath);
    if (candidatePath.is_relative() && GAME != nullptr)
        candidatePath = fs::path(GAME->Get_AssetRoot()) / candidatePath;

    candidatePath = candidatePath.lexically_normal();
    return fs::exists(candidatePath) ? candidatePath.wstring() : wstring{};
}

static bool Is_RibbonTextureSamePath(const wstring& lhs, const wstring& rhs)
{
    if (lhs.empty() || rhs.empty())
        return false;

    return fs::path(lhs).lexically_normal() == fs::path(rhs).lexically_normal();
}

static wstring Resolve_RibbonTexturePath(
    const string& textureGuid,
    const string& texturePath,
    bool& outUsedPathFallback,
    bool& outGuidPathMismatch)
{
    outUsedPathFallback = false;
    outGuidPathMismatch = false;

    wstring resolvedPath = Resolve_RibbonTexturePathByGuid(textureGuid);
    if (!resolvedPath.empty())
    {
        const wstring pathResolved = Resolve_RibbonTexturePathByPath(texturePath);
        outGuidPathMismatch = !pathResolved.empty() && !Is_RibbonTextureSamePath(resolvedPath, pathResolved);
        return resolvedPath;
    }

    resolvedPath = Resolve_RibbonTexturePathByPath(texturePath);
    if (!resolvedPath.empty())
    {
        outUsedPathFallback = true;
        return resolvedPath;
    }

    return {};
}
}

IMPLEMENT_REFLECTION(ComputeRibbonEmitter)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "ComputeRibbonEmitter";
    info.category = "Effect";

    return true;
}

ComputeRibbonEmitter::ComputeRibbonEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : EffectEmitter{ device, context }
{
}

ComputeRibbonEmitter::ComputeRibbonEmitter(const ComputeRibbonEmitter& prototype)
    : EffectEmitter{ prototype }
{
}

ComputeRibbonEmitter::~ComputeRibbonEmitter()
{
    Free();
}

HRESULT ComputeRibbonEmitter::Initialize_Prototype()
{
    return S_OK;
}

HRESULT ComputeRibbonEmitter::Initialize(void* arg)
{
    if (nullptr != arg)
        _desc = *static_cast<ComputeRibbonEmitterDesc*>(arg);

    _desc.playback.duration = max(0.0001f, _desc.playback.duration);
    _desc.playback.delay = max(0.f, _desc.playback.delay);
    _desc.spawn.instanceCount = max(1u, _desc.spawn.instanceCount);
    _desc.spawn.particleSpawn.maxActiveCount = min(
        max(1u, _desc.spawn.particleSpawn.maxActiveCount),
        _desc.spawn.instanceCount
    );
    _desc.lifetime.lifeTime.x = max(0.0001f, _desc.lifetime.lifeTime.x);
    _desc.lifetime.lifeTime.y = max(_desc.lifetime.lifeTime.x, _desc.lifetime.lifeTime.y);
    _desc.maxSampleCount = max(2u, _desc.maxSampleCount);
    _desc.tilingDistance = max(0.f, _desc.tilingDistance);
    _desc.distanceTessellationStepSize = max(0.001f, _desc.distanceTessellationStepSize);

    CHECK_FAILED(__super::Initialize(&_desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);
    Reset_PlaybackRuntime();

    return S_OK;
}

void ComputeRibbonEmitter::Update(float timeDelta)
{
    Sync_FromEffectOwner();
    Advance_Playback(timeDelta);
    _materialElapsedTime += max(0.f, timeDelta);

    if (PlaybackState::Playing == _playbackState)
        Update_Samples(timeDelta);

    CHECK_FAILED_THROTTLED(Dispatch_Compute(), 60);
}

void ComputeRibbonEmitter::Late_Update(float)
{
    if (Can_SubmitRender())
        GAME->Add_RenderGroup(Resolve_RenderGroup(), GetSharedPtr<GameObject>());
}

HRESULT ComputeRibbonEmitter::Render()
{
    CHECK_FAILED_THROTTLED(Bind_ShaderResources(), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Begin(Resolve_ShaderPassIndex()), 60, E_FAIL);

    constexpr uint32 stride0 = sizeof(Vec3);
    constexpr uint32 offset0 = 0;
    constexpr uint32 stride1 = sizeof(RibbonInstanceVertex);
    constexpr uint32 offset1 = 0;
    Buffer* vertexBuffers[]{ _pointVB.Get(), _instanceBuffer.Get() };
    const uint32 strides[]{ stride0, stride1 };
    const uint32 offsets[]{ offset0, offset1 };

    _context->IASetVertexBuffers(0, 2, vertexBuffers, strides, offsets);
    _context->IASetIndexBuffer(_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    _context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    _context->DrawIndexedInstancedIndirect(_indirectArgsBuffer.Get(), 0);
    return S_OK;
}

HRESULT ComputeRibbonEmitter::Reset_ForEffectReplay()
{
    Reset_PlaybackRuntime();
    _samples.clear();
    _burstExecuted.assign(_desc.spawn.particleSpawn.bursts.size(), false);
    _spawnAccumulator = 0.f;
    _materialElapsedTime = 0.f;
    _visibleRibbonLength = 1.f;
    _drawSegmentCount = 0u;
    _nextSampleIndex = 0u;

    if (nullptr != _computeArgsOutput)
        CHECK_FAILED(Reset_IndirectArgs(), E_FAIL);
    if (nullptr != _computeOutput && nullptr != _computeArgsOutput)
        CHECK_FAILED(Copy_ComputeOutput(), E_FAIL);

    return S_OK;
}

bool ComputeRibbonEmitter::Is_EffectFinished() const
{
    return PlaybackState::Completed == _playbackState;
}

bool ComputeRibbonEmitter::Try_Get_BlendSortWorldPosition(Vec3& outWorldPosition) const
{
    if (nullptr == _transformCom)
        return false;

    outWorldPosition = _transformCom->Get_WorldPosition();
    return true;
}

EffectSortPolicy ComputeRibbonEmitter::Get_BlendSortPolicy() const
{
    return _desc.sort.sortPolicy;
}

int32 ComputeRibbonEmitter::Get_BlendSortLayer() const
{
    return _desc.sort.sortLayer;
}

float ComputeRibbonEmitter::Get_BlendSortBias() const
{
    return _desc.sort.artistSortBias;
}

void ComputeRibbonEmitter::Reset_PlaybackRuntime()
{
    _playbackState = PlaybackState::Delayed;
    _loopElapsedTime = 0.f;
    _loopIndex = 0u;
}

void ComputeRibbonEmitter::Advance_Playback(float timeDelta)
{
    if (PlaybackState::Completed == _playbackState)
        return;

    const float safeDeltaTime = max(0.f, timeDelta);
    _loopElapsedTime += safeDeltaTime;

    const float delay = Resolve_CurrentLoopDelay();
    if (_loopElapsedTime < delay)
    {
        _playbackState = PlaybackState::Delayed;
        return;
    }

    _playbackState = PlaybackState::Playing;
    const float activeElapsedTime = _loopElapsedTime - delay;
    if (activeElapsedTime < Resolve_Duration())
        return;

    if (Has_NextLoop())
    {
        Start_NextLoop();
        return;
    }

    _playbackState = PlaybackState::Completed;
}

HRESULT ComputeRibbonEmitter::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), Resolve_ShaderId(), _shader), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), kRibbonComputeShaderId, _computeShader), E_FAIL);
    CHECK_FAILED(Ready_Texture(), E_FAIL);
    CHECK_FAILED(Ready_NoiseTexture(), E_FAIL);
    CHECK_FAILED(Ready_MaskTexture(), E_FAIL);
    CHECK_FAILED(Ready_FlowTexture(), E_FAIL);
    CHECK_FAILED(Ready_DrawBuffers(), E_FAIL);
    CHECK_FAILED(Ready_ComputeBuffers(), E_FAIL);
    return S_OK;
}

HRESULT ComputeRibbonEmitter::Ready_Texture()
{
    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    wstring resolvedPath = Internal::Resolve_RibbonTexturePath(
        _desc.material.mainTextureGuid,
        _desc.material.mainTexturePath,
        usedPathFallback,
        guidPathMismatch
    );

    if (resolvedPath.empty())
    {
        LOG_WARN(
            "ComputeRibbonEmitter main texture resolve failed. mainTextureGuid='{}', mainTexturePath='{}'. Falling back to default ribbon texture.",
            _desc.material.mainTextureGuid,
            _desc.material.mainTexturePath
        );
        resolvedPath = Internal::Resolve_RibbonTexturePathByPath(kFallbackTexturePath);
    }

    if (resolvedPath.empty())
    {
        LOG_ERROR(
            "ComputeRibbonEmitter failed to resolve default ribbon texture. fallbackPath='{}'.",
            kFallbackTexturePath
        );
        return E_FAIL;
    }

    if (guidPathMismatch)
    {
        LOG_WARN(
            "ComputeRibbonEmitter main texture GUID/path mismatch. mainTextureGuid='{}', mainTexturePath='{}', guidResolvedPath='{}'.",
            _desc.material.mainTextureGuid,
            _desc.material.mainTexturePath,
            String::ToString(resolvedPath)
        );
    }
    else if (usedPathFallback)
    {
        LOG_WARN(
            "ComputeRibbonEmitter main texture fell back to path. mainTextureGuid='{}', mainTexturePath='{}', resolvedPath='{}'.",
            _desc.material.mainTextureGuid,
            _desc.material.mainTexturePath,
            String::ToString(resolvedPath)
        );
    }

    _mainTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    CHECK_NULL(_mainTexture, E_FAIL);
    return S_OK;
}

HRESULT ComputeRibbonEmitter::Ready_NoiseTexture()
{
    if (_desc.material.noiseTextureGuid.empty() && _desc.material.noiseTexturePath.empty())
        return S_OK;

    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    const wstring resolvedPath = Internal::Resolve_RibbonTexturePath(
        _desc.material.noiseTextureGuid,
        _desc.material.noiseTexturePath,
        usedPathFallback,
        guidPathMismatch
    );
    if (resolvedPath.empty())
    {
        LOG_WARN(
            "ComputeRibbonEmitter noise texture resolve failed. noiseTextureGuid='{}', noiseTexturePath='{}'. Noise modulation is disabled.",
            _desc.material.noiseTextureGuid,
            _desc.material.noiseTexturePath
        );
        _noiseTexture = nullptr;
        return S_OK;
    }

    if (guidPathMismatch)
    {
        LOG_WARN(
            "ComputeRibbonEmitter noise texture GUID/path mismatch. noiseTextureGuid='{}', noiseTexturePath='{}', guidResolvedPath='{}'.",
            _desc.material.noiseTextureGuid,
            _desc.material.noiseTexturePath,
            String::ToString(resolvedPath)
        );
    }
    else if (usedPathFallback)
    {
        LOG_WARN(
            "ComputeRibbonEmitter noise texture fell back to path. noiseTextureGuid='{}', noiseTexturePath='{}', resolvedPath='{}'.",
            _desc.material.noiseTextureGuid,
            _desc.material.noiseTexturePath,
            String::ToString(resolvedPath)
        );
    }

    _noiseTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    if (_noiseTexture == nullptr)
    {
        LOG_WARN(
            "ComputeRibbonEmitter noise texture create failed. noiseTextureGuid='{}', noiseTexturePath='{}', resolvedPath='{}'. Noise modulation is disabled.",
            _desc.material.noiseTextureGuid,
            _desc.material.noiseTexturePath,
            String::ToString(resolvedPath)
        );
    }

    return S_OK;
}

HRESULT ComputeRibbonEmitter::Ready_MaskTexture()
{
    if (_desc.material.maskTextureGuid.empty() && _desc.material.maskTexturePath.empty())
        return S_OK;

    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    const wstring resolvedPath = Internal::Resolve_RibbonTexturePath(
        _desc.material.maskTextureGuid,
        _desc.material.maskTexturePath,
        usedPathFallback,
        guidPathMismatch
    );
    if (resolvedPath.empty())
    {
        LOG_WARN(
            "ComputeRibbonEmitter mask texture resolve failed. maskTextureGuid='{}', maskTexturePath='{}'. Mask modulation is disabled.",
            _desc.material.maskTextureGuid,
            _desc.material.maskTexturePath
        );
        _maskTexture = nullptr;
        return S_OK;
    }

    if (guidPathMismatch)
    {
        LOG_WARN(
            "ComputeRibbonEmitter mask texture GUID/path mismatch. maskTextureGuid='{}', maskTexturePath='{}', guidResolvedPath='{}'.",
            _desc.material.maskTextureGuid,
            _desc.material.maskTexturePath,
            String::ToString(resolvedPath)
        );
    }
    else if (usedPathFallback)
    {
        LOG_WARN(
            "ComputeRibbonEmitter mask texture fell back to path. maskTextureGuid='{}', maskTexturePath='{}', resolvedPath='{}'.",
            _desc.material.maskTextureGuid,
            _desc.material.maskTexturePath,
            String::ToString(resolvedPath)
        );
    }

    _maskTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    if (_maskTexture == nullptr)
    {
        LOG_WARN(
            "ComputeRibbonEmitter mask texture create failed. maskTextureGuid='{}', maskTexturePath='{}', resolvedPath='{}'. Mask modulation is disabled.",
            _desc.material.maskTextureGuid,
            _desc.material.maskTexturePath,
            String::ToString(resolvedPath)
        );
    }

    return S_OK;
}

HRESULT ComputeRibbonEmitter::Ready_FlowTexture()
{
    if (!Is_DistortionFamily())
        return S_OK;

    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    wstring resolvedPath = Internal::Resolve_RibbonTexturePath(
        _desc.material.flowTextureGuid,
        _desc.material.flowTexturePath,
        usedPathFallback,
        guidPathMismatch
    );

    if (resolvedPath.empty())
    {
        LOG_WARN(
            "ComputeRibbonEmitter flow texture resolve failed. flowTextureGuid='{}', flowTexturePath='{}'. Falling back to neutral flow texture.",
            _desc.material.flowTextureGuid,
            _desc.material.flowTexturePath
        );
        resolvedPath = Internal::Resolve_RibbonTexturePathByPath(kNeutralFlowTexturePath);
        usedPathFallback = !resolvedPath.empty();
    }

    if (resolvedPath.empty())
    {
        LOG_WARN(
            "ComputeRibbonEmitter neutral flow texture resolve failed. fallbackPath='{}'. Distortion flow is disabled.",
            kNeutralFlowTexturePath
        );
        _flowTexture = nullptr;
        return S_OK;
    }

    if (guidPathMismatch)
    {
        LOG_WARN(
            "ComputeRibbonEmitter flow texture GUID/path mismatch. flowTextureGuid='{}', flowTexturePath='{}', guidResolvedPath='{}'.",
            _desc.material.flowTextureGuid,
            _desc.material.flowTexturePath,
            String::ToString(resolvedPath)
        );
    }
    else if (usedPathFallback && (!_desc.material.flowTextureGuid.empty() || !_desc.material.flowTexturePath.empty()))
    {
        LOG_WARN(
            "ComputeRibbonEmitter flow texture fell back to path/default. flowTextureGuid='{}', flowTexturePath='{}', resolvedPath='{}'.",
            _desc.material.flowTextureGuid,
            _desc.material.flowTexturePath,
            String::ToString(resolvedPath)
        );
    }

    _flowTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    if (_flowTexture == nullptr)
    {
        LOG_WARN(
            "ComputeRibbonEmitter flow texture create failed. flowTextureGuid='{}', flowTexturePath='{}', resolvedPath='{}'. Distortion flow is disabled.",
            _desc.material.flowTextureGuid,
            _desc.material.flowTexturePath,
            String::ToString(resolvedPath)
        );
    }

    return S_OK;
}

HRESULT ComputeRibbonEmitter::Ready_DrawBuffers()
{
    const Vec3 point{};
    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.ByteWidth = sizeof(Vec3);
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData{};
    vbData.pSysMem = &point;
    CHECK_FAILED(_device->CreateBuffer(&vbDesc, &vbData, _pointVB.GetAddressOf()), E_FAIL);

    const uint16 index = 0;
    D3D11_BUFFER_DESC ibDesc{};
    ibDesc.ByteWidth = sizeof(uint16);
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ibData{};
    ibData.pSysMem = &index;
    CHECK_FAILED(_device->CreateBuffer(&ibDesc, &ibData, _indexBuffer.GetAddressOf()), E_FAIL);

    D3D11_BUFFER_DESC instanceDesc{};
    instanceDesc.ByteWidth = sizeof(RibbonInstanceVertex) * Compute_MaxRenderSegmentCount();
    instanceDesc.Usage = D3D11_USAGE_DEFAULT;
    instanceDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    CHECK_FAILED(_device->CreateBuffer(&instanceDesc, nullptr, _instanceBuffer.GetAddressOf()), E_FAIL);
    return S_OK;
}

HRESULT ComputeRibbonEmitter::Ready_ComputeBuffers()
{
    const uint32 maxSegmentCount = Compute_MaxRenderSegmentCount();
    _sampleInput = ComputeStructuredBuffer::Create(_device, _context, sizeof(RibbonSamplePayload), maxSegmentCount + 1u);
    CHECK_NULL(_sampleInput, E_FAIL);
    _computeOutput = ComputeStructuredBuffer::Create(_device, _context, sizeof(RibbonInstanceVertex), maxSegmentCount);
    CHECK_NULL(_computeOutput, E_FAIL);
    _computeArgsOutput = ComputeStructuredBuffer::Create(_device, _context, sizeof(DrawIndexedInstancedIndirectArgs), 1);
    CHECK_NULL(_computeArgsOutput, E_FAIL);

    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(RibbonComputeParams);
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    CHECK_FAILED(_device->CreateBuffer(&cbDesc, nullptr, _computeConstantBuffer.GetAddressOf()), E_FAIL);

    D3D11_BUFFER_DESC argsDesc{};
    argsDesc.ByteWidth = sizeof(DrawIndexedInstancedIndirectArgs);
    argsDesc.Usage = D3D11_USAGE_DEFAULT;
    argsDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    argsDesc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
    CHECK_FAILED(_device->CreateBuffer(&argsDesc, nullptr, _indirectArgsBuffer.GetAddressOf()), E_FAIL);
    return Reset_IndirectArgs();
}

void ComputeRibbonEmitter::Update_Samples(float timeDelta)
{
    const float safeDeltaTime = max(0.f, timeDelta);

    for (RibbonSample& sample : _samples)
    {
        sample.age += safeDeltaTime;
        const float lifeProgress = clamp(sample.age / max(sample.lifetime, 0.0001f), 0.f, 1.f);
        const float velocityScale = Evaluate_VelocityScaleByLife(lifeProgress);
        const Vec3 acceleration = Evaluate_AccelerationByLife(sample, lifeProgress) * velocityScale;
        sample.velocity += acceleration * safeDeltaTime;

        if (sample.drag > 0.f)
            sample.velocity *= expf(-sample.drag * safeDeltaTime);

        sample.position += sample.velocity * velocityScale * safeDeltaTime;
    }

    _samples.erase(
        remove_if(
            _samples.begin(),
            _samples.end(),
            [](const RibbonSample& sample)
            {
                return sample.age >= sample.lifetime;
            }
        ),
        _samples.end()
    );

    uint32 spawnRequest = 0u;
    const auto& particleSpawn = _desc.spawn.particleSpawn;
    if (particleSpawn.processSpawnRate)
    {
        const float sampledSpawnRate = Sample_SpawnDistribution(particleSpawn.spawnRateRange, particleSpawn.spawnRateSeed, 0x31B7u);
        const float sampledSpawnRateScale = Sample_SpawnDistribution(particleSpawn.spawnRateScaleRange, particleSpawn.spawnRateScaleSeed, 0x45D9u);
        const float spawnRate = max(0.f, sampledSpawnRate * sampledSpawnRateScale);
        _spawnAccumulator += spawnRate * safeDeltaTime;
        const float wholeSpawn = floorf(_spawnAccumulator + 0.0001f);
        if (wholeSpawn > 0.f)
        {
            spawnRequest += static_cast<uint32>(wholeSpawn);
            _spawnAccumulator -= wholeSpawn;
        }
    }

    if (particleSpawn.processBurstList)
    {
        if (_burstExecuted.size() != particleSpawn.bursts.size())
            _burstExecuted.assign(particleSpawn.bursts.size(), false);

        const float activeElapsedTime = max(0.f, _loopElapsedTime - Resolve_CurrentLoopDelay());
        for (size_t index = 0; index < particleSpawn.bursts.size(); ++index)
        {
            const PointParticleBurstDesc& burst = particleSpawn.bursts[index];
            if (_burstExecuted[index] || activeElapsedTime < burst.time)
                continue;

            const float sampledBurstScale = Sample_SpawnDistribution(
                particleSpawn.burstScaleRange,
                particleSpawn.burstScaleSeed,
                static_cast<uint32>(0xB851u + index * 131u)
            );
            const float scaledCount = static_cast<float>(burst.count) * max(0.f, sampledBurstScale);
            spawnRequest += static_cast<uint32>(floorf(scaledCount));
            _burstExecuted[index] = true;
        }
    }

    if (_desc.spawnInitialParticle && _samples.empty())
        ++spawnRequest;

    const uint32 room = _desc.maxSampleCount > static_cast<uint32>(_samples.size())
                        ? _desc.maxSampleCount - static_cast<uint32>(_samples.size())
                        : 0u;
    spawnRequest = min(spawnRequest, room);

    for (uint32 index = 0; index < spawnRequest; ++index)
        Spawn_RibbonSample(index, max(1u, spawnRequest));

    if (!_samples.empty())
    {
        _samples.front().distance = 0.f;
        for (size_t index = 1; index < _samples.size(); ++index)
        {
            const Vec3 delta = _samples[index].position - _samples[index - 1].position;
            _samples[index].distance = _samples[index - 1].distance + delta.Length();
        }
    }

    vector<RibbonRenderSample> renderSamples{};
    Build_RenderSamples(renderSamples);
    _visibleRibbonLength = renderSamples.size() >= 2
                           ? max(0.0001f, renderSamples.back().distanceFromHead)
                           : 1.f;
}

void ComputeRibbonEmitter::Spawn_RibbonSample(uint32 placementIndex, uint32 placementCount)
{
    RibbonSample sample{};
    PlaneRadialSample planeRadialSample{};
    const uint32 sampleIndex = _nextSampleIndex++;
    const uint32 emitterSeed = _emitterId * 104729u;
    const uint32 seed = sampleIndex * 9781u + _loopIndex * 7919u + emitterSeed + 1u;
    const Vec3 ownerPosition = _transformCom != nullptr ? _transformCom->Get_WorldPosition() : Vec3{};
    const Vec3 spawnOffset = Sample_InitialOffset(seed, placementIndex, max(1u, placementCount), planeRadialSample);
    const Vec3 spawnPosition = ownerPosition + spawnOffset;
    const float lifeMin = max(0.0001f, min(_desc.lifetime.lifeTime.x, _desc.lifetime.lifeTime.y));
    const float lifeMax = max(lifeMin, max(_desc.lifetime.lifeTime.x, _desc.lifetime.lifeTime.y));
    const uint32 lifetimeSeed = seed + Internal::Resolve_SeedSalt(_desc.lifetimeSeed, _effectPlaybackSeed);

    sample.position = spawnPosition;
    sample.velocity = Sample_InitialVelocity(spawnPosition, planeRadialSample, seed + 23u);
    sample.acceleration = Sample_Acceleration(seed + 29u);
    sample.drag = Sample_Drag(seed + 31u);
    sample.age = 0.f;
    sample.lifetime = RandomRange(lifeMin, lifeMax, lifetimeSeed + 11u);
    sample.distance = 0.f;
    sample.startColor = Sample_Color(
        _desc.initialColor.startColorMin,
        _desc.initialColor.startColorMax,
        _desc.initialColor.colorSeed,
        _desc.initialColor.alphaSeed,
        seed + 47u
    );
    sample.endColor = Sample_Color(
        _desc.colorOverLife.endColorMin,
        _desc.colorOverLife.endColorMax,
        _desc.colorOverLife.colorSeed,
        _desc.colorOverLife.alphaSeed,
        seed + 53u
    );
    sample.seed = seed;

    _samples.insert(_samples.begin(), sample);
    while (static_cast<uint32>(_samples.size()) > _desc.maxSampleCount)
        _samples.pop_back();
}

void ComputeRibbonEmitter::Build_RenderSamples(vector<RibbonRenderSample>& outSamples) const
{
    outSamples.clear();
    if (_samples.empty())
        return;

    outSamples.reserve(Compute_MaxRenderSegmentCount() + 1u);
    outSamples.push_back(RibbonRenderSample{ .sample = _samples.front(), .distanceFromHead = 0.f });

    const float tessellationStep = max(0.001f, _desc.distanceTessellationStepSize);
    for (size_t index = 0; index + 1u < _samples.size(); ++index)
    {
        const RibbonSample& from = _samples[index];
        const RibbonSample& to = _samples[index + 1u];
        const float segmentDistance = (to.position - from.position).Length();
        const uint32 stepCount = min(
            max(1u, static_cast<uint32>(ceilf(segmentDistance / tessellationStep))),
            kMaxDistanceTessellationSubdivision
        );

        for (uint32 step = 1u; step <= stepCount; ++step)
        {
            const float ratio = static_cast<float>(step) / static_cast<float>(stepCount);
            RibbonSample renderSample{};
            renderSample.position = Vec3::Lerp(from.position, to.position, ratio);
            renderSample.velocity = Vec3::Lerp(from.velocity, to.velocity, ratio);
            renderSample.age = from.age + (to.age - from.age) * ratio;
            renderSample.lifetime = max(0.0001f, from.lifetime + (to.lifetime - from.lifetime) * ratio);
            renderSample.distance = 0.f;
            renderSample.startColor = Internal::Lerp_Vector4(from.startColor, to.startColor, ratio);
            renderSample.endColor = Internal::Lerp_Vector4(from.endColor, to.endColor, ratio);

            outSamples.push_back(RibbonRenderSample{ .sample = renderSample });
        }
    }

    if (outSamples.empty())
        return;

    outSamples.front().distanceFromHead = 0.f;
    for (size_t index = 1; index < outSamples.size(); ++index)
    {
        const float distance = (outSamples[index].sample.position - outSamples[index - 1].sample.position).Length();
        outSamples[index].distanceFromHead = outSamples[index - 1].distanceFromHead + distance;
        outSamples[index].sample.distance = outSamples[index].distanceFromHead;
    }
}

HRESULT ComputeRibbonEmitter::Dispatch_Compute()
{
    CHECK_FAILED(Reset_IndirectArgs(), E_FAIL);

    if (_samples.size() < 2)
    {
        _drawSegmentCount = 0u;
        CHECK_FAILED(Copy_ComputeOutput(), E_FAIL);
        return S_OK;
    }

    CHECK_FAILED(Update_ComputeInput(), E_FAIL);
    if (_drawSegmentCount == 0u)
    {
        CHECK_FAILED(Copy_ComputeOutput(), E_FAIL);
        return S_OK;
    }

    CHECK_FAILED(Update_ComputeConstants(), E_FAIL);
    CHECK_FAILED(_computeShader->Bind_SRV(0, _sampleInput->Get_SRV()), E_FAIL);
    CHECK_FAILED(_computeShader->Bind_UAV(0, _computeOutput->Get_UAV()), E_FAIL);
    CHECK_FAILED(_computeShader->Bind_UAV(1, _computeArgsOutput->Get_UAV()), E_FAIL);
    CHECK_FAILED(_computeShader->Bind_ConstantBuffer(0, _computeConstantBuffer.Get()), E_FAIL);
    CHECK_FAILED(_computeShader->Dispatch((_drawSegmentCount + kThreadCountX - 1u) / kThreadCountX, 1, 1), E_FAIL);
    CHECK_FAILED(Copy_ComputeOutput(), E_FAIL);
    return S_OK;
}

HRESULT ComputeRibbonEmitter::Update_ComputeInput()
{
    vector<RibbonRenderSample> renderSamples{};
    Build_RenderSamples(renderSamples);
    if (renderSamples.size() < 2)
    {
        _drawSegmentCount = 0u;
        _visibleRibbonLength = 1.f;
        return S_OK;
    }

    _drawSegmentCount = min(static_cast<uint32>(renderSamples.size() - 1u), Compute_MaxRenderSegmentCount());
    _visibleRibbonLength = max(0.0001f, renderSamples[min(renderSamples.size() - 1u, static_cast<size_t>(_drawSegmentCount))].distanceFromHead);

    vector<RibbonSamplePayload> payloads{};
    payloads.reserve(_drawSegmentCount + 1u);

    for (uint32 index = 0; index <= _drawSegmentCount; ++index)
    {
        const RibbonSample& sample = renderSamples[index].sample;
        const float lifeProgress = clamp(sample.age / max(sample.lifetime, 0.0001f), 0.f, 1.f);
        const float widthScale = Evaluate_WidthScaleByLife(lifeProgress);
        const Vec4 currentColor = Evaluate_ColorOverLife(sample, lifeProgress);

        RibbonSamplePayload payload{};
        payload.currentWorldPosition = Vec4(sample.position.x, sample.position.y, sample.position.z, 1.f);
        payload.sampleParams = Vec4(sample.age, sample.lifetime, renderSamples[index].distanceFromHead, max(0.001f, widthScale));
        payload.startColor = currentColor;
        payload.endColor = currentColor;
        payloads.push_back(payload);
    }

    CHECK_FAILED(_sampleInput->Update_Data(payloads.data(), static_cast<uint32>(payloads.size())), E_FAIL);
    return S_OK;
}

HRESULT ComputeRibbonEmitter::Update_ComputeConstants()
{
    RibbonComputeParams params{};
    params.segmentCount = _drawSegmentCount;
    params.renderAxis = static_cast<uint32>(_desc.renderAxis);
    params.tilingDistance = _desc.tilingDistance;
    params.visibleLength = _visibleRibbonLength;
    params.axisFallback = Vec4(1.f, 0.f, 0.f, 0.f);
    params.fadeParams = Vec4(1.f, 0.f, 0.f, 0.f);
    _context->UpdateSubresource(_computeConstantBuffer.Get(), 0, nullptr, &params, 0, 0);
    return S_OK;
}

HRESULT ComputeRibbonEmitter::Reset_IndirectArgs()
{
    const DrawIndexedInstancedIndirectArgs args{};
    CHECK_FAILED(_computeArgsOutput->Update_Data(&args, 1), E_FAIL);
    return Copy_ComputeOutput();
}

HRESULT ComputeRibbonEmitter::Copy_ComputeOutput()
{
    if (nullptr != _instanceBuffer && nullptr != _computeOutput)
        _context->CopyResource(_instanceBuffer.Get(), _computeOutput->Get_Buffer());
    if (nullptr != _indirectArgsBuffer && nullptr != _computeArgsOutput)
        _context->CopyResource(_indirectArgsBuffer.Get(), _computeArgsOutput->Get_Buffer());
    return S_OK;
}

HRESULT ComputeRibbonEmitter::Bind_ShaderResources()
{
    CHECK_FAILED(GAME->Bind_CameraCB(_shader), E_FAIL);
    CHECK_FAILED(Bind_MaterialResources(), E_FAIL);
    if (Is_DistortionFamily())
        CHECK_FAILED(Bind_DistortionResources(), E_FAIL);
    return S_OK;
}

HRESULT ComputeRibbonEmitter::Bind_MaterialResources()
{
    const float emitterPhase = Compute_MaterialScalarModulationPhase(_materialElapsedTime, _desc.playback.duration);
    const EffectRequiredMaterialRuntimeDesc material =
        Internal::Resolve_EmitterTimeCoreColorRgbModulation(_desc.material, emitterPhase);

    CHECK_FAILED(_mainTexture->Bind_ShaderResourceView(_shader.get(), "g_MainTexture", 0), E_FAIL);

    if (nullptr != _noiseTexture)
        CHECK_FAILED(_noiseTexture->Bind_ShaderResourceView(_shader.get(), "g_NoiseTexture", 0), E_FAIL);
    else
        CHECK_FAILED(_shader->Bind_SRV("g_NoiseTexture", nullptr), E_FAIL);

    if (nullptr != _maskTexture)
        CHECK_FAILED(_maskTexture->Bind_ShaderResourceView(_shader.get(), "g_MaskTexture", 0), E_FAIL);
    else
        CHECK_FAILED(_shader->Bind_SRV("g_MaskTexture", nullptr), E_FAIL);

    CHECK_FAILED(_shader->Bind_RawValue("g_Tint", &material.tint, sizeof(material.tint)), E_FAIL);

    const Vec4 effectRibbonParams = Vec4(
        material.intensity,
        max(material.opacityPower, 0.0001f),
        material.noiseStrength,
        nullptr != _noiseTexture ? 1.f : 0.f
    );
    const Vec4 effectRibbonAlphaParams = Vec4(
        clamp(material.alphaCutoff, 0.f, 1.f),
        clamp(material.alphaErosion, 0.f, 1.f),
        nullptr != _maskTexture ? 1.f : 0.f,
        0.f
    );
    const Vec4 effectRibbonMainUVParams = Vec4(
        material.mainUVScale.x,
        material.mainUVScale.y,
        material.mainUVScrollSpeed.x,
        material.mainUVScrollSpeed.y
    );
    const Vec4 effectRibbonNoiseUVParams = Vec4(
        material.noiseUVScale.x,
        material.noiseUVScale.y,
        material.noiseUVScrollSpeed.x,
        material.noiseUVScrollSpeed.y
    );
    const Vec4 effectRibbonMaskUVParams = Vec4(
        material.maskUVScale.x,
        material.maskUVScale.y,
        material.maskUVScrollSpeed.x,
        material.maskUVScrollSpeed.y
    );
    const Vec4 effectRibbonUVOffsetParams = Vec4(
        material.mainUVOffset.x,
        material.mainUVOffset.y,
        material.noiseUVOffset.x,
        material.noiseUVOffset.y
    );
    const Vec4 effectRibbonMaskUVOffsetParams = Vec4(
        material.maskUVOffset.x,
        material.maskUVOffset.y,
        0.f,
        0.f
    );
    const Vec4 effectRibbonUVModeParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.mainUVTilingMode)),
        static_cast<float>(static_cast<uint32>(material.noiseUVTilingMode)),
        static_cast<float>(static_cast<uint32>(material.maskUVTilingMode)),
        0.f
    );
    const Vec4 effectRibbonSourceParams = Vec4(
        static_cast<float>(Internal::Resolve_MaterialSourceIndex(material.noiseSource)),
        static_cast<float>(Internal::Resolve_MaterialSourceIndex(material.maskSource)),
        material.noiseInvert ? 1.f : 0.f,
        material.maskInvert ? 1.f : 0.f
    );
    const Vec4 effectRibbonAdditiveParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.additive.colorSource)),
        static_cast<float>(static_cast<uint32>(material.additive.amountSource)),
        static_cast<float>(static_cast<uint32>(material.additive.coveragePolicy)),
        material.additive.intensityScale
    );
    const Vec4 effectRibbonAdditiveFlags = Vec4(material.additive.blackNeutral ? 1.f : 0.f, 0.f, 0.f, 0.f);
    const Vec4 effectRibbonCoreEmissiveParams = Vec4(
        material.coreEmissive.enabled ? 1.f : 0.f,
        material.coreEmissive.corePower,
        material.coreEmissive.coreIntensity,
        material.coreEmissive.outerPower
    );
    const Vec4 effectRibbonCoreEmissiveColor = Vec4(
        material.coreEmissive.coreColor.x,
        material.coreEmissive.coreColor.y,
        material.coreEmissive.coreColor.z,
        material.coreEmissive.outerIntensity
    );
    const Vec4 effectRibbonAxisParams = Vec4(
        static_cast<float>(static_cast<uint32>(_desc.renderAxis)),
        _visibleRibbonLength,
        _desc.tilingDistance,
        _desc.enablePreviousTangentRecalculation ? 1.f : 0.f
    );
    const Vec4 effectRibbonFallbackAxis{ 1.f, 0.f, 0.f, 0.f };
    const int opacitySourceIndex = Resolve_OpacitySourceIndex();

    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonParams", &effectRibbonParams, sizeof(effectRibbonParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonAlphaParams", &effectRibbonAlphaParams, sizeof(effectRibbonAlphaParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonMainUVParams", &effectRibbonMainUVParams, sizeof(effectRibbonMainUVParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonNoiseUVParams", &effectRibbonNoiseUVParams, sizeof(effectRibbonNoiseUVParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonMaskUVParams", &effectRibbonMaskUVParams, sizeof(effectRibbonMaskUVParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonUVOffsetParams", &effectRibbonUVOffsetParams, sizeof(effectRibbonUVOffsetParams)), E_FAIL);
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectRibbonMaskUVOffsetParams", &effectRibbonMaskUVOffsetParams, sizeof(effectRibbonMaskUVOffsetParams)),
        E_FAIL
    );
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonUVModeParams", &effectRibbonUVModeParams, sizeof(effectRibbonUVModeParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonSourceParams", &effectRibbonSourceParams, sizeof(effectRibbonSourceParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonAdditiveParams", &effectRibbonAdditiveParams, sizeof(effectRibbonAdditiveParams)), E_FAIL);
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectRibbonAdditiveEmissiveColor", &material.additive.emissiveColor, sizeof(material.additive.emissiveColor)),
        E_FAIL
    );
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectRibbonAdditiveConstantColor", &material.additive.constantColor, sizeof(material.additive.constantColor)),
        E_FAIL
    );
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonAdditiveFlags", &effectRibbonAdditiveFlags, sizeof(effectRibbonAdditiveFlags)), E_FAIL);
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectRibbonCoreEmissiveParams", &effectRibbonCoreEmissiveParams, sizeof(effectRibbonCoreEmissiveParams)),
        E_FAIL
    );
    CHECK_FAILED(
        _shader->Bind_RawValue("g_EffectRibbonCoreEmissiveColor", &effectRibbonCoreEmissiveColor, sizeof(effectRibbonCoreEmissiveColor)),
        E_FAIL
    );
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonAxisParams", &effectRibbonAxisParams, sizeof(effectRibbonAxisParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonFallbackAxis", &effectRibbonFallbackAxis, sizeof(effectRibbonFallbackAxis)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonMaterialTime", &_materialElapsedTime, sizeof(_materialElapsedTime)), E_FAIL);
    CHECK_FAILED(Internal::Bind_MaterialScalarModulationPayload(_shader.get(), _desc.material.scalarModulation, emitterPhase), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_OpacitySource", &opacitySourceIndex, sizeof(opacitySourceIndex)), E_FAIL);
    return S_OK;
}

HRESULT ComputeRibbonEmitter::Bind_DistortionResources()
{
    if (nullptr != _flowTexture)
        CHECK_FAILED(_flowTexture->Bind_ShaderResourceView(_shader.get(), "g_FlowTexture", 0), E_FAIL);
    else
        CHECK_FAILED(_shader->Bind_SRV("g_FlowTexture", nullptr), E_FAIL);

    D3D11_VIEWPORT viewport{};
    uint32 viewportCount = 1u;
    _context->RSGetViewports(&viewportCount, &viewport);

    const Vec4 distortionParams(
        _desc.material.refractionIntensity,
        max(0.f, _desc.material.refractionPresence),
        nullptr != _flowTexture ? 1.f : 0.f,
        _materialElapsedTime
    );
    const Vec4 distortionScreenSize(max(1.f, viewport.Width), max(1.f, viewport.Height), 0.f, 0.f);
    const float flowUVTilingMode = static_cast<float>(static_cast<uint32>(_desc.material.flowUVTilingMode));

    CHECK_FAILED(_shader->Bind_RawValue("g_EffectDistortionParams", &distortionParams, sizeof(distortionParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_DistortionScreenSize", &distortionScreenSize, sizeof(distortionScreenSize)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_FlowUVScale", &_desc.material.flowUVScale, sizeof(_desc.material.flowUVScale)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_FlowUVOffset", &_desc.material.flowUVOffset, sizeof(_desc.material.flowUVOffset)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_FlowUVScrollSpeed", &_desc.material.flowUVScrollSpeed, sizeof(_desc.material.flowUVScrollSpeed)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_FlowUVTilingMode", &flowUVTilingMode, sizeof(flowUVTilingMode)), E_FAIL);
    return S_OK;
}

bool ComputeRibbonEmitter::Can_SubmitRender() const
{
    return _drawSegmentCount > 0u && Is_Visible() && nullptr != _shader &&
           nullptr != _pointVB && nullptr != _indexBuffer && nullptr != _instanceBuffer && nullptr != _indirectArgsBuffer;
}

uint32 ComputeRibbonEmitter::Compute_MaxRenderSegmentCount() const
{
    const uint32 maxRawSegments = max(1u, _desc.maxSampleCount - 1u);
    return maxRawSegments * kMaxDistanceTessellationSubdivision;
}

float ComputeRibbonEmitter::Sample_SpawnDistribution(
    const Vec2& range,
    const PointParticleRandomSeedRuntimeDesc& seed,
    uint32 salt) const
{
    const float minValue = min(range.x, range.y);
    const float maxValue = max(range.x, range.y);
    if (minValue == maxValue)
        return minValue;

    const uint32 baseSeed = (_loopIndex + 1u) * 9781u + salt + Internal::Resolve_SeedSalt(seed, _effectPlaybackSeed);
    return lerp(minValue, maxValue, Internal::Hash01(baseSeed));
}

Vec3 ComputeRibbonEmitter::Sample_InitialOffset(
    uint32 seed,
    uint32 placementIndex,
    uint32 placementCount,
    PlaneRadialSample& outPlaneRadialSample) const
{
    Vec3 offset{};

    if (_desc.initialLocation.enabled)
    {
        const uint32 locationSeed = seed + Internal::Resolve_SeedSalt(_desc.initialLocationSeed, _effectPlaybackSeed);
        offset += Vec3{
            RandomRange(_desc.initialLocation.minOffset.x, _desc.initialLocation.maxOffset.x, locationSeed + 1u),
            RandomRange(_desc.initialLocation.minOffset.y, _desc.initialLocation.maxOffset.y, locationSeed + 2u),
            RandomRange(_desc.initialLocation.minOffset.z, _desc.initialLocation.maxOffset.z, locationSeed + 3u)
        };
    }

    if (_desc.sphereLocation.enabled && _desc.sphereLocation.radius > 0.f)
    {
        const uint32 sphereSeed = seed + Internal::Resolve_SeedSalt(_desc.sphereLocationSeed, _effectPlaybackSeed);
        Vec3 direction{};
        const bool useEvenSurfacePlacement =
            _desc.sphereLocation.mode == PointParticleSphereLocationMode::Surface &&
            _desc.sphereLocation.placementMode == PointParticleSphereLocationPlacementMode::EvenByParticleIndex;
        if (useEvenSurfacePlacement)
            direction = Internal::Resolve_EvenByParticleIndexSphereDirection(placementIndex, placementCount);
        else
        {
            const float z = RandomRange(-1.f, 1.f, sphereSeed + 5u);
            const float angle = XM_2PI * Random01(sphereSeed + 7u);
            const float radiusOnPlane = sqrtf(max(0.f, 1.f - z * z));
            direction = Vec3{
                cosf(angle) * radiusOnPlane,
                sinf(angle) * radiusOnPlane,
                z
            };
        }

        float radius = _desc.sphereLocation.radius;
        if (_desc.sphereLocation.mode == PointParticleSphereLocationMode::Volume)
            radius *= cbrtf(Random01(sphereSeed + 9u));

        offset += _desc.sphereLocation.offset + direction * radius;
    }

    if (_desc.planeRadialLocation.enabled)
    {
        Vec3 axisU{};
        Vec3 axisV{};
        Vec3 planeNormal{};
        Internal::Resolve_PlaneRadialBasis(_desc.planeRadialLocation.plane, axisU, axisV, planeNormal);

        float u = 0.f;
        float v = 0.f;
        float sampleAngle = 0.f;
        if (_desc.planeRadialLocation.shape == PointParticlePlaneRadialLocationShape::Rectangle)
        {
            u = RandomRange(
                _desc.planeRadialLocation.uRange.x,
                _desc.planeRadialLocation.uRange.y,
                seed + Internal::Resolve_SeedSalt(_desc.planeRadialLocation.uSeed, _effectPlaybackSeed) + 1601u
            );
            v = RandomRange(
                _desc.planeRadialLocation.vRange.x,
                _desc.planeRadialLocation.vRange.y,
                seed + Internal::Resolve_SeedSalt(_desc.planeRadialLocation.vSeed, _effectPlaybackSeed) + 1613u
            );
            sampleAngle = atan2f(v, u);
        }
        else
        {
            const float radiusMin = min(_desc.planeRadialLocation.radiusRange.x, _desc.planeRadialLocation.radiusRange.y);
            const float radiusMax = max(_desc.planeRadialLocation.radiusRange.x, _desc.planeRadialLocation.radiusRange.y);
            const float angleMinDegrees = _desc.planeRadialLocation.angleDegreesRange.x;
            const float angleMaxDegrees = _desc.planeRadialLocation.angleDegreesRange.y;
            const float angleMin = XMConvertToRadians(angleMinDegrees);
            const float angleMax = XMConvertToRadians(angleMaxDegrees);
            const float radius = RandomRange(
                radiusMin,
                radiusMax,
                seed + Internal::Resolve_SeedSalt(_desc.planeRadialLocation.radiusSeed, _effectPlaybackSeed) + 1601u
            );
            const float angleRatio = _desc.planeRadialLocation.placementMode == PointParticlePlaneRadialLocationPlacementMode::EvenByParticleIndex
                                     ? Internal::Resolve_EvenByParticleIndexAngleRatio(
                                         placementIndex,
                                         placementCount,
                                         angleMinDegrees,
                                         angleMaxDegrees
                                     )
                                     : Random01(seed + Internal::Resolve_SeedSalt(_desc.planeRadialLocation.angleDegreesSeed, _effectPlaybackSeed) + 1613u);
            sampleAngle = lerp(angleMin, angleMax, angleRatio);
            u = cosf(sampleAngle) * radius;
            v = sinf(sampleAngle) * radius;
        }

        const float thickness = max(0.f, _desc.planeRadialLocation.thickness);
        const float normalOffset = thickness > 0.0001f ? (Random01(seed + 1621u) - 0.5f) * thickness : 0.f;
        const Vec3 radialVector = axisU * u + axisV * v;
        const Vec3 fallbackRadial = Internal::Normalize_OrFallback(axisU * cosf(sampleAngle) + axisV * sinf(sampleAngle), axisU);
        outPlaneRadialSample.enabled = true;
        outPlaneRadialSample.localOffset = _desc.planeRadialLocation.offset + radialVector + planeNormal * normalOffset;
        outPlaneRadialSample.radialDirection = Internal::Normalize_OrFallback(radialVector, fallbackRadial);
        offset += outPlaneRadialSample.localOffset;
    }

    return Resolve_LocalVector(offset);
}

Vec3 ComputeRibbonEmitter::Sample_InitialVelocity(const Vec3& spawnPosition, const PlaneRadialSample& planeRadialSample, uint32 seed) const
{
    Vec3 velocity{};
    const uint32 initialVelocitySeed = seed + Internal::Resolve_SeedSalt(_desc.motion.initialVelocitySeed, _effectPlaybackSeed);
    const uint32 initialRadialVelocitySeed = seed + Internal::Resolve_SeedSalt(_desc.motion.initialRadialVelocitySeed, _effectPlaybackSeed);
    const uint32 velocityConeSeed = seed + Internal::Resolve_SeedSalt(_desc.motion.velocityConeSeed, _effectPlaybackSeed);

    if (_desc.motion.initialVelocityEnabled)
    {
        const Vec3 sampledVelocity{
            RandomRange(_desc.motion.initialVelocityMin.x, _desc.motion.initialVelocityMax.x, initialVelocitySeed + 1u),
            RandomRange(_desc.motion.initialVelocityMin.y, _desc.motion.initialVelocityMax.y, initialVelocitySeed + 2u),
            RandomRange(_desc.motion.initialVelocityMin.z, _desc.motion.initialVelocityMax.z, initialVelocitySeed + 3u)
        };
        velocity += _desc.motion.initialVelocityInWorldSpace ? sampledVelocity : Resolve_LocalVector(sampledVelocity);
    }

    if (_desc.motion.initialRadialVelocityEnabled)
    {
        const Vec3 radialPivotOffset = _desc.motion.initialRadialVelocityInWorldSpace
                                       ? _desc.motion.radialPivot
                                       : Resolve_LocalVector(_desc.motion.radialPivot);
        const Vec3 emitterPosition = _transformCom != nullptr ? _transformCom->Get_WorldPosition() : Vec3{};
        const Vec3 pivotPosition = emitterPosition + radialPivotOffset;
        Vec3 radialDirection = spawnPosition - pivotPosition;
        if (radialDirection.LengthSquared() < 0.0001f)
        {
            if (_desc.motion.initialRadialVelocityCenterDirectionMode ==
                PointParticleInitialRadialVelocityCenterDirectionMode::PlaneRadial &&
                planeRadialSample.enabled)
                radialDirection = Resolve_LocalVector(planeRadialSample.radialDirection);
            else
            {
                radialDirection = Resolve_LocalVector(
                    Vec3{
                        Random01(initialRadialVelocitySeed + 307u) * 2.f - 1.f,
                        0.25f,
                        Random01(initialRadialVelocitySeed + 401u) * 2.f - 1.f
                    }
                );
            }
        }
        radialDirection.Normalize();

        const float radialSpeed = RandomRange(
            min(_desc.motion.radialSpeed.x, _desc.motion.radialSpeed.y),
            max(_desc.motion.radialSpeed.x, _desc.motion.radialSpeed.y),
            initialRadialVelocitySeed + 5u
        );
        velocity += radialDirection * radialSpeed;
    }

    if (_desc.motion.velocityConeEnabled)
    {
        const Vec3 axis = Internal::Normalize_OrFallback(_desc.motion.velocityConeAxis, Vec3{ 0.f, 1.f, 0.f });
        Vec3 sampledDirection = axis;
        const float angleDegrees = clamp(_desc.motion.velocityConeAngleDegrees, 0.f, 180.f);
        if (angleDegrees > 0.0001f)
        {
            const float cosMax = cosf(XMConvertToRadians(angleDegrees));
            const float cosTheta = lerp(1.f, cosMax, Random01(velocityConeSeed + 1703u));
            const float sinTheta = sqrtf(max(0.f, 1.f - cosTheta * cosTheta));
            const float phi = XM_2PI * Random01(velocityConeSeed + 1721u);
            const Vec3 helper = fabsf(axis.y) < 0.999f ? Vec3{ 0.f, 1.f, 0.f } : Vec3{ 1.f, 0.f, 0.f };
            Vec3 tangent = helper.Cross(axis);
            tangent.Normalize();
            const Vec3 bitangent = axis.Cross(tangent);
            sampledDirection = axis * cosTheta + (tangent * cosf(phi) + bitangent * sinf(phi)) * sinTheta;
        }

        const Vec3 coneDirection = _desc.motion.velocityConeInWorldSpace
                                   ? sampledDirection
                                   : Resolve_LocalVector(sampledDirection);
        const float coneSpeed = RandomRange(
            min(_desc.motion.velocityConeSpeed.x, _desc.motion.velocityConeSpeed.y),
            max(_desc.motion.velocityConeSpeed.x, _desc.motion.velocityConeSpeed.y),
            velocityConeSeed + 1747u
        );
        velocity += coneDirection * coneSpeed;
    }

    return velocity;
}

Vec3 ComputeRibbonEmitter::Sample_Acceleration(uint32 seed) const
{
    const uint32 accelerationSeed = seed + Internal::Resolve_SeedSalt(_desc.motion.accelerationSeed, _effectPlaybackSeed);
    const Vec3 sampledAcceleration{
        RandomRange(_desc.motion.accelerationMin.x, _desc.motion.accelerationMax.x, accelerationSeed + 1u),
        RandomRange(_desc.motion.accelerationMin.y, _desc.motion.accelerationMax.y, accelerationSeed + 2u),
        RandomRange(_desc.motion.accelerationMin.z, _desc.motion.accelerationMax.z, accelerationSeed + 3u)
    };
    return Resolve_LocalVector(sampledAcceleration);
}

float ComputeRibbonEmitter::Sample_Drag(uint32 seed) const
{
    const float minDrag = min(_desc.motion.drag.x, _desc.motion.drag.y);
    const float maxDrag = max(_desc.motion.drag.x, _desc.motion.drag.y);
    const uint32 dragSeed = seed + Internal::Resolve_SeedSalt(_desc.motion.dragSeed, _effectPlaybackSeed);
    return max(0.f, RandomRange(minDrag, maxDrag, dragSeed));
}

Vec4 ComputeRibbonEmitter::Sample_Color(
    const Vec4& minColor,
    const Vec4& maxColor,
    const PointParticleRandomSeedRuntimeDesc& colorSeedDesc,
    const PointParticleRandomSeedRuntimeDesc& alphaSeedDesc,
    uint32 seed) const
{
    const uint32 colorSeed = seed + Internal::Resolve_SeedSalt(colorSeedDesc, _effectPlaybackSeed);
    const uint32 alphaSeed = seed + Internal::Resolve_SeedSalt(alphaSeedDesc, _effectPlaybackSeed);
    return Vec4{
        RandomRange(min(minColor.x, maxColor.x), max(minColor.x, maxColor.x), colorSeed + 1u),
        RandomRange(min(minColor.y, maxColor.y), max(minColor.y, maxColor.y), colorSeed + 2u),
        RandomRange(min(minColor.z, maxColor.z), max(minColor.z, maxColor.z), colorSeed + 3u),
        RandomRange(min(minColor.w, maxColor.w), max(minColor.w, maxColor.w), alphaSeed + 4u)
    };
}

float ComputeRibbonEmitter::Evaluate_VelocityScaleByLife(float lifeProgress) const
{
    const float fallbackScale = lerp(_desc.motion.velocityScaleByLife.x, _desc.motion.velocityScaleByLife.y, lifeProgress);
    const float curveScale = Evaluate_CompactCurve(
        lifeProgress,
        _desc.motion.velocityScaleByLifeCurveTimes,
        _desc.motion.velocityScaleByLifeCurveTimesBlock1,
        _desc.motion.velocityScaleByLifeCurveValues,
        _desc.motion.velocityScaleByLifeCurveValuesBlock1,
        _desc.motion.velocityScaleByLifeCurveKeyCount,
        fallbackScale
    );
    return max(0.f, curveScale);
}

Vec3 ComputeRibbonEmitter::Evaluate_AccelerationByLife(const RibbonSample& sample, float lifeProgress) const
{
    if (!_desc.motion.accelerationCurveEnabled)
        return sample.acceleration;

    const uint32 keyCount = max(1u, min(::Engine::kEffectDistributionCurveMaxKeys, _desc.motion.accelerationCurveKeyCount));
    const Vec3 acceleration{
        Evaluate_CompactCurve(
            lifeProgress,
            _desc.motion.accelerationCurveTimes,
            _desc.motion.accelerationCurveTimesBlock1,
            _desc.motion.accelerationCurveValuesX,
            _desc.motion.accelerationCurveValuesXBlock1,
            keyCount,
            sample.acceleration.x
        ),
        Evaluate_CompactCurve(
            lifeProgress,
            _desc.motion.accelerationCurveTimes,
            _desc.motion.accelerationCurveTimesBlock1,
            _desc.motion.accelerationCurveValuesY,
            _desc.motion.accelerationCurveValuesYBlock1,
            keyCount,
            sample.acceleration.y
        ),
        Evaluate_CompactCurve(
            lifeProgress,
            _desc.motion.accelerationCurveTimes,
            _desc.motion.accelerationCurveTimesBlock1,
            _desc.motion.accelerationCurveValuesZ,
            _desc.motion.accelerationCurveValuesZBlock1,
            keyCount,
            sample.acceleration.z
        )
    };
    return Resolve_LocalVector(acceleration);
}

Vec4 ComputeRibbonEmitter::Evaluate_ColorOverLife(const RibbonSample& sample, float lifeProgress) const
{
    Vec4 color = {
        lerp(sample.startColor.x, sample.endColor.x, lifeProgress),
        lerp(sample.startColor.y, sample.endColor.y, lifeProgress),
        lerp(sample.startColor.z, sample.endColor.z, lifeProgress),
        lerp(sample.startColor.w, sample.endColor.w, lifeProgress)
    };

    const PointParticleColorOverLifeCurveDesc& curve = _desc.colorOverLife.curve;
    if (curve.colorCurveEnabled)
    {
        const uint32 keyCount = max(1u, min(::Engine::kEffectDistributionCurveMaxKeys, curve.colorCurveKeyCount));
        color.x = Evaluate_CompactCurve(
            lifeProgress,
            curve.colorCurveTimes,
            curve.colorCurveTimesBlock1,
            curve.colorCurveValuesR,
            curve.colorCurveValuesRBlock1,
            keyCount,
            color.x
        );
        color.y = Evaluate_CompactCurve(
            lifeProgress,
            curve.colorCurveTimes,
            curve.colorCurveTimesBlock1,
            curve.colorCurveValuesG,
            curve.colorCurveValuesGBlock1,
            keyCount,
            color.y
        );
        color.z = Evaluate_CompactCurve(
            lifeProgress,
            curve.colorCurveTimes,
            curve.colorCurveTimesBlock1,
            curve.colorCurveValuesB,
            curve.colorCurveValuesBBlock1,
            keyCount,
            color.z
        );
    }

    if (curve.alphaCurveEnabled)
    {
        const uint32 keyCount = max(1u, min(::Engine::kEffectDistributionCurveMaxKeys, curve.alphaCurveKeyCount));
        color.w = Evaluate_CompactCurve(
            lifeProgress,
            curve.alphaCurveTimes,
            curve.alphaCurveTimesBlock1,
            curve.alphaCurveValues,
            curve.alphaCurveValuesBlock1,
            keyCount,
            color.w
        );
    }

    color.w = clamp(color.w, 0.f, 1.f);
    return color;
}

float ComputeRibbonEmitter::Evaluate_WidthScaleByLife(float lifeProgress) const
{
    if (!_desc.sizeByLife.enabled)
        return 1.f;

    const float fallbackScale = lerp(_desc.sizeByLife.multiplyXStart, _desc.sizeByLife.multiplyXEnd, lifeProgress);
    const float curveScale = Evaluate_CompactCurve(
        lifeProgress,
        _desc.sizeByLife.curveKeyTimes,
        _desc.sizeByLife.curveKeyTimesBlock1,
        _desc.sizeByLife.curveKeyValuesX,
        _desc.sizeByLife.curveKeyValuesXBlock1,
        _desc.sizeByLife.curveKeyCount,
        fallbackScale
    );
    return _desc.sizeByLife.multiplyX ? max(0.f, curveScale) : 1.f;
}

float ComputeRibbonEmitter::Evaluate_CompactCurve(
    float lifeProgress,
    const Vec4& times,
    const Vec4& timesBlock1,
    const Vec4& values,
    const Vec4& valuesBlock1,
    uint32 keyCount,
    float fallbackValue) const
{
    const uint32 clampedKeyCount = max(1u, min(::Engine::kEffectDistributionCurveMaxKeys, keyCount));
    if (clampedKeyCount == 1u || lifeProgress <= Internal::Read_Vec4Component(times, timesBlock1, 0u))
        return Internal::Read_Vec4Component(values, valuesBlock1, 0u);

    for (uint32 index = 1u; index < clampedKeyCount; ++index)
    {
        const float rightTime = Internal::Read_Vec4Component(times, timesBlock1, index);
        if (lifeProgress > rightTime)
            continue;

        const float leftTime = Internal::Read_Vec4Component(times, timesBlock1, index - 1u);
        const float ratio = clamp((lifeProgress - leftTime) / max(0.0001f, rightTime - leftTime), 0.f, 1.f);
        return lerp(
            Internal::Read_Vec4Component(values, valuesBlock1, index - 1u),
            Internal::Read_Vec4Component(values, valuesBlock1, index),
            ratio
        );
    }

    return lifeProgress > Internal::Read_Vec4Component(times, timesBlock1, clampedKeyCount - 1u)
           ? Internal::Read_Vec4Component(values, valuesBlock1, clampedKeyCount - 1u)
           : fallbackValue;
}

Vec3 ComputeRibbonEmitter::Resolve_LocalVector(const Vec3& localVector) const
{
    if (_transformCom == nullptr)
        return localVector;

    return _transformCom->Get_WorldRight() * localVector.x +
           _transformCom->Get_WorldUp() * localVector.y +
           _transformCom->Get_WorldForward() * localVector.z;
}

float ComputeRibbonEmitter::Random01(uint32 seed) const
{
    return Internal::Hash01(seed);
}

float ComputeRibbonEmitter::RandomRange(float minValue, float maxValue, uint32 seed) const
{
    if (minValue > maxValue)
        std::swap(minValue, maxValue);

    return lerp(minValue, maxValue, Random01(seed));
}

void ComputeRibbonEmitter::Start_NextLoop()
{
    ++_loopIndex;
    _loopElapsedTime = 0.f;
    _playbackState = PlaybackState::Delayed;
    _samples.clear();
    _burstExecuted.assign(_desc.spawn.particleSpawn.bursts.size(), false);
    _spawnAccumulator = 0.f;
    _drawSegmentCount = 0u;
    _nextSampleIndex = 0u;
}

float ComputeRibbonEmitter::Resolve_CurrentLoopDelay() const
{
    if (_desc.playback.delayFirstLoopOnly && _loopIndex > 0u)
        return 0.f;

    return max(0.f, _desc.playback.delay);
}

float ComputeRibbonEmitter::Resolve_Duration() const
{
    return max(0.0001f, _desc.playback.duration);
}

bool ComputeRibbonEmitter::Has_NextLoop() const
{
    return 0u == _desc.playback.loopCount || _loopIndex + 1u < _desc.playback.loopCount;
}

bool ComputeRibbonEmitter::Is_DistortionFamily() const
{
    return _desc.material.materialFamily == EffectMaterialFamily::SpriteDistortion;
}

RenderGroup ComputeRibbonEmitter::Resolve_RenderGroup() const
{
    if (_desc.renderLayerOverride == EffectEmitterRenderLayerOverride::UIEffect)
        return RenderGroup::UIEffect;

    return Is_DistortionFamily() ? RenderGroup::Distortion : RenderGroup::Blend;
}

const wchar_t* ComputeRibbonEmitter::Resolve_ShaderId() const
{
    return Is_DistortionFamily() ? kRibbonDistortionShaderId : kRibbonShaderId;
}

uint32 ComputeRibbonEmitter::Resolve_ShaderPassIndex() const
{
    if (Is_DistortionFamily())
        return 0u;

    return _desc.material.blendMode == EffectMaterialBlendMode::Additive ? 1u : 0u;
}

int ComputeRibbonEmitter::Resolve_OpacitySourceIndex() const
{
    if (_desc.material.opacitySource == "Red" || _desc.material.opacitySource == "red")
        return 1;

    if (_desc.material.opacitySource == "Luminance" || _desc.material.opacitySource == "luminance")
        return 2;

    return 0;
}

Shared<ComputeRibbonEmitter> ComputeRibbonEmitter::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<ComputeRibbonEmitter>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : ComputeRibbonEmitter");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> ComputeRibbonEmitter::Clone(void* arg)
{
    auto instance = make_shared<ComputeRibbonEmitter>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : ComputeRibbonEmitter");
        MSG_BOX("Failed to Clone : ComputeRibbonEmitter");
        return nullptr;
    }

    return instance;
}

void ComputeRibbonEmitter::Free()
{
    _computeArgsOutput = nullptr;
    _computeOutput = nullptr;
    _sampleInput = nullptr;
    _indirectArgsBuffer.Reset();
    _computeConstantBuffer.Reset();
    _instanceBuffer.Reset();
    _indexBuffer.Reset();
    _pointVB.Reset();
    _flowTexture = nullptr;
    _maskTexture = nullptr;
    _noiseTexture = nullptr;
    _mainTexture = nullptr;
    _computeShader = nullptr;
    _shader = nullptr;
    _samples.clear();
    _burstExecuted.clear();
    _desc = {};
    Reset_PlaybackRuntime();
    __super::Free();
}
