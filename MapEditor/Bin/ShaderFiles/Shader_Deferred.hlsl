#include "Engine_Shader_Defines.hlsli"

//==================================================
// MATRIX
//==================================================
float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;
float4x4 g_ViewMatrixInverse, g_ProjMatrixInverse;
float4x4 g_LightViewMatrix, g_LightProjMatrix;

//==================================================
// TEXTURE
//==================================================
texture2D g_Texture;
texture2D g_DiffuseTexture;
texture2D g_NormalTexture;
texture2D g_ShadeTexture;
texture2D g_SpecularTexture;
texture2D g_DepthTexture;
texture2D g_LightDepthTexture;

//==================================================
// LIGHT
//==================================================
vector g_vLightDir;
vector g_vLightPos;
float  g_fLightRange;
vector g_vLightDiffuse;
vector g_vLightAmbient;
vector g_vLightSpecular;
vector g_vMtrlAmbient = float4(1.f, 1.f, 1.f, 1.f);
vector g_vMtrlSpecular = float4(1.f, 1.f, 1.f, 1.f);

//==================================================
// SHADOW
//==================================================
float   g_fLightFar = 0.f;
float   g_fBias = 0.01f;
float   g_fShadowIntensity = 0.4f;

//==================================================
// CAMERA
//==================================================
vector  g_vCamPosition;
float   g_fFar = 0.f;

//==================================================
// SAMPLER
//==================================================
sampler DefaultSampler = sampler_state
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = wrap;
    AddressV = wrap;
};

sampler ShadowSampler = sampler_state
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = clamp;
    AddressV = clamp;
};

float4 Reconstruct_WorldPos(float2 vTexcoord)
{
    vector vDepthDesc = g_DepthTexture.Sample(DefaultSampler, vTexcoord);
    float fViewZ = vDepthDesc.y * g_fFar;

    float4 vWorldPos;

    // -> ndc space
    vWorldPos.x = vTexcoord.x * 2.f - 1.f;
    vWorldPos.y = vTexcoord.y * -2.f + 1.f;
    vWorldPos.z = vDepthDesc.x;
    vWorldPos.w = 1.f;

    // -> clip space
    vWorldPos *= fViewZ;

    // -> view space
    vWorldPos = mul(vWorldPos, g_ProjMatrixInverse);

    // -> world space
    vWorldPos = mul(vWorldPos, g_ViewMatrixInverse);

    return vWorldPos;
}

//==================================================
// VS
//==================================================
struct VS_IN
{
    float3 vPosition : POSITION;
    float2 vTexcoord : TEXCOORD0;
};

struct VS_OUT
{
    float4 vPosition : SV_POSITION;
    float2 vTexcoord : TEXCOORD0;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out;

    float4x4 matWV, matWVP;

    matWV = mul(g_WorldMatrix, g_ViewMatrix);
    matWVP = mul(matWV, g_ProjMatrix);

    Out.vPosition = mul(float4(In.vPosition, 1.f), matWVP);
    Out.vTexcoord = In.vTexcoord;

    return Out;
}

//==================================================
// PS
//==================================================
struct PS_IN
{
    float4 vPosition : SV_POSITION; 
    float2 vTexcoord : TEXCOORD0;
};

struct PS_OUT_BACKBUFFER
{
    vector vColor : SV_TARGET0;
};

PS_OUT_BACKBUFFER PS_MAIN_DEBUG(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;
    
    Out.vColor = g_Texture.Sample(DefaultSampler, In.vTexcoord);
    
    return Out;
}

struct PS_OUT_LIGHT
{
    vector vShade : SV_TARGET0;
    vector vSpecular : SV_TARGET1;
};

PS_OUT_LIGHT PS_MAIN_DIRECTIONAL(PS_IN In)
{
    PS_OUT_LIGHT Out;

    // Normal : DXGI_FORMAT_R16G16B16A16_UNORM
    vector vNormalDesc = g_NormalTexture.Sample(DefaultSampler, In.vTexcoord);
    vector vNormal = vector(normalize(vNormalDesc.xyz * 2.f - 1.f), 0.f);

    // Light
    float4 vReflect = reflect(normalize(g_vLightDir), vNormal);
    float4 vWorldPos = Reconstruct_WorldPos(In.vTexcoord);
    float4 vLook = vWorldPos - g_vCamPosition;

    float fSpecular = pow(max(dot(normalize(vLook) * -1.f, normalize(vReflect)), 0.f), 50.f);

    Out.vShade = g_vLightDiffuse *
        (max(dot(normalize(g_vLightDir) * -1.f, normalize(vNormal)), 0.f) + (g_vLightAmbient * g_vMtrlAmbient));

    Out.vSpecular = (g_vLightSpecular * g_vMtrlSpecular) * fSpecular;

    return Out;
}

