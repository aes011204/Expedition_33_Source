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

//==================================================
// VS
//==================================================
struct VS_IN
{
    float3 Position : POSITION;
    float2 Texcoord : TEXCOORD0;
};

struct VS_OUT
{
    float4 Position : SV_POSITION;
    float2 Texcoord : TEXCOORD0;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out;

    float4x4 matWV, matWVP;

    matWV = mul(g_WorldMatrix, g_ScreenViewMatrix);
    matWVP = mul(matWV, g_ScreenProjMatrix);

    Out.Position = mul(float4(In.Position, 1.f), matWVP);
    Out.Texcoord = In.Texcoord;

    return Out;
}

//==================================================
// PS
//==================================================
struct PS_IN
{
    float4 Position : SV_POSITION; 
    float2 Texcoord : TEXCOORD0;
};

struct PS_OUT_BACKBUFFER
{
    vector Color : SV_TARGET0;
};

PS_OUT_BACKBUFFER PS_MAIN_DEBUG(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;
    
    Out.Color = g_Texture.Sample(LinearWrapSampler, In.Texcoord);
    
    return Out;
}

struct PS_OUT_LIGHT
{
    vector Shade    : SV_TARGET0;
    vector Specular : SV_TARGET1;
};

PS_OUT_LIGHT PS_MAIN_DIRECTIONAL(PS_IN In)
{
    PS_OUT_LIGHT Out;

    // Normal : DXGI_FORMAT_R16G16B16A16_UNORM
    vector NormalDesc = g_NormalTexture.Sample(LinearWrapSampler, In.Texcoord);
    vector Normal = vector(normalize(DecodeNormal(NormalDesc.xyz)), 0.f);

    // ORM
    vector MaterialDesc = g_MaterialTexture.Sample(LinearWrapSampler, In.Texcoord);
    float AO = MaterialDesc.r;
    float Roughness = saturate(MaterialDesc.g);
    float Metalness = saturate(MaterialDesc.b);

    // Light
    float4 Reflect = reflect(normalize(g_LightDir), Normal);
    float4 WorldPos = Reconstruct_WorldPos(In.Texcoord);
    float4 Look = WorldPos - g_DeferredCamPosition;                                                    

    float SpecularPower = lerp(128.f, 8.f, Roughness);
    float Specular = pow(max(dot(normalize(Look) * -1.f, normalize(Reflect)), 0.f), SpecularPower);

    float NDotL = max(dot(normalize(g_LightDir) * -1.f, normalize(Normal)), 0.f);

    vector Ambient = g_LightAmbient * g_MtrlAmbient * AO;
    vector DiffuseLight = g_LightDiffuse * NDotL;

    Out.Shade = DiffuseLight + Ambient;

    float SpecularScale = 1.f - Roughness;
     
    Out.Specular = (g_LightSpecular * g_MtrlSpecular) * Specular * SpecularScale;

    return Out;
}

PS_OUT_LIGHT PS_MAIN_POINT(PS_IN In)
{
    PS_OUT_LIGHT Out;

    // Normal : DXGI_FORMAT_R16G16B16A16_UNORM
    vector NormalDesc = g_NormalTexture.Sample(LinearWrapSampler, In.Texcoord);
    vector Normal = vector(normalize(DecodeNormal(NormalDesc.xyz)), 0.f);

    vector MaterialDesc = g_MaterialTexture.Sample(LinearWrapSampler, In.Texcoord);

    float AO = saturate(MaterialDesc.r);
    float Roughness = saturate(MaterialDesc.g);
    float Metalness = saturate(MaterialDesc.b);

    // Light
    float4 WorldPos = Reconstruct_WorldPos(In.Texcoord);
    vector LightVec = WorldPos - g_LightPos;
    float Distance = length(LightVec);
    vector vLightDir = (Distance > 0.f) ? (LightVec / Distance) : float4(0.f, 0.f, 0.f, 0.f);

    float fAtt = saturate((g_LightRange - Distance) / g_LightRange);

    float NdotL = max(dot(normalize(vLightDir) * -1.f, normalize(Normal)), 0.f);

    vector DiffuseLight = g_LightDiffuse * NdotL;
    vector AmbientLight = g_LightAmbient * g_MtrlAmbient * AO;

    Out.Shade = (DiffuseLight + AmbientLight) * fAtt;

    Out.Specular = 0.f;

    return Out;
}

