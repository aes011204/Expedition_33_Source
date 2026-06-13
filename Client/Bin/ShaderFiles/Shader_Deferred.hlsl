#include "Engine_Shader_Deferred.hlsl"

#define IBL_DUMMY_TEST 0

Texture2D g_FogNoiseTexture;
float g_fDT = 0.f;

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

// ===================================================================

// 카메라 ~ 현재 픽셀까지의 거리를 받아서 안개의 세기를 반환한다.
float ComputeGlobalDistanceFog(float distanceToSurface)
{
    const float g_GlobalFogStart =3.f;
    const float g_GlobalFogDensity = 0.003f;
    const float g_GlobalFogMaxDensity = 0.8f;

    float fogDistance = max(distanceToSurface - g_GlobalFogStart, 0.f);
    return saturate(1.f - exp(-g_GlobalFogDensity * fogDistance));
}

// 지면 높이 안개
float ComputeGroundFog(FogZoneEntry zone, float3 camPos, float3 rayDir, float rayLen, float3 safeRayDir)
{
    float3 invD = rcp(safeRayDir);
    float3 t0 = (zone.zoneMin - camPos) * invD;
    float3 t1 = (zone.zoneMax - camPos) * invD;

    float3 tMin = min(t0, t1);
    float3 tMax = max(t0, t1);

    float tNear = max(max(tMin.x, tMin.y), tMin.z);
    float tFar = min(min(tMax.x, tMax.y), tMax.z);

    // Fog box의 진입/출 지점
    float segmentStart = max(tNear, 0.f);
    float segmentEnd = min(tFar, min(rayLen, g_GroundFogMaxDistance)); // 안개 최대 거리

    if (segmentEnd <= segmentStart)
        return 0.f;

    float segmentLength = segmentEnd - segmentStart;

    // Fog box의 25%, 50%, 75% 세 위치만 샘플링한다. (비용 쌈)
    float t25 = lerp(segmentStart, segmentEnd, 0.25f);
    float t50 = lerp(segmentStart, segmentEnd, 0.50f);
    float t75 = lerp(segmentStart, segmentEnd, 0.75f);

    float3 samplePos25 = camPos + rayDir * t25;
    float3 samplePos50 = camPos + rayDir * t50;
    float3 samplePos75 = camPos + rayDir * t75;

    // 안개 표현할 높이 구간 구하기
    float baseHeightWS = zone.zoneMin.y + zone.baseHeight;
    float topFadeStartWS = zone.zoneMin.y + zone.topFadeStart;
    float topHeightWS = zone.zoneMin.y + zone.topHeight;

    baseHeightWS = max(baseHeightWS, zone.zoneMin.y + EPSILON);
    topFadeStartWS = max(topFadeStartWS, baseHeightWS);
    topHeightWS = max(topHeightWS, topFadeStartWS + EPSILON);

    // Fog box 전체 구간이 아니라, 실제 안개가 존재할 수 있는 높이 슬랩과의 교차 구간을 다시 구한다.
    if (abs(rayDir.y) > EPSILON)
    {
        float tHeight0 = (zone.zoneMin.y - camPos.y) / rayDir.y;
        float tHeight1 = (topHeightWS - camPos.y) / rayDir.y;

        float tHeightNear = min(tHeight0, tHeight1);
        float tHeightFar = max(tHeight0, tHeight1);

        segmentStart = max(segmentStart, tHeightNear);
        segmentEnd = min(segmentEnd, tHeightFar);
    }
    else
    {
        if (camPos.y < zone.zoneMin.y || camPos.y > topHeightWS)
            return 0.f;
    }

    if (segmentEnd <= segmentStart)
        return 0.f;

    segmentLength = segmentEnd - segmentStart;

    // 실제 안개가 존재하는 구간의 25%, 50%, 75% 세 위치만 다시 샘플링한다. (비용 쌈)
    t25 = lerp(segmentStart, segmentEnd, 0.25f);
    t50 = lerp(segmentStart, segmentEnd, 0.50f);
    t75 = lerp(segmentStart, segmentEnd, 0.75f);

    samplePos25 = camPos + rayDir * t25;
    samplePos50 = camPos + rayDir * t50;
    samplePos75 = camPos + rayDir * t75;

    float bottomFade25 = smoothstep(zone.zoneMin.y, baseHeightWS, samplePos25.y);
    float bottomFade50 = smoothstep(zone.zoneMin.y, baseHeightWS, samplePos50.y);
    float bottomFade75 = smoothstep(zone.zoneMin.y, baseHeightWS, samplePos75.y);

    float topFade25 = 1.f - smoothstep(topFadeStartWS, topHeightWS, samplePos25.y);
    float topFade50 = 1.f - smoothstep(topFadeStartWS, topHeightWS, samplePos50.y);
    float topFade75 = 1.f - smoothstep(topFadeStartWS, topHeightWS, samplePos75.y);

    float2 distanceToEdge25 = min(samplePos25.xz - zone.zoneMin.xz, zone.zoneMax.xz - samplePos25.xz);
    float2 distanceToEdge50 = min(samplePos50.xz - zone.zoneMin.xz, zone.zoneMax.xz - samplePos50.xz);
    float2 distanceToEdge75 = min(samplePos75.xz - zone.zoneMin.xz, zone.zoneMax.xz - samplePos75.xz);

    float nearestEdgeDistance25 = min(distanceToEdge25.x, distanceToEdge25.y);
    float nearestEdgeDistance50 = min(distanceToEdge50.x, distanceToEdge50.y);
    float nearestEdgeDistance75 = min(distanceToEdge75.x, distanceToEdge75.y);

    float edgeFadeDistance = max(zone.edgeFade, EPSILON);
    float edgeMask25 = smoothstep(0.f, edgeFadeDistance, nearestEdgeDistance25);
    float edgeMask50 = smoothstep(0.f, edgeFadeDistance, nearestEdgeDistance50);
    float edgeMask75 = smoothstep(0.f, edgeFadeDistance, nearestEdgeDistance75);

    float heightDensity25 = bottomFade25 * topFade25 * edgeMask25;
    float heightDensity50 = bottomFade50 * topFade50 * edgeMask50;
    float heightDensity75 = bottomFade75 * topFade75 * edgeMask75;

    float averageHeightDensity = (heightDensity25 + heightDensity50 + heightDensity75) / 3.f;

    if (averageHeightDensity <= EPSILON)
        return 0.f;

    // 움직이는 noise 표현. 두 번 샘플링해서 패턴 줄이기.
    float2 noiseOffset = g_fDT * zone.noiseSpeed;
    float2 softNoiseOffset = noiseOffset * 0.43f + float2(1.37f, -2.11f);
    const float2x2 softNoiseRotation = float2x2(0.866f, -0.5f, 0.5f, 0.866f);

    float2 noiseUV25A = samplePos25.xz * zone.noiseScale + noiseOffset;
    float2 noiseUV50A = samplePos50.xz * zone.noiseScale + noiseOffset;
    float2 noiseUV75A = samplePos75.xz * zone.noiseScale + noiseOffset;

    float2 noiseUV25B = samplePos25.xz * (zone.noiseScale * 2.17f) - noiseOffset * 0.47f;
    float2 noiseUV50B = samplePos50.xz * (zone.noiseScale * 2.17f) - noiseOffset * 0.47f;
    float2 noiseUV75B = samplePos75.xz * (zone.noiseScale * 2.17f) - noiseOffset * 0.47f;

    // 같은 XZ 평면 노이즈라도 저주파 레이어를 더 섞어서 판이 겹친 결보다 덩어리감이 먼저 보이게 만든다.
    float2 noiseUV25C = mul(samplePos25.xz * (zone.noiseScale * 0.53f) + softNoiseOffset, softNoiseRotation);
    float2 noiseUV50C = mul(samplePos50.xz * (zone.noiseScale * 0.53f) + softNoiseOffset, softNoiseRotation);
    float2 noiseUV75C = mul(samplePos75.xz * (zone.noiseScale * 0.53f) + softNoiseOffset, softNoiseRotation);

    float2 noiseUV25D = samplePos25.xz * (zone.noiseScale * 0.91f) - softNoiseOffset * 0.31f + float2(-3.1f, 2.7f);
    float2 noiseUV50D = samplePos50.xz * (zone.noiseScale * 0.91f) - softNoiseOffset * 0.31f + float2(-3.1f, 2.7f);
    float2 noiseUV75D = samplePos75.xz * (zone.noiseScale * 0.91f) - softNoiseOffset * 0.31f + float2(-3.1f, 2.7f);

    float noise25A = g_FogNoiseTexture.SampleLevel(LinearWrapSampler, noiseUV25A, 0.f).r;
    float noise50A = g_FogNoiseTexture.SampleLevel(LinearWrapSampler, noiseUV50A, 0.f).r;
    float noise75A = g_FogNoiseTexture.SampleLevel(LinearWrapSampler, noiseUV75A, 0.f).r;

    float noise25B = g_FogNoiseTexture.SampleLevel(LinearWrapSampler, noiseUV25B, 0.f).r;
    float noise50B = g_FogNoiseTexture.SampleLevel(LinearWrapSampler, noiseUV50B, 0.f).r;
    float noise75B = g_FogNoiseTexture.SampleLevel(LinearWrapSampler, noiseUV75B, 0.f).r;

    float noise25C = g_FogNoiseTexture.SampleLevel(LinearWrapSampler, noiseUV25C, 0.f).r;
    float noise50C = g_FogNoiseTexture.SampleLevel(LinearWrapSampler, noiseUV50C, 0.f).r;
    float noise75C = g_FogNoiseTexture.SampleLevel(LinearWrapSampler, noiseUV75C, 0.f).r;

    float noise25D = g_FogNoiseTexture.SampleLevel(LinearWrapSampler, noiseUV25D, 0.f).r;
    float noise50D = g_FogNoiseTexture.SampleLevel(LinearWrapSampler, noiseUV50D, 0.f).r;
    float noise75D = g_FogNoiseTexture.SampleLevel(LinearWrapSampler, noiseUV75D, 0.f).r;

    float noise25AB = noise25A * 0.7f + noise25B * 0.3f;
    float noise50AB = noise50A * 0.7f + noise50B * 0.3f;
    float noise75AB = noise75A * 0.7f + noise75B * 0.3f;

    float noise25CD = noise25C * 0.62f + noise25D * 0.38f;
    float noise50CD = noise50C * 0.62f + noise50D * 0.38f;
    float noise75CD = noise75C * 0.62f + noise75D * 0.38f;

    float noise25 = noise25AB * 0.5f + noise25CD * 0.5f;
    float noise50 = noise50AB * 0.5f + noise50CD * 0.5f;
    float noise75 = noise75AB * 0.55f + noise75CD * 0.5f;

    float noise = (noise25 + noise50 + noise75) / 3.f;
    noise = lerp(noise, 0.5f + (noise - 0.5f) * 0.55f, 0.45f);
    noise = smoothstep(0.25f, 0.75f, noise);

    float softenedNoiseStrength = saturate(zone.noiseStrength * 0.78f);
    noise = lerp(1.f, max(noise, zone.noiseMin), softenedNoiseStrength);

    // Fog box를 오래 통과할수록 안개가 진해져야 한다. (안개의 누적 깊이?)
    float accumulationDistance = lerp(
        g_GroundFogMinAccumulationDistance, // 카메라 기준 안개가 쌓인 거리 보장
        segmentLength,
        g_GroundFogDistanceInfluence // 위 거리를 안개 누적에 얼마나 보장할 것인지
    );

    float opticalDepth = zone.fogDensity * averageHeightDensity * noise * accumulationDistance;
    float groundFogFactor = saturate(1.f - exp(-opticalDepth));

    float nearFade = smoothstep(0.f, g_GroundFogNearFadeDistance, rayLen); // 이 거리까지 근거리 안개를 연하게 처리
    nearFade = lerp(g_GroundFogNearMinFactor, 1.f, nearFade); // 카메라 바로 앞에서 유지할 안개 세기

    return groundFogFactor * nearFade;
}

