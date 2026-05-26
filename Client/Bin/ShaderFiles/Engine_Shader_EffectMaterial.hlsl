#ifndef ENGINE_SHADER_EFFECT_MATERIAL_HLSL
#define ENGINE_SHADER_EFFECT_MATERIAL_HLSL

struct EffectMaterialSurface
{
    float3 color;
    float coverage;
};

float Effect_SelectOpacity(float4 texel, int opacitySource)
{
    float selectedOpacity = texel.a;

    if (opacitySource == 1)
        selectedOpacity = texel.r;
    else if (opacitySource == 2)
        selectedOpacity = dot(texel.rgb, float3(0.299f, 0.587f, 0.114f));

    return selectedOpacity;
}

float Effect_ApplyAlphaErosion(float maskAlpha, float erosionSource, float alphaErosion)
{
    float coverage = saturate(maskAlpha);

    if (alphaErosion > 0.f)
    {
        const float erosionFactor = smoothstep(alphaErosion, 1.f, saturate(erosionSource));
        coverage *= erosionFactor;
    }

    return saturate(coverage);
}

float Effect_BuildCoverage(
    float selectedOpacity,
    float opacityPower,
    float maskCoverage,
    float coverageVariation,
    float alphaMultiplier)
{
    const float opacity = pow(saturate(selectedOpacity), max(opacityPower, 0.0001f));
    return saturate(opacity * saturate(maskCoverage) * saturate(coverageVariation) * saturate(alphaMultiplier));
}

float4 Effect_ComposeAlphaBlend(EffectMaterialSurface surface, float intensity)
{
    return float4(surface.color * max(intensity, 0.f), surface.coverage);
}

#endif
