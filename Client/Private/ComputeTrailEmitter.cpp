#include "ComputeTrailEmitter.h"

#include "ComputeShaderCom.h"
#include "ComputeStructuredBuffer.h"
#include "EffectMaterialScalarModulationRuntime.h"
#include "GameInstance.h"
#include "Renderer_Define.h"
#include "Texture.h"

namespace Client::Internal
{
static HRESULT Bind_MaterialScalarModulationPayload(
    ShaderCom* shader,
    const EffectMaterialScalarModulationRuntimeDesc& modulation,
    float emitterPhase)
{
    CHECK_NULL(shader, E_FAIL);

    const EffectMaterialScalarModulationShaderPayload payload =
        Build_MaterialScalarModulationShaderPayload(modulation, emitterPhase);

    CHECK_FAILED(shader->Bind_RawValue("g_EffectMaterialScalarModulationParams", &payload.params, sizeof(payload.params)), E_FAIL);
    CHECK_FAILED(shader->Bind_RawValue("g_EffectMaterialScalarModulationMeta", payload.meta.data(), sizeof(Vec4) * payload.meta.size()), E_FAIL);
    CHECK_FAILED(shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyTimes", payload.keyTimes.data(), sizeof(Vec4) * payload.keyTimes.size()), E_FAIL);
    CHECK_FAILED(shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyValues", payload.keyValues.data(), sizeof(Vec4) * payload.keyValues.size()), E_FAIL);
    CHECK_FAILED(
        shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyArriveTangents", payload.keyArriveTangents.data(), sizeof(Vec4) * payload.keyArriveTangents.size()),
        E_FAIL
    );
    CHECK_FAILED(
        shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyLeaveTangents", payload.keyLeaveTangents.data(), sizeof(Vec4) * payload.keyLeaveTangents.size()),
        E_FAIL
    );
    CHECK_FAILED(shader->Bind_RawValue("g_EffectMaterialScalarModulationKeyModes", payload.keyModes.data(), sizeof(Vec4) * payload.keyModes.size()), E_FAIL);

    return S_OK;
}

Vec3 Catmull_Rom(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, const float ratio)
{
    const float t2 = ratio * ratio;
    const float t3 = t2 * ratio;

    return (
               p1 * 2.f +
               (p2 - p0) * ratio +
               (p0 * 2.f - p1 * 5.f + p2 * 4.f - p3) * t2 +
               (p1 * 3.f - p0 - p2 * 3.f + p3) * t3
           ) * 0.5f;
}

EffectTrailSample Catmull_RomTrailSample(
    const EffectTrailSample& p0,
    const EffectTrailSample& p1,
    const EffectTrailSample& p2,
    const EffectTrailSample& p3,
    const float ratio)
{
    EffectTrailSample result{};
    result.baseWorldPosition = Catmull_Rom(
        p0.baseWorldPosition,
        p1.baseWorldPosition,
        p2.baseWorldPosition,
        p3.baseWorldPosition,
        ratio
    );
    result.tipWorldPosition = Catmull_Rom(
        p0.tipWorldPosition,
        p1.tipWorldPosition,
        p2.tipWorldPosition,
        p3.tipWorldPosition,
        ratio
    );

    return result;
}

EffectTrailSample Lerp_TrailSample(const EffectTrailSample& from, const EffectTrailSample& to, const float ratio)
{
    const float clampedRatio = clamp(ratio, 0.f, 1.f);

    EffectTrailSample result{};
    result.baseWorldPosition = Vec3::Lerp(from.baseWorldPosition, to.baseWorldPosition, clampedRatio);
    result.tipWorldPosition = Vec3::Lerp(from.tipWorldPosition, to.tipWorldPosition, clampedRatio);
    return result;
}

Vec3 Compute_TrailCenter(const EffectTrailSample& sample)
{
    return (sample.baseWorldPosition + sample.tipWorldPosition) * 0.5f;
}

float Compute_TrailCenterDistance(const EffectTrailSample& from, const EffectTrailSample& to)
{
    return (Compute_TrailCenter(to) - Compute_TrailCenter(from)).Length();
}

// 회전처럼 base/tip이 반대 방향으로 움직여 center가 상쇄되는 경우를 커버하기 위해
// base/tip/center 세 거리 중 최댓값을 반환한다.
// movementTolerance 판정과 stepCount 계산에 사용하며,
// 텔레포트 감지(maxFrameDistance)에는 center 기반을 별도로 사용한다.
float Compute_TrailMotionDistance(const EffectTrailSample& from, const EffectTrailSample& to)
{
    const float baseDistance = (to.baseWorldPosition - from.baseWorldPosition).Length();
    const float tipDistance = (to.tipWorldPosition - from.tipWorldPosition).Length();
    const float centerDistance = Compute_TrailCenterDistance(from, to);
    return max(centerDistance, max(baseDistance, tipDistance));
}

wstring Resolve_TrailTexturePathByGuid(const string& textureGuid)
{
    if (GAME == nullptr || textureGuid.empty())
        return {};

    const AssetMeta* assetMeta = GAME->Find_AssetByGUID(textureGuid);
    if (assetMeta == nullptr || assetMeta->type != "Texture")
        return {};

    const wstring resolvedPath = GAME->Resolve_AssetPath(textureGuid);
    return !resolvedPath.empty() && fs::exists(resolvedPath) ? resolvedPath : wstring{};
}

wstring Resolve_TrailTexturePathByPath(const string& texturePath)
{
    if (texturePath.empty())
        return {};

    fs::path candidatePath = String::ToWString(texturePath);
    if (candidatePath.is_relative() && GAME != nullptr)
        candidatePath = fs::path(GAME->Get_AssetRoot()) / candidatePath;

    candidatePath = candidatePath.lexically_normal();
    if (fs::exists(candidatePath))
        return candidatePath.wstring();

    return {};
}

bool Is_TrailTextureSamePath(const wstring& lhs, const wstring& rhs)
{
    if (lhs.empty() || rhs.empty())
        return false;

    return fs::path(lhs).lexically_normal() == fs::path(rhs).lexically_normal();
}

Vec4 Average_Color(const Vec4& minColor, const Vec4& maxColor)
{
    return Vec4{
        (minColor.x + maxColor.x) * 0.5f,
        (minColor.y + maxColor.y) * 0.5f,
        (minColor.z + maxColor.z) * 0.5f,
        (minColor.w + maxColor.w) * 0.5f
    };
}

wstring Resolve_TrailTexturePath(const string& textureGuid, const string& texturePath, bool& outUsedPathFallback, bool& outGuidPathMismatch)
{
    outUsedPathFallback = false;
    outGuidPathMismatch = false;

    wstring resolvedPath = Resolve_TrailTexturePathByGuid(textureGuid);
    if (!resolvedPath.empty())
    {
        const wstring pathResolved = Resolve_TrailTexturePathByPath(texturePath);
        outGuidPathMismatch = !pathResolved.empty() && !Is_TrailTextureSamePath(resolvedPath, pathResolved);
        return resolvedPath;
    }

    resolvedPath = Resolve_TrailTexturePathByPath(texturePath);
    if (!resolvedPath.empty())
    {
        outUsedPathFallback = true;
        return resolvedPath;
    }

    return {};
}
}

IMPLEMENT_REFLECTION(ComputeTrailEmitter)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "ComputeTrailEmitter";
    info.category = "Effect";

    return true;
}

ComputeTrailEmitter::ComputeTrailEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : EffectEmitter{ device, context }
{
}

ComputeTrailEmitter::ComputeTrailEmitter(const ComputeTrailEmitter& prototype)
    : EffectEmitter{ prototype }
{
}

ComputeTrailEmitter::~ComputeTrailEmitter()
{
    Free();
}

HRESULT ComputeTrailEmitter::Initialize_Prototype()
{
    return S_OK;
}

HRESULT ComputeTrailEmitter::Initialize(void* arg)
{
    if (nullptr != arg)
        _desc = *static_cast<ComputeTrailEmitterDesc*>(arg);

    _desc.historyCount = max(2u, _desc.historyCount);
    _desc.segmentLifetime = max(1e-3f, _desc.segmentLifetime);
    _desc.width = max(1e-3f, _desc.width);
    _desc.sampleSpacing = max(1e-3f, _desc.sampleSpacing);
    _desc.curveSubdivision = min(_desc.curveSubdivision, kMaxCurveSubdivision);
    _desc.sideFade = clamp(_desc.sideFade, 1e-3f, 0.49f);
    _desc.maxTrailLength = max(0.f, _desc.maxTrailLength);
    _desc.tailFadeLength = max(0.f, _desc.tailFadeLength);
    _desc.spawnPerUnit.spawnPerUnit = max(0.f, _desc.spawnPerUnit.spawnPerUnit);
    _desc.spawnPerUnit.unitScalar = max(1e-4f, _desc.spawnPerUnit.unitScalar);
    _desc.spawnPerUnit.movementTolerance = max(0.f, _desc.spawnPerUnit.movementTolerance);
    _desc.spawnPerUnit.maxFrameDistance = max(0.f, _desc.spawnPerUnit.maxFrameDistance);

    CHECK_FAILED(__super::Initialize(&_desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    return S_OK;
}

void ComputeTrailEmitter::Update(float timeDelta)
{
    Sync_FromEffectOwner();
    _materialElapsedTime += max(0.f, timeDelta);
    Update_History(timeDelta);
    CHECK_FAILED_THROTTLED(Dispatch_Compute(), 60);
}

void ComputeTrailEmitter::Late_Update(float)
{
    if (_drawSegmentCount > 0 && Is_Visible())
        GAME->Add_RenderGroup(Resolve_RenderGroup(), GetSharedPtr<GameObject>());
}

HRESULT ComputeTrailEmitter::Render()
{
    CHECK_FAILED_THROTTLED(Bind_ShaderResources(), 60, E_FAIL);

    CHECK_FAILED_THROTTLED(_shader->Begin(Resolve_ShaderPassIndex()), 60, E_FAIL);

    constexpr uint32 stride0 = sizeof(Vec3);
    constexpr uint32 offset0 = 0;
    constexpr uint32 stride1 = sizeof(TrailInstanceVertex);
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

HRESULT ComputeTrailEmitter::Reset_ForEffectReplay()
{
    _history.clear();
    _drawSegmentCount = 0u;
    _materialElapsedTime = 0.f;
    _visibleTrailLength = 1.f;

    if (nullptr != _computeArgsOutput)
        CHECK_FAILED(Reset_IndirectArgs(), E_FAIL);

    if (nullptr != _computeOutput && nullptr != _computeArgsOutput &&
        nullptr != _instanceBuffer && nullptr != _indirectArgsBuffer)
    {
        CHECK_FAILED(Copy_ComputeOutput(), E_FAIL);
    }

    return S_OK;
}

bool ComputeTrailEmitter::Is_EffectFinished() const
{
    return false;
}

void ComputeTrailEmitter::Bind_TrailSampleProvider(const Weak<IEffectTrailSampleProvider>& provider)
{
    _desc.sampleProvider = provider;
}

HRESULT ComputeTrailEmitter::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), String::ToWString(Resolve_ShaderId()), _shader), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), String::ToWString(kTrailComputeShaderId), _computeShader), E_FAIL);
    CHECK_FAILED(Ready_Texture(), E_FAIL);
    CHECK_FAILED(Ready_NoiseTexture(), E_FAIL);
    CHECK_FAILED(Ready_MaskTexture(), E_FAIL);
    CHECK_FAILED(Ready_FlowTexture(), E_FAIL);
    CHECK_FAILED(Ready_DrawBuffers(), E_FAIL);
    CHECK_FAILED(Ready_ComputeBuffers(), E_FAIL);
    return S_OK;
}

