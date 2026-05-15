#ifndef ENGINE_SHADER_CONSTANTBUFFER_HLSL
#define ENGINE_SHADER_CONSTANTBUFFER_HLSL

//cbuffer CameraInv
//{
//    float4x4 g_ViewInvMatrix;       //   0 ~  63
//    float4x4 g_ProjInvMatrix;       //  64 ~ 127
//}; // 128 bytes

cbuffer CameraBuffer
{
    row_major float4x4 g_ViewMatrix;
    row_major float4x4 g_ProjMatrix;
    row_major float4x4 g_ViewMatrixInverse;
    row_major float4x4 g_ProjMatrixInverse;

    float4 g_CamPosition;
    float g_Far;
    float3 g_CameraPadding0;
};

cbuffer ObjectBuffer
{
    row_major float4x4 g_WorldMatrix;
};

cbuffer ShadowBuffer
{
    row_major float4x4 g_LightViewMatrix; // 64
    row_major float4x4 g_LightProjMatrix; // 128

    float g_LightFar;           // 132
    float g_Bias;               // 136
    float g_ShadowIntensity;    // 140
    float g_ShadowPadding0;     // 144
};

cbuffer DeferredBuffer
{
    row_major float4x4 g_ScreenViewMatrix;  // 64
    row_major float4x4 g_ScreenProjMatrix;  // 128

    row_major float4x4 g_DeferredViewMatrixInverse; // 192
    row_major float4x4 g_DeferredProjMatrixInverse; // 256

    float4  g_DeferredCamPosition;           // 272

    float   g_DeferredFar;                   // 276
    float3  g_DeferredPadding0;              // 288
};

cbuffer LightBuffer
{
    float4 g_LightDir;
    float4 g_LightPos;

    float4 g_LightAmbient;
    float4 g_LightDiffuse;
    float4 g_LightSpecular;

    float g_LightRange;
    float3 g_LightPadding0;
};

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
    float g_UVRotate;              // 80 ~ 83

    uint g_iMaterialMask;           // 84 ~ 87
    float2 g_MaterialPadding1;      // 88 ~ 95
}; // 96 bytes

cbuffer PostProcessBuffer
{
    float g_PP_Exposure;        //  0 ~  3
    float g_PP_InvWhitePoint;   //  4 ~  7
    float g_PP_Gamma;           //  8 ~ 11
    float g_PP_BloomRadius;     // 12 ~ 15

    float g_PP_BloomThreshold;  // 16 ~ 19
    float g_PP_BloomIntensity;  // 20 ~ 23
    float g_PP_BloomSoftKnee;   // 24 ~ 27
    float g_PP_BloomClamp;      // 28 ~ 31

    float g_PP_BloomScatter;    // 32 ~ 35
    uint g_PP_ToneMapMode;      // 36 ~ 39
    float2 g_PP_BloomTexelSize; // 40 ~ 47

    uint g_PP_FeatureMask;      // 48 ~ 51
    float3 g_PP_Padding2;       // 52 ~ 63
}; // 64 bytes
#endif
