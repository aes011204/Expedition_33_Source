#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_Samplers.hlsl"

float4x4 g_WorldMatrix, g_ViewMatrix, g_ProjMatrix;

Texture2D g_Texture;
Texture2D g_MaskTexture;
Texture2D g_PatternMaskTexture;

bool g_IsOverlay;
bool g_UseGrayScale;
float4 g_GrayAmount;
float4 g_Color = float4(1.f, 1.f, 1.f, 1.f);
float2 g_PatternUvOffset = float2(0.f, 0.f);
float2 g_PatternUvScale = float2(1.f, 1.f);

// 원형 마스크ㄴ
float2 g_HoleCenterUV = float2(0.5f, 0.5f);
float g_HoleRadius = 0.0f;
float g_HoleFeather = 0.05f;
float g_ViewportAspect = 1.0f;
bool g_UseFillRatio = false;
float g_FillRatio = 1.0f;
uint g_FillOrigin = 0;
float4 g_MaterialVfxColor = float4(1.0f, 0.12f, 0.95f, 1.0f);
float g_MaterialVfxGlow = 4.0f;
float g_MaterialVfxAlpha = 1.0f;
float g_MaterialVfxNoiseStrength = 0.35f;
float2 g_MaterialVfxUvScroll = float2(0.08f, -0.18f);
float g_MaterialVfxEdgeWidth = 0.18f;
float g_MaterialVfxEdgeSoftness = 0.08f;
float g_MaterialVfxErosion = 0.18f;
float g_MaterialVfxFlicker = 0.22f;
float g_MaterialVfxDistortion = 0.015f;
float g_MaterialVfxEdgeWobbleSpeed = 1.0f;
float g_MaterialVfxTime = 0.0f;
float g_MaterialVfxUseBaseMask = 0.0f;
float g_MaterialVfxHasNoise = 0.0f;
uint g_MaterialVfxMode = 0;
float4 g_MaterialVfxCoreColor = float4(1.0f, 0.92f, 1.0f, 1.0f);
float4 g_MaterialVfxFlowColor = float4(1.0f, 1.0f, 1.0f, 0.0f);
float g_MaterialVfxCoreStrength = 2.2f;
float g_MaterialVfxCoreWidth = 0.13f;
float g_MaterialVfxCoreSoftness = 0.05f;
float4 g_ButtonSheenColor = float4(1.0f, 0.9608f, 0.8824f, 0.5882f);
float g_ButtonSheenSpeed = 0.75f;
float g_ButtonSheenWidth = 0.18f;
float g_ButtonSheenSoftness = 0.12f;
float g_ButtonSheenAngle = -0.08f;
float g_ButtonSheenNoiseStrength = 0.35f;
float g_ButtonSheenTime = 0.0f;

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

    float4x4 matWV = mul(g_WorldMatrix, g_ViewMatrix);
    float4x4 matWVP = mul(matWV, g_ProjMatrix);

    Out.vPosition = mul(float4(In.vPosition, 1.f), matWVP);
    Out.vTexcoord = In.vTexcoord;

    return Out;
}

struct PS_IN
{
    float4 vPosition : SV_POSITION;
    float2 vTexcoord : TEXCOORD0;
};

struct PS_OUT
{
    float4 vColor : SV_TARGET0;
};

PS_OUT PS_NORMAL(PS_IN In)
{
    PS_OUT Out;

    Out.vColor = g_Texture.Sample(LinearClampSampler, In.vTexcoord) * g_Color;

    float4 finalColor = Out.vColor;

    if(g_UseGrayScale)
    {
        float gray = dot(Out.vColor.rgb, float3(0.299f, 0.587f, 0.114f));
        float3 grayColor = float3(gray, gray, gray);

    //    finalColor.rgb = lerp(Out.vColor.rgb, grayColor, g_GrayAmount);
        finalColor.rgb = grayColor * 0.5f;
    }

    if(g_IsOverlay)
    {
        float3 invertedRgb = 1.f - Out.vColor.rgb;

        finalColor.rgb = lerp(Out.vColor.rgb, invertedRgb, Out.vColor.a);
    }


    Out.vColor = finalColor;

    return Out;
}