HRESULT ComputeTrailEmitter::Ready_Texture()
{
    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    wstring resolvedPath = Internal::Resolve_TrailTexturePath(
        _desc.material.mainTextureGuid,
        _desc.material.mainTexturePath,
        usedPathFallback,
        guidPathMismatch
    );

    if (resolvedPath.empty())
    {
        LOG_WARN(
            "ComputeTrailEmitter main texture resolve failed. path='{}'. Falling back to default trail texture.",
            _desc.material.mainTexturePath
        );

        resolvedPath = Internal::Resolve_TrailTexturePathByPath(kFallbackTexturePath);
    }

    if (resolvedPath.empty())
    {
        LOG_ERROR(
            "ComputeTrailEmitter failed to resolve default trail texture. path='{}'.",
            kFallbackTexturePath
        );
        return E_FAIL;
    }

    if (guidPathMismatch)
    {
        LOG_WARN(
            "ComputeTrailEmitter main texture GUID/path mismatch. mainTextureGuid='{}', mainTexturePath='{}', guidResolvedPath='{}'.",
            _desc.material.mainTextureGuid,
            _desc.material.mainTexturePath,
            String::ToString(resolvedPath)
        );
    }
    else if (usedPathFallback)
    {
        LOG_WARN(
            "ComputeTrailEmitter main texture fell back to path. mainTextureGuid='{}', mainTexturePath='{}', resolvedPath='{}'.",
            _desc.material.mainTextureGuid,
            _desc.material.mainTexturePath,
            String::ToString(resolvedPath)
        );
    }

    _mainTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    CHECK_NULL(_mainTexture, E_FAIL);

    return S_OK;
}

