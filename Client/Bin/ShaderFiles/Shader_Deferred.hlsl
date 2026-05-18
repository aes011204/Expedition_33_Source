#include "Engine_Shader_Deferred.hlsl"

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

PS_OUT_BACKBUFFER PS_CombinedHDR(PS_IN In)
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
    float4 worldPos = Reconstruct_WorldPos(In.Texcoord);

    float dynamicShadow = SampleShadowAmount(
    g_LightDepthTexture,
    worldPos,
    g_LightViewMatrix,
    g_LightProjMatrix,
    g_Bias);

    float staticShadow = 0.f;
    if (g_StaticShadowEnabled > 0.5f)
    {
        staticShadow = SampleShadowAmount(
        g_StaticLightDepthTexture,
        worldPos,
        g_StaticLightViewMatrix,
        g_StaticLightProjMatrix,
        g_Bias);
    }

    float finalShadow = max(dynamicShadow, staticShadow);

    if (finalShadow > 0.f)
    {
        float shadowBrightness = lerp(1.f, 0.45f, saturate(g_ShadowIntensity) * finalShadow);
        Out.Color.rgb *= shadowBrightness;
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

PS_OUT_BACKBUFFER PS_MAIN_COMBINED_PREVIEW(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    vector Diffuse = g_DiffuseTexture.Sample(LinearWrapSampler, In.Texcoord);
    if (Diffuse.a == 0.f)
        discard;

    vector Shade = g_ShadeTexture.Sample(LinearSampler, In.Texcoord);
    vector Specular = g_SpecularTexture.Sample(LinearWrapSampler, In.Texcoord);
    vector Emissive = g_EmissiveTexture.Sample(LinearWrapSampler, In.Texcoord);

    Out.Color = Diffuse * Shade + Specular;     // 프리뷰는 direct/deferred lighting 결과만 사용
    Out.Color.rgb += Emissive.rgb;              // emissive는 그대로 더하기
    Out.Color.a = Diffuse.a;

    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(Debug,             RS_Default, DSS_Default,    BS_Default, VS_MAIN,    PS_MAIN_DEBUG)              // 0
    PASS_RS_DS_BS_VP(Directional,       RS_Default, DSS_None,       BS_Blend,   VS_MAIN,    PS_MAIN_DIRECTIONAL)        // 1
    PASS_RS_DS_BS_VP(Point,             RS_Default, DSS_None,       BS_Blend,   VS_MAIN,    PS_MAIN_POINT)              // 2
    PASS_RS_DS_BS_VP(Combined,          RS_Default, DSS_None,       BS_Default, VS_MAIN,    PS_CombinedHDR)             // 3
    PASS_RS_DS_BS_VP(CombinedSimple,    RS_Default, DSS_None,       BS_Default, VS_MAIN,    PS_MAIN_COMBINED_SIMPLE)    // 4
    PASS_RS_DS_BS_VP(CombinedPreview,   RS_Default, DSS_None,       BS_Default, VS_MAIN,    PS_MAIN_COMBINED_PREVIEW)   // 5
}
