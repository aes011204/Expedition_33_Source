// Engine 원본 경로: Engine/Bin/ShaderFiles/Engine_Shader_ConstantBuffer.hlsl
// Client 복사본 경로: Client/Bin/ShaderFiles/Engine_Shader_ConstantBuffer.hlsl - 먼저 이 원본의 의도를 확인하세요.
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

    row_major float4x4 g_CascadeLightViewMatrices[4];
    row_major float4x4 g_CascadeLightProjMatrices[4];

    row_major float4x4 g_StaticLightViewMatrix;
    row_major float4x4 g_StaticLightProjMatrix;

    float4 g_CascadeSplits;
    float g_CascadeBlendDistance;
    float g_LightFar;
    float g_Bias;

    float g_ShadowIntensity;
    float g_StaticShadowEnabled;
    uint g_ShadowCascadeCount;
    uint g_ActiveShadowCascadeIndex;
};

cbuffer ScreenSpaceBuffer
{
    row_major float4x4 g_ScreenViewMatrix;          // 64
    row_major float4x4 g_ScreenProjMatrix;          // 128

    row_major float4x4 g_DeferredViewMatrixInverse; // 192
    row_major float4x4 g_DeferredProjMatrixInverse; // 256

    float4  g_DeferredCamPosition;                  // 272

    float2 g_TexelSize;                             // 272
    float2 g_ScreenSize;                            // 280
    float g_DeferredFar;                            // 288
    float3 g_DeferredPadding0;                      // 292
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
    float4 g_FeatureParam2;
    float4 g_FeatureParam3;
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

cbuffer DeferredLightingBuffer
{
    float g_IBLIntensity;
    float g_IBLDiffuseIntensity;
    float g_IBLSpecularIntensity;
    float g_IBLPrefilterMaxMip;

    float g_SpecAAStrength;
    float g_SpecAAMinVariance;
    float g_SpecAAMaxAddedRoughness;
    uint g_IBLEnabled;

    uint g_SpecAAEnabled;
    uint3 g_IBLPadding0;
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

cbuffer LightShaftBuffer
{
    float2 g_LS_LightScreenUV;          // 0 ~  7
    float g_LS_LightVisible;            // 8 ~ 11
    float g_LS_LightFacingCamera;       // 12 ~ 15

    float3 g_LS_ShaftColor;             // 16 ~ 27
    float g_LS_Intensity;               // 28 ~ 31

    uint g_LS_SampleCount;              // 32 ~ 35
    float g_LS_RayLength;               // 36 ~ 39
    float g_LS_Decay;                   // 40 ~ 43
    float g_LS_SunDiscRadius;           // 44 ~ 47

    float g_LS_EdgeFade;                // 48 ~ 51
    float g_LS_DepthEmptyThreshold;     // 52 ~ 55
    float2 g_LS_Padding0;               // 56 ~ 63
};  // 64 bytes

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

cbuffer ScreenFxBuffer
{
    float3 g_vignetteColor;
    float g_vignetteIntensity;
    float g_vignetteRadius;
    float g_vignetteSoftness;

    float g_ghostingIntensity;
    float g_transitionProgress;

    float3 g_transitionColor;
    float g_radialBlurIntensity;

    float2 g_radialBlurCenter;
    float g_radialBlurRadius;
    float g_chromaticAberrationIntensity;

    float g_bulgeIntensity;
    float g_pullIntensity;
    float2 g_pullDirection;

    float g_wipeRadius;
    float g_edgeSoftness;
    float2 pad;
};

struct FogZoneEntry
{
    float3 zoneMin;
    float zoneRadius;

    float3 zoneMax;
    float _pad0;

    float4 fogColor;

    float fogDensity;
    float fogStart;
    float _pad1;
    float _pad2;
};

cbuffer FogZoneBuffer
{
    FogZoneEntry g_FogZones[8];
    int g_FogAABBCount;
    int g_FogSphereCount;
    float2 g_FogZonePad0;
};

cbuffer WaterBuffer : register(b13)
{
    float g_LargeWaveScale;
    float g_LargeWaveSpeed;
    float g_LargeWaveAmplify;
    float g_SmallWaveScale;

    float g_SmallWaveSpeed;
    float g_SmallWaveAmplify;
    float g_SeafoamScale;
    float g_SeafoamSpeed;

    float g_SeafoamHeightPower;
    float g_SeafoamHeightMultiply;
    float g_FoamDistortion;
    float g_DepthFade;

    float g_CameraDistanceFade;
    float g_ReflectionAmount;
    float g_LuminanceBias;
    float g_WaterElapsedTime;

    float4 g_WaterColor;
    float4 g_WaterFresnelColor;
}

#endif