HRESULT ComputeTrailEmitter::Ready_NoiseTexture()
{
    if (_desc.material.noiseTextureGuid.empty() && _desc.material.noiseTexturePath.empty())
        return S_OK;

    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    const wstring resolvedPath = Internal::Resolve_TrailTexturePath(
        _desc.material.noiseTextureGuid,
        _desc.material.noiseTexturePath,
        usedPathFallback,
        guidPathMismatch
    );
    if (resolvedPath.empty())
    {
        _noiseTexture = nullptr;
        LOG_WARN(
            "ComputeTrailEmitter noise texture resolve failed. noiseTextureGuid='{}', noiseTexturePath='{}'. Noise modulation is disabled.",
            _desc.material.noiseTextureGuid,
            _desc.material.noiseTexturePath
        );
        return S_OK;
    }

    if (guidPathMismatch)
    {
        LOG_WARN(
            "ComputeTrailEmitter noise texture GUID/path mismatch. noiseTextureGuid='{}', noiseTexturePath='{}', guidResolvedPath='{}'.",
            _desc.material.noiseTextureGuid,
            _desc.material.noiseTexturePath,
            String::ToString(resolvedPath)
        );
    }
    else if (usedPathFallback)
    {
        LOG_WARN(
            "ComputeTrailEmitter noise texture fell back to path. noiseTextureGuid='{}', noiseTexturePath='{}', resolvedPath='{}'.",
            _desc.material.noiseTextureGuid,
            _desc.material.noiseTexturePath,
            String::ToString(resolvedPath)
        );
    }

    _noiseTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    if (_noiseTexture == nullptr)
    {
        LOG_WARN(
            "ComputeTrailEmitter noise texture create failed. noiseTextureGuid='{}', noiseTexturePath='{}', resolvedPath='{}'. Noise modulation is disabled.",
            _desc.material.noiseTextureGuid,
            _desc.material.noiseTexturePath,
            String::ToString(resolvedPath)
        );
    }

    return S_OK;
}

HRESULT ComputeTrailEmitter::Ready_MaskTexture()
{
    if (_desc.material.maskTextureGuid.empty() && _desc.material.maskTexturePath.empty())
        return S_OK;

    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    const wstring resolvedPath = Internal::Resolve_TrailTexturePath(
        _desc.material.maskTextureGuid,
        _desc.material.maskTexturePath,
        usedPathFallback,
        guidPathMismatch
    );

    if (resolvedPath.empty())
    {
        _maskTexture = nullptr;
        LOG_WARN(
            "ComputeTrailEmitter mask texture resolve failed. maskTextureGuid='{}', maskTexturePath='{}'.",
            _desc.material.maskTextureGuid,
            _desc.material.maskTexturePath
        );
        return S_OK;
    }

    if (guidPathMismatch)
    {
        LOG_WARN(
            "ComputeTrailEmitter mask texture GUID/path mismatch. maskTextureGuid='{}', maskTexturePath='{}', guidResolvedPath='{}'.",
            _desc.material.maskTextureGuid,
            _desc.material.maskTexturePath,
            String::ToString(resolvedPath)
        );
    }
    else if (usedPathFallback)
    {
        LOG_WARN(
            "ComputeTrailEmitter mask texture fell back to path. maskTextureGuid='{}', maskTexturePath='{}', resolvedPath='{}'.",
            _desc.material.maskTextureGuid,
            _desc.material.maskTexturePath,
            String::ToString(resolvedPath)
        );
    }

    _maskTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    if (nullptr == _maskTexture)
    {
        LOG_WARN(
            "ComputeTrailEmitter mask texture create failed. maskTextureGuid='{}', maskTexturePath='{}', resolvedPath='{}'.",
            _desc.material.maskTextureGuid,
            _desc.material.maskTexturePath,
            String::ToString(resolvedPath)
        );
    }

    return S_OK;
}

