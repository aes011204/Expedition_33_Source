#include "../Engine_Shader_ComputeAnimTypes.hlsli"

StructuredBuffer<PoseSRT> g_EvaluatedPoses : register(t0);
RWStructuredBuffer<PoseSRT> g_OutputPoses : register(u0);

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

PoseSRT BlendPose(PoseSRT a, PoseSRT b, float t)
{
    PoseSRT outPose;
    outPose.Scale = lerp(a.Scale, b.Scale, t);
    outPose.Rotation = SlerpQuat(a.Rotation, b.Rotation, t);
    outPose.Translation = lerp(a.Translation, b.Translation, t);
    outPose.Pad0 = 0.0f;
    return outPose;
}

[numthreads(64, 1, 1)]
void CS_Main(uint3 id : SV_DispatchThreadID)
{
    const uint boneIndex = id.x;
    if (boneIndex >= g_NumBones)
        return;

    PoseSRT basePose = g_EvaluatedPoses[boneIndex];

    if (g_NumPrimaryTracks < 2)
    {
        g_OutputPoses[boneIndex] = basePose;
        return;
    }

    PoseSRT targetPose = g_EvaluatedPoses[g_NumBones + boneIndex];
    g_OutputPoses[boneIndex] = BlendPose(basePose, targetPose, saturate(g_BlendRatio));
}