PS_OUT_LIGHT PS_MAIN_POINT(PS_IN In)
{
    PS_OUT_LIGHT Out;

    // Normal : DXGI_FORMAT_R16G16B16A16_UNORM
    vector vNormalDesc = g_NormalTexture.Sample(DefaultSampler, In.vTexcoord);
    vector vNormal = vector(normalize(vNormalDesc.xyz * 2.f - 1.f), 0.f);

    // Light
    float4 vWorldPos = Reconstruct_WorldPos(In.vTexcoord);
    vector vLightVec = vWorldPos - g_vLightPos;
    float fDistance = length(vLightVec);
    vector vLightDir = (fDistance > 0.f) ? (vLightVec / fDistance) : float4(0.f, 0.f, 0.f, 0.f);

    float fAtt = saturate((g_fLightRange - fDistance) / g_fLightRange);

    Out.vShade
        = (g_vLightDiffuse * (max(dot(normalize(vLightDir) * -1.f, normalize(vNormal)), 0.f) + (g_vLightAmbient * g_vMtrlAmbient))) * fAtt;

    Out.vSpecular = 0.f;

    return Out;
}

PS_OUT_BACKBUFFER PS_MAIN_COMBINED(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    // Diffuse
    vector vDiffuse = g_DiffuseTexture.Sample(DefaultSampler, In.vTexcoord);
    if (vDiffuse.a == 0.f)
        discard;

    // Light
    vector vShade = g_ShadeTexture.Sample(DefaultSampler, In.vTexcoord);
    vector vSpecular = g_SpecularTexture.Sample(DefaultSampler, In.vTexcoord);

    Out.vColor = vDiffuse * vShade + vSpecular;

    // Shadow
    float4 vWorldPos = Reconstruct_WorldPos(In.vTexcoord);

    float4 vShadowPos = mul(vWorldPos, g_LightViewMatrix);
    vShadowPos = mul(vShadowPos, g_LightProjMatrix);

    float2 vLightTexcoord;
    vLightTexcoord.x = (vShadowPos.x / vShadowPos.w) * 0.5f + 0.5f;
    vLightTexcoord.y = (vShadowPos.y / vShadowPos.w) * -0.5f + 0.5f;

    float4 vLightDepthDesc = g_LightDepthTexture.Sample(ShadowSampler, vLightTexcoord);
    float fOldLightZ = vLightDepthDesc.y * g_fLightFar;

    if (all(vLightTexcoord >= 0.f) && all(vLightTexcoord <= 1.f))
    {
        if (fOldLightZ + g_fBias < vShadowPos.w)
            Out.vColor.rgb *= g_fShadowIntensity;
    }

    return Out;
}

PS_OUT_BACKBUFFER PS_MAIN_COMBINED_SIMPLE(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    vector vDiffuse = g_DiffuseTexture.Sample(DefaultSampler, In.vTexcoord);

    vector vShade = g_ShadeTexture.Sample(DefaultSampler, In.vTexcoord);
    vector vSpecular = g_SpecularTexture.Sample(DefaultSampler, In.vTexcoord);

    Out.vColor = vDiffuse * vShade + vSpecular;
    return Out;
}

technique11 DefaultTechnique
{
    pass Debug
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_Default, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_MAIN_DEBUG();
    }

    pass Directional
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_None, 0);
        SetBlendState(BS_Blend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_MAIN_DIRECTIONAL();
    }

    pass Point
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_None, 0);
        SetBlendState(BS_Blend, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_MAIN_POINT();
    }

    pass Combined
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_None, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_MAIN_COMBINED();
    }

    pass CombinedSimple // index 4
    {
        SetRasterizerState(RS_Default);
        SetDepthStencilState(DSS_None, 0);
        SetBlendState(BS_Default, float4(0.f, 0.f, 0.f, 0.f), 0xffffffff);

        VertexShader = compile vs_5_0 VS_MAIN();
        GeometryShader = NULL;
        PixelShader = compile ps_5_0 PS_MAIN_COMBINED_SIMPLE();
    }
}