HRESULT ComputeTrailEmitter::Ready_FlowTexture()
{
    if (!Is_DistortionFamily())
        return S_OK;

    if (_desc.material.flowTextureGuid.empty() && _desc.material.flowTexturePath.empty())
        return S_OK;

    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    const wstring resolvedPath = Internal::Resolve_TrailTexturePath(
        _desc.material.flowTextureGuid,
        _desc.material.flowTexturePath,
        usedPathFallback,
        guidPathMismatch
    );

    if (resolvedPath.empty())
    {
        _flowTexture = nullptr;
        LOG_WARN(
            "ComputeTrailEmitter flow texture resolve failed. flowTextureGuid='{}', flowTexturePath='{}'.",
            _desc.material.flowTextureGuid,
            _desc.material.flowTexturePath
        );
        return S_OK;
    }

    if (guidPathMismatch)
    {
        LOG_WARN(
            "ComputeTrailEmitter flow texture GUID/path mismatch. flowTextureGuid='{}', flowTexturePath='{}', guidResolvedPath='{}'.",
            _desc.material.flowTextureGuid,
            _desc.material.flowTexturePath,
            String::ToString(resolvedPath)
        );
    }
    else if (usedPathFallback)
    {
        LOG_WARN(
            "ComputeTrailEmitter flow texture fell back to path. flowTextureGuid='{}', flowTexturePath='{}', resolvedPath='{}'.",
            _desc.material.flowTextureGuid,
            _desc.material.flowTexturePath,
            String::ToString(resolvedPath)
        );
    }

    _flowTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    if (nullptr == _flowTexture)
    {
        LOG_WARN(
            "ComputeTrailEmitter flow texture create failed. flowTextureGuid='{}', flowTexturePath='{}', resolvedPath='{}'.",
            _desc.material.flowTextureGuid,
            _desc.material.flowTexturePath,
            String::ToString(resolvedPath)
        );
    }

    return S_OK;
}

HRESULT ComputeTrailEmitter::Ready_DrawBuffers()
{
    const uint32 maxRenderSegmentCount = Compute_MaxRenderSegmentCount();
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
    instanceDesc.ByteWidth = sizeof(TrailInstanceVertex) * maxRenderSegmentCount;
    instanceDesc.Usage = D3D11_USAGE_DEFAULT;
    instanceDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    CHECK_FAILED(_device->CreateBuffer(&instanceDesc, nullptr, _instanceBuffer.GetAddressOf()), E_FAIL);

    return S_OK;
}

HRESULT ComputeTrailEmitter::Ready_ComputeBuffers()
{
    const uint32 maxSegmentCount = Compute_MaxRenderSegmentCount();
    _historyInput = ComputeStructuredBuffer::Create(
        _device,
        _context,
        sizeof(TrailSamplePayload),
        maxSegmentCount + 1u
    );
    CHECK_NULL(_historyInput, E_FAIL);

    _computeOutput = ComputeStructuredBuffer::Create(
        _device,
        _context,
        sizeof(TrailInstanceVertex),
        maxSegmentCount
    );
    CHECK_NULL(_computeOutput, E_FAIL);

    _computeArgsOutput = ComputeStructuredBuffer::Create(
        _device,
        _context,
        sizeof(DrawIndexedInstancedIndirectArgs),
        1
    );
    CHECK_NULL(_computeArgsOutput, E_FAIL);

    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(TrailComputeParams);
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    CHECK_FAILED(_device->CreateBuffer(&cbDesc, nullptr, _computeConstantBuffer.GetAddressOf()), E_FAIL);

    D3D11_BUFFER_DESC argsBufferDesc{};
    argsBufferDesc.ByteWidth = sizeof(DrawIndexedInstancedIndirectArgs);
    argsBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    argsBufferDesc.BindFlags = 0;
    argsBufferDesc.CPUAccessFlags = 0;
    argsBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
    CHECK_FAILED(_device->CreateBuffer(&argsBufferDesc, nullptr, _indirectArgsBuffer.GetAddressOf()), E_FAIL);

    CHECK_FAILED(Reset_IndirectArgs(), E_FAIL);
    CHECK_FAILED(Copy_ComputeOutput(), E_FAIL);

    return S_OK;
}

void ComputeTrailEmitter::Update_History(float timeDelta)
{
    if (timeDelta <= 0.f)
        return;

    for (TrailHistorySample& sample : _history)
        sample.age += timeDelta;

    erase_if(
        _history,
        [this](const TrailHistorySample& sample)
        {
            return sample.age >= _desc.segmentLifetime;
        }
    );

    EffectTrailSample trailSample{};
    const Shared<IEffectTrailSampleProvider> provider = _desc.sampleProvider.lock();
    bool sampleAccepted = false;
    if (provider != nullptr && provider->Try_GetTrailSample(trailSample))
    {
        sampleAccepted = true;
        Insert_TrailSamplesForFrame(trailSample);
    }

    if (!sampleAccepted)
    {
        LOG_WARN_THROTTLED(
            120,
            "ComputeTrailEmitter failed to acquire trail sample. providerBound={}, history={}, drawSegments={}. Enable Trail Preview and verify preview weapon visibility.",
            provider != nullptr,
            static_cast<uint32>(_history.size()),
            _drawSegmentCount
        );
    }

    while (_history.size() > _desc.historyCount)
    {
        _history.pop_back();
    }

    if (_history.size() < 2)
        _drawSegmentCount = 0;
}

uint32 ComputeTrailEmitter::Compute_MaxRenderSegmentCount() const
{
    const uint32 maxIntervalSubdivision = max(1u, _desc.curveSubdivision);
    return max(1u, (_desc.historyCount - 1u) * maxIntervalSubdivision);
}

