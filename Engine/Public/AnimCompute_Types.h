#pragma once

#include "Engine_Define.h"

NS_BEGIN(Engine)

#pragma region CPU
struct AnimPipelineContext
{
    float timeDelta{ 0.f };
    float adjustedTimeDelta{ 0.f };
    float previousTimeSec{ 0.f };
    uint32 previousAnimIndex{ UINT32_MAX };      // notify 검사 시 애니 전환 프레임인지 구분한다.
    uint32 previousAnimStateIndex{ UINT32_MAX }; // 상태 기반 애니 전환도 같이 보존한다.
    int32 rootIndex{ -1 };
};

struct AnimComputeTrackDesc
{
    uint32 animationIndex{ UINT32_MAX };
    float  trackPositionSec{ 0.f };
    float  weight{ 0.f };
    uint32 flags{ 0u }; // enabled / loop / additive / blendTarget
};
#pragma endregion

#pragma region GPU -> Compute Shader

struct AnimComputeFrameContext
{
    AnimPipelineContext modelContext{};
    float blendRatio{ 1.f };
    bool  hasBlendTarget{ false };
};

struct AnimComputePoseSRT
{
    Vec3 scale{};
    Quat rotation{};
    Vec3 translation{};
    float pad0{ 0.f };
};

struct AnimComputeTrackInput
{
    uint32 animationIndex{ UINT32_MAX };
    float  trackPositionSec{ 0.f };
    float  weight{ 0.f };
    uint32 flags{ 0u };
};

struct AnimComputeBoneParent
{
    int32  parentIndex{ -1 };
    uint32 pad0{ 0u };
    uint32 pad1{ 0u };
    uint32 pad2{ 0u };
};

struct AnimComputeMeshBoneRef
{
    uint32 sourceBoneIndex{ 0u };
    uint32 pad0{ 0u };
    uint32 pad1{ 0u };
    uint32 pad2{ 0u };
    Matrix offsetMatrix{};
};

struct MorphGpuInfluence
{
    uint32 targetIndex{};
    float weightScale{ 1.f };
    Vec2 pad0{};
    Vec3 deltaPosition{};
    float pad1{};
    Vec3 deltaNormal{};
    float pad2{};
};

struct MorphDeformCBData
{
    uint32 vertexCount{};
    uint32 targetCount{};
    uint32 pad0{};
    uint32 pad1{};
};

struct AnimComputeCBData
{
    uint32 numBones{ 0u };
    uint32 numPrimaryTracks{ 0u };
    uint32 numMixTracks{ 0u };
    uint32 numAdditiveTracks{ 0u };

    float  blendRatio{ 1.f };
    float  adjustedTimeDelta{ 0.f };
    float  previousTimeSec{ 0.f };
    int32  rootIndex{ -1 };

    Matrix preLocalTransformMatrix{}; // BoneCombine 루트 본에서 필요
};

struct AnimComputeAnimationMeta
{
    uint32 channelOffset{ 0 };
    uint32 channelCount{ 0 };
    float  durationSec{ 0.f };
    float  tickPerSecond{ 0.f };
};

struct AnimComputeChannelMeta
{
    int32  boneIndex{ -1 };
    uint32 keyframeOffset{ 0 };
    uint32 keyframeCount{ 0 };
};

#pragma endregion

NS_END

