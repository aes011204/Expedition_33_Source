#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_EffectMaterial.hlsl"

cbuffer cbEffectRibbonMaterial : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectRibbonParams = { 1.f, 1.f, 0.f, 0.f };       // x: authored energy scalar, y: opacityPower, z: noiseStrength, w: useNoise
    float4 g_EffectRibbonAlphaParams = { 0.f, 0.f, 0.f, 0.f };  // x: alphaCutoff, y: alphaErosion, z: useMask, w: reserved
    float4 g_EffectRibbonMainUVParams = { 1.f, 1.f, 0.f, 0.f }; // xy: scale, zw: scroll speed
    float4 g_EffectRibbonNoiseUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectRibbonMaskUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectRibbonUVOffsetParams = { 0.f, 0.f, 0.f, 0.f }; // xy: main offset, zw: noise offset
    float4 g_EffectRibbonMaskUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectRibbonUVModeParams = { 0.f, 0.f, 0.f, 0.f };   // x: main, y: noise, z: mask, w: reserved
    float4 g_EffectRibbonSourceParams = { 1.f, 0.f, 0.f, 0.f };   // x: noise source, y: mask source, z: noise invert, w: mask invert
    float4 g_EffectRibbonAdditiveParams = { 0.f, 0.f, 0.f, 1.f }; // x: colorSource, y: amountSource, z: coveragePolicy, w: intensityScale
    float4 g_EffectRibbonAdditiveEmissiveColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectRibbonAdditiveConstantColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectRibbonAdditiveFlags = { 1.f, 0.f, 0.f, 0.f };
    float4 g_EffectRibbonCoreEmissiveParams = { 0.f, 4.f, 2.f, 1.f };    // x: enabled, y: corePower, z: coreIntensity, w: outerPower
    float4 g_EffectRibbonCoreEmissiveColor = { 1.f, 0.85f, 0.45f, 1.f }; // rgb: coreColor, a: outerIntensity
    float4 g_EffectRibbonAxisParams = { 0.f, 1.f, 0.f, 0.f };            // x: renderAxis, y: visibleLength, z: tilingDistance, w: smooth start tangent
    float4 g_EffectRibbonFallbackAxis = { 1.f, 0.f, 0.f, 0.f };
    float4 g_EffectMaterialScalarModulationParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectMaterialScalarModulationMeta[8];
    float4 g_EffectMaterialScalarModulationKeyTimes[8];
    float4 g_EffectMaterialScalarModulationKeyTimesBlock1[8];
    float4 g_EffectMaterialScalarModulationKeyValues[8];
    float4 g_EffectMaterialScalarModulationKeyValuesBlock1[8];
    float4 g_EffectMaterialScalarModulationKeyArriveTangents[8];
    float4 g_EffectMaterialScalarModulationKeyArriveTangentsBlock1[8];
    float4 g_EffectMaterialScalarModulationKeyLeaveTangents[8];
    float4 g_EffectMaterialScalarModulationKeyLeaveTangentsBlock1[8];
    float4 g_EffectMaterialScalarModulationKeyModes[8];
    float4 g_EffectMaterialScalarModulationKeyModesBlock1[8];
    int g_OpacitySource = 0;
    float g_EffectRibbonMaterialTime = 0.f;
    float2 g_EffectRibbonPadding;
};

#include "Shader_EffectMaterialScalarModulation.hlsli"

Texture2D g_MainTexture : register(t0);
Texture2D g_NoiseTexture : register(t1);
Texture2D g_MaskTexture : register(t2);

static const int RIBBON_RENDER_AXIS_CAMERA_UP = 0;
static const int RIBBON_RENDER_AXIS_VIEW_UP = 1;
static const int RIBBON_RENDER_AXIS_WORLD_UP = 2;
static const float kRibbonEdgeFade = 0.04f;

bool Use_RibbonClampSampler(float mode)
{
    return mode >= 0.5f && mode < 2.5f;
}

float2 Build_RibbonBaseUV(float2 rawUV, float mode)
{
    return mode >= 2.5f ? rawUV : rawUV;
}

