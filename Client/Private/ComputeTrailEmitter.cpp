#include "ComputeTrailEmitter.h"

#include "ComputeShaderCom.h"
#include "ComputeStructuredBuffer.h"
#include "GameInstance.h"
#include "Texture.h"

namespace Client::Internal
{
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

Vec3 Compute_TrailCenter(const EffectTrailSample& sample)
{
    return (sample.baseWorldPosition + sample.tipWorldPosition) * 0.5f;
}

float Compute_TrailCenterDistance(const EffectTrailSample& from, const EffectTrailSample& to)
{
    return (Compute_TrailCenter(to) - Compute_TrailCenter(from)).Length();
}

wstring Resolve_TrailTexturePathByGuid(const string& textureGuid)
{
    if (GAME != nullptr && !textureGuid.empty())
    {
        const wstring resolvedPath = GAME->Resolve_AssetPath(textureGuid);
        if (!resolvedPath.empty() && fs::exists(resolvedPath))
            return resolvedPath;
    }

    return {};
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

wstring Resolve_TrailTexturePath(const string& textureGuid, const string& texturePath, bool& outUsedLegacyGuid)
{
    outUsedLegacyGuid = false;

    wstring resolvedPath = Resolve_TrailTexturePathByPath(texturePath);
    if (!resolvedPath.empty())
        return resolvedPath;

    resolvedPath = Resolve_TrailTexturePathByGuid(textureGuid);
    if (!resolvedPath.empty())
    {
        outUsedLegacyGuid = true;
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

    CHECK_FAILED(__super::Initialize(&_desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);

    return S_OK;
}

void ComputeTrailEmitter::Update(float timeDelta)
{
    Sync_FromEffectOwner();
    Update_History(timeDelta);
    CHECK_FAILED_THROTTLED(Dispatch_Compute(), 60);
}

void ComputeTrailEmitter::Late_Update(float)
{
    if (_drawSegmentCount > 0 && Is_Visible())
        GAME->Add_RenderGroup(RenderGroup::Blend, GetSharedPtr<GameObject>());
}

HRESULT ComputeTrailEmitter::Render()
{
    CHECK_FAILED_THROTTLED(Bind_ShaderResources(), 60, E_FAIL);
    CHECK_FAILED_THROTTLED(_shader->Begin(_desc.material.blendMode == "Additive" ? 1u : 0u), 60, E_FAIL);

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

HRESULT ComputeTrailEmitter::Ready_Components()
{
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), String::ToWString(kTrailShaderId), _shader), E_FAIL);
    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), String::ToWString(kTrailComputeShaderId), _computeShader), E_FAIL);
    CHECK_FAILED(Ready_Texture(), E_FAIL);
    CHECK_FAILED(Ready_DrawBuffers(), E_FAIL);
    CHECK_FAILED(Ready_ComputeBuffers(), E_FAIL);
    return S_OK;
}

HRESULT ComputeTrailEmitter::Ready_Texture()
{
    bool usedLegacyGuid = false;
    wstring resolvedPath = Internal::Resolve_TrailTexturePath(
        _desc.material.mainTextureGuid,
        _desc.material.mainTexturePath,
        usedLegacyGuid
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

    if (usedLegacyGuid)
    {
        LOG_WARN(
            "ComputeTrailEmitter main texture recovered by legacy GUID. resolvedPath='{}'.",
            String::ToString(resolvedPath)
        );
    }

    _texture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    CHECK_NULL(_texture, E_FAIL);

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
        Insert_TrailSample(trailSample);
        sampleAccepted = true;
    }

    if (!sampleAccepted)
    {
        LOG_WARN_THROTTLED(
            60,
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

void ComputeTrailEmitter::Insert_TrailSample(const EffectTrailSample& trailSample)
{
    _history.insert(_history.begin(), TrailHistorySample{ trailSample, 0.f });
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
        return E_FAIL;
    }

    _drawSegmentCount = static_cast<uint32>(renderSamples.size() - 1u);

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
    params.tint = _desc.material.tint;

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
    CHECK_FAILED(_texture->Bind_ShaderResourceView(_shader.get(), "g_Texture", 0), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_Tint", &_desc.material.tint, sizeof(Vec4)), E_FAIL);
    return S_OK;
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
    __super::Free();
}
