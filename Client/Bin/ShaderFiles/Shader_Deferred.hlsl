#include "Engine_Shader_Deferred.hlsl"

#define IBL_DUMMY_TEST 0

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

struct PS_OUT_HDR_WITH_MASK         // 해당 픽셀이 그려졌는지 (안 그려졌다면 해당 픽셀을 배경 기본색으로 덮기 위해)
{
    vector Color    : SV_TARGET0;
    float Mask      : SV_TARGET1;
};

PS_OUT_BACKBUFFER PS_MAIN_DEBUG(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;
    
    Out.Color = g_Texture.Sample(LinearWrapSampler, In.Texcoord);
    
    return Out;
}

struct PS_OUT_LIGHT
{
    vector Shade    : SV_TARGET0;   // 픽셀의 재질이 그 빛을 받아 diffuse 방향으로 반사한 결과 색
    vector Ambient  : SV_TARGET1;   // 픽셀이 빛을 받아서 카메라 방향으로 반사한 스페큘러 결과
    vector Specular : SV_TARGET2;
};

PS_OUT_LIGHT PS_MAIN_DIRECTIONAL(PS_IN In)
{
    PS_OUT_LIGHT Out;

    // Diffuse 
    vector baseColorDesc = g_DiffuseTexture.Sample(LinearWrapSampler, In.Texcoord);
    if (baseColorDesc.a == 0.f) // 비어있는 픽셀
        discard;
    float3 baseColor = saturate(baseColorDesc.rgb);

    // Normal : DXGI_FORMAT_R16G16B16A16_UNORM
    vector NormalDesc = g_NormalTexture.Sample(LinearWrapSampler, In.Texcoord);
    float3 N = normalize(DecodeNormal(NormalDesc.xyz));

    // ORM
    vector MaterialDesc = g_MaterialTexture.Sample(LinearWrapSampler, In.Texcoord);
    float roughness = ApplySpecAA(N, MaterialDesc.g);   // Specular Anti-Aliasing 적용된 roughness
    float metalness = saturate(MaterialDesc.b);

    float4 WorldPos = Reconstruct_WorldPos(In.Texcoord);
    float3 V = normalize(g_DeferredCamPosition.xyz - WorldPos.xyz); // 현재 픽셀에서 카메라를 향하는 방향
    float3 L = normalize(-g_LightDir.xyz);                          // 현재 픽셀에서 빛을 향하는 방향

    float3 Hraw = V + L;
    float invHLen = rsqrt(max(dot(Hraw, Hraw), EPSILON));   
    float3 H = Hraw * invHLen;                                      // V와 L의 중간 방향. 하프벡터

    float NdotL = saturate(dot(N, L));  // 표면이 빛을 얼마나 정면으로 받는지
    if (NdotL <= 0.0f)
    {
        Out.Shade = float4(0, 0, 0, 1);
        Out.Specular = float4(0, 0, 0, 1);
        Out.Ambient = float4(0, 0, 0, 1);
        return Out;
    }

    float NdotV = saturate(dot(N, V));  // 표면이 카메라를 얼마나 정면으로 보고 있는지
    float NdotH = saturate(dot(N, H)); 
    float VdotH = saturate(dot(V, H));

    // Cook-Torrance Specular BRDF
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), baseColor, metalness);    // 비금속 반사 ~ 금속 반사 
    float3 F = FresnelSchlick(VdotH, F0);               // 보는 각도에 따라 반사율이 얼마나 강해지는지
    float D = DistributionGGX(NdotH, roughness);        // roughness에 따라 하이라이트가 어떻게 펴질지
    float G = GeometrySmith(NdotV, NdotL, roughness);   // 미세 요철 때문에 반사광이 얼마나 줄어드는지

    // Cook-Torrance Microfacet Specular BRDF
    float3 specularBRDF = (D * G * F) / max(4.0f * NdotV * NdotL, EPSILON);

    float3 kS = F;
    float3 kD = (1.0f - kS) * (1.0f - metalness);
    float3 diffuseBRDF = kD * baseColor / PI;

    // 픽셀에 도달하는 빛의 색/세기
    float3 radiance = g_LightDiffuse.rgb;

    Out.Shade = float4(diffuseBRDF * radiance * NdotL, 1.f);
    Out.Specular = float4(specularBRDF * radiance * NdotL, 1.f);
    Out.Ambient = float4(0.f, 0.f, 0.f, 1.f);

    return Out;
}

