#ifndef ENGINE_SHADER_CONSTANTBUFFER_HLSL
#define ENGINE_SHADER_CONSTANTBUFFER_HLSL

//cbuffer Camera
//{
//    float4x4 g_ViewMatrix;          //   0 ~  63
//    float4x4 g_ProjMatrix;          //  64 ~ 127
//    float4x4 g_VPMatrix;            // 128 ~ 191
//
//    float g_ViewZ;                  // 192 ~ 195
//    float3 g_CameraPadding0;        // 196 ~ 207
//}; // 208 bytes
//
//cbuffer CameraInv
//{
//    float4x4 g_ViewInvMatrix;       //   0 ~  63
//    float4x4 g_ProjInvMatrix;       //  64 ~ 127
//}; // 128 bytes
//
//cbuffer Light
//{
//    float4 g_LightAmbient;          //   0 ~  15
//    float4 g_LightDiffuse;          //  16 ~  31
//    float4 g_LightSpecular;         //  32 ~  47
//    float4 g_LightPos;              //  48 ~  63
//    float4 g_LightDir;              //  64 ~  79
//}; // 80 bytes

cbuffer MaterialBuffer
{
    float4 g_BaseColorTint;         //  0 ~ 15
    float4 g_EmissiveColor;         // 16 ~ 31

    float g_EmissiveIntensity;      // 32 ~ 35
    float g_EmissiveMaskMul;        // 36 ~ 39
    float g_NormalStrength;         // 40 ~ 43
    float g_OpacityMaskClipValue;   // 44 ~ 47

    float g_AoScale;                // 48 ~ 51
    float g_RoughnessScale;         // 52 ~ 55
    float g_MetalnessScale;         // 56 ~ 59
    float g_MaterialPadding0;       // 60 ~ 63

    float2 g_UVScale;               // 64 ~ 71
    float2 g_UVOffset;              // 72 ~ 79

    uint g_iMaterialMask;           // 80 ~ 83
    float3 g_MaterialPadding1;      // 84 ~ 95
}; // 96 bytes

#endif