PS_OUT PS_MASK(PS_IN In)
{
    PS_OUT Out;

    Out.vColor = g_Texture.Sample(LinearClampSampler, In.vTexcoord) * g_Color;
    Out.vColor.a *= g_MaskTexture.Sample(LinearClampSampler, In.vTexcoord).a;

    return Out;
}

PS_OUT PS_BLACK_WHITE_MASK(PS_IN In)
{
    PS_OUT Out;

    const float3 lumaWeights = float3(0.299f, 0.587f, 0.114f);
    const float mask = saturate(dot(g_MaskTexture.Sample(LinearClampSampler, In.vTexcoord).rgb, lumaWeights));

    Out.vColor = g_Texture.Sample(LinearClampSampler, In.vTexcoord) * g_Color;
    Out.vColor.a *= mask;

    return Out;
}

PS_OUT PS_CIRCULAR_HOLE(PS_IN In)
{
    PS_OUT Out;

    float4 baseColor = g_Texture.Sample(LinearClampSampler, In.vTexcoord) * g_Color;
    float2 delta = In.vTexcoord - g_HoleCenterUV;
    delta.x *= g_ViewportAspect;
    float dist = length(delta);

    // 반경 안쪽은 투명, 바깥은 불투명
    float alpha = smoothstep(g_HoleRadius - g_HoleFeather, g_HoleRadius, dist);

    Out.vColor = baseColor;
    Out.vColor.a *= alpha;

    return Out;
}

PS_OUT PS_PATTERN_OVERLAY(PS_IN In)
{
    PS_OUT Out;

    const float3 lumaWeights = float3(0.299f, 0.587f, 0.114f);
    const float2 safePatternUvScale = max(g_PatternUvScale, float2(0.0001f, 0.0001f));
    const float2 patternUv = ((In.vTexcoord - float2(0.5f, 0.5f)) / safePatternUvScale) + float2(0.5f, 0.5f) + g_PatternUvOffset;
    const float4 pattern = g_Texture.Sample(LinearClampSampler, patternUv);
    const float paintMask = saturate(dot(g_MaskTexture.Sample(LinearClampSampler, In.vTexcoord).rgb, lumaWeights));
    const float patternMask = saturate(dot(g_PatternMaskTexture.Sample(LinearClampSampler, In.vTexcoord).rgb, lumaWeights));
    const float patternCoverage = saturate(min(pattern.a, dot(pattern.rgb, lumaWeights)));

    Out.vColor.rgb = g_Color.rgb;
    Out.vColor.a = patternCoverage * paintMask * patternMask * g_Color.a;

    return Out;
}

float Compute_FillVisible(float2 uv)
{
    if (g_FillOrigin == 0)
        return step(uv.x, g_FillRatio);
    if (g_FillOrigin == 1)
        return step(1.0f - g_FillRatio, uv.x);
    if (g_FillOrigin == 2)
        return step(uv.y, g_FillRatio);

    return step(1.0f - g_FillRatio, uv.y);
}

PS_OUT PS_PATTERN_OVERLAY_FILL_RATIO(PS_IN In)
{
    PS_OUT Out = PS_PATTERN_OVERLAY(In);
    Out.vColor.a *= Compute_FillVisible(In.vTexcoord);
    return Out;
}

PS_OUT PS_FILL_RATIO(PS_IN In)
{
    PS_OUT Out;

    float2 uv = In.vTexcoord;
    float visible = Compute_FillVisible(uv);

    Out.vColor = g_Texture.Sample(LinearClampSampler, uv) * g_Color;
    Out.vColor.a *= visible;

    return Out;
}

float Sample_MaterialVfxMask(float2 uv)
{
    const float4 maskSample = g_MaskTexture.Sample(LinearClampSampler, uv);
    const float luminance = dot(maskSample.rgb, float3(0.299f, 0.587f, 0.114f));
    if (g_MaterialVfxUseBaseMask > 0.5f)
        return saturate(luminance * maskSample.a);

    return saturate(max(maskSample.a, luminance));
}

float Sample_MaterialVfxNoise(float2 uv)
{
    const float4 noiseSample = g_PatternMaskTexture.Sample(LinearWrapSampler, uv);
    const float luminance = dot(noiseSample.rgb, float3(0.299f, 0.587f, 0.114f));
    return saturate(luminance * noiseSample.a);
}

