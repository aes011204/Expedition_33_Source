#include "ComputeSourceHistoryRibbonEmitter.h"

#include "ComputeShaderCom.h"
#include "ComputeStructuredBuffer.h"
#include "EffectInstance.h"
#include "EffectMaterialScalarModulationRuntime.h"
#include "GameInstance.h"
#include "Renderer_Define.h"
#include "ShaderCom.h"
#include "Texture.h"

namespace Client::EffectAssetRuntimeLoad
{
static int Resolve_MaterialSourceIndex(const string& source)
{
    if (source == "Red" || source == "red")
        return 1;

    if (source == "Luminance" || source == "luminance")
        return 2;

    return 0;
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

static Vec3 Catmull_Rom(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float ratio)
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

static float Read_Vec4Component(const Vec4& values, uint32 index)
{
    switch (index)
    {
    case 0:
        return values.x;
    case 1:
        return values.y;
    case 2:
        return values.z;
    case 3:
        return values.w;
    default:
        return values.x;
    }
}

static float Read_Vec4Component(const Vec4& valuesBlock0, const Vec4& valuesBlock1, uint32 index)
{
    if (index < 4u)
        return Read_Vec4Component(valuesBlock0, index);

    return Read_Vec4Component(valuesBlock1, index - 4u);
}

static Vec4 Average_Color(const Vec4& minColor, const Vec4& maxColor)
{
    return Vec4{
        (minColor.x + maxColor.x) * 0.5f,
        (minColor.y + maxColor.y) * 0.5f,
        (minColor.z + maxColor.z) * 0.5f,
        (minColor.w + maxColor.w) * 0.5f
    };
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

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(ComputeSourceHistoryRibbonEmitter)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "ComputeSourceHistoryRibbonEmitter";
    info.category = "Effect";

    return true;
}

ComputeSourceHistoryRibbonEmitter::ComputeSourceHistoryRibbonEmitter(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : EffectEmitter{ device, context }
{
}

ComputeSourceHistoryRibbonEmitter::ComputeSourceHistoryRibbonEmitter(const ComputeSourceHistoryRibbonEmitter& prototype)
    : EffectEmitter{ prototype }
{
}

ComputeSourceHistoryRibbonEmitter::~ComputeSourceHistoryRibbonEmitter()
{
    Free();
}

HRESULT ComputeSourceHistoryRibbonEmitter::Initialize_Prototype()
{
    return S_OK;
}

HRESULT ComputeSourceHistoryRibbonEmitter::Initialize(void* arg)
{
    if (nullptr != arg)
        _desc = *static_cast<ComputeRibbonEmitterDesc*>(arg);

    _desc.playback.duration = max(0.0001f, _desc.playback.duration);
    _desc.playback.delay = max(0.f, _desc.playback.delay);
    _desc.lifetime.lifeTime.x = max(0.0001f, _desc.lifetime.lifeTime.x);
    _desc.lifetime.lifeTime.y = max(_desc.lifetime.lifeTime.x, _desc.lifetime.lifeTime.y);
    _desc.maxSampleCount = max(2u, _desc.maxSampleCount);
    _desc.followerLaneCount = max(1u, _desc.followerLaneCount);
    if (_desc.sourceMode != EffectSourceHistoryRibbonSourceMode::ParticleEmitter)
        _desc.sourceEmitterId = 0u;
    _desc.sampleSpacing = max(0.001f, _desc.sampleSpacing);
    _desc.curveSubdivision = min(_desc.curveSubdivision, kMaxCurveSubdivision);
    _desc.sampleInterval = max(0.f, _desc.sampleInterval);
    _desc.sampleLifetime = max(0.0001f, _desc.sampleLifetime);
    _desc.maxLength = max(0.f, _desc.maxLength);
    _desc.tailFadeLength = max(0.f, _desc.tailFadeLength);
    _desc.tailCollapseSpeed = max(0.f, _desc.tailCollapseSpeed);
    _desc.laneSpawnFadeInDuration = max(0.f, _desc.laneSpawnFadeInDuration);
    _desc.baseWidth = max(0.001f, _desc.baseWidth);
    _desc.tilingDistance = max(0.f, _desc.tilingDistance);
    _desc.material.subUVRows = max(1u, _desc.material.subUVRows);
    _desc.material.subUVCols = max(1u, _desc.material.subUVCols);
    _desc.subUVFrameOverLife.frameCurveKeyCount =
        max(1u, min(::Engine::kEffectDistributionCurveMaxKeys, _desc.subUVFrameOverLife.frameCurveKeyCount));
    CHECK_FAILED(__super::Initialize(&_desc), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);
    Reset_PlaybackRuntime();

    return S_OK;
}

void ComputeSourceHistoryRibbonEmitter::Update(float timeDelta)
{
    Sync_FromEffectOwner();
    Advance_Playback(timeDelta);
    _materialElapsedTime += max(0.f, timeDelta);

    if (Is_UpdatingHistory())
        Update_History(timeDelta);

    if (_finishEmissionAfterHistoryUpdate)
    {
        _finishEmissionAfterHistoryUpdate = false;
        _playbackState = PlaybackState::Draining;
    }

    if (PlaybackState::Draining == _playbackState && !Has_RenderableHistory())
        _playbackState = PlaybackState::Completed;

    CHECK_FAILED_THROTTLED(Dispatch_Compute(), 60);
}

void ComputeSourceHistoryRibbonEmitter::Late_Update(float)
{
    if (Can_SubmitRender())
        GAME->Add_RenderGroup(Resolve_RenderGroup(), GetSharedPtr<GameObject>());
}

HRESULT ComputeSourceHistoryRibbonEmitter::Render()
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

HRESULT ComputeSourceHistoryRibbonEmitter::Reset_ForEffectReplay()
{
    Reset_PlaybackRuntime();
    _history.clear();
    _sampleIntervalAccumulator = 0.f;
    _selfRootCollapseLength = -1.f;
    _hasSelfRootPreviousSourcePosition = false;
    Clear_ParticleFollowerHistory();
    _retiredLoopStrokes.clear();
    _materialElapsedTime = 0.f;
    _visibleRibbonLength = 1.f;
    _drawSegmentCount = 0u;

    if (nullptr != _computeArgsOutput)
        CHECK_FAILED(Reset_IndirectArgs(), E_FAIL);
    if (nullptr != _computeOutput && nullptr != _computeArgsOutput)
        CHECK_FAILED(Copy_ComputeOutput(), E_FAIL);

    return S_OK;
}

bool ComputeSourceHistoryRibbonEmitter::Is_EffectFinished() const
{
    return PlaybackState::Completed == _playbackState;
}

void ComputeSourceHistoryRibbonEmitter::Bind_SourcePointSampleProvider(const Weak<IEffectSourcePointSampleProvider>& provider)
{
    _desc.sourcePointSampleProvider = provider;
}

bool ComputeSourceHistoryRibbonEmitter::Try_Get_BlendSortWorldPosition(Vec3& outWorldPosition) const
{
    if (nullptr == _transformCom)
        return false;

    outWorldPosition = _transformCom->Get_WorldPosition();
    return true;
}

EffectSortPolicy ComputeSourceHistoryRibbonEmitter::Get_BlendSortPolicy() const
{
    return _desc.sort.sortPolicy;
}

int32 ComputeSourceHistoryRibbonEmitter::Get_BlendSortLayer() const
{
    return _desc.sort.sortLayer;
}

float ComputeSourceHistoryRibbonEmitter::Get_BlendSortBias() const
{
    return _desc.sort.artistSortBias;
}

void ComputeSourceHistoryRibbonEmitter::Reset_PlaybackRuntime()
{
    _playbackState = PlaybackState::Delayed;
    _finishEmissionAfterHistoryUpdate = false;
    _loopElapsedTime = 0.f;
    _loopIndex = 0u;
    Resample_SampleLifetime();
    Resample_RandomSubUVFrameIndex();
}

void ComputeSourceHistoryRibbonEmitter::Advance_Playback(float timeDelta)
{
    _finishEmissionAfterHistoryUpdate = false;

    if (PlaybackState::Completed == _playbackState ||
        PlaybackState::Draining == _playbackState)
        return;

    const float safeDeltaTime = max(0.f, timeDelta);
    _loopElapsedTime += safeDeltaTime;

    const float delay = Resolve_CurrentLoopDelay();
    if (_loopElapsedTime < delay)
    {
        _playbackState = PlaybackState::Delayed;
        return;
    }

    const float activeElapsedTime = _loopElapsedTime - delay;
    if (activeElapsedTime < Resolve_Duration())
    {
        _playbackState = PlaybackState::Emitting;
        return;
    }

    if (Has_NextLoop())
    {
        Start_NextLoop();
        return;
    }

    _playbackState = PlaybackState::Emitting;
    _finishEmissionAfterHistoryUpdate = true;
}

bool ComputeSourceHistoryRibbonEmitter::Is_UpdatingHistory() const
{
    return PlaybackState::Emitting == _playbackState ||
           PlaybackState::Draining == _playbackState;
}

bool ComputeSourceHistoryRibbonEmitter::Is_EmittingSourceSamples() const
{
    return PlaybackState::Emitting == _playbackState;
}

void ComputeSourceHistoryRibbonEmitter::Update_History(float timeDelta)
{
    Update_RetiredLoopStrokes(timeDelta);

    if (_desc.sourceMode == EffectSourceHistoryRibbonSourceMode::ParticleEmitter)
    {
        Update_ParticleFollowerHistory(timeDelta);
        Refresh_VisibleRibbonLengthFromRetired();
        return;
    }

    Update_SelfRootHistory(timeDelta);
    Refresh_VisibleRibbonLengthFromRetired();
}

void ComputeSourceHistoryRibbonEmitter::Update_SelfRootHistory(float timeDelta)
{
    const float safeDeltaTime = max(0.f, timeDelta);
    for (HistorySample& sample : _history)
        sample.age += safeDeltaTime;

    if (Is_EmittingSourceSamples())
    {
        _sampleIntervalAccumulator += safeDeltaTime;

        Vec3 sourcePosition{};
        if (!Try_Resolve_SourcePosition(sourcePosition))
            return;

        _selfRootSourcePosition = sourcePosition;
        const float frameMovedDistance =
            _hasSelfRootPreviousSourcePosition
            ? (sourcePosition - _selfRootPreviousSourcePosition).Length()
            : _desc.sampleSpacing;
        const bool isIdle =
            _hasSelfRootPreviousSourcePosition && frameMovedDistance <= max(_desc.sampleSpacing * 0.1f, 0.0001f);
        const bool movedEnough =
            _history.empty() || (sourcePosition - _history.front().position).Length() >= _desc.sampleSpacing;
        const bool intervalReady =
            _desc.sampleInterval > 0.f && _sampleIntervalAccumulator >= _desc.sampleInterval;
        const bool shouldInsertSample = movedEnough || (!_desc.tailCollapseOnIdle && intervalReady);
        if (!_desc.tailCollapseOnIdle || _desc.tailCollapseSpeed <= 0.f)
            _selfRootCollapseLength = -1.f;

        if (shouldInsertSample)
        {
            Insert_HistorySample(_history, _sampleIntervalAccumulator, sourcePosition);
            _selfRootCollapseLength = -1.f;
        }
        else if (_desc.tailCollapseOnIdle && _desc.tailCollapseSpeed > 0.f)
        {
            vector<RenderSample> fullRenderSamples{};
            Build_RenderSamples(_history, fullRenderSamples, &_selfRootSourcePosition);
            const float currentLength =
                _selfRootCollapseLength >= 0.f
                ? _selfRootCollapseLength
                : fullRenderSamples.size() >= 2u ? fullRenderSamples.back().distanceFromHead : 0.f;
            if (isIdle)
            {
                const float collapseRatio = clamp(_desc.tailCollapseSpeed * safeDeltaTime, 0.f, 1.f);
                _selfRootCollapseLength = max(0.f, currentLength * (1.f - collapseRatio));
            }
            else
                _selfRootCollapseLength = -1.f;
        }

        _selfRootPreviousSourcePosition = sourcePosition;
        _hasSelfRootPreviousSourcePosition = true;
    }

    Prune_History(_history);

    vector<RenderSample> renderSamples{};
    Build_RenderSamples(renderSamples);
    _visibleRibbonLength = renderSamples.size() >= 2
                           ? max(0.0001f, renderSamples.back().distanceFromHead)
                           : 1.f;
}

void ComputeSourceHistoryRibbonEmitter::Update_ParticleFollowerHistory(float timeDelta)
{
    const float safeDeltaTime = max(0.f, timeDelta);
    for (HistoryLane& lane : _sourceLanes)
    {
        lane.activeThisFrame = false;
        lane.age += safeDeltaTime;
        lane.missingTime += safeDeltaTime;
        if (Is_EmittingSourceSamples())
            lane.sampleIntervalAccumulator += safeDeltaTime;
        for (HistorySample& sample : lane.history)
            sample.age += safeDeltaTime;
        Prune_History(lane.history);
    }

    if (Is_EmittingSourceSamples())
    {
        const Shared<Engine::EffectInstance> effectOwner = dynamic_pointer_cast<Engine::EffectInstance>(_effectOwner.lock());
        const Shared<EffectEmitter> sourceEmitter =
            effectOwner != nullptr ? effectOwner->Find_EmitterById(_desc.sourceEmitterId) : nullptr;

        vector<EffectFollowerSourcePoint> sourcePoints{};
        if (sourceEmitter != nullptr && _desc.followerLaneCount > 0u)
            sourceEmitter->Collect_FollowerSourcePoints(sourcePoints, _desc.followerLaneCount);

        for (const EffectFollowerSourcePoint& sourcePoint : sourcePoints)
        {
            HistoryLane* lane = Find_HistoryLane(sourcePoint.sourceIndex);
            if (lane == nullptr)
            {
                _sourceLanes.push_back(HistoryLane{ .sourceIndex = sourcePoint.sourceIndex });
                lane = &_sourceLanes.back();
            }

            lane->activeThisFrame = true;
            lane->missingTime = 0.f;
            const float frameMovedDistance =
                lane->hasPreviousSourcePosition
                ? (sourcePoint.position - lane->previousSourcePosition).Length()
                : _desc.sampleSpacing;
            const bool isIdle =
                lane->hasPreviousSourcePosition && frameMovedDistance <= max(_desc.sampleSpacing * 0.1f, 0.0001f);
            lane->currentSourcePosition = sourcePoint.position;
            const bool movedEnough =
                lane->history.empty() || (sourcePoint.position - lane->history.front().position).Length() >= _desc.sampleSpacing;
            const bool intervalReady =
                _desc.sampleInterval > 0.f && lane->sampleIntervalAccumulator >= _desc.sampleInterval;
            const bool shouldInsertSample = movedEnough || (!_desc.tailCollapseOnIdle && intervalReady);
            if (!_desc.tailCollapseOnIdle || _desc.tailCollapseSpeed <= 0.f)
                lane->collapseLength = -1.f;

            if (shouldInsertSample)
            {
                Insert_HistorySample(lane->history, lane->sampleIntervalAccumulator, sourcePoint.position);
                lane->collapseLength = -1.f;
            }
            else if (_desc.tailCollapseOnIdle && _desc.tailCollapseSpeed > 0.f)
            {
                vector<RenderSample> fullRenderSamples{};
                Build_RenderSamples(lane->history, fullRenderSamples, &lane->currentSourcePosition);
                const float currentLength =
                    lane->collapseLength >= 0.f
                    ? lane->collapseLength
                    : fullRenderSamples.size() >= 2u ? fullRenderSamples.back().distanceFromHead : 0.f;
                if (isIdle)
                {
                    const float collapseRatio = clamp(_desc.tailCollapseSpeed * safeDeltaTime, 0.f, 1.f);
                    lane->collapseLength = max(0.f, currentLength * (1.f - collapseRatio));
                }
                else
                    lane->collapseLength = -1.f;
            }

            lane->previousSourcePosition = sourcePoint.position;
            lane->hasPreviousSourcePosition = true;
            Prune_History(lane->history);
        }
    }

    erase_if(
        _sourceLanes,
        [this](const HistoryLane& lane)
        {
            return !lane.activeThisFrame &&
                   lane.missingTime >= kParticleFollowerLaneRetireGrace &&
                   !Has_RenderableHistory(lane.history, &lane.currentSourcePosition);
        }
    );

    _visibleRibbonLength = 1.f;
    vector<RenderSample> renderSamples{};
    for (const HistoryLane& lane : _sourceLanes)
    {
        const float* visibleLengthLimit = lane.collapseLength >= 0.f ? &lane.collapseLength : nullptr;
        Build_RenderSamples(lane.history, renderSamples, &lane.currentSourcePosition, visibleLengthLimit);
        if (renderSamples.size() >= 2)
            _visibleRibbonLength = max(_visibleRibbonLength, renderSamples.back().distanceFromHead);
    }
}

void ComputeSourceHistoryRibbonEmitter::Insert_HistorySample(
    vector<HistorySample>& history,
    float& sampleIntervalAccumulator,
    const Vec3& sourcePosition)
{
    if (!history.empty())
    {
        const HistorySample previousHead = history.front();
        const float movedDistance = (sourcePosition - previousHead.position).Length();
        if (movedDistance >= _desc.sampleSpacing)
        {
            const uint32 stepCount = min(
                max(1u, static_cast<uint32>(ceilf(movedDistance / _desc.sampleSpacing))),
                kMaxDistanceTessellationSubdivision
            );

            for (uint32 step = 1u; step <= stepCount; ++step)
            {
                const float ratio = static_cast<float>(step) / static_cast<float>(stepCount);
                HistorySample sample{};
                sample.position = Vec3::Lerp(previousHead.position, sourcePosition, ratio);
                sample.age = previousHead.age * (1.f - ratio);
                history.insert(history.begin(), sample);
            }
        }
        else
            history.insert(history.begin(), HistorySample{ .position = sourcePosition, .age = 0.f, .distance = 0.f });
    }
    else
        history.insert(history.begin(), HistorySample{ .position = sourcePosition, .age = 0.f, .distance = 0.f });

    sampleIntervalAccumulator = 0.f;

    while (static_cast<uint32>(history.size()) > _desc.maxSampleCount)
    {
        history.pop_back();
    }
}

void ComputeSourceHistoryRibbonEmitter::Prune_History(vector<HistorySample>& history)
{
    for (size_t index = 0; index < history.size(); ++index)
    {
        if (history[index].age < _desc.sampleLifetime)
            continue;

        const size_t eraseBegin = min(index + size_t{ 1 }, history.size());
        history.erase(history.begin() + eraseBegin, history.end());
        break;
    }

    while (static_cast<uint32>(history.size()) > _desc.maxSampleCount)
    {
        history.pop_back();
    }
}

void ComputeSourceHistoryRibbonEmitter::Build_RenderSamples(vector<RenderSample>& outSamples) const
{
    const float* visibleLengthLimit = _selfRootCollapseLength >= 0.f ? &_selfRootCollapseLength : nullptr;
    Build_RenderSamples(_history, outSamples, &_selfRootSourcePosition, visibleLengthLimit);
}

void ComputeSourceHistoryRibbonEmitter::Build_RenderSamples(
    const vector<HistorySample>& history,
    vector<RenderSample>& outSamples,
    const Vec3* currentSourcePosition,
    const float* visibleLengthLimit) const
{
    outSamples.clear();
    if (history.empty() && currentSourcePosition == nullptr)
        return;

    if (visibleLengthLimit != nullptr)
    {
        const float cutoffThreshold = min(
            max(_desc.baseWidth * 0.5f, _desc.sampleSpacing * 0.5f),
            max(_desc.sampleSpacing * 0.5f, _desc.maxLength > 0.f ? _desc.maxLength * 0.25f : _desc.baseWidth * 0.5f)
        );
        if (*visibleLengthLimit <= cutoffThreshold)
            return;
    }

    vector<HistorySample> sourceSamples{};
    sourceSamples.reserve(history.size() + (currentSourcePosition != nullptr ? 1u : 0u));
    if (currentSourcePosition != nullptr)
        sourceSamples.push_back(HistorySample{ .position = *currentSourcePosition });
    sourceSamples.insert(sourceSamples.end(), history.begin(), history.end());

    if (sourceSamples.empty())
        return;

    outSamples.reserve(Compute_MaxLaneRenderSegmentCount() + 1u);

    const auto resolve_source_sample =
        [this, &sourceSamples](size_t sampleIndex)
    {
        HistorySample sample = sourceSamples[sampleIndex];
        if (!_desc.smoothTangent || sampleIndex == 0u || sampleIndex + 1u >= sourceSamples.size())
            return sample;

        const Vec3& newer = sourceSamples[sampleIndex - 1u].position;
        const Vec3& current = sourceSamples[sampleIndex].position;
        const Vec3& older = sourceSamples[sampleIndex + 1u].position;
        sample.position =
            newer * kSmoothSampleSideWeight +
            current * (1.f - kSmoothSampleSideWeight * 2.f) +
            older * kSmoothSampleSideWeight;
        return sample;
    };

    outSamples.push_back(RenderSample{ .sample = resolve_source_sample(0u), .distanceFromHead = 0.f });
    float accumulatedDistance = 0.f;

    for (size_t sourceIndex = 0u; sourceIndex + 1u < sourceSamples.size(); ++sourceIndex)
    {
        const HistorySample& currentSample = sourceSamples[sourceIndex];
        const HistorySample& targetSample = sourceSamples[sourceIndex + 1u];
        const float segmentDistance = (targetSample.position - currentSample.position).Length();
        if (segmentDistance <= kMinRenderableRibbonLength)
            continue;

        float trimRatio = 1.f;
        if (currentSample.age >= _desc.sampleLifetime)
            break;

        if (targetSample.age >= _desc.sampleLifetime)
        {
            const float ageRange = targetSample.age - currentSample.age;
            if (ageRange > 0.0001f)
                trimRatio = min(trimRatio, clamp((_desc.sampleLifetime - currentSample.age) / ageRange, 0.f, 1.f));
        }

        if (_desc.maxLength > 0.f)
        {
            if (accumulatedDistance >= _desc.maxLength)
                break;

            const float nextDistance = accumulatedDistance + segmentDistance;
            if (nextDistance >= _desc.maxLength)
                trimRatio = min(trimRatio, clamp((_desc.maxLength - accumulatedDistance) / segmentDistance, 0.f, 1.f));
        }

        if (visibleLengthLimit != nullptr)
        {
            if (accumulatedDistance >= *visibleLengthLimit)
                break;

            const float nextDistance = accumulatedDistance + segmentDistance;
            if (nextDistance >= *visibleLengthLimit)
                trimRatio = min(trimRatio, clamp((*visibleLengthLimit - accumulatedDistance) / segmentDistance, 0.f, 1.f));
        }

        if (trimRatio <= 0.f)
            break;

        const uint32 stepCount = min(
            max(1u, static_cast<uint32>(ceilf(segmentDistance / _desc.sampleSpacing))),
            max(1u, _desc.curveSubdivision)
        );
        bool trimmed = false;
        const HistorySample p0 = resolve_source_sample(sourceIndex > 0u ? sourceIndex - 1u : sourceIndex);
        const HistorySample p1 = resolve_source_sample(sourceIndex);
        const HistorySample p2 = resolve_source_sample(sourceIndex + 1u);
        const HistorySample p3 = resolve_source_sample(min(sourceIndex + 2u, sourceSamples.size() - 1u));

        for (uint32 step = 1u; step <= stepCount; ++step)
        {
            const float ratio = static_cast<float>(step) / static_cast<float>(stepCount);
            const float sampleRatio = min(ratio, trimRatio);
            HistorySample sample{};
            sample.position = EffectAssetRuntimeLoad::Catmull_Rom(p0.position, p1.position, p2.position, p3.position, sampleRatio);
            sample.age = currentSample.age + (targetSample.age - currentSample.age) * sampleRatio;
            outSamples.push_back(RenderSample{ .sample = sample });

            if (trimRatio < 1.f && ratio >= trimRatio)
            {
                trimmed = true;
                break;
            }
        }

        if (trimmed)
            break;
        accumulatedDistance += segmentDistance;
    }

    outSamples.front().distanceFromHead = 0.f;
    for (size_t index = 1; index < outSamples.size(); ++index)
    {
        const float distance = (outSamples[index].sample.position - outSamples[index - 1].sample.position).Length();
        outSamples[index].distanceFromHead = outSamples[index - 1].distanceFromHead + distance;
        outSamples[index].sample.distance = outSamples[index].distanceFromHead;
    }

    const float minRenderableLength =
        visibleLengthLimit != nullptr
        ? min(
            max(_desc.baseWidth * 0.5f, _desc.sampleSpacing * 0.5f),
            max(_desc.sampleSpacing * 0.5f, _desc.maxLength > 0.f ? _desc.maxLength * 0.25f : _desc.baseWidth * 0.5f)
        )
        : kMinRenderableRibbonLength;
    if (outSamples.size() < 2u ||
        outSamples.back().distanceFromHead <= minRenderableLength)
        outSamples.clear();
}

bool ComputeSourceHistoryRibbonEmitter::Has_RenderableHistory() const
{
    if (_desc.sourceMode != EffectSourceHistoryRibbonSourceMode::ParticleEmitter)
    {
        if (Has_RenderableHistory(_history, &_selfRootSourcePosition))
            return true;
    }
    else
    {
        const bool hasActiveLane = ranges::any_of(
            _sourceLanes,
            [this](const HistoryLane& lane)
            {
                return Has_RenderableHistory(lane.history, &lane.currentSourcePosition);
            }
        );
        if (hasActiveLane)
            return true;
    }

    return ranges::any_of(
        _retiredLoopStrokes,
        [this](const RetiredLoopStroke& stroke)
        {
            return Has_RenderableHistory(stroke.history, nullptr);
        }
    );
}

bool ComputeSourceHistoryRibbonEmitter::Has_RenderableHistory(const vector<HistorySample>& history, const Vec3* currentSourcePosition) const
{
    vector<RenderSample> renderSamples{};
    Build_RenderSamples(history, renderSamples, currentSourcePosition);
    return renderSamples.size() >= 2u;
}

ComputeSourceHistoryRibbonEmitter::HistoryLane* ComputeSourceHistoryRibbonEmitter::Find_HistoryLane(uint32 sourceIndex)
{
    for (HistoryLane& lane : _sourceLanes)
    {
        if (lane.sourceIndex == sourceIndex)
            return &lane;
    }

    return nullptr;
}

void ComputeSourceHistoryRibbonEmitter::Clear_ParticleFollowerHistory()
{
    _sourceLanes.clear();
}

void ComputeSourceHistoryRibbonEmitter::Retire_ActiveLoopStrokes()
{
    if (_desc.sourceMode == EffectSourceHistoryRibbonSourceMode::ParticleEmitter)
    {
        for (const HistoryLane& lane : _sourceLanes)
        {
            const float fadeDuration = max(_desc.laneSpawnFadeInDuration, 0.0001f);
            const float laneWidthMultiplier =
                _desc.laneSpawnFadeInEnabled && _desc.laneSpawnFadeInDuration > 0.f
                ? clamp(lane.age / fadeDuration, 0.f, 1.f)
                : 1.f;
            Retire_HistoryStroke(lane.history, &lane.currentSourcePosition, laneWidthMultiplier);
        }
        return;
    }

    if (_hasSelfRootPreviousSourcePosition || !_history.empty())
        Retire_HistoryStroke(_history, &_selfRootSourcePosition, 1.f);
}

void ComputeSourceHistoryRibbonEmitter::Retire_HistoryStroke(
    vector<HistorySample> history,
    const Vec3* currentSourcePosition,
    float widthMultiplier)
{
    if (currentSourcePosition != nullptr)
    {
        if (history.empty() ||
            (history.front().position - *currentSourcePosition).Length() > kMinRenderableRibbonLength)
        {
            history.insert(history.begin(), HistorySample{ .position = *currentSourcePosition, .age = 0.f });
        }
        else
        {
            history.front().position = *currentSourcePosition;
            history.front().age = 0.f;
        }
    }

    if (!Has_RenderableHistory(history, nullptr))
        return;

    _retiredLoopStrokes.push_back(
        RetiredLoopStroke{
            .history = std::move(history),
            .widthMultiplier = clamp(widthMultiplier, 0.f, 1.f)
        }
    );

    while (_retiredLoopStrokes.size() > kMaxRetiredLoopStrokeCount)
        _retiredLoopStrokes.erase(_retiredLoopStrokes.begin());
}

void ComputeSourceHistoryRibbonEmitter::Update_RetiredLoopStrokes(float timeDelta)
{
    const float safeDeltaTime = max(0.f, timeDelta);
    for (RetiredLoopStroke& stroke : _retiredLoopStrokes)
    {
        for (HistorySample& sample : stroke.history)
            sample.age += safeDeltaTime;
        Prune_History(stroke.history);
    }

    erase_if(
        _retiredLoopStrokes,
        [this](const RetiredLoopStroke& stroke)
        {
            return !Has_RenderableHistory(stroke.history, nullptr);
        }
    );
}

void ComputeSourceHistoryRibbonEmitter::Refresh_VisibleRibbonLengthFromRetired()
{
    vector<RenderSample> renderSamples{};
    for (const RetiredLoopStroke& stroke : _retiredLoopStrokes)
    {
        Build_RenderSamples(stroke.history, renderSamples, nullptr);
        if (renderSamples.size() >= 2u)
            _visibleRibbonLength = max(_visibleRibbonLength, renderSamples.back().distanceFromHead);
    }
}

HRESULT ComputeSourceHistoryRibbonEmitter::Ready_Components()
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

HRESULT ComputeSourceHistoryRibbonEmitter::Ready_Texture()
{
    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    wstring resolvedPath = EffectAssetRuntimeLoad::Resolve_RibbonTexturePath(
        _desc.material.mainTextureGuid,
        _desc.material.mainTexturePath,
        usedPathFallback,
        guidPathMismatch
    );
    if (resolvedPath.empty())
        resolvedPath = EffectAssetRuntimeLoad::Resolve_RibbonTexturePathByPath(kFallbackTexturePath);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), L"Texture_Effect_DefaultTexture", _mainTexture), E_FAIL);
    if (!resolvedPath.empty())
        _mainTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);

    CHECK_NULL(_mainTexture, E_FAIL);
    return S_OK;
}

