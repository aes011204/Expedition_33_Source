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

float3 Effect_ApplyCoreEmissiveShaping(
    float3 outerColor,
    float4 coreColor,
    float selectedOpacity,
    float4 coreEmissiveParams)
{
    if (coreEmissiveParams.x < 0.5f)
        return outerColor;

    const float opacity = saturate(selectedOpacity);
    const float outerPower = max(coreEmissiveParams.w, 0.0001f);
    const float corePower = max(coreEmissiveParams.y, 0.0001f);
    const float outer = pow(opacity, outerPower);
    const float inner = pow(opacity, corePower);
    const float coreIntensity = max(coreEmissiveParams.z, 0.f);
    const float outerIntensity = max(coreColor.a, 0.f);

    return outerColor * outer * outerIntensity + coreColor.rgb * inner * coreIntensity;
}

float Effect_SelectAdditiveAmount(float4 texel, float maskAlpha, int amountSource)
{
    if (amountSource == 1)
        return saturate(texel.r);
    if (amountSource == 2)
        return saturate(dot(texel.rgb, float3(0.299f, 0.587f, 0.114f)));
    if (amountSource == 3)
        return saturate(maskAlpha);
    if (amountSource == 4)
        return 1.f;

    return saturate(texel.a);
}

float3 Effect_ResolveAdditiveColor(
    float3 mainRGB,
    float3 tintedRGB,
    float4 emissiveColor,
    float4 constantColor,
    int colorSource)
{
    if (colorSource == 1)
        return tintedRGB;
    if (colorSource == 2)
        return emissiveColor.rgb;
    if (colorSource == 3)
        return constantColor.rgb;

    return mainRGB;
}

float Effect_ResolveAdditivePolicyAmount(float amount, float coverageBase, int coveragePolicy)
{
    if (coveragePolicy == 1)
        return saturate(amount) * saturate(coverageBase);

    return saturate(amount);
}

float Effect_ResolveAdditiveDiscardSource(float amount, float coverageBase, int coveragePolicy)
{
    if (coveragePolicy == 2)
        return saturate(coverageBase);

    return saturate(amount);
}

float Effect_ResolveAdditiveBlackGate(float3 mainRGB, int colorSource, float blackNeutral)
{
    if (blackNeutral == 0.f || colorSource < 2)
        return 1.f;

    return saturate(dot(mainRGB, float3(0.299f, 0.587f, 0.114f)));
}

#endif