PS_OUT_LIGHT PS_MAIN_POINT(PS_IN In)
{
    PS_OUT_LIGHT Out;

    // Diffuse
    vector baseColorDesc = g_DiffuseTexture.Sample(LinearWrapSampler, In.Texcoord);
    if (baseColorDesc.a == 0.f) // 비어있는 픽셀
        discard;
    float3 baseColor = saturate(baseColorDesc.rgb);

    // Normal : DXGI_FORMAT_R16G16B16A16_UNORM
    vector NormalDesc = g_NormalTexture.Sample(LinearWrapSampler, In.Texcoord);
    float3 N = normalize(DecodeNormal(NormalDesc.xyz));

    vector MaterialDesc = g_MaterialTexture.Sample(LinearWrapSampler, In.Texcoord);
    float roughness = ApplySpecAA(N, MaterialDesc.g);   // Specular Anti-Aliasing 적용된 roughness
    float metalness = saturate(MaterialDesc.b);

    // Light
    float4 WorldPos = Reconstruct_WorldPos(In.Texcoord);
    float3 LightVec = g_LightPos.xyz - WorldPos.xyz;
    float Distance = length(LightVec);
    float3 L = (Distance > 0.f) ? (LightVec / Distance) : float3(0.f, 0.f, 0.f);    // 현재 픽셀에서 빛을 향하는 방향
    float3 V = normalize(g_DeferredCamPosition.xyz - WorldPos.xyz);                 // 현재 픽셀에서 카메라를 향하는 방향
    float3 Hraw = V + L;
    float invHLen = rsqrt(max(dot(Hraw, Hraw), EPSILON));
    float3 H = Hraw * invHLen;                                                      // V와 L의 중간 방향. 하프벡터

    float att = saturate((g_LightRange - Distance) / g_LightRange);

    float NdotL = saturate(dot(N, L));  // 표면이 빛을 얼마나 정면으로 받는지
    if (NdotL <= 0.0f)
    {
        Out.Shade = float4(0, 0, 0, 1);
        Out.Specular = float4(0, 0, 0, 1);
        Out.Ambient = float4(0, 0, 0, 1);
        return Out;
    }

    float NdotV = saturate(dot(N, V));  // 표면이 카메라를 얼마나 정면으로 보고 있는지
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));

    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), baseColor, metalness);    // 비금속 반사 ~ 금속 반사
    float3 F = FresnelSchlick(VdotH, F0);                                    // 보는 각도에 따라 반사율이 얼마나 강해지는지
    float D = DistributionGGX(NdotH, roughness);                             // roughness에 따라 하이라이트가 어떻게 펴질지
    float G = GeometrySmith(NdotV, NdotL, roughness);                        // 미세 요철 때문에 반사광이 얼마나 줄어드는지

    // Cook-Torrance Microfacet Specular BRDF
    float3 specularBRDF = (D * G * F) / max(4.0f * NdotV * NdotL, EPSILON);
    // Specular와 Diffuse의 비율
    float3 kS = F;
    float3 kD = (1.0f - kS) * (1.0f - metalness);
    float3 diffuseBRDF = kD * baseColor / PI;

    float3 radiance = g_LightDiffuse.rgb * att;

    Out.Shade = float4(diffuseBRDF * radiance * NdotL, 1.f);
    Out.Specular = float4(specularBRDF * radiance * NdotL, 1.f);
    Out.Ambient = float4(0.f, 0.f, 0.f, 1.f);

    return Out;
}