HRESULT ComputeSourceHistoryRibbonEmitter::Ready_NoiseTexture()
{
    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    const wstring resolvedPath = EffectAssetRuntimeLoad::Resolve_RibbonTexturePath(
        _desc.material.noiseTextureGuid,
        _desc.material.noiseTexturePath,
        usedPathFallback,
        guidPathMismatch
    );
    if (!resolvedPath.empty())
        _noiseTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    return S_OK;
}

HRESULT ComputeSourceHistoryRibbonEmitter::Ready_MaskTexture()
{
    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    const wstring resolvedPath = EffectAssetRuntimeLoad::Resolve_RibbonTexturePath(
        _desc.material.maskTextureGuid,
        _desc.material.maskTexturePath,
        usedPathFallback,
        guidPathMismatch
    );
    if (!resolvedPath.empty())
        _maskTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    return S_OK;
}

HRESULT ComputeSourceHistoryRibbonEmitter::Ready_FlowTexture()
{
    bool usedPathFallback = false;
    bool guidPathMismatch = false;
    wstring resolvedPath = EffectAssetRuntimeLoad::Resolve_RibbonTexturePath(
        _desc.material.flowTextureGuid,
        _desc.material.flowTexturePath,
        usedPathFallback,
        guidPathMismatch
    );
    if (resolvedPath.empty())
        resolvedPath = EffectAssetRuntimeLoad::Resolve_RibbonTexturePathByPath(kNeutralFlowTexturePath);
    if (!resolvedPath.empty())
        _flowTexture = Texture::Create(_device, _context, resolvedPath.c_str(), 1);
    return S_OK;
}

