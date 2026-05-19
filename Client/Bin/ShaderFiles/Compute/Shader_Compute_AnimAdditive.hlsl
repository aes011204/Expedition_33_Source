#include "../Engine_Shader_ComputeAnimTypes.hlsli"

StructuredBuffer<PoseSRT> g_BasePoses : register(t0);
StructuredBuffer<TrackInput> g_Tracks : register(t1);
StructuredBuffer<ChannelMeta> g_ChannelMetas : register(t2);
StructuredBuffer<Keyframe> g_Keyframes : register(t3);
StructuredBuffer<AnimationMeta> g_AnimationMetas : register(t4);
StructuredBuffer<PoseSRT> g_DefaultLocalPoses : register(t5);
RWStructuredBuffer<PoseSRT> g_OutputPoses : register(u0);

float4 NormalizeQuat(float4 q)
{
    return normalize(q);
}

float4 ConjugateQuat(float4 q)
{
    return float4(-q.xyz, q.w);
}

float4 SlerpQuat(float4 a, float4 b, float t)
{
    float cosOmega = dot(a, b);

    if (cosOmega < 0.0f)
    {
        b = -b;
        cosOmega = -cosOmega;
    }

    if (cosOmega > 0.9995f)
        return normalize(lerp(a, b, t));

    float omega = acos(saturate(cosOmega));
    float sinOmega = sin(omega);
    float scale0 = sin((1.0f - t) * omega) / sinOmega;
    float scale1 = sin(t * omega) / sinOmega;
    return normalize(a * scale0 + b * scale1);
}

PoseSRT SampleChannelPose(ChannelMeta meta, float tickPosition)
{
    if (meta.KeyframeCount == 0)
        return g_DefaultLocalPoses[max(meta.BoneIndex, 0)];

    const uint firstIndex = meta.KeyframeOffset;
    const uint lastIndex = meta.KeyframeOffset + meta.KeyframeCount - 1;

    if (meta.KeyframeCount == 1 || tickPosition <= g_Keyframes[firstIndex].TrackPosition)
    {
        Keyframe k = g_Keyframes[firstIndex];
        PoseSRT pose = { k.Scale, NormalizeQuat(k.Rotation), k.Translation, 0.0f };
        return pose;
    }

    if (tickPosition >= g_Keyframes[lastIndex].TrackPosition)
    {
        Keyframe k = g_Keyframes[lastIndex];
        PoseSRT pose = { k.Scale, NormalizeQuat(k.Rotation), k.Translation, 0.0f };
        return pose;
    }

    uint currentIndex = firstIndex;
    while (currentIndex + 1 <= lastIndex && tickPosition >= g_Keyframes[currentIndex + 1].TrackPosition)
        ++currentIndex;

    Keyframe left = g_Keyframes[currentIndex];
    Keyframe right = g_Keyframes[currentIndex + 1];
    float denom = max(right.TrackPosition - left.TrackPosition, 0.0001f);
    float ratio = saturate((tickPosition - left.TrackPosition) / denom);

    PoseSRT pose;
    pose.Scale = lerp(left.Scale, right.Scale, ratio);
    pose.Rotation = SlerpQuat(left.Rotation, right.Rotation, ratio);
    pose.Translation = lerp(left.Translation, right.Translation, ratio);
    pose.Pad0 = 0.0f;
    return pose;
}

bool FindChannelMeta(uint boneIndex, TrackInput track, out ChannelMeta outMeta)
{
    outMeta.BoneIndex = -1;
    outMeta.KeyframeOffset = 0;
    outMeta.KeyframeCount = 0;

    if (track.AnimationIndex == 0xFFFFFFFFu)
        return false;

    AnimationMeta anim = g_AnimationMetas[track.AnimationIndex];
    const uint beginIndex = anim.ChannelOffset;
    const uint endIndex = anim.ChannelOffset + anim.ChannelCount;

    for (uint i = beginIndex; i < endIndex; ++i)
    {
        ChannelMeta meta = g_ChannelMetas[i];
        if (meta.BoneIndex == int(boneIndex))
        {
            outMeta = meta;
            return true;
        }
    }

    return false;
}

PoseSRT SampleTrackPose(uint boneIndex, TrackInput track)
{
    ChannelMeta meta;
    if (!FindChannelMeta(boneIndex, track, meta))
        return g_DefaultLocalPoses[boneIndex];

    AnimationMeta anim = g_AnimationMetas[track.AnimationIndex];
    float tickPosition = track.TrackPositionSec * anim.TickPerSecond;
    return SampleChannelPose(meta, tickPosition);
}

PoseSRT SampleReferencePose(uint boneIndex, TrackInput track)
{
    ChannelMeta meta;
    if (!FindChannelMeta(boneIndex, track, meta))
        return g_DefaultLocalPoses[boneIndex];

    if (meta.KeyframeCount == 0)
        return g_DefaultLocalPoses[boneIndex];

    Keyframe first = g_Keyframes[meta.KeyframeOffset];
    PoseSRT pose = { first.Scale, NormalizeQuat(first.Rotation), first.Translation, 0.0f };
    return pose;
}

PoseSRT ApplyAdditivePose(PoseSRT basePose, PoseSRT additivePose, PoseSRT referencePose, float weight)
{
    PoseSRT outPose;

    float3 safeRefScale = max(referencePose.Scale, 0.0001.xxx);
    float3 scaleDelta = additivePose.Scale / safeRefScale;
    outPose.Scale = basePose.Scale * lerp(1.0.xxx, scaleDelta, weight);

    float4 rotationDelta = mul(additivePose.Rotation, ConjugateQuat(referencePose.Rotation));
    float4 weightedDelta = SlerpQuat(float4(0, 0, 0, 1), rotationDelta, weight);
    outPose.Rotation = NormalizeQuat(mul(weightedDelta, basePose.Rotation));

    outPose.Translation = basePose.Translation + (additivePose.Translation - referencePose.Translation) * weight;
    outPose.Pad0 = 0.0f;
    return outPose;
}

[numthreads(64, 1, 1)]
void CS_Main(uint3 id : SV_DispatchThreadID)
{
    const uint boneIndex = id.x;
    if (boneIndex >= g_NumBones)
        return;

    PoseSRT resultPose = g_BasePoses[boneIndex];

    for (uint i = 0; i < g_NumAdditiveTracks; ++i)
    {
        TrackInput track = g_Tracks[i];
        float weight = saturate(track.Weight);

        if (weight <= 0.0f)
            continue;

        PoseSRT additivePose = SampleTrackPose(boneIndex, track);
        PoseSRT referencePose = SampleReferencePose(boneIndex, track);
        resultPose = ApplyAdditivePose(resultPose, additivePose, referencePose, weight);
    }

    g_OutputPoses[boneIndex] = resultPose;
}