float2 Apply_RibbonMaterialUV(float2 uv, float4 uvParams, float2 uvOffset)
{
    return uv * uvParams.xy + uvOffset + uvParams.zw * g_EffectRibbonMaterialTime;
}

float4 Sample_RibbonMainTexture(float2 uv, float mode)
{
    return Use_RibbonClampSampler(mode)
           ? g_MainTexture.Sample(LinearClampSampler, uv)
           : g_MainTexture.Sample(LinearWrapSampler, uv);
}

float4 Sample_RibbonNoiseTexture(float2 uv, float mode)
{
    return Use_RibbonClampSampler(mode)
           ? g_NoiseTexture.Sample(LinearClampSampler, uv)
           : g_NoiseTexture.Sample(LinearWrapSampler, uv);
}

float4 Sample_RibbonMaskTexture(float2 uv, float mode)
{
    return Use_RibbonClampSampler(mode)
           ? g_MaskTexture.Sample(LinearClampSampler, uv)
           : g_MaskTexture.Sample(LinearWrapSampler, uv);
}

struct VS_IN
{
    float3 position : POSITION;
    float4 previousPosition : WORLD0;
    float4 currentPosition : WORLD1;
    float4 nextPosition : WORLD2;
    float2 lifeTime : TEXCOORD0;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD1;
    float4 segmentParams : TEXCOORD2; // x: life progress, y: width, z: alpha scale, w: hasPrevious
};

struct VS_OUT
{
    float3 previousPosition : POSITION;
    float3 currentPosition : TEXCOORD0;
    float3 nextPosition : TEXCOORD1;
    float2 lifeTime : TEXCOORD2;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD3;
    float4 segmentParams : TEXCOORD4;
};

struct GS_IN
{
    float3 previousPosition : POSITION;
    float3 currentPosition : TEXCOORD0;
    float3 nextPosition : TEXCOORD1;
    float2 lifeTime : TEXCOORD2;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD3;
    float4 segmentParams : TEXCOORD4;
};

struct GS_OUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float2 edgeCoord : TEXCOORD2;
    float4 color : COLOR0;
    float4 segmentParams : TEXCOORD3;
};

struct PS_IN
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float2 edgeCoord : TEXCOORD2;
    float4 color : COLOR0;
    float4 segmentParams : TEXCOORD3;
};

struct PS_OUT
{
    float4 color : SV_TARGET0;
};

float3 NormalizeOrFallback(float3 value, float3 fallbackAxis)
{
    const float lengthSq = dot(value, value);
    return lengthSq > 0.000001f ? value * rsqrt(lengthSq) : fallbackAxis;
}

float3 Resolve_ViewUpAxis()
{
    return NormalizeOrFallback(g_ViewMatrixInverse[1].xyz, float3(0.f, 1.f, 0.f));
}

float3 Resolve_RibbonTangent(float3 previousPosition, float3 currentPosition, float3 nextPosition, float hasPreviousSample)
{
    const float3 forward = NormalizeOrFallback(nextPosition - currentPosition, float3(0.f, 0.f, 1.f));
    if (g_EffectRibbonAxisParams.w < 0.5f || hasPreviousSample < 0.5f)
        return forward;

    const float3 backward = NormalizeOrFallback(currentPosition - previousPosition, forward);
    return NormalizeOrFallback(backward + forward, forward);
}

