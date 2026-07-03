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

    float pointLightWrap = saturate(NdotL * 0.5f + 0.5f);
    Out.Shade = float4(diffuseBRDF * radiance * pointLightWrap, 1.f);
    Out.Specular = float4(specularBRDF * radiance * pointLightWrap, 1.f);

    Out.Shade = float4(diffuseBRDF * radiance * NdotL, 1.f);
    Out.Specular = float4(specularBRDF * radiance * NdotL, 1.f);
    Out.Ambient = float4(0.f, 0.f, 0.f, 1.f);

    return Out;
}

// 점조명의 표면 조명 패스. 현재 픽셀이 점조명에 얼마나 밝아지는지 계산한다.
PS_OUT_LIGHT PS_MAIN_POINT(PS_IN In)
{
    // MRT::LightAcc의 Shade/Specular에 additive blend로 누적된다.
    PS_OUT_LIGHT Out;
    Out.Shade = float4(0.f, 0.f, 0.f, 1.f);
    Out.Specular = float4(0.f, 0.f, 0.f, 1.f);
    Out.Ambient = float4(0.f, 0.f, 0.f, 1.f);

    // range가 없으면 영향 반경이 없는 조명이므로 누적할 값 없이 종료.
    if (g_LightRange <= EPSILON)
        return Out;

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

    // Depth에서 복원한 월드 좌표 기준으로, 픽셀 -> 점조명 방향과 거리를 구한다.
    // 이 패스는 화면 사각형을 전체에 그리므로 실제 반경 밖 픽셀은 attenuation으로 0에 가까워진다.
    float4 WorldPos = Reconstruct_WorldPos(In.Texcoord);
    float3 LightVec = g_LightPos.xyz - WorldPos.xyz;
    float Distance = length(LightVec);
    float3 L = (Distance > 0.f) ? (LightVec / Distance) : float3(0.f, 0.f, 0.f);    // 현재 픽셀에서 빛을 향하는 방향
    float3 V = normalize(g_DeferredCamPosition.xyz - WorldPos.xyz);                 // 현재 픽셀에서 카메라를 향하는 방향
    float3 Hraw = V + L;
    float invHLen = rsqrt(max(dot(Hraw, Hraw), EPSILON));
    float3 H = Hraw * invHLen;                                                      // V와 L의 중간 방향. 하프벡터

    // range 기준 감쇠. power가 커질수록 조명 중심부는 남고 가장자리는 더 빨리 죽는다.
    float linearAtt = saturate(1.f - Distance / g_LightRange);
    float att = pow(linearAtt, max(g_PointLightAttenuationPower, EPSILON));

    float NdotL = saturate(dot(N, L));  // 표면이 빛을 얼마나 정면으로 받는지
    if (NdotL <= 0.0f)
    {
        // 뒷면은 표면 조명만 끄고, discard하지 않는다.
        // LightAcc RT에는 검은 값이 더해져 다른 조명 누적 결과를 해치지 않는다.
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

    // diffuse.a를 점조명 개별 alpha/intensity로 사용한다.
    float lightAlpha = saturate(g_LightDiffuse.a);
    if (lightAlpha <= EPSILON)
    {
        return Out;
    }

    // 조명 색/세기에 거리 감쇠와 개별 alpha를 곱해 이 픽셀에 도달한 광량으로 사용한다.
    float3 radiance = g_LightDiffuse.rgb * att * lightAlpha;

    // 완전히 옆면으로 꺼지는 것을 막는 아트 보정.
    // 물리 기반 NdotL보다 더 넓게 점조명 영향이 보이게 만든다.
    NdotL = max(NdotL, 0.25f);

    // CombinedHDR에서 g_ShadeTexture/g_SpecularTexture로 다시 읽혀 최종 HDR 색에 합쳐진다.
    // Ambient는 현재 점조명에서 사용하지 않으므로 0 유지.
    Out.Shade = float4(diffuseBRDF * radiance * NdotL, 1.f);
    Out.Specular = float4(specularBRDF * radiance * NdotL, 1.f);
    Out.Ambient = float4(0.f, 0.f, 0.f, 1.f);

    return Out;
}

PS_OUT_BACKBUFFER PS_POINT_GLOW_SOURCE(PS_IN In)
{
    // 점조명의 "보이는 광륜" 소스 패스.
    // 표면을 밝히는 PS_MAIN_POINT와 별개로, 점조명 위치에 고정된 얕은 screen-space halo를 만들어
    // RT::PointGlow에 glow 색을 쌓고 이후 PostProcess의 PointGlowCompositeHDR에서 HDRColor에 합성한다.
    PS_OUT_BACKBUFFER Out;
    Out.Color = float4(0.f, 0.f, 0.f, 0.f);

    // 반경 또는 alpha가 없으면 광륜도 만들 수 없으므로 종료.
    float lightAlpha = saturate(g_LightDiffuse.a);
    if (g_LightRange <= EPSILON || lightAlpha <= EPSILON)
        return Out;

    // glow 전용 아트 튜닝 값.
    // 표면 조명 밝기와는 별개라서, "공중에 보이는 빛 덩어리"만 조절하고 싶을 때는 여기 계열을 만진다.
    static const float POINT_GLOW_INTENSITY = 1.4f;
    static const float POINT_GLOW_RADIUS_SCALE = 0.45f;
    static const float POINT_GLOW_MIN_RADIUS_PIXELS = 6.f;
    static const float POINT_GLOW_EDGE_POWER = 2.2f;

    // 조명 중심을 화면에 투영해 대략적인 화면 반경과 화면 밖 컬링을 계산한다.
    float4 lightViewPos = mul(float4(g_LightPos.xyz, 1.f), g_ViewMatrix);

    // 카메라 뒤쪽에 완전히 있는 점조명만 제거
    if (lightViewPos.z <= EPSILON)
        return Out;

    float4 lightClipPos = mul(lightViewPos, g_ProjMatrix);
    if (abs(lightClipPos.w) <= EPSILON)
        return Out;

    float2 lightNDC = lightClipPos.xy / lightClipPos.w;
    float2 lightUV = float2(
        lightNDC.x * 0.5f + 0.5f,
       -lightNDC.y * 0.5f + 0.5f
    );

    float lightDepth = max(lightViewPos.z, 1.f);

    // 월드 반경을 현재 깊이 기준의 픽셀 반경으로 환산한다.
    // 가까운 조명은 크게, 먼 조명은 작게 보이지만 최소 픽셀 반경은 보장한다.
    float radiusPixels = (g_LightRange * POINT_GLOW_RADIUS_SCALE / lightDepth) * g_ProjMatrix._22 * g_ScreenSize.y * 0.5f;

    radiusPixels = clamp(
        radiusPixels,
        POINT_GLOW_MIN_RADIUS_PIXELS,
        max(g_ScreenSize.x, g_ScreenSize.y) * 1.25f
    );

    // 중심이 화면 밖이어도 원이 화면에 걸치면 그릴 수 있게 확장 판정
    float2 radiusUV = radiusPixels / g_ScreenSize;
    if (lightUV.x < -radiusUV.x || lightUV.x > 1.f + radiusUV.x ||
        lightUV.y < -radiusUV.y || lightUV.y > 1.f + radiusUV.y)
    {
        return Out;
    }

    // 기존 Ray-Sphere 방식은 카메라 각도와 foreground geometry 클리핑에 따라
    // 광륜 중심이 흔들리고 먼 거리에서 큰 구체 core가 보였다.
    // 여기서는 조명 투영 위치를 기준으로 한 얕은 screen-space halo만 만든다.
    float2 pixelDelta = (In.Texcoord - lightUV) * g_ScreenSize;
    float radial = length(pixelDelta) / max(radiusPixels, EPSILON);
    if (radial >= 1.f)
        return Out;

    // 실제 씬 geometry가 조명보다 앞에 있으면 halo를 줄인다.
    // 불꽃/광원 뒤의 컵, 벽 같은 foreground에 큰 원형 glow가 덮이는 것을 막는다.
    float sceneViewDepth01 = g_DepthTexture.Sample(PointClampSampler, In.Texcoord).y;
    float occlusionFade = 1.f;
    if (sceneViewDepth01 > EPSILON)
    {
        float3 sceneWorldPos = Reconstruct_WorldPos(In.Texcoord).xyz;
        float sceneDistance = length(sceneWorldPos - g_DeferredCamPosition.xyz);
        float lightDistance = length(g_LightPos.xyz - g_DeferredCamPosition.xyz);
        float depthMargin = max(g_LightRange * 0.08f, 0.05f);
        occlusionFade = smoothstep(-depthMargin, depthMargin, sceneDistance - lightDistance);
    }

    float falloff = pow(saturate(1.f - radial), POINT_GLOW_EDGE_POWER);
    float distanceFade = saturate(radiusPixels / 64.f);
    float glow = falloff * occlusionFade * distanceFade;

    if (glow <= EPSILON)
        return Out;

    // 합성 패스에서 depth 비교에 쓸 조명 중심 깊이.
    float lightDepth01 = saturate(lightDepth / max(g_DeferredFar, EPSILON));

    // RGB: 부드러운 halo 색
    // A: 합성 단계에서 depth compare할 light depth
    Out.Color = float4(
        g_LightDiffuse.rgb * glow * POINT_GLOW_INTENSITY * lightAlpha,
        lightDepth01
    );

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

    float ambientVisibility = g_AmbientVisibilityTexture.Sample(PointClampSampler, In.Texcoord).r;

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

    indirect.rgb *= ambientVisibility;
    Out.Color = direct + indirect;

    // Shadow
    float4 worldPos = WorldPos;
    float viewDepth = Get_ViewZ(In.Texcoord);
    float finalShadow = max(SampleDynamicShadowAmount(worldPos, viewDepth),
                            SampleCachedInstancedShadowAmount(worldPos));

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

PS_OUT_BACKBUFFER PS_FroxelDebug(PS_IN In)
{
    PS_OUT_BACKBUFFER Out;

    if (g_FroxelDebugWidth == 0u || g_FroxelDebugHeight == 0u || g_FroxelDebugDepth == 0u)
    {
        Out.Color = float4(1.f, 0.f, 0.f, 1.f);
        return Out;
    }

    const uint sliceIndex = min(g_FroxelDebugSlice, g_FroxelDebugDepth - 1u);
    const float sliceW = (sliceIndex + 0.5f) / max((float)g_FroxelDebugDepth, 1.f);
    const float3 froxelUVW = float3(saturate(In.Texcoord), sliceW);
    const float4 froxelSample = g_FroxelTexture.Sample(PointClampSampler, froxelUVW);

    float3 value = froxelSample.rgb;
    if (g_FroxelDebugMode == 1u)
    {
        const float rawEnergy = max(max(value.r, value.g), value.b);
        const float rawMask = max(rawEnergy * max(g_FroxelDebugGain, 1.f), froxelSample.a * 4.f);
        value = float3(rawMask, rawMask * 0.9f, rawMask * 0.6f);
    }
    else
    {
        value = saturate(value * max(g_FroxelDebugGain, 1.f));
    }

    Out.Color = float4(saturate(value), 1.f);

    return Out;
}

float PS_AmbientVisibilityResolve(PS_IN In) : SV_Target
{
    float sceneDepth = g_DepthTexture.Sample(PointClampSampler, In.Texcoord).y;
    float frontDepth = g_AmbientVisibilityFrontDepthTexture.Sample(PointClampSampler, In.Texcoord).r;
    float backDepth = g_AmbientVisibilityBackDepthTexture.Sample(PointClampSampler, In.Texcoord).r;
    float amount = g_AmbientVisibilityAmountTexture.Sample(PointClampSampler, In.Texcoord).r;

    static const float AMBIENT_VISIBILITY_DEPTH_FADE = 0.01f;
    float entryDepth = (frontDepth >= 0.999f && backDepth > 0.0001f) ? 0.f : frontDepth;
    float validInterval = step(entryDepth + 0.0001f, backDepth);
    float enterFade = saturate((sceneDepth - entryDepth) / AMBIENT_VISIBILITY_DEPTH_FADE);
    float exitFade = saturate((backDepth - sceneDepth) / AMBIENT_VISIBILITY_DEPTH_FADE);
    float intervalFade = min(enterFade, exitFade) * validInterval;

    return lerp(1.f, amount, intervalFade);
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
        FogZoneEntry zone = g_FogAABBZones[i];
        float zoneFogFactor = ComputeGroundFogAABB(zone, camPos, rayDir, rayLen, safeRayDir);
        AccumulateGroundFog(fogColorOut, fogFactorOut, zone.fogColor.rgb, zoneFogFactor);
    }

    [loop]
    for (int obbIndex = 0; obbIndex < g_FogOBBCount; ++obbIndex)
    {
        FogOBBZoneEntry zone = g_FogOBBZones[obbIndex];
        float zoneFogFactor = ComputeGroundFogOBB(zone, camPos, rayDir, rayLen);
        AccumulateGroundFog(fogColorOut, fogFactorOut, zone.fogColor.rgb, zoneFogFactor);
    }

    fogFactorOut = saturate(fogFactorOut);

    float3 straightFogColor = fogFactorOut > EPSILON
        ? fogColorOut / fogFactorOut
        : float3(0.f, 0.f, 0.f);

    return float4(straightFogColor, fogFactorOut);
    return float4(0.65f, 0.65f, 0.65f, 0.2f);
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
    PASS_RS_DS_BS_VP(Fog,               RS_Default, DSS_None,       BS_AlphaBlend, VS_MAIN,        PS_Fog)             // 8
    PASS_RS_DS_BS_VP(FroxelDebug,       RS_Default, DSS_None,       BS_Default, VS_MAIN,    PS_FroxelDebug)     // 9
    PASS_RS_DS_BS_VP(AmbientVisibilityResolve, RS_Default, DSS_None, BS_AmbientVisibilityMin, VS_MAIN, PS_AmbientVisibilityResolve) // 10
    PASS_RS_DS_BS_VP(PointGlowSource,   RS_Default, DSS_None,       BS_Blend,   VS_MAIN,    PS_POINT_GLOW_SOURCE)       // 11
}