PS_OUT_HDR_WITH_MASK PS_CombinedHDR(PS_IN In)
{
    PS_OUT_HDR_WITH_MASK Out;

    // Diffuse
    vector Diffuse = g_DiffuseTexture.Sample(LinearWrapSampler, In.Texcoord);
    if (Diffuse.a == 0.f)
        discard;

    // Light
    vector Shade = g_ShadeTexture.Sample(LinearWrapSampler, In.Texcoord);
    vector Specular = g_SpecularTexture.Sample(LinearWrapSampler, In.Texcoord);
    vector Emissive = g_EmissiveTexture.Sample(LinearWrapSampler, In.Texcoord);
    vector NormalDesc = g_NormalTexture.Sample(LinearWrapSampler, In.Texcoord);
    vector MaterialDesc = g_MaterialTexture.Sample(LinearWrapSampler, In.Texcoord);

    float aoRaw = g_SSAOTexture.Sample(LinearClampSampler, In.Texcoord);
    float ao = lerp(1.f, aoRaw, saturate(g_SSAOIntensity));
    ao = pow(saturate(ao), max(g_SSAOPower, EPSILON));

    // Direct BRDF는 Light pass에서 계산 완료.
    vector direct = Shade + Specular;
    vector indirect = float4(0.f, 0.f, 0.f, 0.f);

    // IBL 계산용 표면 정보
    float3 N = normalize(DecodeNormal(NormalDesc.xyz));               // 월드 노멀
    float4 WorldPos = Reconstruct_WorldPos(In.Texcoord);              // 현재 픽셀 월드 위치
    float3 V = normalize(g_DeferredCamPosition.xyz - WorldPos.xyz);   // 현재 픽셀 -> 카메라 방향
    float NdotV = saturate(dot(N, V));                                  // 카메라 정면도

    // PBR 파라미터
    float finalAO = saturate(MaterialDesc.r) * ao;

    float roughness = ApplySpecAA(N, MaterialDesc.g);   // Specular Anti-Aliasing 적용된 roughness
    float metalness = saturate(MaterialDesc.b);
    float3 baseColor = saturate(Diffuse.rgb);

    if (g_IBLEnabled != 0u)
    {
        // Fresnel / 에너지 분배 계수
        float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), baseColor, metalness);
        float3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
        float3 kS = F;
        float3 kD = (1.0f - kS) * (1.0f - metalness);

        // Indirect Lighting
        // Diffuse IBL : irradiance map 기반 확산 간접광
        float3 envDiffuse = g_IrradianceMap.Sample(LinearSampler, N).rgb;
        float3 diffuseIBL = envDiffuse * baseColor * kD;

        // Specular IBL : prefilter map + BRDF LUT(split-sum)
        float3 R = reflect(-V, N);
        float prefilterMip = roughness * g_IBLPrefilterMaxMip;
        float3 envSpecular = g_PrefilterMap.SampleLevel(LinearSampler, R, prefilterMip).rgb;
        float2 envBRDF = g_BRDFLUT.Sample(LinearClampSampler, float2(NdotV, roughness)).rg;
        if (envBRDF.x == 0.f && envBRDF.y == 0.f)
        {
            envBRDF = EnvBRDFApprox(roughness, NdotV);
        }
        float3 specularIBL = envSpecular * (F * envBRDF.x + envBRDF.y);

        // AO는 diffuse IBL에만 적용한다. specular AO는 v1에서 제외.
        float3 indirectDiffuse = diffuseIBL * finalAO * g_IBLDiffuseIntensity;
        float3 indirectSpecular = specularIBL * g_IBLSpecularIntensity;
        indirect.rgb = (indirectDiffuse + indirectSpecular) * g_IBLIntensity;
    }

    Out.Color = direct + indirect;

    // Shadow
    float4 worldPos = WorldPos;
    float viewDepth = Get_ViewZ(In.Texcoord);
    float finalShadow = SampleDynamicShadowAmount(worldPos, viewDepth);;

    if (finalShadow > 0.f)
    {
        float shadowBrightness = lerp(1.f, 0.45f, saturate(g_ShadowIntensity) * finalShadow);
        Out.Color.rgb *= shadowBrightness;
    }

#if SHADOW_DEBUG_MODE == 1
    Out.Color.rgb = finalShadow.xxx;
    Out.Color.a = Diffuse.a;
    Out.Mask = 1.f;
    return Out;
