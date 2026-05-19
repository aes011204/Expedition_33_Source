#include "../Engine_Shader_ComputeAnimTypes.hlsli"

StructuredBuffer<PoseSRT> g_LocalPoses : register(t0);
StructuredBuffer<BoneParent> g_BoneParents : register(t1);
RWStructuredBuffer<BoneMatrixElement> g_GlobalBones : register(u0);

row_major float4x4 BuildLocalMatrix(PoseSRT pose, bool zeroTranslation)
{
    float4 q = normalize(pose.Rotation);

    float xx = q.x * q.x;
    float yy = q.y * q.y;
    float zz = q.z * q.z;
    float xy = q.x * q.y;
    float xz = q.x * q.z;
    float yz = q.y * q.z;
    float wx = q.w * q.x;
    float wy = q.w * q.y;
    float wz = q.w * q.z;

    float3 row0 = float3(1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz), 2.0f * (xz - wy));
    float3 row1 = float3(2.0f * (xy - wz), 1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx));
    float3 row2 = float3(2.0f * (xz + wy), 2.0f * (yz - wx), 1.0f - 2.0f * (xx + yy));

    float3 translation = zeroTranslation ? 0.0.xxx : pose.Translation;

    row_major float4x4 m;
    m[0] = float4(row0 * pose.Scale.x, 0.0f);
    m[1] = float4(row1 * pose.Scale.y, 0.0f);
    m[2] = float4(row2 * pose.Scale.z, 0.0f);
    m[3] = float4(translation, 1.0f);
    return m;
}

[numthreads(1, 1, 1)]
void CS_Main(uint3 id : SV_DispatchThreadID)
{
    if (id.x != 0)
        return;

    for (uint boneIndex = 0; boneIndex < g_NumBones; ++boneIndex)
    {
        PoseSRT localPose = g_LocalPoses[boneIndex];
        row_major float4x4 localMatrix = BuildLocalMatrix(localPose, int(boneIndex) == g_RootIndex);

        int parentIndex = g_BoneParents[boneIndex].ParentIndex;

        if (parentIndex < 0)
        {
            g_GlobalBones[boneIndex].BoneMatrix = mul(localMatrix, g_PreLocalTransformMatrix);
        }
        else
        {
            g_GlobalBones[boneIndex].BoneMatrix = mul(localMatrix, g_GlobalBones[parentIndex].BoneMatrix);
        }
    }
}