void ComputeTrailEmitter::Insert_TrailSamplesForFrame(const EffectTrailSample& trailSample)
{
    if (!_desc.spawnPerUnit.enabled)
    {
        Insert_TrailSample(trailSample, 0.f);
        return;
    }

    if (_history.empty())
    {
        Insert_TrailSample(trailSample, 0.f);
        return;
    }

    const TrailHistorySample previousHead = _history.front();
    // 회전 등 base/tip이 반대로 움직이는 경우를 커버하기 위해 최대 거리 기준으로 판정한다.
    const float motionDistance = Internal::Compute_TrailMotionDistance(previousHead.sample, trailSample);
    if (motionDistance < _desc.spawnPerUnit.movementTolerance)
        return;

    // 텔레포트 감지는 tip jitter 오감지를 막기 위해 center 기반을 별도로 사용한다.
    const float centerDistance = Internal::Compute_TrailCenterDistance(previousHead.sample, trailSample);
    if (_desc.spawnPerUnit.maxFrameDistance > 0.f &&
        centerDistance > _desc.spawnPerUnit.maxFrameDistance)
    {
        _history.clear();
        Insert_TrailSample(trailSample, 0.f);
        return;
    }

    const float sampleDensity = _desc.spawnPerUnit.spawnPerUnit / max(_desc.spawnPerUnit.unitScalar, 1e-4f);
    if (sampleDensity <= 0.f)
    {
        Insert_TrailSample(trailSample, 0.f);
        return;
    }

    const uint32 stepCount = min(
        max(1u, static_cast<uint32>(ceilf(motionDistance * sampleDensity))),
        kMaxSpawnPerUnitFrameSamples
    );

    for (uint32 step = 1u; step <= stepCount; ++step)
    {
        const float ratio = static_cast<float>(step) / static_cast<float>(stepCount);
        const EffectTrailSample interpolatedSample = Internal::Lerp_TrailSample(previousHead.sample, trailSample, ratio);
        const float interpolatedAge = previousHead.age * (1.f - ratio);
        Insert_TrailSample(interpolatedSample, interpolatedAge);
    }
}

void ComputeTrailEmitter::Insert_TrailSample(const EffectTrailSample& trailSample, float age)
{
    _history.insert(_history.begin(), TrailHistorySample{ trailSample, max(0.f, age) });
}

void ComputeTrailEmitter::Build_RenderSamples(vector<TrailRenderSample>& outSamples) const
{
    outSamples.clear();
    if (_history.empty())
        return;

    outSamples.reserve(Compute_MaxRenderSegmentCount() + 1u);
    outSamples.push_back(TrailRenderSample{ Build_SmoothedHistorySample(0), _history.front().age, 0.f });

    const uint32 maxIntervalSubdivision = max(1u, _desc.curveSubdivision);
    const float sampleSpacing = max(1e-3f, _desc.sampleSpacing);
    for (uint32 index = 0; index + 1u < static_cast<uint32>(_history.size()); ++index)
    {
        const EffectTrailSample p0 = Build_SmoothedHistorySample(index > 0u ? index - 1u : index);
        const EffectTrailSample p1 = Build_SmoothedHistorySample(index);
        const EffectTrailSample p2 = Build_SmoothedHistorySample(index + 1u);
        const EffectTrailSample p3 = Build_SmoothedHistorySample(
            min(index + 2u, static_cast<uint32>(_history.size() - 1u))
        );

        const float centerDistance = Internal::Compute_TrailCenterDistance(p1, p2);
        const uint32 stepCount = min(
            max(1u, static_cast<uint32>(ceilf(centerDistance / sampleSpacing))),
            maxIntervalSubdivision
        );

        for (uint32 step = 1u; step <= stepCount; ++step)
        {
            const float ratio = static_cast<float>(step) / static_cast<float>(stepCount);
            TrailRenderSample renderSample{};
            renderSample.sample = Internal::Catmull_RomTrailSample(p0, p1, p2, p3, ratio);
            renderSample.age = _history[index].age + (_history[index + 1u].age - _history[index].age) * ratio;
            outSamples.push_back(renderSample);
        }
    }

    outSamples.front().distanceFromHead = 0.f;
    for (size_t index = 1; index < outSamples.size(); ++index)
    {
        const float segmentDistance = Internal::Compute_TrailCenterDistance(
            outSamples[index - 1].sample,
            outSamples[index].sample
        );
        outSamples[index].distanceFromHead = outSamples[index - 1].distanceFromHead + segmentDistance;
    }
}

EffectTrailSample ComputeTrailEmitter::Build_SmoothedHistorySample(uint32 sampleIndex) const
{
    if (!_desc.smoothTangent || sampleIndex == 0u || sampleIndex + 1u >= _history.size())
        return _history[sampleIndex].sample;

    const EffectTrailSample& newer = _history[sampleIndex - 1u].sample;
    const EffectTrailSample& current = _history[sampleIndex].sample;
    const EffectTrailSample& older = _history[sampleIndex + 1u].sample;

    EffectTrailSample smoothed{};
    smoothed.baseWorldPosition =
        newer.baseWorldPosition * kSmoothSampleSideWeight +
        current.baseWorldPosition * (1.f - kSmoothSampleSideWeight * 2.f) +
        older.baseWorldPosition * kSmoothSampleSideWeight;

    smoothed.tipWorldPosition =
        newer.tipWorldPosition * kSmoothSampleSideWeight +
        current.tipWorldPosition * (1.f - kSmoothSampleSideWeight * 2.f) +
        older.tipWorldPosition * kSmoothSampleSideWeight;

    return smoothed;
}

HRESULT ComputeTrailEmitter::Dispatch_Compute()
{
    CHECK_FAILED(Reset_IndirectArgs(), E_FAIL);

    if (_history.size() < 2)
    {
        _drawSegmentCount = 0;
        CHECK_FAILED(Copy_ComputeOutput(), E_FAIL);
        return S_OK;
    }

    CHECK_FAILED(Update_ComputeInput(), E_FAIL);
    if (_drawSegmentCount == 0)
    {
        CHECK_FAILED(Copy_ComputeOutput(), E_FAIL);
        return S_OK;
    }

    CHECK_FAILED(Update_ComputeConstants(), E_FAIL);
    CHECK_FAILED(_computeShader->Bind_SRV(0, _historyInput->Get_SRV()), E_FAIL);
    CHECK_FAILED(_computeShader->Bind_UAV(0, _computeOutput->Get_UAV()), E_FAIL);
    CHECK_FAILED(_computeShader->Bind_UAV(1, _computeArgsOutput->Get_UAV()), E_FAIL);
    CHECK_FAILED(_computeShader->Bind_ConstantBuffer(0, _computeConstantBuffer.Get()), E_FAIL);
    CHECK_FAILED(_computeShader->Dispatch((_drawSegmentCount + kThreadCountX - 1u) / kThreadCountX, 1, 1), E_FAIL);
    CHECK_FAILED(Copy_ComputeOutput(), E_FAIL);
    return S_OK;
}