#elif SHADOW_DEBUG_MODE == 2
    uint dynamicCascadeIndex = SelectShadowCascade(viewDepth);
    Out.Color.rgb = GetCascadeDebugColor(dynamicCascadeIndex);
    Out.Color.a = Diffuse.a;
    Out.Mask = 1.f;
    return Out;
#elif SHADOW_DEBUG_MODE == 3
    float selectedCascadeShadowAmount = 0.f;
    uint dynamicCascadeIndex = SelectShadowCascade(viewDepth);
    const bool hasSelectedCascadeCoords = TrySampleCascadeShadowAmount(worldPos, dynamicCascadeIndex, selectedCascadeShadowAmount);
    Out.Color.rgb = hasSelectedCascadeCoords ? float3(0.15f, 0.85f, 0.2f) : float3(0.95f, 0.15f, 0.15f);
    Out.Color.a = Diffuse.a;
    Out.Mask = 1.f;
    return Out;
#endif
// # else
    Out.Color.rgb += Emissive.rgb;
    Out.Color.a = Diffuse.a;
    Out.Mask = 1.f;

    return Out;
}

PS_OUT_HDR_WITH_MASK PS_MAIN_COMBINED_SIMPLE(PS_IN In)
{
    PS_OUT_HDR_WITH_MASK Out;

    vector Diffuse = g_DiffuseTexture.Sample(LinearWrapSampler, In.Texcoord);
    if (Diffuse.a == 0.f)
        discard;
    vector Shade = g_ShadeTexture.Sample(LinearWrapSampler, In.Texcoord);
    vector Ambient = g_AmbientTexture.Sample(LinearWrapSampler, In.Texcoord);
    vector Specular = g_SpecularTexture.Sample(LinearWrapSampler, In.Texcoord);

    // Simple pass still shows the full lit result, but intentionally skips SSAO/emissive.
    Out.Color = Diffuse * (Shade + Ambient) + Specular;
    Out.Mask = 1.f;

    return Out;
}

PS_OUT_HDR_WITH_MASK PS_MAIN_COMBINED_PREVIEW(PS_IN In)
{
    PS_OUT_HDR_WITH_MASK Out;

    vector Diffuse = g_DiffuseTexture.Sample(LinearWrapSampler, In.Texcoord);
    if (Diffuse.a == 0.f)
        discard;

    vector Shade = g_ShadeTexture.Sample(LinearSampler, In.Texcoord);
    vector Ambient = g_AmbientTexture.Sample(LinearSampler, In.Texcoord);
    vector Specular = g_SpecularTexture.Sample(LinearWrapSampler, In.Texcoord);
    vector Emissive = g_EmissiveTexture.Sample(LinearWrapSampler, In.Texcoord);

    Out.Color = Diffuse * (Shade + Ambient) + Specular;     // 프리뷰는 shadow/SSAO 없이 direct+ambient 결과를 본다.
    Out.Color.rgb += Emissive.rgb;              // emissive는 그대로 더하기
    Out.Color.a = Diffuse.a;
    Out.Mask = 1.f;

    return Out;
}

PS_OUT_BACKBUFFER PS_SSAO(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;
    Out.Color = Compute_DepthSSAO(In.Texcoord);
    return Out;
}

PS_OUT_BACKBUFFER PS_SSAOBlur(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;
    Out.Color = Compute_SSAOBilateralBlur(In.Texcoord);
    return Out;
}

