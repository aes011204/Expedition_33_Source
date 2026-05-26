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
    row_major float4x4 g_LightViewMatrix;
    row_major float4x4 g_LightProjMatrix;

    row_major float4x4 g_StaticLightViewMatrix;
    row_major float4x4 g_StaticLightProjMatrix;

    float g_LightFar;           
    float g_Bias;               
    float g_ShadowIntensity;    
    float g_StaticShadowEnabled;
};

cbuffer ScreenSpaceBuffer
{
    row_major float4x4 g_ScreenViewMatrix;          // 64
    row_major float4x4 g_ScreenProjMatrix;          // 128

    row_major float4x4 g_DeferredViewMatrixInverse; // 192
    row_major float4x4 g_DeferredProjMatrixInverse; // 256

    float4  g_DeferredCamPosition;                  // 272

    float   g_DeferredFar;                          // 276
    float2  g_TexelSize;                            // 284
    float2  g_ScreenSize;                           // 292
    float3  g_DeferredPadding0;                     // 304
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
    float g_UVRotate;               // 80 ~ 83
    uint g_iMaterialMask;           // 84 ~ 87
    uint g_OrmAoSource;             // 88 ~ 91
    uint g_OrmRoughnessSource;      // 92 ~ 95
    uint g_OrmMetalnessSource;      // 96 ~ 99
    uint g_OpacityMaskSource;       // 100 ~ 103
    float2 g_MaterialPadding1;      // 104 ~ 111
}; // 112 bytes

cbuffer DrawFeatureBuffer
{
    uint g_ActiveFeatureMask;
    uint g_SupportFeatureMask;
    uint g_FeatureTextureMask;
    uint g_DrawFeaturePadding0;

    float4 g_FeatureParam0;
    float4 g_FeatureParam1;
};

cbuffer SSAOBuffer
{
    float g_SSAORadius;
    float g_SSAOBias;
    float g_SSAOIntensity;
    float g_SSAOPower;

    float g_SSAODepthRange;
    float g_SSAOBlurRadius;
    float g_SSAODepthSharpness;
    float g_SSAONormalSharpness;

    uint g_SSAOSampleCount;
    uint g_SSAOEnabled;
    uint g_SSAOBilateralEnabled;
    uint g_SSAOPad0;
};

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

    uint g_PP_FeatureMask;             // 40 ~ 43
    uint g_PP_UseBackgroundFallback;   // 44 ~ 47
    float4 g_PP_Padding2;              // 48 ~ 63

    float4 g_PP_BackgroundFallbackColor; // 64 ~ 79
}; // 80 bytes

cbuffer HairDescBuffer
{
    float4 g_HairColorTop;
    float4 g_HairColorDown;
    float4 g_HairHighlightColor;

    float g_HairOpacityMaskClipValue;
    float g_HairRoughness;
    float g_HairSpecular;
    float g_HairScatter;

    float g_HairBrightness;
    float g_HairRim;
    float g_HairRandomHueVariation;
    float g_HairRandomValueVariation;

    float g_HairBumpStrength;
    float g_HairBumpDistance;
    float g_HairGradientColor;
    float g_HairGradientContrast;

    float g_HairGradientHardness;
    float g_HairGradientHeight;
    float g_HairEdgeContrast;
    float g_HairEdgeMin;
};

#endif