HRESULT ComputeTrailEmitter::Update_ComputeInput()
{
    vector<TrailRenderSample> renderSamples{};
    Build_RenderSamples(renderSamples);
    if (renderSamples.size() < 2)
    {
        _drawSegmentCount = 0;
        _visibleTrailLength = 1.f;
        return E_FAIL;
    }

    _drawSegmentCount = static_cast<uint32>(renderSamples.size() - 1u);
    const float totalTrailLength = renderSamples.back().distanceFromHead;
    _visibleTrailLength = max(
        0.0001f,
        _desc.maxTrailLength > 0.f ? min(totalTrailLength, _desc.maxTrailLength) : totalTrailLength
    );

    vector<TrailSamplePayload> payloads{};
    payloads.reserve(renderSamples.size());

    for (const TrailRenderSample& renderSample : renderSamples)
    {
        TrailSamplePayload payload{};
        payload.baseWorldPosition = Vec4(
            renderSample.sample.baseWorldPosition.x,
            renderSample.sample.baseWorldPosition.y,
            renderSample.sample.baseWorldPosition.z,
            1.f
        );
        payload.tipWorldPosition = Vec4(
            renderSample.sample.tipWorldPosition.x,
            renderSample.sample.tipWorldPosition.y,
            renderSample.sample.tipWorldPosition.z,
            1.f
        );
        payload.sampleParams = Vec4(renderSample.age, renderSample.distanceFromHead, 0.f, 0.f);
        payloads.push_back(payload);
    }

    CHECK_FAILED(_historyInput->Update_Data(payloads.data(), static_cast<uint32>(payloads.size())), E_FAIL);
    return S_OK;
}

HRESULT ComputeTrailEmitter::Update_ComputeConstants()
{
    if (nullptr == _computeConstantBuffer)
        return E_FAIL;

    TrailComputeParams params{};
    params.segmentCount = _drawSegmentCount;
    params.segmentLifetime = _desc.segmentLifetime;
    params.width = _desc.width;
    params.uvTiling = _desc.uvTiling;
    params.sideFade = _desc.sideFade;
    params.startColor = Internal::Average_Color(_desc.initialColor.startColorMin, _desc.initialColor.startColorMax);
    params.endColor = Internal::Average_Color(_desc.colorOverLife.endColorMin, _desc.colorOverLife.endColorMax);
    const PointParticleColorOverLifeCurveDesc& colorCurve = _desc.colorOverLife.curve;
    params.colorOverLifeCurveParams = Vec4{
        static_cast<float>(max(1u, min(4u, colorCurve.colorCurveKeyCount))),
        static_cast<float>(max(1u, min(4u, colorCurve.alphaCurveKeyCount))),
        colorCurve.colorCurveEnabled ? 1.f : 0.f,
        colorCurve.alphaCurveEnabled ? 1.f : 0.f
    };
    params.colorOverLifeColorCurveTimes = colorCurve.colorCurveTimes;
    params.colorOverLifeColorCurveValuesR = colorCurve.colorCurveValuesR;
    params.colorOverLifeColorCurveValuesG = colorCurve.colorCurveValuesG;
    params.colorOverLifeColorCurveValuesB = colorCurve.colorCurveValuesB;
    params.colorOverLifeColorCurveArriveR = colorCurve.colorCurveArriveR;
    params.colorOverLifeColorCurveArriveG = colorCurve.colorCurveArriveG;
    params.colorOverLifeColorCurveArriveB = colorCurve.colorCurveArriveB;
    params.colorOverLifeColorCurveLeaveR = colorCurve.colorCurveLeaveR;
    params.colorOverLifeColorCurveLeaveG = colorCurve.colorCurveLeaveG;
    params.colorOverLifeColorCurveLeaveB = colorCurve.colorCurveLeaveB;
    params.colorOverLifeColorCurveModes = colorCurve.colorCurveModes;
    params.colorOverLifeAlphaCurveTimes = colorCurve.alphaCurveTimes;
    params.colorOverLifeAlphaCurveValues = colorCurve.alphaCurveValues;
    params.colorOverLifeAlphaCurveArrive = colorCurve.alphaCurveArrive;
    params.colorOverLifeAlphaCurveLeave = colorCurve.alphaCurveLeave;
    params.colorOverLifeAlphaCurveModes = colorCurve.alphaCurveModes;
    params.sizeByLifeParams = Vec4{
        _desc.sizeByLife.multiplyXStart,
        _desc.sizeByLife.multiplyXEnd,
        _desc.sizeByLife.enabled ? 1.f : 0.f,
        0.f
    };
    params.sizeByLifeCurveTimes = _desc.sizeByLife.curveKeyTimes;
    params.sizeByLifeCurveValuesX = _desc.sizeByLife.curveKeyValuesX;
    params.sizeByLifeCurveParams = Vec4{
        static_cast<float>(max(1u, min(4u, _desc.sizeByLife.curveKeyCount))),
        _desc.sizeByLife.multiplyX ? 1.f : 0.f,
        0.f,
        0.f
    };
    params.lengthParams = Vec4{ _desc.maxTrailLength, _desc.tailFadeLength, 0.f, 0.f };

    _context->UpdateSubresource(_computeConstantBuffer.Get(), 0, nullptr, &params, 0, 0);
    return S_OK;
}

HRESULT ComputeTrailEmitter::Reset_IndirectArgs()
{
    if (nullptr == _computeArgsOutput)
        return E_FAIL;

    DrawIndexedInstancedIndirectArgs args{};
    args.indexCountPerInstance = 1u;
    args.instanceCount = 0u;
    args.startIndexLocation = 0u;
    args.baseVertexLocation = 0;
    args.startInstanceLocation = 0u;

    return _computeArgsOutput->Update_Data(&args, 1);
}