PS_OUT PS_MATERIAL_BRUSH_FLOW(PS_IN In)
{
    PS_OUT Out;

    const float2 flowScroll = g_MaterialVfxUvScroll * g_MaterialVfxTime;
    const float2 baseUv = saturate(In.vTexcoord);

    const float4 fixedBrush = g_Texture.Sample(LinearClampSampler, baseUv);
    const float fixedBrushLuma = dot(fixedBrush.rgb, float3(0.299f, 0.587f, 0.114f));
    const float baseCoverage = saturate(fixedBrush.a);

    const float2 detailUvA = In.vTexcoord + flowScroll;
    const float2 detailUvB = In.vTexcoord * (1.17f + g_MaterialVfxDistortion * 12.0f) - flowScroll * 0.41f + float2(0.37f, 0.19f);
    const float2 detailUvC = In.vTexcoord * 0.73f + flowScroll.yx * float2(0.31f, -0.52f) + float2(0.11f, 0.61f);
    const float4 detailBrushA = g_Texture.Sample(LinearWrapSampler, detailUvA);
    const float4 detailBrushB = g_Texture.Sample(LinearWrapSampler, detailUvB);
    const float4 detailBrushC = g_Texture.Sample(LinearWrapSampler, detailUvC);
    const float detailA = saturate(max(detailBrushA.a, dot(detailBrushA.rgb, float3(0.299f, 0.587f, 0.114f)) * detailBrushA.a));
    const float detailB = saturate(max(detailBrushB.a, dot(detailBrushB.rgb, float3(0.299f, 0.587f, 0.114f)) * detailBrushB.a));
    const float detailC = saturate(max(detailBrushC.a, dot(detailBrushC.rgb, float3(0.299f, 0.587f, 0.114f)) * detailBrushC.a));
    float flowDetail = saturate(detailA * 0.40f + detailB * 0.37f + detailC * 0.23f);
    float movingCoverage = saturate(max(detailA, max(detailB * 0.86f, detailC * 0.68f)));
    float edgeNoiseA = saturate(max(detailB, detailA * 0.58f));
    float edgeNoiseB = saturate(max(detailC, detailB * 0.62f));

    if (g_MaterialVfxHasNoise > 0.5f)
    {
        const float noiseA = Sample_MaterialVfxNoise(In.vTexcoord + flowScroll * 0.67f);
        const float noiseB = Sample_MaterialVfxNoise(In.vTexcoord * 1.31f - flowScroll * 0.43f + float2(0.37f, 0.19f));
        const float noiseC = Sample_MaterialVfxNoise(In.vTexcoord * 0.79f + flowScroll.yx * float2(-0.58f, 0.27f) + float2(0.13f, 0.71f));
        flowDetail = saturate(flowDetail * 0.46f + noiseA * 0.27f + noiseB * 0.17f + noiseC * 0.10f);
        movingCoverage = saturate(max(movingCoverage * 0.55f, max(noiseA * 0.9f, max(noiseB * 0.7f, noiseC * 0.5f))));
        edgeNoiseA = saturate(max(noiseB, noiseA * 0.48f));
        edgeNoiseB = saturate(max(noiseC, noiseB * 0.55f));
    }

    const float2 edgeScroll = flowScroll * g_MaterialVfxEdgeWobbleSpeed;
    const float edgeWarpA = g_MaterialVfxHasNoise > 0.5f
        ? Sample_MaterialVfxNoise(In.vTexcoord * 0.91f + edgeScroll * 0.73f + float2(0.21f, 0.47f))
        : edgeNoiseA;
    const float edgeWarpB = g_MaterialVfxHasNoise > 0.5f
        ? Sample_MaterialVfxNoise(In.vTexcoord * 1.07f - edgeScroll.yx * 0.61f + float2(0.63f, 0.17f))
        : edgeNoiseB;
    const float edgeSoft = max(0.001f, g_MaterialVfxEdgeSoftness);
    const float edgeOffsetScale = g_MaterialVfxDistortion * (4.0f + g_MaterialVfxEdgeWobbleSpeed * 1.65f);
    const float verticalSign = step(0.5f, In.vTexcoord.y) * 2.0f - 1.0f;
    const float horizontalSign = step(0.5f, In.vTexcoord.x) * 2.0f - 1.0f;
    const float edgeWaveA = sin(In.vTexcoord.x * 35.0f + edgeWarpA * 7.0f + g_MaterialVfxTime * (4.2f + g_MaterialVfxEdgeWobbleSpeed * 2.1f));
    const float edgeWaveB = sin((In.vTexcoord.x * -18.0f + In.vTexcoord.y * 27.0f) + edgeWarpB * 8.0f - g_MaterialVfxTime * (3.1f + g_MaterialVfxEdgeWobbleSpeed * 2.7f));
    const float edgeWave = (edgeWaveA * 0.62f + edgeWaveB * 0.38f) * saturate(g_MaterialVfxNoiseStrength);
    const float2 edgeWaveOffset = float2(horizontalSign * edgeWave * 0.16f, verticalSign * edgeWave * 0.34f) * edgeOffsetScale;
    const float2 edgeOffsetA = float2(edgeWarpA - 0.5f, edgeWarpB - 0.5f) * edgeOffsetScale + edgeWaveOffset;
    const float2 edgeOffsetB = float2(edgeNoiseB - 0.5f, edgeNoiseA - 0.5f) * edgeOffsetScale * 0.72f - edgeWaveOffset * 0.68f;
    const float2 edgeOffsetC = float2(edgeWaveB, edgeWaveA) * edgeOffsetScale * 0.38f;
    const float2 movingUv = In.vTexcoord + flowScroll * (1.0f + g_MaterialVfxNoiseStrength * 0.35f);
    const float warpedCoverageA = saturate(g_Texture.Sample(LinearWrapSampler, movingUv + edgeOffsetA).a);
    const float warpedCoverageB = saturate(g_Texture.Sample(LinearWrapSampler, movingUv - edgeOffsetB).a);
    const float warpedCoverageC = saturate(g_Texture.Sample(LinearWrapSampler, movingUv + edgeOffsetC).a);
    const float warpedCoverage = saturate(max(max(warpedCoverageA, warpedCoverageB * 0.92f), warpedCoverageC * 0.74f));

    const float flowCoverageBlend = saturate(0.52f + g_MaterialVfxNoiseStrength * 0.43f);
    const float coverageSeed = saturate(lerp(baseCoverage, movingCoverage, flowCoverageBlend));
    const float edgeCoverageBasis = max(coverageSeed, warpedCoverage);
    const float edgeBandOuter = smoothstep(0.001f, max(0.001f, g_MaterialVfxEdgeWidth), edgeCoverageBasis);
    const float edgeBandInner = 1.0f - smoothstep(1.0f - edgeSoft, 1.0f, edgeCoverageBasis);
    const float edgeBand = saturate(edgeBandOuter * edgeBandInner);
    const float edgeFlame = saturate(max(flowDetail, max(edgeWarpA * 0.92f, max(edgeWarpB * 0.84f, abs(edgeWave) * 0.8f))));
    const float edgeBlend = saturate(edgeBand * (0.55f + g_MaterialVfxDistortion * 7.5f));
    const float edgeAlphaPulse = lerp(1.0f, 0.54f + edgeFlame * 0.68f, saturate(g_MaterialVfxNoiseStrength * edgeBand));
    const float brushCoverage = saturate(lerp(coverageSeed, warpedCoverage, edgeBlend) * edgeAlphaPulse);

    const float mask = g_MaterialVfxUseBaseMask > 0.5f ? 1.0f : Sample_MaterialVfxMask(In.vTexcoord);
    const float flowAmount = saturate(g_MaterialVfxNoiseStrength);
    const float internalAlpha = lerp(1.0f, 0.76f + flowDetail * 0.24f, flowAmount);
    const float internalShade = lerp(0.90f + fixedBrushLuma * 0.10f, 0.56f + flowDetail * 0.44f, flowAmount);
    const float flamePulse = saturate((sin(g_MaterialVfxTime * 18.0f + edgeFlame * 11.0f) + 1.0f) * 0.5f);
    const float flicker = lerp(1.0f, 0.76f + 0.24f * saturate(flamePulse * 0.45f + edgeFlame * 0.55f), g_MaterialVfxFlicker);
    const float alpha = saturate(brushCoverage * mask * internalAlpha * flicker * g_MaterialVfxAlpha * g_MaterialVfxColor.a);

    const float edgeHeat = edgeBand * saturate(edgeFlame * 1.4f - 0.22f) * g_MaterialVfxCoreColor.a;
    const float glowBoost = 1.0f + g_MaterialVfxGlow * (0.035f + edgeHeat * 0.05f);
    const float core = pow(saturate(brushCoverage), 1.7f) * g_MaterialVfxCoreStrength * (0.03f + flowDetail * 0.08f);
    const float edgeCore = edgeHeat * g_MaterialVfxCoreStrength * (0.16f + g_MaterialVfxGlow * 0.015f);
    Out.vColor.rgb = g_MaterialVfxColor.rgb * internalShade * glowBoost + g_MaterialVfxCoreColor.rgb * (core + edgeCore);
    Out.vColor.a = alpha * g_Color.a;

    return Out;
}