float3 Resolve_RibbonSide(float3 currentPosition, float3 tangent)
{
    const float3 fallbackAxis = NormalizeOrFallback(g_EffectRibbonFallbackAxis.xyz, float3(1.f, 0.f, 0.f));
    const int renderAxis = (int)g_EffectRibbonAxisParams.x;

    float3 axis = NormalizeOrFallback(g_CamPosition.xyz - currentPosition, float3(0.f, 0.f, -1.f));
    if (renderAxis == RIBBON_RENDER_AXIS_VIEW_UP)
        axis = Resolve_ViewUpAxis();
    else if (renderAxis == RIBBON_RENDER_AXIS_WORLD_UP)
        axis = float3(0.f, 1.f, 0.f);

    float3 side = NormalizeOrFallback(cross(axis, tangent), fallbackAxis);
    if (abs(dot(side, tangent)) > 0.98f)
        side = NormalizeOrFallback(cross(Resolve_ViewUpAxis(), tangent), fallbackAxis);

    return side;
}

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out = (VS_OUT)0;

    Out.previousPosition = In.previousPosition.xyz;
    Out.currentPosition = In.currentPosition.xyz;
    Out.nextPosition = In.nextPosition.xyz;
    Out.lifeTime = In.lifeTime;
    Out.startColor = In.startColor;
    Out.endColor = In.endColor;
    Out.subUVRect = In.subUVRect;
    Out.segmentParams = In.segmentParams;

    return Out;
}

GS_OUT Build_RibbonVertex(
    float3 worldPosition,
    float2 texCoord,
    float2 edgeCoord,
    GS_IN input)
{
    const matrix viewProj = mul(g_ViewMatrix, g_ProjMatrix);

    GS_OUT Out = (GS_OUT)0;
    Out.position = mul(float4(worldPosition, 1.f), viewProj);
    Out.texCoord = texCoord;
    Out.lifeTime = input.lifeTime;
    Out.edgeCoord = edgeCoord;
    Out.color = lerp(input.startColor, input.endColor, saturate(input.segmentParams.x));
    Out.segmentParams = input.segmentParams;
    return Out;
}

[maxvertexcount(4)]
void GS_MAIN(point GS_IN In[1], inout TriangleStream<GS_OUT> OutStream)
{
    const float3 currentPosition = In[0].currentPosition;
    const float3 nextPosition = In[0].nextPosition;
    const float3 tangent = Resolve_RibbonTangent(
        In[0].previousPosition,
        currentPosition,
        nextPosition,
        In[0].segmentParams.w
    );
    const float3 side = Resolve_RibbonSide(currentPosition, tangent);
    const float halfWidth = max(In[0].segmentParams.y, 0.001f) * 0.5f;

    const float3 currentA = currentPosition - side * halfWidth;
    const float3 currentB = currentPosition + side * halfWidth;
    const float3 nextA = nextPosition - side * halfWidth;
    const float3 nextB = nextPosition + side * halfWidth;

    OutStream.Append(Build_RibbonVertex(currentA, In[0].subUVRect.xy, float2(0.f, 0.f), In[0]));
    OutStream.Append(Build_RibbonVertex(currentB, float2(In[0].subUVRect.z, In[0].subUVRect.y), float2(0.f, 1.f), In[0]));
    OutStream.Append(Build_RibbonVertex(nextA, float2(In[0].subUVRect.x, In[0].subUVRect.w), float2(1.f, 0.f), In[0]));
    OutStream.Append(Build_RibbonVertex(nextB, In[0].subUVRect.zw, float2(1.f, 1.f), In[0]));
}