float4 PS_Fog(PS_IN In) : SV_Target
{
    float3 camPos = g_DeferredCamPosition.xyz;
    float depthVal = g_DepthTexture.Sample(LinearClampSampler, In.Texcoord).r;

    float2 ndc = float2(In.Texcoord.x * 2.f - 1.f, 1.f - In.Texcoord.y * 2.f);
    float4 viewPos = mul(float4(ndc, depthVal, 1.f), g_DeferredProjMatrixInverse);
    viewPos /= viewPos.w;
    float3 worldPos = mul(viewPos, g_DeferredViewMatrixInverse).xyz;

    float3 rayVec = worldPos - camPos;
    float3 rayDir = normalize(rayVec);
    float geoLen = length(rayVec);

    float noGeometry = max(step(0.9999f, depthVal), step(depthVal, 0.0001f));
    float rayLen = lerp(geoLen, g_DeferredFar, noGeometry);

    float3 fogColorOut = float3(0, 0, 0);
    float fogFactorOut = 0.f;

    [loop]
    for (int i = 0; i < g_FogAABBCount; ++i)
    {
        FogZoneEntry zone = g_FogZones[i];
        float3 invD = rcp(rayDir);
        float3 t0 = (zone.zoneMin - camPos) * invD;
        float3 t1 = (zone.zoneMax - camPos) * invD;
        float tNear = max(max(min(t0.x, t1.x), min(t0.y, t1.y)), min(t0.z, t1.z));
        float tFar = min(min(max(t0.x, t1.x), max(t0.y, t1.y)), max(t0.z, t1.z));
        if (tFar <= max(tNear, 0.f))
            continue;
        
        float pathLen = max(0.f, min(tFar, rayLen) - max(tNear, zone.fogStart));
        float factor = saturate(1.f - exp(-zone.fogDensity * pathLen)) * zone.fogColor.a;
        fogColorOut = lerp(fogColorOut, zone.fogColor.rgb, factor);
        fogFactorOut = saturate(fogFactorOut + factor);
    }

    [loop]
    for (int j = g_FogAABBCount; j < g_FogAABBCount + g_FogSphereCount; ++j)
    {
        FogZoneEntry zone = g_FogZones[j];
        float3 oc = camPos - zone.zoneMin;
        float b = dot(oc, rayDir);
        float c = dot(oc, oc) - zone.zoneRadius * zone.zoneRadius;
        float disc = b * b - c;
        if (disc < 0.f)
            continue;
        
        float sq = sqrt(disc);
        float tNear = -b - sq;
        float tFar = -b + sq;
        if (tFar <= max(tNear, 0.f))
            continue;
        
        float pathLen = max(0.f, min(tFar, rayLen) - max(tNear, zone.fogStart));
        float factor = saturate(1.f - exp(-zone.fogDensity * pathLen)) * zone.fogColor.a;
        fogColorOut = lerp(fogColorOut, zone.fogColor.rgb, factor);
        fogFactorOut = saturate(fogFactorOut + factor);
    }

    return float4(fogColorOut, fogFactorOut);
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(Debug,             RS_Default, DSS_Default,    BS_Default, VS_MAIN,    PS_MAIN_DEBUG)              // 0
    PASS_RS_DS_BS_VP(Directional,       RS_Default, DSS_None,       BS_Blend,   VS_MAIN,    PS_MAIN_DIRECTIONAL)        // 1
    PASS_RS_DS_BS_VP(Point,             RS_Default, DSS_None,       BS_Blend,   VS_MAIN,    PS_MAIN_POINT)              // 2
    PASS_RS_DS_BS_VP(Combined,          RS_Default, DSS_None,       BS_Default, VS_MAIN,    PS_CombinedHDR)             // 3
    PASS_RS_DS_BS_VP(CombinedSimple,    RS_Default, DSS_None,       BS_Default, VS_MAIN,    PS_MAIN_COMBINED_SIMPLE)    // 4
    PASS_RS_DS_BS_VP(CombinedPreview,   RS_Default, DSS_None,       BS_Default, VS_MAIN,    PS_MAIN_COMBINED_PREVIEW)   // 5
    PASS_RS_DS_BS_VP(SSAO,              RS_Default, DSS_None,       BS_Default, VS_MAIN,    PS_SSAO)                    // 6
    PASS_RS_DS_BS_VP(SSAOBlur,          RS_Default, DSS_None,       BS_Default, VS_MAIN,    PS_SSAOBlur)                // 7
    PASS_RS_DS_BS_VP(Fog, RS_Default,   DSS_None,                   BS_AlphaBlend, VS_MAIN, PS_Fog)                     // 8
}