PS_OUT PS_MATERIAL_VFX(PS_IN In)
{
    if (g_MaterialVfxMode == 1)
        return PS_MATERIAL_BRUSH_FLOW(In);

    PS_OUT Out;

    const float2 flowScroll = g_MaterialVfxUvScroll * g_MaterialVfxTime;
    const float2 edgeScroll = flowScroll * g_MaterialVfxEdgeWobbleSpeed;
    const float noiseA = Sample_MaterialVfxNoise(In.vTexcoord + flowScroll);
    const float noiseB = Sample_MaterialVfxNoise(In.vTexcoord + flowScroll * 0.73f + float2(0.37f, 0.53f));
    const float noiseC = Sample_MaterialVfxNoise(In.vTexcoord * 1.41f - flowScroll * 0.41f + float2(0.11f, 0.79f));
    const float noise = saturate(max(noiseA, max(noiseB * 0.85f, noiseC * 0.65f)));
    const float wobbleX = Sample_MaterialVfxNoise(In.vTexcoord * 0.85f + edgeScroll * 0.38f + float2(0.21f, 0.47f)) - 0.5f;
    const float wobbleY = Sample_MaterialVfxNoise(In.vTexcoord * 0.90f - edgeScroll.yx * 0.31f + float2(0.63f, 0.17f)) - 0.5f;
    float2 uv = saturate(In.vTexcoord + float2(wobbleX, wobbleY) * g_MaterialVfxDistortion);

    float mask = Sample_MaterialVfxMask(uv);
    float inner = smoothstep(g_MaterialVfxErosion, g_MaterialVfxErosion + g_MaterialVfxEdgeSoftness, mask);
    float edgeOuter = smoothstep(0.001f, g_MaterialVfxEdgeWidth, mask);
    float edgeInner = smoothstep(g_MaterialVfxEdgeWidth, g_MaterialVfxEdgeWidth + g_MaterialVfxEdgeSoftness, mask);
    float edge = saturate(edgeOuter - edgeInner);
    float coreOuter = smoothstep(g_MaterialVfxErosion, g_MaterialVfxErosion + g_MaterialVfxCoreSoftness, mask);
    float coreInner = smoothstep(g_MaterialVfxErosion + g_MaterialVfxCoreWidth, g_MaterialVfxErosion + g_MaterialVfxCoreWidth + g_MaterialVfxCoreSoftness, mask);
    float core = saturate(coreOuter - coreInner);

    float animatedNoise = lerp(1.0f, noise, g_MaterialVfxNoiseStrength);
    float flicker = lerp(1.0f, 0.65f + 0.35f * sin(g_MaterialVfxTime * 19.0f + noise * 12.0f), g_MaterialVfxFlicker);
    float coreAlpha = core * g_MaterialVfxCoreColor.a * g_MaterialVfxCoreStrength;
    float coverage = saturate(max(max(inner * 0.35f, edge), coreAlpha) * animatedNoise * flicker);
    float flowPattern = saturate(noise * inner * mask * (1.0f - edge) * (1.0f - core) * g_MaterialVfxNoiseStrength);
    float flowTint = flowPattern * g_MaterialVfxFlowColor.a;
    float alpha = saturate(coverage * g_MaterialVfxAlpha * g_MaterialVfxColor.a);
    float solidAlpha = smoothstep(0.08f, 0.22f, coverage);
    float glow = edge * g_MaterialVfxGlow;

    float3 edgeColor = g_MaterialVfxColor.rgb * (0.55f + glow);
    float3 noiseColor = lerp(edgeColor, g_MaterialVfxFlowColor.rgb, flowTint);
    float3 coreColor = g_MaterialVfxCoreColor.rgb * coreAlpha;
    Out.vColor.rgb = noiseColor + coreColor;
    Out.vColor.a = saturate(max(alpha, solidAlpha * 0.82f)) * g_Color.a;

    return Out;
}