EffectMaterialSurface Resolve_EffectRibbonMaterialSurface(PS_IN In)
{
    EffectMaterialSurface surface = (EffectMaterialSurface)0;

    const float lifeProgress = saturate(In.lifeTime.y / max(In.lifeTime.x, 0.0001f));
    const float mainUVMode = g_EffectRibbonUVModeParams.x;
    const float noiseUVMode = g_EffectRibbonUVModeParams.y;
    const float maskUVMode = g_EffectRibbonUVModeParams.z;
    const float2 mainBaseUV = Build_RibbonBaseUV(In.texCoord, mainUVMode);
    const float2 noiseBaseUV = Build_RibbonBaseUV(In.texCoord, noiseUVMode);
    const float2 maskBaseUV = Build_RibbonBaseUV(In.texCoord, maskUVMode);
    const float2 mainUV = Apply_RibbonMaterialUV(mainBaseUV, g_EffectRibbonMainUVParams, g_EffectRibbonUVOffsetParams.xy);
    const float2 noiseUV = Apply_RibbonMaterialUV(noiseBaseUV, g_EffectRibbonNoiseUVParams, g_EffectRibbonUVOffsetParams.zw);
    const float2 maskUV = Apply_RibbonMaterialUV(maskBaseUV, g_EffectRibbonMaskUVParams, g_EffectRibbonMaskUVOffsetParams.xy);
    const float4 tex = Sample_RibbonMainTexture(mainUV, mainUVMode);
    const float sideAlpha =
        smoothstep(0.0f, kRibbonEdgeFade, In.edgeCoord.y) *
        smoothstep(0.0f, kRibbonEdgeFade, 1.0f - In.edgeCoord.y);
    const float resolvedOpacityPower = g_EffectRibbonParams.y
                                       * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_OPACITY_POWER, lifeProgress);
    const float resolvedNoiseStrength = g_EffectRibbonParams.z
                                        * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_NOISE_STRENGTH, lifeProgress);
    const float resolvedAlphaErosion = g_EffectRibbonAlphaParams.y
                                       * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_EROSION, lifeProgress);
    const float resolvedAlphaCutoff = g_EffectRibbonAlphaParams.x
                                      * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_CUTOFF, lifeProgress);
    const float resolvedCoreIntensity = g_EffectRibbonCoreEmissiveParams.z
                                        * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_INTENSITY, lifeProgress);
    const float resolvedOuterIntensity = g_EffectRibbonCoreEmissiveColor.a
                                         * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_OUTER_INTENSITY, lifeProgress);
    const float resolvedCoreColorMultiplier =
        EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_COLOR, lifeProgress);
    const float opacityPower = max(resolvedOpacityPower, 0.0001f);
    const float alphaCutoff = saturate(resolvedAlphaCutoff);
    const float alphaErosion = saturate(resolvedAlphaErosion);
    const float selectedOpacity = Effect_SelectOpacity(tex, g_OpacitySource);
    const float4 resolvedCoreEmissiveParams = float4(
        g_EffectRibbonCoreEmissiveParams.x,
        g_EffectRibbonCoreEmissiveParams.y,
        resolvedCoreIntensity,
        g_EffectRibbonCoreEmissiveParams.w
    );
    const float4 resolvedCoreEmissiveColor = float4(
        g_EffectRibbonCoreEmissiveColor.rgb * resolvedCoreColorMultiplier,
        resolvedOuterIntensity
    );
    float noiseRaw = 1.f;
    float maskAlpha = 1.f;

    const float noiseStrength = saturate(resolvedNoiseStrength);
    if (0 != g_EffectRibbonParams.w && resolvedNoiseStrength > 0.f)
    {
        const float4 noiseTexel = Sample_RibbonNoiseTexture(noiseUV, noiseUVMode);
        noiseRaw = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(noiseTexel, (int)g_EffectRibbonSourceParams.x),
            g_EffectRibbonSourceParams.z
        );
    }

    if (0 != g_EffectRibbonAlphaParams.z)
    {
        const float4 maskTexel = Sample_RibbonMaskTexture(maskUV, maskUVMode);
        maskAlpha = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(maskTexel, (int)g_EffectRibbonSourceParams.y),
            g_EffectRibbonSourceParams.w
        );
    }

    const float edgeDistance = abs(In.edgeCoord.y - 0.5f) * 2.f;
    const float edgeWeight = smoothstep(0.35f, 1.f, edgeDistance);
    const float breakupNoise = lerp(1.f, noiseRaw, noiseStrength);
    const float edgeBreakup = lerp(1.f, noiseRaw, noiseStrength * edgeWeight);
    float coverage = maskAlpha;
    coverage = Effect_ApplyAlphaErosion(coverage, maskAlpha * edgeBreakup, alphaErosion);

    const float energyVariation = lerp(1.f, lerp(0.75f, 1.25f, noiseRaw), noiseStrength);
    const float3 baseColor = Effect_ApplyCoreEmissiveShaping(
        In.color.rgb * g_Tint.rgb * tex.rgb,
        resolvedCoreEmissiveColor,
        selectedOpacity,
        resolvedCoreEmissiveParams
    );

    surface.color = baseColor * energyVariation;
    const float shapeCoverage = Effect_BuildCoverage(
        selectedOpacity,
        opacityPower,
        coverage,
        breakupNoise,
        1.f
    );
    surface.coverage = shapeCoverage * saturate(In.color.a * g_Tint.a * saturate(In.segmentParams.z) * sideAlpha);

    if (shapeCoverage < max(alphaCutoff, 0.01f))
        discard;

    return surface;
}

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;
    const EffectMaterialSurface surface = Resolve_EffectRibbonMaterialSurface(In);
    const float lifeProgress = saturate(In.lifeTime.y / max(In.lifeTime.x, 0.0001f));
    const float resolvedIntensity = g_EffectRibbonParams.x
                                    * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_INTENSITY, lifeProgress);
    Out.color = Effect_ComposeAlphaBlend(surface, resolvedIntensity);
    return Out;
}