HRESULT ComputeTrailEmitter::Copy_ComputeOutput()
{
    if (nullptr == _computeOutput || nullptr == _computeArgsOutput || nullptr == _instanceBuffer || nullptr == _indirectArgsBuffer)
        return E_FAIL;

    _context->CopyResource(_instanceBuffer.Get(), _computeOutput->Get_Buffer());
    _context->CopyResource(_indirectArgsBuffer.Get(), _computeArgsOutput->Get_Buffer());
    return S_OK;
}

HRESULT ComputeTrailEmitter::Bind_ShaderResources()
{
    CHECK_FAILED(GAME->Bind_CameraCB(_shader), E_FAIL);

    if (Is_DistortionFamily())
    {
        CHECK_FAILED(_shader->Bind_RawValue("g_Tint", &_desc.material.tint, sizeof(Vec4)), E_FAIL);
        CHECK_FAILED(Bind_DistortionResources(), E_FAIL);
        return S_OK;
    }

    CHECK_FAILED(_mainTexture->Bind_ShaderResourceView(_shader.get(), "g_MainTexture", 0), E_FAIL);

    if (nullptr != _noiseTexture)
    {
        CHECK_FAILED(_noiseTexture->Bind_ShaderResourceView(_shader.get(), "g_NoiseTexture", 0), E_FAIL);
    }
    else
    {
        CHECK_FAILED(_shader->Bind_SRV("g_NoiseTexture", nullptr), E_FAIL);
    }

    if (nullptr != _maskTexture)
    {
        CHECK_FAILED(_maskTexture->Bind_ShaderResourceView(_shader.get(), "g_MaskTexture", 0), E_FAIL);
    }
    else
    {
        CHECK_FAILED(_shader->Bind_SRV("g_MaskTexture", nullptr), E_FAIL);
    }

    CHECK_FAILED(_shader->Bind_RawValue("g_Tint", &_desc.material.tint, sizeof(Vec4)), E_FAIL);

    const Vec4 effectTrailParams = Vec4(
        _desc.material.intensity,
        max(_desc.material.opacityPower, 0.0001f),
        _desc.material.noiseStrength,
        nullptr != _noiseTexture ? 1.f : 0.f
    );
    const Vec4 effectTrailAlphaParams = Vec4(
        clamp(_desc.material.alphaCutoff, 0.f, 1.f),
        clamp(_desc.material.alphaErosion, 0.f, 1.f),
        nullptr != _maskTexture ? 1.f : 0.f,
        0.f
    );
    const Vec4 effectTrailMainUVParams = Vec4(
        _desc.material.mainUVScale.x,
        _desc.material.mainUVScale.y,
        _desc.material.mainUVScrollSpeed.x,
        _desc.material.mainUVScrollSpeed.y
    );
    const Vec4 effectTrailNoiseUVParams = Vec4(
        _desc.material.noiseUVScale.x,
        _desc.material.noiseUVScale.y,
        _desc.material.noiseUVScrollSpeed.x,
        _desc.material.noiseUVScrollSpeed.y
    );
    const Vec4 effectTrailMaskUVParams = Vec4(
        _desc.material.maskUVScale.x,
        _desc.material.maskUVScale.y,
        _desc.material.maskUVScrollSpeed.x,
        _desc.material.maskUVScrollSpeed.y
    );
    const Vec4 effectTrailUVOffsetParams = Vec4(
        _desc.material.mainUVOffset.x,
        _desc.material.mainUVOffset.y,
        _desc.material.noiseUVOffset.x,
        _desc.material.noiseUVOffset.y
    );
    const Vec4 effectTrailMaskUVOffsetParams = Vec4(
        _desc.material.maskUVOffset.x,
        _desc.material.maskUVOffset.y,
        0.f,
        0.f
    );
    const Vec4 effectTrailUVPolicyParams = Vec4(
        _visibleTrailLength,
        max(_desc.uvTiling, 0.0001f),
        0.f,
        0.f
    );
    const Vec4 effectTrailUVModeParams = Vec4(
        static_cast<float>(static_cast<uint32>(_desc.material.mainUVTilingMode)),
        static_cast<float>(static_cast<uint32>(_desc.material.noiseUVTilingMode)),
        static_cast<float>(static_cast<uint32>(_desc.material.maskUVTilingMode)),
        0.f
    );
    const int opacitySourceIndex = Resolve_OpacitySourceIndex();
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectTrailParams", &effectTrailParams, sizeof(effectTrailParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectTrailAlphaParams", &effectTrailAlphaParams, sizeof(effectTrailAlphaParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectTrailMainUVParams", &effectTrailMainUVParams, sizeof(effectTrailMainUVParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectTrailNoiseUVParams", &effectTrailNoiseUVParams, sizeof(effectTrailNoiseUVParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectTrailMaskUVParams", &effectTrailMaskUVParams, sizeof(effectTrailMaskUVParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectTrailUVOffsetParams", &effectTrailUVOffsetParams, sizeof(effectTrailUVOffsetParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectTrailMaskUVOffsetParams", &effectTrailMaskUVOffsetParams, sizeof(effectTrailMaskUVOffsetParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectTrailUVPolicyParams", &effectTrailUVPolicyParams, sizeof(effectTrailUVPolicyParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectTrailUVModeParams", &effectTrailUVModeParams, sizeof(effectTrailUVModeParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectTrailMaterialTime", &_materialElapsedTime, sizeof(_materialElapsedTime)), E_FAIL);
    const float emitterPhase = Client::Compute_MaterialScalarModulationPhase(_materialElapsedTime, _desc.playback.duration);
    CHECK_FAILED(
        Internal::Bind_MaterialScalarModulationPayload(_shader.get(), _desc.material.scalarModulation, emitterPhase),
        E_FAIL
    );
    CHECK_FAILED(_shader->Bind_RawValue("g_OpacitySource", &opacitySourceIndex, sizeof(opacitySourceIndex)), E_FAIL);

    const EffectMaterialAdditiveContributionData& additive = _desc.material.additive;
    const EffectMaterialCoreEmissiveData& coreEmissive = _desc.material.coreEmissive;
    const Vec4 effectTrailAdditiveParams = Vec4(
        static_cast<float>(static_cast<uint32>(additive.colorSource)),
        static_cast<float>(static_cast<uint32>(additive.amountSource)),
        static_cast<float>(static_cast<uint32>(additive.coveragePolicy)),
        additive.intensityScale
    );
    const Vec4 effectTrailAdditiveFlags = Vec4(additive.blackNeutral ? 1.f : 0.f, 0.f, 0.f, 0.f);
    const Vec4 effectTrailCoreEmissiveParams = Vec4(
        coreEmissive.enabled ? 1.f : 0.f,
        coreEmissive.corePower,
        coreEmissive.coreIntensity,
        coreEmissive.outerPower
    );
    const Vec4 effectTrailCoreEmissiveColor = Vec4(
        coreEmissive.coreColor.x,
        coreEmissive.coreColor.y,
        coreEmissive.coreColor.z,
        coreEmissive.outerIntensity
    );
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectTrailAdditiveParams", &effectTrailAdditiveParams, sizeof(effectTrailAdditiveParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectTrailAdditiveEmissiveColor", &additive.emissiveColor, sizeof(additive.emissiveColor)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectTrailAdditiveConstantColor", &additive.constantColor, sizeof(additive.constantColor)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectTrailAdditiveFlags", &effectTrailAdditiveFlags, sizeof(effectTrailAdditiveFlags)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectTrailCoreEmissiveParams", &effectTrailCoreEmissiveParams, sizeof(effectTrailCoreEmissiveParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectTrailCoreEmissiveColor", &effectTrailCoreEmissiveColor, sizeof(effectTrailCoreEmissiveColor)), E_FAIL);
    return S_OK;
}

