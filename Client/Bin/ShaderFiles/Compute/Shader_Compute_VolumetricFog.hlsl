RWTexture3D<float4> g_FroxelVolumeUAV : register(u0);

cbuffer VolumetricFogBuffer
{
    uint g_FroxelWidth;
    uint g_FroxelHeight;
    uint g_FroxelDepth;
    float g_FroxelPadding;
}