HRESULT ComputeSourceHistoryRibbonEmitter::Ready_DrawBuffers()
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

HRESULT ComputeSourceHistoryRibbonEmitter::Ready_ComputeBuffers()
{
    const uint32 maxSegmentCount = Compute_MaxRenderSegmentCount();
    _sampleInput = ComputeStructuredBuffer::Create(_device, _context, sizeof(SegmentPayload), maxSegmentCount);
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

HRESULT ComputeSourceHistoryRibbonEmitter::Dispatch_Compute()
{
    CHECK_FAILED(Reset_IndirectArgs(), E_FAIL);

    if (!Has_RenderableHistory())
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

HRESULT ComputeSourceHistoryRibbonEmitter::Update_ComputeInput()
{
    vector<SegmentPayload> payloads{};
    payloads.reserve(Compute_MaxRenderSegmentCount());

    if (_desc.sourceMode == EffectSourceHistoryRibbonSourceMode::ParticleEmitter)
    {
        vector<RenderSample> laneRenderSamples{};
        const uint32 maxSegmentCount = Compute_MaxRenderSegmentCount();
        for (const HistoryLane& lane : _sourceLanes)
        {
            const float* visibleLengthLimit = lane.collapseLength >= 0.f ? &lane.collapseLength : nullptr;
            Build_RenderSamples(lane.history, laneRenderSamples, &lane.currentSourcePosition, visibleLengthLimit);

            const float fadeDuration = max(_desc.laneSpawnFadeInDuration, 0.0001f);
            const float laneWidthMultiplier =
                _desc.laneSpawnFadeInEnabled && _desc.laneSpawnFadeInDuration > 0.f
                ? clamp(lane.age / fadeDuration, 0.f, 1.f)
                : 1.f;
            CHECK_FAILED(Append_SegmentPayloads(laneRenderSamples, payloads, laneWidthMultiplier), E_FAIL);

            if (payloads.size() >= maxSegmentCount)
                break;
        }
    }
    else
    {
        vector<RenderSample> renderSamples{};
        Build_RenderSamples(renderSamples);
        CHECK_FAILED(Append_SegmentPayloads(renderSamples, payloads), E_FAIL);
    }

    vector<RenderSample> retiredRenderSamples{};
    for (const RetiredLoopStroke& stroke : _retiredLoopStrokes)
    {
        Build_RenderSamples(stroke.history, retiredRenderSamples, nullptr);
        CHECK_FAILED(Append_SegmentPayloads(retiredRenderSamples, payloads, stroke.widthMultiplier), E_FAIL);
    }

    if (payloads.empty())
    {
        _drawSegmentCount = 0u;
        _visibleRibbonLength = 1.f;
        return S_OK;
    }

    _drawSegmentCount = static_cast<uint32>(payloads.size());
    CHECK_FAILED(_sampleInput->Update_Data(payloads.data(), _drawSegmentCount), E_FAIL);
    return S_OK;
}

HRESULT ComputeSourceHistoryRibbonEmitter::Append_SegmentPayloads(
    const vector<RenderSample>& renderSamples,
    vector<SegmentPayload>& outPayloads,
    float widthMultiplier) const
{
    if (renderSamples.size() < 2)
        return S_OK;

    const Vec4 subUVRect = Resolve_SubUVRect(Evaluate_SubUVFrameIndex());
    const uint32 maxSegmentCount = Compute_MaxRenderSegmentCount();
    if (outPayloads.size() >= maxSegmentCount)
        return S_OK;

    const uint32 segmentCount = min(
        static_cast<uint32>(renderSamples.size() - 1u),
        maxSegmentCount - static_cast<uint32>(outPayloads.size())
    );

    for (uint32 index = 0; index < segmentCount; ++index)
    {
        const RenderSample& previous = index > 0u ? renderSamples[index - 1u] : renderSamples[index];
        const RenderSample& current = renderSamples[index];
        const RenderSample& next = renderSamples[index + 1u];
        const RenderSample& nextNext = index + 2u < renderSamples.size() ? renderSamples[index + 2u] : next;

        const float currentLifeProgress = clamp(current.sample.age / max(_desc.sampleLifetime, 0.0001f), 0.f, 1.f);
        const float nextLifeProgress = clamp(next.sample.age / max(_desc.sampleLifetime, 0.0001f), 0.f, 1.f);
        const auto resolveSampleAlphaScale =
            [this](const RenderSample& sample, float lifeProgress)
        {
            float alphaScale = _desc.autoLifeFade ? 1.f - lifeProgress : 1.f;
            if (_desc.maxLength > 0.f && _desc.tailFadeLength > 0.f)
            {
                const float fadeLength = max(_desc.tailFadeLength, 0.001f);
                const float fadeStart = max(0.f, _desc.maxLength - fadeLength);
                alphaScale *= clamp((_desc.maxLength - sample.distanceFromHead) / max(_desc.maxLength - fadeStart, 0.001f), 0.f, 1.f);
            }

            return clamp(alphaScale, 0.f, 1.f);
        };
        const float safeWidthMultiplier = clamp(widthMultiplier, 0.f, 1.f);
        const float currentAlphaScale = resolveSampleAlphaScale(current, currentLifeProgress);
        const float nextAlphaScale = resolveSampleAlphaScale(next, nextLifeProgress);
        const float currentBaseWidth = max(0.001f, _desc.baseWidth * Evaluate_WidthScaleByLife(currentLifeProgress));
        const float nextBaseWidth = max(0.001f, _desc.baseWidth * Evaluate_WidthScaleByLife(nextLifeProgress));
        const float currentWidth = currentBaseWidth * safeWidthMultiplier * currentAlphaScale;
        const float nextWidth = nextBaseWidth * safeWidthMultiplier * nextAlphaScale;
        Vec4 currentColor = Evaluate_ColorOverLife(currentLifeProgress);
        Vec4 nextColor = Evaluate_ColorOverLife(nextLifeProgress);
        currentColor.w *= currentAlphaScale;
        nextColor.w *= nextAlphaScale;

        SegmentPayload payload{};
        payload.previousPosition = Vec4(previous.sample.position.x, previous.sample.position.y, previous.sample.position.z, index > 0u ? 1.f : 0.f);
        payload.currentPosition = Vec4(current.sample.position.x, current.sample.position.y, current.sample.position.z, 1.f);
        payload.nextPosition = Vec4(next.sample.position.x, next.sample.position.y, next.sample.position.z, 1.f);
        payload.nextNextPosition = Vec4(
            nextNext.sample.position.x,
            nextNext.sample.position.y,
            nextNext.sample.position.z,
            index + 2u < renderSamples.size() ? 1.f : 0.f
        );
        payload.currentSampleParams = Vec4(current.sample.age, _desc.sampleLifetime, current.distanceFromHead, currentWidth);
        payload.nextSampleParams = Vec4(next.sample.age, _desc.sampleLifetime, next.distanceFromHead, nextWidth);
        payload.startColor = currentColor;
        payload.endColor = nextColor;
        payload.subUVRect = subUVRect;
        outPayloads.push_back(payload);
    }

    return S_OK;
}

HRESULT ComputeSourceHistoryRibbonEmitter::Update_ComputeConstants()
{
    RibbonComputeParams params{};
    params.segmentCount = _drawSegmentCount;
    params.renderAxis = static_cast<uint32>(EffectRibbonRenderAxis::CameraUp);
    params.tilingDistance = _desc.tilingDistance;
    params.visibleLength = _visibleRibbonLength;
    params.axisFallback = Vec4(1.f, 0.f, 0.f, 0.f);
    params.fadeParams = Vec4(_desc.autoLifeFade ? 1.f : 0.f, _desc.maxLength, _desc.tailFadeLength, 0.f);
    _context->UpdateSubresource(_computeConstantBuffer.Get(), 0, nullptr, &params, 0, 0);
    return S_OK;
}

HRESULT ComputeSourceHistoryRibbonEmitter::Reset_IndirectArgs()
{
    const DrawIndexedInstancedIndirectArgs args{};
    CHECK_FAILED(_computeArgsOutput->Update_Data(&args, 1), E_FAIL);
    return Copy_ComputeOutput();
}

HRESULT ComputeSourceHistoryRibbonEmitter::Copy_ComputeOutput()
{
    if (nullptr != _instanceBuffer && nullptr != _computeOutput)
        _context->CopyResource(_instanceBuffer.Get(), _computeOutput->Get_Buffer());
    if (nullptr != _indirectArgsBuffer && nullptr != _computeArgsOutput)
        _context->CopyResource(_indirectArgsBuffer.Get(), _computeArgsOutput->Get_Buffer());
    return S_OK;
}

HRESULT ComputeSourceHistoryRibbonEmitter::Bind_ShaderResources()
{
    CHECK_FAILED(GAME->Bind_CameraCB(_shader), E_FAIL);

    if (Is_DistortionFamily())
    {
        const Vec4 effectRibbonAxisParams = Vec4(static_cast<float>(static_cast<uint32>(EffectRibbonRenderAxis::CameraUp)), _visibleRibbonLength, 0.f, 1.f);
        const Vec4 effectRibbonFallbackAxis{ 1.f, 0.f, 0.f, 0.f };

        CHECK_FAILED(_shader->Bind_RawValue("g_Tint", &_desc.material.tint, sizeof(_desc.material.tint)), E_FAIL);
        CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonAxisParams", &effectRibbonAxisParams, sizeof(effectRibbonAxisParams)), E_FAIL);
        CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonFallbackAxis", &effectRibbonFallbackAxis, sizeof(effectRibbonFallbackAxis)), E_FAIL);
        CHECK_FAILED(Bind_DistortionResources(), E_FAIL);
        return S_OK;
    }

    CHECK_FAILED(Bind_MaterialResources(), E_FAIL);
    return S_OK;
}

HRESULT ComputeSourceHistoryRibbonEmitter::Bind_MaterialResources()
{
    const float emitterPhase = Compute_MaterialScalarModulationPhase(_materialElapsedTime, _desc.playback.duration);
    const EffectRequiredMaterialRuntimeDesc material =
        EffectAssetRuntimeLoad::Resolve_EmitterTimeCoreColorRgbModulation(_desc.material, emitterPhase);

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

    const Vec4 effectRibbonParams = Vec4(material.intensity, max(material.opacityPower, 0.0001f), material.noiseStrength, nullptr != _noiseTexture ? 1.f : 0.f);
    const Vec4 effectRibbonAlphaParams = Vec4(
        clamp(material.alphaCutoff, 0.f, 1.f),
        clamp(material.alphaErosion, 0.f, 1.f),
        nullptr != _maskTexture ? 1.f : 0.f,
        0.f
    );
    const Vec4 effectRibbonMainUVParams = Vec4(material.mainUVScale.x, material.mainUVScale.y, material.mainUVScrollSpeed.x, material.mainUVScrollSpeed.y);
    const Vec4 effectRibbonNoiseUVParams = Vec4(material.noiseUVScale.x, material.noiseUVScale.y, material.noiseUVScrollSpeed.x, material.noiseUVScrollSpeed.y);
    const Vec4 effectRibbonMaskUVParams = Vec4(material.maskUVScale.x, material.maskUVScale.y, material.maskUVScrollSpeed.x, material.maskUVScrollSpeed.y);
    const Vec4 effectRibbonUVOffsetParams = Vec4(material.mainUVOffset.x, material.mainUVOffset.y, material.noiseUVOffset.x, material.noiseUVOffset.y);
    const Vec4 effectRibbonMaskUVOffsetParams = Vec4(material.maskUVOffset.x, material.maskUVOffset.y, 0.f, 0.f);
    const Vec4 effectRibbonUVModeParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.mainUVTilingMode)),
        static_cast<float>(static_cast<uint32>(material.noiseUVTilingMode)),
        static_cast<float>(static_cast<uint32>(material.maskUVTilingMode)),
        0.f
    );
    const Vec4 effectRibbonUVAxisPolicyParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.mainUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(material.mainUVPolicy.vPolicy)),
        static_cast<float>(static_cast<uint32>(material.noiseUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(material.noiseUVPolicy.vPolicy))
    );
    const Vec4 effectRibbonMaskUVAxisPolicyParams = Vec4(
        static_cast<float>(static_cast<uint32>(material.maskUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(material.maskUVPolicy.vPolicy)),
        0.f,
        0.f
    );
    const Vec4 effectRibbonSourceParams = Vec4(
        static_cast<float>(EffectAssetRuntimeLoad::Resolve_MaterialSourceIndex(material.noiseSource)),
        static_cast<float>(EffectAssetRuntimeLoad::Resolve_MaterialSourceIndex(material.maskSource)),
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
    const Vec4 effectRibbonAxisParams = Vec4(static_cast<float>(EffectRibbonRenderAxis::CameraUp), _visibleRibbonLength, 0.f, 1.f);
    const Vec4 effectRibbonFallbackAxis{ 1.f, 0.f, 0.f, 0.f };
    const int opacitySourceIndex = Resolve_OpacitySourceIndex();

    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonParams", &effectRibbonParams, sizeof(effectRibbonParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonAlphaParams", &effectRibbonAlphaParams, sizeof(effectRibbonAlphaParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonMainUVParams", &effectRibbonMainUVParams, sizeof(effectRibbonMainUVParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonNoiseUVParams", &effectRibbonNoiseUVParams, sizeof(effectRibbonNoiseUVParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonMaskUVParams", &effectRibbonMaskUVParams, sizeof(effectRibbonMaskUVParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonUVOffsetParams", &effectRibbonUVOffsetParams, sizeof(effectRibbonUVOffsetParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonMaskUVOffsetParams", &effectRibbonMaskUVOffsetParams, sizeof(effectRibbonMaskUVOffsetParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonUVModeParams", &effectRibbonUVModeParams, sizeof(effectRibbonUVModeParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonUVAxisPolicyParams", &effectRibbonUVAxisPolicyParams, sizeof(effectRibbonUVAxisPolicyParams)), E_FAIL);
    CHECK_FAILED(
        _shader->Bind_RawValue(
            "g_EffectRibbonMaskUVAxisPolicyParams",
            &effectRibbonMaskUVAxisPolicyParams,
            sizeof(effectRibbonMaskUVAxisPolicyParams)
        ),
        E_FAIL
    );
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
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonCoreEmissiveParams", &effectRibbonCoreEmissiveParams, sizeof(effectRibbonCoreEmissiveParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonCoreEmissiveColor", &effectRibbonCoreEmissiveColor, sizeof(effectRibbonCoreEmissiveColor)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonAxisParams", &effectRibbonAxisParams, sizeof(effectRibbonAxisParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonFallbackAxis", &effectRibbonFallbackAxis, sizeof(effectRibbonFallbackAxis)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_EffectRibbonMaterialTime", &_materialElapsedTime, sizeof(_materialElapsedTime)), E_FAIL);
    CHECK_FAILED(EffectAssetRuntimeLoad::Bind_MaterialScalarModulationPayload(_shader.get(), _desc.material.scalarModulation, emitterPhase), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_OpacitySource", &opacitySourceIndex, sizeof(opacitySourceIndex)), E_FAIL);
    return S_OK;
}

HRESULT ComputeSourceHistoryRibbonEmitter::Bind_DistortionResources()
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
    const Vec2 flowUVPolicyParams(
        static_cast<float>(static_cast<uint32>(_desc.material.flowUVPolicy.uPolicy)),
        static_cast<float>(static_cast<uint32>(_desc.material.flowUVPolicy.vPolicy))
    );

    CHECK_FAILED(_shader->Bind_RawValue("g_EffectDistortionParams", &distortionParams, sizeof(distortionParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_DistortionScreenSize", &distortionScreenSize, sizeof(distortionScreenSize)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_FlowUVPolicyParams", &flowUVPolicyParams, sizeof(flowUVPolicyParams)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_FlowUVScale", &_desc.material.flowUVScale, sizeof(_desc.material.flowUVScale)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_FlowUVOffset", &_desc.material.flowUVOffset, sizeof(_desc.material.flowUVOffset)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_FlowUVScrollSpeed", &_desc.material.flowUVScrollSpeed, sizeof(_desc.material.flowUVScrollSpeed)), E_FAIL);
    CHECK_FAILED(_shader->Bind_RawValue("g_FlowUVTilingMode", &flowUVTilingMode, sizeof(flowUVTilingMode)), E_FAIL);
    return S_OK;
}

bool ComputeSourceHistoryRibbonEmitter::Can_SubmitRender() const
{
    return _drawSegmentCount > 0u && Is_Visible() && nullptr != _shader &&
           nullptr != _pointVB && nullptr != _indexBuffer && nullptr != _instanceBuffer && nullptr != _indirectArgsBuffer;
}

uint32 ComputeSourceHistoryRibbonEmitter::Compute_MaxRenderSegmentCount() const
{
    const uint32 laneSegmentCount = Compute_MaxLaneRenderSegmentCount();
    if (_desc.sourceMode != EffectSourceHistoryRibbonSourceMode::ParticleEmitter)
        return laneSegmentCount * (1u + kMaxRetiredLoopStrokeCount);

    return laneSegmentCount * (max(1u, _desc.followerLaneCount) + kMaxRetiredLoopStrokeCount);
}

uint32 ComputeSourceHistoryRibbonEmitter::Compute_MaxLaneRenderSegmentCount() const
{
    const uint32 maxRawSegments = max(1u, _desc.maxSampleCount);
    return maxRawSegments * max(1u, min(_desc.curveSubdivision, kMaxCurveSubdivision));
}

void ComputeSourceHistoryRibbonEmitter::Start_NextLoop()
{
    Retire_ActiveLoopStrokes();

    ++_loopIndex;
    _loopElapsedTime = 0.f;
    _playbackState = PlaybackState::Delayed;
    _finishEmissionAfterHistoryUpdate = false;
    _history.clear();
    _sampleIntervalAccumulator = 0.f;
    _selfRootCollapseLength = -1.f;
    Clear_ParticleFollowerHistory();
    _drawSegmentCount = 0u;
    Resample_RandomSubUVFrameIndex();
}

float ComputeSourceHistoryRibbonEmitter::Resolve_CurrentLoopDelay() const
{
    if (_desc.playback.delayFirstLoopOnly && _loopIndex > 0u)
        return 0.f;

    return max(0.f, _desc.playback.delay);
}

float ComputeSourceHistoryRibbonEmitter::Resolve_Duration() const
{
    return max(0.0001f, _desc.playback.duration);
}

bool ComputeSourceHistoryRibbonEmitter::Has_NextLoop() const
{
    return 0u == _desc.playback.loopCount || _loopIndex + 1u < _desc.playback.loopCount;
}

bool ComputeSourceHistoryRibbonEmitter::Is_DistortionFamily() const
{
    return _desc.material.materialFamily == EffectMaterialFamily::SpriteDistortion;
}

RenderGroup ComputeSourceHistoryRibbonEmitter::Resolve_RenderGroup() const
{
    if (_desc.renderLayerOverride == EffectEmitterRenderLayerOverride::UIEffect)
        return RenderGroup::UIEffect;

    return Is_DistortionFamily() ? RenderGroup::Distortion : RenderGroup::Blend;
}

const wchar_t* ComputeSourceHistoryRibbonEmitter::Resolve_ShaderId() const
{
    return Is_DistortionFamily() ? kRibbonDistortionShaderId : kRibbonShaderId;
}

uint32 ComputeSourceHistoryRibbonEmitter::Resolve_ShaderPassIndex() const
{
    if (Is_DistortionFamily())
        return 0u;

    return _desc.material.blendMode == EffectMaterialBlendMode::Additive ? 1u : 0u;
}

int ComputeSourceHistoryRibbonEmitter::Resolve_OpacitySourceIndex() const
{
    if (_desc.material.opacitySource == "Red" || _desc.material.opacitySource == "red")
        return 1;

    if (_desc.material.opacitySource == "Luminance" || _desc.material.opacitySource == "luminance")
        return 2;

    return 0;
}

bool ComputeSourceHistoryRibbonEmitter::Try_Resolve_SourcePosition(Vec3& outPosition) const
{
    const Shared<IEffectSourcePointSampleProvider> sampleProvider = _desc.sourcePointSampleProvider.lock();
    if (sampleProvider != nullptr)
    {
        EffectSourcePointSample sample{};
        if (!sampleProvider->Try_GetSourcePointSample(sample))
            return false;

        outPosition = sample.worldPosition;
        return true;
    }

    outPosition = _transformCom != nullptr ? _transformCom->Get_WorldPosition() : Vec3{};
    return true;
}

Vec3 ComputeSourceHistoryRibbonEmitter::Resolve_SourcePosition() const
{
    Vec3 sourcePosition{};
    Try_Resolve_SourcePosition(sourcePosition);
    return sourcePosition;
}

Vec4 ComputeSourceHistoryRibbonEmitter::Evaluate_ColorOverLife(float lifeProgress) const
{
    const Vec4 startColor = EffectAssetRuntimeLoad::Average_Color(_desc.initialColor.startColorMin, _desc.initialColor.startColorMax);
    const Vec4 endColor = EffectAssetRuntimeLoad::Average_Color(_desc.colorOverLife.endColorMin, _desc.colorOverLife.endColorMax);
    Vec4 color{
        lerp(startColor.x, endColor.x, lifeProgress),
        lerp(startColor.y, endColor.y, lifeProgress),
        lerp(startColor.z, endColor.z, lifeProgress),
        lerp(startColor.w, endColor.w, lifeProgress)
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

float ComputeSourceHistoryRibbonEmitter::Evaluate_WidthScaleByLife(float lifeProgress) const
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

uint32 ComputeSourceHistoryRibbonEmitter::Evaluate_SubUVFrameIndex() const
{
    const uint32 rows = max(1u, _desc.material.subUVRows);
    const uint32 cols = max(1u, _desc.material.subUVCols);
    const uint32 frameCount = max(1u, rows * cols);
    if (!_desc.subUVFrameOverLife.enabled)
        return 0u;

    const uint32 lastFrame = frameCount - 1u;
    const uint32 startFrame = min(_desc.subUVFrameOverLife.startFrame, lastFrame);
    uint32 endFrame = min(_desc.subUVFrameOverLife.endFrame, lastFrame);
    if (endFrame < startFrame)
        endFrame = startFrame;

    const uint32 span = endFrame - startFrame;
    const uint32 rangeCount = span + 1u;
    const float activeElapsedTime = max(0.f, _loopElapsedTime - Resolve_CurrentLoopDelay());
    switch (_desc.subUVFrameOverLife.playbackMode)
    {
    case SubUVFramePlaybackMode::LifeProgress:
    {
        const float lifeProgress = clamp(activeElapsedTime / Resolve_Duration(), 0.f, 1.f);
        const uint32 frameOffset = min(
            static_cast<uint32>(floorf(lifeProgress * static_cast<float>(rangeCount))),
            rangeCount - 1u
        );
        return startFrame + frameOffset;
    }
    case SubUVFramePlaybackMode::FramesPerSecond:
    {
        const uint32 frameOffset = static_cast<uint32>(floorf(activeElapsedTime * max(0.f, _desc.subUVFrameOverLife.framesPerSecond)));
        return startFrame + (_desc.subUVFrameOverLife.loop ? frameOffset % rangeCount : min(frameOffset, span));
    }
    case SubUVFramePlaybackMode::RandomFrame:
        return min(_randomSubUVFrameIndex, lastFrame);
    case SubUVFramePlaybackMode::FixedFrame:
    default:
        return startFrame;
    }
}

Vec4 ComputeSourceHistoryRibbonEmitter::Resolve_SubUVRect(uint32 frameIndex) const
{
    const uint32 rows = max(1u, _desc.material.subUVRows);
    const uint32 cols = max(1u, _desc.material.subUVCols);
    const uint32 clampedFrameIndex = min(frameIndex, rows * cols - 1u);
    const uint32 row = clampedFrameIndex / cols;
    const uint32 col = clampedFrameIndex % cols;
    const float invCols = 1.f / static_cast<float>(cols);
    const float invRows = 1.f / static_cast<float>(rows);
    const float u0 = static_cast<float>(col) * invCols;
    const float v0 = static_cast<float>(row) * invRows;
    return Vec4{ u0, v0, u0 + invCols, v0 + invRows };
}

void ComputeSourceHistoryRibbonEmitter::Resample_SampleLifetime()
{
    if (!_desc.useLifetimeSampleLifetime)
    {
        _desc.sampleLifetime = max(0.0001f, _desc.sampleLifetime);
        return;
    }

    const float lifeMin = max(0.0001f, min(_desc.lifetime.lifeTime.x, _desc.lifetime.lifeTime.y));
    const float lifeMax = max(lifeMin, max(_desc.lifetime.lifeTime.x, _desc.lifetime.lifeTime.y));
    if (lifeMin == lifeMax)
    {
        _desc.sampleLifetime = lifeMin;
        return;
    }

    const uint32 seed =
        EffectAssetRuntimeLoad::Resolve_SeedSalt(_desc.lifetimeSeed, _effectPlaybackSeed) +
        1319u;
    _desc.sampleLifetime = lerp(lifeMin, lifeMax, EffectAssetRuntimeLoad::Hash01(seed));
}

void ComputeSourceHistoryRibbonEmitter::Resample_RandomSubUVFrameIndex()
{
    const uint32 rows = max(1u, _desc.material.subUVRows);
    const uint32 cols = max(1u, _desc.material.subUVCols);
    const uint32 frameCount = max(1u, rows * cols);
    const uint32 lastFrame = frameCount - 1u;
    const uint32 startFrame = min(_desc.subUVFrameOverLife.startFrame, lastFrame);
    uint32 endFrame = min(_desc.subUVFrameOverLife.endFrame, lastFrame);
    if (endFrame < startFrame)
        endFrame = startFrame;

    const uint32 rangeCount = endFrame - startFrame + 1u;
    const uint32 seed = (_loopIndex + 1u) * 9781u + EffectAssetRuntimeLoad::Resolve_SeedSalt(_desc.subUVRandomFrameSeed, _effectPlaybackSeed) + 1201u;
    const uint32 frameOffset = min(static_cast<uint32>(floorf(EffectAssetRuntimeLoad::Hash01(seed) * static_cast<float>(rangeCount))), rangeCount - 1u);
    _randomSubUVFrameIndex = startFrame + frameOffset;
}

float ComputeSourceHistoryRibbonEmitter::Evaluate_CompactCurve(
    float lifeProgress,
    const Vec4& times,
    const Vec4& timesBlock1,
    const Vec4& values,
    const Vec4& valuesBlock1,
    uint32 keyCount,
    float fallbackValue) const
{
    const uint32 clampedKeyCount = max(1u, min(::Engine::kEffectDistributionCurveMaxKeys, keyCount));
    if (clampedKeyCount == 1u || lifeProgress <= EffectAssetRuntimeLoad::Read_Vec4Component(times, timesBlock1, 0u))
        return EffectAssetRuntimeLoad::Read_Vec4Component(values, valuesBlock1, 0u);

    for (uint32 index = 1u; index < clampedKeyCount; ++index)
    {
        const float rightTime = EffectAssetRuntimeLoad::Read_Vec4Component(times, timesBlock1, index);
        if (lifeProgress > rightTime)
            continue;

        const float leftTime = EffectAssetRuntimeLoad::Read_Vec4Component(times, timesBlock1, index - 1u);
        const float ratio = clamp((lifeProgress - leftTime) / max(0.0001f, rightTime - leftTime), 0.f, 1.f);
        return lerp(
            EffectAssetRuntimeLoad::Read_Vec4Component(values, valuesBlock1, index - 1u),
            EffectAssetRuntimeLoad::Read_Vec4Component(values, valuesBlock1, index),
            ratio
        );
    }

    return lifeProgress > EffectAssetRuntimeLoad::Read_Vec4Component(times, timesBlock1, clampedKeyCount - 1u)
           ? EffectAssetRuntimeLoad::Read_Vec4Component(values, valuesBlock1, clampedKeyCount - 1u)
           : fallbackValue;
}

Shared<ComputeSourceHistoryRibbonEmitter> ComputeSourceHistoryRibbonEmitter::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<ComputeSourceHistoryRibbonEmitter>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_CRITICAL("Failed to Create : ComputeSourceHistoryRibbonEmitter");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> ComputeSourceHistoryRibbonEmitter::Clone(void* arg)
{
    auto instance = make_shared<ComputeSourceHistoryRibbonEmitter>(*this);

    if (FAILED(instance->Initialize(arg)))
    {
        LOG_CRITICAL("Failed to Clone : ComputeSourceHistoryRibbonEmitter");
        MSG_BOX("Failed to Clone : ComputeSourceHistoryRibbonEmitter");
        return nullptr;
    }

    return instance;
}

void ComputeSourceHistoryRibbonEmitter::Free()
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
}

NS_END