HRESULT ComputeTrailEmitter::Bind_DistortionResources()
{
    const auto& material = _desc.material;

    if (nullptr != _flowTexture)
        CHECK_FAILED(_flowTexture->Bind_ShaderResourceView(_shader.get(), "g_FlowTexture", 0), E_FAIL);
    else
        CHECK_FAILED(_shader->Bind_SRV("g_FlowTexture", nullptr), E_FAIL);

    D3D11_VIEWPORT viewport{};
    uint32 viewportCount = 1u;
    _context->RSGetViewports(&viewportCount, &viewport);

    const Vec4 distortionParams(
        max(0.f, material.refractionIntensity),
        max(0.f, material.refractionPresence),
        nullptr != _flowTexture ? 1.f : 0.f,
        _materialElapsedTime
    );
    const Vec4 distortionScreenSize(max(1.f, viewport.Width), max(1.f, viewport.Height), 0.f, 0.f);
    const float flowUVTilingMode = static_cast<float>(static_cast<uint32>(material.flowUVTilingMode));

    CHECK_FAILED(_shader->Bind_RawValue("g_EffectDistortionParams", &distortionParams, sizeof(distortionParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_DistortionScreenSize", &distortionScreenSize, sizeof(distortionScreenSize)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_FlowUVScale", &material.flowUVScale, sizeof(material.flowUVScale)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_FlowUVOffset", &material.flowUVOffset, sizeof(material.flowUVOffset)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_FlowUVScrollSpeed", &material.flowUVScrollSpeed, sizeof(material.flowUVScrollSpeed)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_FlowUVTilingMode", &flowUVTilingMode, sizeof(flowUVTilingMode)), E_FAIL);

    return S_OK;
}

int ComputeTrailEmitter::Resolve_OpacitySourceIndex() const
{
    if (_desc.material.opacitySource == "Alpha" || _desc.material.opacitySource == "alpha")
        return kOpacitySourceAlpha;

    if (_desc.material.opacitySource == "Red" || _desc.material.opacitySource == "red")
        return kOpacitySourceRed;

    if (_desc.material.opacitySource == "Luminance" || _desc.material.opacitySource == "luminance")
        return kOpacitySourceLuminance;

    return kOpacitySourceAlpha;
}

uint32 ComputeTrailEmitter::Resolve_BlendPassIndex() const
{
    return _desc.material.blendMode == EffectMaterialBlendMode::Additive ? 1u : 0u;
}

bool ComputeTrailEmitter::Is_DistortionFamily() const
{
    return _desc.material.materialFamily == EffectMaterialFamily::SpriteDistortion;
}

RenderGroup ComputeTrailEmitter::Resolve_RenderGroup() const
{
    if (_desc.renderLayerOverride == EffectEmitterRenderLayerOverride::UIEffect)
        return RenderGroup::UIEffect;

    return Is_DistortionFamily() ? RenderGroup::Distortion : RenderGroup::Blend;
}

const char* ComputeTrailEmitter::Resolve_ShaderId() const
{
    return Is_DistortionFamily() ? kTrailDistortionShaderId : kTrailShaderId;
}

uint32 ComputeTrailEmitter::Resolve_ShaderPassIndex() const
{
    return Is_DistortionFamily() ? 0u : Resolve_BlendPassIndex();
}

Shared<ComputeTrailEmitter> ComputeTrailEmitter::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<ComputeTrailEmitter>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : ComputeTrailEmitter");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> ComputeTrailEmitter::Clone(void* arg)
{
    auto instance = make_shared<ComputeTrailEmitter>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : ComputeTrailEmitter");
        MSG_BOX("Failed to Clone : ComputeTrailEmitter");
        return nullptr;
    }

    return instance;
}

void ComputeTrailEmitter::Free()
{
    _history.clear();
    _drawSegmentCount = 0u;
    _materialElapsedTime = 0.f;
    _visibleTrailLength = 1.f;

    _shader.reset();
    _computeShader.reset();
    _mainTexture.reset();
    _noiseTexture.reset();
    _maskTexture.reset();

    _pointVB.Reset();
    _indexBuffer.Reset();
    _instanceBuffer.Reset();
    _computeConstantBuffer.Reset();
    _indirectArgsBuffer.Reset();

    _historyInput.reset();
    _computeOutput.reset();
    _computeArgsOutput.reset();

    _desc = {};

    __super::Free();
}