float Sample_ButtonSheenBrush(float2 uv)
{
    const float4 sampleColor = g_Texture.Sample(LinearClampSampler, uv);
    const float luminance = dot(sampleColor.rgb, float3(0.299f, 0.587f, 0.114f));
    return saturate(max(sampleColor.a, luminance));
}

float Sample_ButtonSheenMask(float2 uv)
{
    const float4 sampleColor = g_MaskTexture.Sample(LinearClampSampler, uv);
    const float luminance = dot(sampleColor.rgb, float3(0.299f, 0.587f, 0.114f));
    return saturate(max(sampleColor.a, luminance));
}

PS_OUT PS_BUTTON_SHEEN(PS_IN In)
{
    PS_OUT Out;

    const float mask = Sample_ButtonSheenMask(In.vTexcoord);
    const float brush = Sample_ButtonSheenBrush(In.vTexcoord);

    const float2 centeredUv = In.vTexcoord - float2(0.5f, 0.5f);
    const float angleSin = sin(g_ButtonSheenAngle);
    const float angleCos = cos(g_ButtonSheenAngle);
    const float2 sweepUv = float2(
        centeredUv.x * angleCos - centeredUv.y * angleSin,
        centeredUv.x * angleSin + centeredUv.y * angleCos) + float2(0.5f, 0.5f);

    const float phase = frac(g_ButtonSheenTime * g_ButtonSheenSpeed);
    const float sweepCenter = lerp(-g_ButtonSheenWidth, 1.0f + g_ButtonSheenWidth, phase);
    const float bandDistance = abs(sweepUv.x - sweepCenter);
    const float band = 1.0f - smoothstep(
        max(0.001f, g_ButtonSheenWidth),
        max(0.001f, g_ButtonSheenWidth + g_ButtonSheenSoftness),
        bandDistance);
    const float brushNoise = lerp(1.0f, brush, saturate(g_ButtonSheenNoiseStrength));
    const float alpha = saturate(band * brushNoise * mask * g_ButtonSheenColor.a * g_Color.a);

    Out.vColor.rgb = g_ButtonSheenColor.rgb * g_Color.rgb;
    Out.vColor.a = alpha;

    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VP(NormalPass,    RS_UI, DSS_None, BS_AlphaBlend, VS_MAIN, PS_NORMAL) // 0
    PASS_RS_DS_BS_VP(MaskPass,      RS_UI, DSS_None, BS_AlphaBlend, VS_MAIN, PS_MASK)   // 1
    PASS_RS_DS_BS_VP(CircularHolePass, RS_UI, DSS_None, BS_AlphaBlend, VS_MAIN, PS_CIRCULAR_HOLE) // 2
    PASS_RS_DS_BS_VP(PatternOverlayPass, RS_UI, DSS_None, BS_AlphaBlend, VS_MAIN, PS_PATTERN_OVERLAY) // 3
    PASS_RS_DS_BS_VP(FillRatioPass, RS_UI, DSS_None, BS_AlphaBlend, VS_MAIN, PS_FILL_RATIO) // 4
    PASS_RS_DS_BS_VP(BlackWhiteMaskPass, RS_UI, DSS_None, BS_AlphaBlend, VS_MAIN, PS_BLACK_WHITE_MASK) // 5
    PASS_RS_DS_BS_VP(PatternOverlayFillRatioPass, RS_UI, DSS_None, BS_AlphaBlend, VS_MAIN, PS_PATTERN_OVERLAY_FILL_RATIO) // 6
    PASS_RS_DS_BS_VP(MaterialVfxPass, RS_UI, DSS_None, BS_AlphaBlend, VS_MAIN, PS_MATERIAL_VFX) // 7
    PASS_RS_DS_BS_VP(ButtonSheenPass, RS_UI, DSS_None, BS_AlphaBlend, VS_MAIN, PS_BUTTON_SHEEN) // 8
}