PS_OUT PS_ADDITIVE(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;

    const float lifeProgress = saturate(In.lifeTime.y / max(In.lifeTime.x, 0.0001f));
    const float mainUVMode = g_EffectRibbonUVModeParams.x;
    const float noiseUVMode = g_EffectRibbonUVModeParams.y;
    const float maskUVMode = g_EffectRibbonUVModeParams.z;
    const float2 mainBaseUV = Build_RibbonBaseUV(In.texCoord, mainUVMode);
    const float2 noiseBaseUV = Build_RibbonBaseUV(In.texCoord, noiseUVMode);
    const float2 maskBaseUV = Build_RibbonBaseUV(In.texCoord, maskUVMode);
    const float2 mainUV = Apply_RibbonMaterialUV(mainBaseUV, g_EffectRibbonMainUVParams, g_EffectRibbonUVOffsetParams.xy);
    const float2 noiseUV = Apply_RibbonMaterialUV(noiseBaseUV, g_EffectRibbonNoiseUVParams, g_EffectRibbonUVOffsetParams.zw);
    const float2 maskUV = Apply_RibbonMaterialUV(maskBaseUV, g_EffectRibbonMaskUVParams, g_EffectRibbonMaskUVOffsetParams.xy);
    const float4 tex = Sample_RibbonMainTexture(mainUV, mainUVMode);
    const float sideAlpha =
        smoothstep(0.0f, kRibbonEdgeFade, In.edgeCoord.y) *
        smoothstep(0.0f, kRibbonEdgeFade, 1.0f - In.edgeCoord.y);
    const float resolvedNoiseStrength = g_EffectRibbonParams.z
                                        * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_NOISE_STRENGTH, lifeProgress);
    const float resolvedAlphaErosion = g_EffectRibbonAlphaParams.y
                                       * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_EROSION, lifeProgress);
    const float resolvedAlphaCutoff = g_EffectRibbonAlphaParams.x
                                      * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_ALPHA_CUTOFF, lifeProgress);
    const float resolvedCoreIntensity = g_EffectRibbonCoreEmissiveParams.z
                                        * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_INTENSITY, lifeProgress);
    const float resolvedOuterIntensity = g_EffectRibbonCoreEmissiveColor.a
                                         * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_OUTER_INTENSITY, lifeProgress);
    const float resolvedCoreColorMultiplier =
        EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_CORE_COLOR, lifeProgress);
    const float resolvedIntensity = g_EffectRibbonParams.x
                                    * EffectMaterialScalar_ResolveMultiplier(EFFECT_MATERIAL_SCALAR_TARGET_INTENSITY, lifeProgress);
    const float alphaCutoff = saturate(resolvedAlphaCutoff);
    const float alphaErosion = saturate(resolvedAlphaErosion);
    const float4 resolvedCoreEmissiveParams = float4(
        g_EffectRibbonCoreEmissiveParams.x,
        g_EffectRibbonCoreEmissiveParams.y,
        resolvedCoreIntensity,
        g_EffectRibbonCoreEmissiveParams.w
    );
    const float4 resolvedCoreEmissiveColor = float4(
        g_EffectRibbonCoreEmissiveColor.rgb * resolvedCoreColorMultiplier,
        resolvedOuterIntensity
    );
    float noiseRaw = 1.f;
    float maskAlpha = 1.f;

    const float noiseStrength = saturate(resolvedNoiseStrength);
    if (0 != g_EffectRibbonParams.w && resolvedNoiseStrength > 0.f)
    {
        const float4 noiseTexel = Sample_RibbonNoiseTexture(noiseUV, noiseUVMode);
        noiseRaw = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(noiseTexel, (int)g_EffectRibbonSourceParams.x),
            g_EffectRibbonSourceParams.z
        );
    }

    if (0 != g_EffectRibbonAlphaParams.z)
    {
        const float4 maskTexel = Sample_RibbonMaskTexture(maskUV, maskUVMode);
        maskAlpha = Effect_ApplySourceInvert(
            Effect_SelectScalarSource(maskTexel, (int)g_EffectRibbonSourceParams.y),
            g_EffectRibbonSourceParams.w
        );
    }

    const float edgeDistance = abs(In.edgeCoord.y - 0.5f) * 2.f;
    const float edgeWeight = smoothstep(0.35f, 1.f, edgeDistance);
    const float breakupNoise = lerp(1.f, noiseRaw, noiseStrength);
    const float edgeBreakup = lerp(1.f, noiseRaw, noiseStrength * edgeWeight);
    float coverage = Effect_ApplyAlphaErosion(maskAlpha, maskAlpha * edgeBreakup, alphaErosion);
    const float coverageBase = saturate(coverage * breakupNoise);
    const float additiveLifeAlpha = saturate(In.color.a * g_Tint.a * saturate(In.segmentParams.z) * sideAlpha);
    const float energyVariation = lerp(1.f, lerp(0.75f, 1.25f, noiseRaw), noiseStrength);
    const int colorSource = (int)g_EffectRibbonAdditiveParams.x;
    const int amountSource = (int)g_EffectRibbonAdditiveParams.y;
    const int coveragePolicy = (int)g_EffectRibbonAdditiveParams.z;
    const float selectedOpacity = Effect_SelectOpacity(tex, g_OpacitySource);
    const float amount = Effect_SelectAdditiveAmount(tex, maskAlpha, amountSource);
    const float policyAmount = Effect_ResolveAdditivePolicyAmount(amount, coverageBase, coveragePolicy);
    const float discardSource = Effect_ResolveAdditiveDiscardSource(amount, coverageBase, coveragePolicy);
    const float blackGate = Effect_ResolveAdditiveBlackGate(tex.rgb, colorSource, g_EffectRibbonAdditiveFlags.x);

    if (discardSource < max(alphaCutoff, 0.01f))
        discard;

    const float3 materialColor = Effect_ResolveAdditiveColor(
        tex.rgb,
        tex.rgb * g_Tint.rgb,
        g_EffectRibbonAdditiveEmissiveColor,
        g_EffectRibbonAdditiveConstantColor,
        colorSource
    );
    const float3 shapedMaterialColor = Effect_ApplyCoreEmissiveShaping(
        materialColor * In.color.rgb,
        resolvedCoreEmissiveColor,
        selectedOpacity,
        resolvedCoreEmissiveParams
    );
    const float additiveEnergyScalar = max(resolvedIntensity, 0.f) * max(g_EffectRibbonAdditiveParams.w, 0.f);
    const float3 contribution = shapedMaterialColor * energyVariation * policyAmount * additiveEnergyScalar * blackGate * additiveLifeAlpha;
    Out.color = float4(contribution, 1.f);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VGP(AlphaBlendPass, RS_CullNone, DSS_DepthRead, BS_EffectAlphaBlend, VS_MAIN, GS_MAIN, PS_MAIN)
    PASS_RS_DS_BS_VGP(AdditivePass, RS_CullNone, DSS_DepthRead, BS_EffectAdditive, VS_MAIN, GS_MAIN, PS_ADDITIVE)
}
