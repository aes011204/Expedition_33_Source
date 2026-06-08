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

static Vec4 Average_Color(const Vec4& minColor, const Vec4& maxColor)
{
    return (minColor + maxColor) * 0.5f;
}

static Vec3 Average_Vector(const Vec3& minValue, const Vec3& maxValue)
{
    return (minValue + maxValue) * 0.5f;
}

static Vec4 Lerp_Vector4(const Vec4& from, const Vec4& to, float ratio)
{
    return from + (to - from) * ratio;
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
        sample.position += sample.velocity * safeDeltaTime;
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
        const float spawnRate = max(0.f, particleSpawn.spawnRate * particleSpawn.spawnRateScale);
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

            spawnRequest += burst.count;
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
        Spawn_RibbonSample();

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

void ComputeRibbonEmitter::Spawn_RibbonSample()
{
    RibbonSample sample{};
    const Vec3 ownerPosition = _transformCom != nullptr ? _transformCom->Get_WorldPosition() : Vec3{};
    Vec3 localOffset{};
    if (_desc.initialLocation.enabled)
        localOffset += Internal::Average_Vector(_desc.initialLocation.minOffset, _desc.initialLocation.maxOffset);
    if (_desc.sphereLocation.enabled)
        localOffset += _desc.sphereLocation.offset;

    sample.position = ownerPosition + localOffset;
    sample.velocity = _desc.motion.initialVelocityEnabled
                      ? Internal::Average_Vector(_desc.motion.initialVelocityMin, _desc.motion.initialVelocityMax)
                      : Vec3{};
    sample.age = 0.f;
    sample.lifetime = max(0.0001f, _desc.lifetime.lifeTime.y);
    sample.distance = 0.f;
    sample.startColor = Internal::Average_Color(_desc.initialColor.startColorMin, _desc.initialColor.startColorMax);
    sample.endColor = Internal::Average_Color(_desc.colorOverLife.endColorMin, _desc.colorOverLife.endColorMax);

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
        const float widthScale = _desc.sizeByLife.enabled
                                 ? lerp(_desc.sizeByLife.multiplyXStart, _desc.sizeByLife.multiplyXEnd, lifeProgress)
                                 : 1.f;

        RibbonSamplePayload payload{};
        payload.currentWorldPosition = Vec4(sample.position.x, sample.position.y, sample.position.z, 1.f);
        payload.sampleParams = Vec4(sample.age, sample.lifetime, renderSamples[index].distanceFromHead, max(0.001f, widthScale));
        payload.startColor = sample.startColor;
        payload.endColor = sample.endColor;
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

void ComputeRibbonEmitter::Start_NextLoop()
{
    ++_loopIndex;
    _loopElapsedTime = 0.f;
    _playbackState = PlaybackState::Delayed;
    _samples.clear();
    _burstExecuted.assign(_desc.spawn.particleSpawn.bursts.size(), false);
    _spawnAccumulator = 0.f;
    _drawSegmentCount = 0u;
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