PS_OUT_BACKBUFFER PS_MAIN_COMBINED(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    // Diffuse
    vector Diffuse = g_DiffuseTexture.Sample(LinearWrapSampler, In.Texcoord);
    if (Diffuse.a == 0.f)
        discard;

    // Light
    vector Shade = g_ShadeTexture.Sample(LinearWrapSampler, In.Texcoord);
    vector Specular = g_SpecularTexture.Sample(LinearWrapSampler, In.Texcoord);
    vector Emissive = g_EmissiveTexture.Sample(LinearWrapSampler, In.Texcoord);

    Out.Color = Diffuse * Shade + Specular;

    // Shadow
    float4 vWorldPos = Reconstruct_WorldPos(In.Texcoord);

    float4 ShadowPos = mul(vWorldPos, g_LightViewMatrix);
    ShadowPos = mul(ShadowPos, g_LightProjMatrix);

    float2 LightTexcoord;
    LightTexcoord.x = (ShadowPos.x / ShadowPos.w) * 0.5f + 0.5f;
    LightTexcoord.y = (ShadowPos.y / ShadowPos.w) * -0.5f + 0.5f;

    float4 vLightDepthDesc = g_LightDepthTexture.Sample(LinearClampSampler, LightTexcoord);
    float fOldLightZ = vLightDepthDesc.y * g_LightFar;

    if (all(LightTexcoord >= 0.f) && all(LightTexcoord <= 1.f))
    {
        if (fOldLightZ + g_Bias < ShadowPos.w)
            Out.Color.rgb *= g_ShadowIntensity;
    }

    Out.Color.rgb += Emissive.rgb;
    Out.Color.a = Diffuse.a;

    return Out;
}

PS_OUT_BACKBUFFER PS_MAIN_COMBINED_SIMPLE(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    vector Diffuse = g_DiffuseTexture.Sample(LinearWrapSampler, In.Texcoord);
    if (Diffuse.a == 0.f)
        discard;
    vector Shade = g_ShadeTexture.Sample(LinearWrapSampler, In.Texcoord);
    vector Specular = g_SpecularTexture.Sample(LinearWrapSampler, In.Texcoord);

    Out.Color = Diffuse * Shade + Specular;
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(Debug,         RS_Default, DSS_Default,    BS_Default, VS_MAIN,    PS_MAIN_DEBUG)          // 0
    PASS_RS_DS_BS_VP(Directional,   RS_Default, DSS_None,       BS_Blend,   VS_MAIN,    PS_MAIN_DIRECTIONAL)    // 1
    PASS_RS_DS_BS_VP(Point,         RS_Default, DSS_None,       BS_Blend,   VS_MAIN,    PS_MAIN_POINT)          // 2
    //PASS_RS_DS_BS_VP(Combined,      RS_Default, DSS_None,       BS_Default, VS_MAIN,    PS_MAIN_COMBINED)       // 3
    // 지금은 그림자 없어서 -> GamePlay에서 y축으로 일정 값 넘어서 내려가면 그림자처리돼서 어두워서 안보임
    PASS_RS_DS_BS_VP(Combined, RS_Default, DSS_None, BS_Default, VS_MAIN, PS_MAIN_COMBINED_SIMPLE) // 3
    PASS_RS_DS_BS_VP(CombinedSimple, RS_Default, DSS_None, BS_Default, VS_MAIN, PS_MAIN_COMBINED_SIMPLE)        // 4
}
