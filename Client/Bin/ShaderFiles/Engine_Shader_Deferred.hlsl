#ifndef ENGINE_SHADER_DEFERRED_HLSL
#define ENGINE_SHADER_DEFERRED_HLSL

#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Utils.hlsl"

//==================================================
// TEXTURE
//==================================================
Texture2D g_Texture;
Texture2D g_DiffuseTexture;
Texture2D g_NormalTexture;
Texture2D g_ShadeTexture;
Texture2D g_SpecularTexture;
Texture2D g_DepthTexture;
Texture2D g_LightDepthTexture;
Texture2D g_MaterialTexture;
Texture2D g_EmissiveTexture;

//==================================================
// LIGHT
//==================================================
vector g_MtrlAmbient = float4(1.f, 1.f, 1.f, 1.f);
vector g_MtrlSpecular = float4(1.f, 1.f, 1.f, 1.f);



float4 Reconstruct_WorldPos(float2 Texcoord)
{
    vector DepthDesc = g_DepthTexture.Sample(LinearWrapSampler, Texcoord);
    float fViewZ = DepthDesc.y * g_DeferredFar;

    float4 WorldPos;

    // -> ndc space
    WorldPos.x = Texcoord.x * 2.f - 1.f;
    WorldPos.y = Texcoord.y * -2.f + 1.f;
    WorldPos.z = DepthDesc.x;
    WorldPos.w = 1.f;

    // -> clip space
    WorldPos *= fViewZ;

    // -> view space
    WorldPos = mul(WorldPos, g_DeferredProjMatrixInverse);

    // -> world space
    WorldPos = mul(WorldPos, g_DeferredViewMatrixInverse);

    return WorldPos;
}

#endif
