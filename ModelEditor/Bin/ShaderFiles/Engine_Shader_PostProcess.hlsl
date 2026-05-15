#ifndef ENGINE_SHADER_POSTPROCESS_HLSLI
#define ENGINE_SHADER_POSTPROCESS_HLSLI

#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_ConstantBuffer.hlsl"

//==================================================
// MATERIAL TEXTURES
//==================================================
Texture2D g_MaterialTextures[MATERIAL_SLOT_END];

//==================================================
// 컴파일 단계에서 inline 최적화 진행됨
//==================================================
bool HasPostProcessFeature(uint feature)
{
    return (g_PP_FeatureMask & feature) != 0;
}


//==================================================
// SAMPLE FUNCTIONS
//==================================================


#endif
