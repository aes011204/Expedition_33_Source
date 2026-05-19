#include "../Engine_Shader_ComputeAnimTypes.hlsli"

StructuredBuffer<TrackInput> g_Tracks : register(t0);
StructuredBuffer<ChannelMeta> g_ChannelMetas : register(t1);
StructuredBuffer<Keyframe> g_Keyframes : register(t2);
StructuredBuffer<AnimationMeta> g_AnimationMetas : register(t3);
StructuredBuffer<PoseSRT> g_DefaultLocalPoses : register(t4);
RWStructuredBuffer<PoseSRT> g_OutputPoses : register(u0);

float4 NormalizeQuat(float4 q)
{
    return normalize(q);
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

PoseSRT MakePose(float3 s, float4 r, float3 t)
{
    PoseSRT pose;
    pose.Scale = s;
    pose.Rotation = NormalizeQuat(r);
    pose.Translation = t;
    pose.Pad0 = 0.0f;
    return pose;
}

PoseSRT SampleChannelPose(ChannelMeta meta, float tickPosition)
{
    if (meta.KeyframeCount == 0)
        return MakePose(1.0.xxx, float4(0, 0, 0, 1), 0.0.xxx);

    const uint firstIndex = meta.KeyframeOffset;
    const uint lastIndex = meta.KeyframeOffset + meta.KeyframeCount - 1;

    if (meta.KeyframeCount == 1 || tickPosition <= g_Keyframes[firstIndex].TrackPosition)
    {
        Keyframe k = g_Keyframes[firstIndex];
        return MakePose(k.Scale, k.Rotation, k.Translation);
    }

    if (tickPosition >= g_Keyframes[lastIndex].TrackPosition)
    {
        Keyframe k = g_Keyframes[lastIndex];
        return MakePose(k.Scale, k.Rotation, k.Translation);
    }

    uint currentIndex = firstIndex;
    while (currentIndex + 1 <= lastIndex && tickPosition >= g_Keyframes[currentIndex + 1].TrackPosition)
        ++currentIndex;

    Keyframe left = g_Keyframes[currentIndex];
    Keyframe right = g_Keyframes[currentIndex + 1];

    float denom = max(right.TrackPosition - left.TrackPosition, 0.0001f);
    float ratio = saturate((tickPosition - left.TrackPosition) / denom);

    return MakePose(
        lerp(left.Scale, right.Scale, ratio),
        SlerpQuat(left.Rotation, right.Rotation, ratio),
        lerp(left.Translation, right.Translation, ratio));
}

PoseSRT SampleTrackPose(uint boneIndex, TrackInput track)
{
    if (track.AnimationIndex == 0xFFFFFFFFu)
        return g_DefaultLocalPoses[boneIndex];

    AnimationMeta anim = g_AnimationMetas[track.AnimationIndex];
    float tickPosition = track.TrackPositionSec * anim.TickPerSecond;

    const uint beginIndex = anim.ChannelOffset;
    const uint endIndex = anim.ChannelOffset + anim.ChannelCount;

    for (uint i = beginIndex; i < endIndex; ++i)
    {
        ChannelMeta meta = g_ChannelMetas[i];
        if (meta.BoneIndex == int(boneIndex))
            return SampleChannelPose(meta, tickPosition);
    }

    return g_DefaultLocalPoses[boneIndex];
}

[numthreads(64, 1, 1)]
void CS_Main(uint3 id : SV_DispatchThreadID)
{
    const uint totalCount = g_NumBones * g_NumPrimaryTracks;
    if (id.x >= totalCount)
        return;

    const uint boneIndex = id.x % g_NumBones;
    const uint trackIndex = id.x / g_NumBones;

    g_OutputPoses[id.x] = SampleTrackPose(boneIndex, g_Tracks[trackIndex]);
}