float4 PS_Fog(PS_IN In) : SV_Target
{
    float3 camPos = g_DeferredCamPosition.xyz;
    float3 worldPos = Reconstruct_WorldPos(In.Texcoord).xyz;
    float3 rayVec = worldPos - camPos;

    // 카메라 ray
    float geoLen = length(rayVec);
    if (geoLen <= EPSILON)
        return float4(0.f, 0.f, 0.f, 0.f);
    float3 rayDir = rayVec / geoLen;

    // 현재 픽셀에 물체가 없는지 판단
    float depthVal = g_DepthTexture.Sample(PointClampSampler, In.Texcoord).r;
    float noGeometry = step(depthVal, EPSILON);     // 현재 픽셀에 물체 없음
    float rayLen = lerp(geoLen, g_DeferredFar, noGeometry);

    // Global Distance Fog
    float globalFogFactor = ComputeGlobalDistanceFog(rayLen);
    float3 globalFogColor = float3(0.65f, 0.68f, 0.65f);

    float3 fogColorOut = globalFogColor * globalFogFactor;
    float fogFactorOut = globalFogFactor;

    float3 safeRayDir = float3(
        abs(rayDir.x) > EPSILON ? rayDir.x : (rayDir.x >= 0.f ? EPSILON : -EPSILON),
        abs(rayDir.y) > EPSILON ? rayDir.y : (rayDir.y >= 0.f ? EPSILON : -EPSILON),
        abs(rayDir.z) > EPSILON ? rayDir.z : (rayDir.z >= 0.f ? EPSILON : -EPSILON)
    );

    // 현재 화면 픽셀의 광선이 그 박스를 통과하는지 계산
    [loop]
    for (int i = 0; i < g_FogAABBCount; ++i)
    {
        FogZoneEntry zone = g_FogZones[i];
        float zoneFogFactor = ComputeGroundFog(zone, camPos, rayDir, rayLen, safeRayDir);
        if (zoneFogFactor <= EPSILON)
            continue;

        float remaining = 1.f - fogFactorOut;
        float contribution = remaining * zoneFogFactor;

        fogColorOut += zone.fogColor.rgb * contribution;
        fogFactorOut += contribution;
    }

    fogFactorOut = saturate(fogFactorOut);

    float3 straightFogColor = fogFactorOut > EPSILON
        ? fogColorOut / fogFactorOut
        : float3(0.f, 0.f, 0.f);

    return float4(straightFogColor, fogFactorOut);
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
