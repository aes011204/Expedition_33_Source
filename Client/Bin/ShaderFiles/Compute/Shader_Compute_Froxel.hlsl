RWTexture3D<float4> g_FroxelVolumeUAV : register(u0);
Texture3D<float4> g_FroxelTexture : register(t0);

cbuffer FroxelBuffer : register(b0)
{
    uint g_FroxelWidth;
    uint g_FroxelHeight;
    uint g_FroxelDepth;
    float froxelNear;

    float froxelFar;
    float3 froxelPadding;

    Matrix inverseProjection;
    float g_FroxelPadding;
}

// 카메라 시야 공간을 3D 격자로 나눠 놓고, 현재는 Z 방향으로만 그라데이션
[numthreads(8, 8, 1)]
void InJectFroxelBuffer(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    // 현재 담당 중인 Froxel 칸
    if (dispatchThreadID.x >= g_FroxelWidth || dispatchThreadID.y >= g_FroxelHeight || dispatchThreadID.z >= g_FroxelDepth)
        return;

    float sliceRatio = (float) dispatchThreadID.z / max((float) (g_FroxelDepth - 1u), 1.f);

    float3 froxelCoord = float3(
    (float) dispatchThreadID.x / max((float) (g_FroxelWidth - 1u), 1.f),
    (float) dispatchThreadID.y / max((float) (g_FroxelHeight - 1u), 1.f),
    (float) dispatchThreadID.z / max((float) (g_FroxelDepth - 1u), 1.f));

    g_FroxelVolumeUAV[dispatchThreadID] = float4(froxelCoord, 1.f);
}
