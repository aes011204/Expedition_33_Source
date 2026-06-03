#ifndef ENGINE_SHADER_MATERIAL_HLSLI
#define ENGINE_SHADER_MATERIAL_HLSLI

#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_ConstantBuffer.hlsl"

//==================================================
// MATERIAL TEXTURES
//==================================================
Texture2D g_MaterialTextures[MATERIAL_SLOT_END];

//==================================================
// 컴파일 단계에서 inline 최적화 진행됨
//==================================================
bool HasMaterialTexture(uint iSlot)
{
    return (g_iMaterialMask & (1u << iSlot)) != 0;
}

//==================================================
// UV
//==================================================
float2 ApplyUVTransform(float2 uv)
{
    float2 scaled = uv * g_UVScale;

    float s = sin(g_UVRotate);
    float c = cos(g_UVRotate);

    float2 rotated;
    rotated.x = scaled.x * c - scaled.y * s;
    rotated.y = scaled.x * s + scaled.y * c;

    return rotated + g_UVOffset;
}

//==================================================
// SAMPLE FUNCTIONS
//==================================================
float4 SampleBaseColor(float2 uv)
{
    float4 baseColor = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, uv);
    
    if (baseColor.a < 0.3f)
        discard;

    baseColor.rgb *= g_BaseColorTint.rgb;
    baseColor.a *= g_BaseColorTint.a;
    return baseColor;
}

float ResolveScalarChannel(float4 tex, uint source)
{
    if (source == MATERIAL_CHANNEL_CONST0)
        return 0.f;
    if (source == MATERIAL_CHANNEL_CONST1)
        return 1.f;
    if (source == MATERIAL_CHANNEL_R)
        return tex.r;
    if (source == MATERIAL_CHANNEL_G)
        return tex.g;
    if (source == MATERIAL_CHANNEL_B)
        return tex.b;
    if (source == MATERIAL_CHANNEL_A)
        return tex.a;
    if (source == MATERIAL_CHANNEL_MAX_RGB)
        return max(tex.r, max(tex.g, tex.b));
    if (source == MATERIAL_CHANNEL_LUMA)
        return dot(tex.rgb, float3(0.299f, 0.587f, 0.114f));

    return 0.f;
}

float GetOpacityMaskForMasked(float2 uv, float4 baseColor)
{
    if (HasMaterialTexture(MATERIAL_SLOT_OPACITY_MASK))
    {
        float4 maskTex = g_MaterialTextures[MATERIAL_SLOT_OPACITY_MASK].Sample(LinearWrapSampler, uv);
        return ResolveScalarChannel(maskTex, g_OpacityMaskSource);
    }

    return 1.f;
}

float3 ComputeWorldNormal(float3 normalW, float3 tangentW, float3 binormalW, float2 uv)
{
    float3 n = normalize(normalW);
    float3 t = normalize(tangentW);
    float3 b = normalize(binormalW);

    if (!HasMaterialTexture(MATERIAL_SLOT_NORMAL))
        return n;

    float4 normalSample = g_MaterialTextures[MATERIAL_SLOT_NORMAL].Sample(LinearWrapSampler, uv);

    // BC5 normal maps store tangent-space X/Y in RG. Reconstruct Z after strength scaling.
    float2 normalXY = normalSample.rg * 2.f - 1.f;

    const bool flipNormalY = false;
    if (flipNormalY)
        normalXY.y = -normalXY.y;

    normalXY *= g_NormalStrength;

    float normalZ = sqrt(saturate(1.f - dot(normalXY, normalXY)));
    float3 normalTS = normalize(float3(normalXY, normalZ));

    return normalize(normalTS.x * t + normalTS.y * b + normalTS.z * n);
}

float3 SampleOrm(float2 uv)
{
    float3 orm = float3(1.f, 1.f, 0.f);

    if (HasMaterialTexture(MATERIAL_SLOT_ORM))
    {
        float4 ormTex = g_MaterialTextures[MATERIAL_SLOT_ORM].Sample(LinearWrapSampler, uv);
        orm.r = ResolveScalarChannel(ormTex, g_OrmAoSource);
        orm.g = ResolveScalarChannel(ormTex, g_OrmRoughnessSource);
        orm.b = ResolveScalarChannel(ormTex, g_OrmMetalnessSource);
    }

    orm.r *= g_AoScale;         // O
    orm.g *= g_RoughnessScale;  // R
    orm.b *= g_MetalnessScale;  // M

    return orm;
}

float3 SampleEmissive(float2 uv)
{
    if (!HasMaterialTexture(MATERIAL_SLOT_EMISSIVE))
        return float3(0.f, 0.f, 0.f);

    float3 emissiveTex = g_MaterialTextures[MATERIAL_SLOT_EMISSIVE].Sample(LinearWrapSampler, uv).rgb;

    float3 emissive = emissiveTex * g_EmissiveColor.rgb * g_EmissiveIntensity;
    emissive += emissiveTex * g_EmissiveMaskMul;

    return emissive;
}

struct HairMaterialSample
{
    float3  baseColor;  // 최종 diffuse
    float   opacity;    // 마스크
    float   depth;      // hair card 내부 깊이/겹침 보정값
    float   id;         // 가닥/영역별 variation 값
};

HairMaterialSample SampleHairMaterial(float2 uv)
{
    HairMaterialSample Out;

    // Color
    float4 colorTex = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, uv);

    // Mask
    float opacity = colorTex.a;
    if (HasMaterialTexture(MATERIAL_SLOT_OPACITY_MASK))
    {
        float maskTex = g_MaterialTextures[MATERIAL_SLOT_OPACITY_MASK].Sample(LinearWrapSampler, uv).r;
        // 헤어에서는 opacity mask를 우선한다. (알파 보조는 약하게만 사용)
        opacity = lerp(maskTex, max(maskTex, colorTex.a), 0.15f);
    }

    // 마스크 결을 살리기 위해 과한 채움 보정을 줄인다.
    opacity = smoothstep(0.18f, 0.82f, saturate(opacity));
    opacity = pow(opacity, 0.92f);

    // Depth
    // 일단은 PixelDepthOffset으로 쓰지않고, fake occlusion 값으로 사용
    float depth = 0.5f;
    if (HasMaterialTexture(MATERIAL_SLOT_HAIR_DEPTH))
    {
        depth = g_MaterialTextures[MATERIAL_SLOT_HAIR_DEPTH].Sample(LinearWrapSampler, uv).r;
    }

    // ID / Unique Hair Value
    // 가닥마다 색이 완전히 균일해 보이지 않게 variation을 만드는
    float id = 0.5f;
    if (HasMaterialTexture(MATERIAL_SLOT_HAIR_ID))
    {
        id = g_MaterialTextures[MATERIAL_SLOT_HAIR_ID].Sample(LinearWrapSampler, uv).r;
    }

    // 간단한 Lune hair 테스트용 shading
    float depthShade = lerp(0.90f, 1.03f, depth);
    float idVaration = (id - 0.5f) * 0.07f;

    Out.baseColor = saturate(colorTex.rgb * g_BaseColorTint.rgb * depthShade + idVaration);
    // depth/id 기반으로 미세 밀도 보정 (탈모처럼 비어 보이는 현상 완화)
    float densityBoost = lerp(0.98f, 1.06f, depth) * lerp(0.99f, 1.03f, id);
    Out.opacity = saturate(opacity * densityBoost);
    Out.depth = depth;
    Out.id = id;

    return Out;
}

// 수염 풍부함을 늘리거나 줄일 때는 주로 이 블록의 UV 오프셋, coverage 팽창값, clip 기준을 조정한다.
struct BeardSampleCoords
{
    float2 center;
    float2 left;
    float2 right;
    float2 down;
    float2 farLeft;
    float2 farRight;
    float2 wideLeft;
    float2 wideRight;
    float2 farDown;
    float2 downLeft;
    float2 downRight;
    float2 farDownLeft;
    float2 farDownRight;
};

struct BeardTextureSamples
{
    float4 center;
    float4 left;
    float4 right;
    float4 down;
    float4 farLeft;
    float4 farRight;
    float4 wideLeft;
    float4 wideRight;
    float4 farDown;
    float4 downLeft;
    float4 downRight;
    float4 farDownLeft;
    float4 farDownRight;
};

// 수염 카드 주변 UV를 넓게 잡아 빈 영역도 함께 샘플링한다.
BeardSampleCoords BuildBeardSampleCoords(float2 centerUV)
{
    BeardSampleCoords coords;

    float2 sideOffsetX = float2(0.0060f, 0.f);
    float2 sideOffsetY = float2(0.f, 0.0042f);

    coords.center = centerUV;
    coords.left = centerUV - sideOffsetX;
    coords.right = centerUV + sideOffsetX;
    coords.down = centerUV + sideOffsetY;
    coords.farLeft = centerUV - sideOffsetX * 2.f;
    coords.farRight = centerUV + sideOffsetX * 2.f;
    coords.wideLeft = centerUV - sideOffsetX * 3.f;
    coords.wideRight = centerUV + sideOffsetX * 3.f;
    coords.farDown = centerUV + sideOffsetY * 3.f;
    coords.downLeft = centerUV - sideOffsetX + sideOffsetY;
    coords.downRight = centerUV + sideOffsetX + sideOffsetY;
    coords.farDownLeft = centerUV - sideOffsetX * 2.f + sideOffsetY * 2.f;
    coords.farDownRight = centerUV + sideOffsetX * 2.f + sideOffsetY * 2.f;

    return coords;
}

// 같은 수염 텍스처를 여러 지점에서 읽어 sparse card를 하나의 면처럼 다룬다.
BeardTextureSamples SampleBeardTextures(BeardSampleCoords coords)
{
    BeardTextureSamples samples;

    samples.center = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, coords.center);
    samples.left = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, coords.left);
    samples.right = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, coords.right);
    samples.down = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, coords.down);
    samples.farLeft = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, coords.farLeft);
    samples.farRight = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, coords.farRight);
    samples.wideLeft = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, coords.wideLeft);
    samples.wideRight = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, coords.wideRight);
    samples.farDown = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, coords.farDown);
    samples.downLeft = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, coords.downLeft);
    samples.downRight = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, coords.downRight);
    samples.farDownLeft = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, coords.farDownLeft);
    samples.farDownRight = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, coords.farDownRight);

    return samples;
}

// 여러 마스크를 합쳐 sparse card를 하나의 수염 면처럼 보이게 만든다.
float ComputeBeardCoverage(BeardSampleCoords coords, BeardTextureSamples samples)
{
    float4 maskBaseColor = float4(samples.center.rgb * g_BaseColorTint.rgb, samples.center.a * g_BaseColorTint.a);

    float centerMask = GetOpacityMaskForMasked(coords.center, maskBaseColor);
    float leftMask = GetOpacityMaskForMasked(coords.left, maskBaseColor);
    float rightMask = GetOpacityMaskForMasked(coords.right, maskBaseColor);
    float downMask = GetOpacityMaskForMasked(coords.down, maskBaseColor);
    float farLeftMask = GetOpacityMaskForMasked(coords.farLeft, maskBaseColor);
    float farRightMask = GetOpacityMaskForMasked(coords.farRight, maskBaseColor);
    float wideLeftMask = GetOpacityMaskForMasked(coords.wideLeft, maskBaseColor);
    float wideRightMask = GetOpacityMaskForMasked(coords.wideRight, maskBaseColor);
    float farDownMask = GetOpacityMaskForMasked(coords.farDown, maskBaseColor);
    float downLeftMask = GetOpacityMaskForMasked(coords.downLeft, maskBaseColor);
    float downRightMask = GetOpacityMaskForMasked(coords.downRight, maskBaseColor);
    float farDownLeftMask = GetOpacityMaskForMasked(coords.farDownLeft, maskBaseColor);
    float farDownRightMask = GetOpacityMaskForMasked(coords.farDownRight, maskBaseColor);

    // 샘플 범위를 넓힐수록 수염 면적이 더 잘 연결된다.
    float baseCoverage = max(centerMask, max(leftMask, rightMask));
    baseCoverage = max(baseCoverage, max(farLeftMask, farRightMask));
    baseCoverage = max(baseCoverage, max(wideLeftMask, wideRightMask));
    baseCoverage = max(baseCoverage, max(downMask, max(downLeftMask, downRightMask)));
    baseCoverage = max(baseCoverage, farDownMask);
    baseCoverage = max(baseCoverage, max(farDownLeftMask, farDownRightMask));

    // 두께를 더 키우고 싶으면 이 팽창식을 먼저 조정한다.
    return saturate(baseCoverage * 2.35f + 0.42f);
}

// coverage와 같은 범위를 따라가며 수염 색 끊김을 줄일 밀도를 계산한다.
float ComputeBeardDensity(BeardTextureSamples samples)
{
    float centerDensity = dot(samples.center.rgb, float3(0.299f, 0.587f, 0.114f));
    float leftDensity = dot(samples.left.rgb, float3(0.299f, 0.587f, 0.114f));
    float rightDensity = dot(samples.right.rgb, float3(0.299f, 0.587f, 0.114f));
    float downDensity = dot(samples.down.rgb, float3(0.299f, 0.587f, 0.114f));
    float farLeftDensity = dot(samples.farLeft.rgb, float3(0.299f, 0.587f, 0.114f));
    float farRightDensity = dot(samples.farRight.rgb, float3(0.299f, 0.587f, 0.114f));
    float wideLeftDensity = dot(samples.wideLeft.rgb, float3(0.299f, 0.587f, 0.114f));
    float wideRightDensity = dot(samples.wideRight.rgb, float3(0.299f, 0.587f, 0.114f));
    float farDownDensity = dot(samples.farDown.rgb, float3(0.299f, 0.587f, 0.114f));
    float downLeftDensity = dot(samples.downLeft.rgb, float3(0.299f, 0.587f, 0.114f));
    float downRightDensity = dot(samples.downRight.rgb, float3(0.299f, 0.587f, 0.114f));
    float farDownLeftDensity = dot(samples.farDownLeft.rgb, float3(0.299f, 0.587f, 0.114f));
    float farDownRightDensity = dot(samples.farDownRight.rgb, float3(0.299f, 0.587f, 0.114f));

    float density = max(centerDensity, max(leftDensity, rightDensity));
    density = max(density, max(farLeftDensity, farRightDensity));
    density = max(density, max(wideLeftDensity, wideRightDensity));
    density = max(density, max(downDensity, max(downLeftDensity, downRightDensity)));
    density = max(density, farDownDensity);
    density = max(density, max(farDownLeftDensity, farDownRightDensity));
    return density;
}

// 수염 색 농도는 material json의 baseColorTint와 여기의 density 보정 강도로 조절한다.
float3 ComputeBeardColor(float density)
{
    float3 finalBeardColor = g_BaseColorTint.rgb;
    finalBeardColor *= lerp(0.94f, 1.0f, density);
    finalBeardColor *= g_HairBrightness;
    return saturate(finalBeardColor);
}

struct EyeMaterialSample
{
    float3 baseColor;
    float opacity;
    float irisArea;
    float pupilMask;
};

EyeMaterialSample SampleEyeMaterial(float2 uv)
{
    EyeMaterialSample Out;

    float3 baseColorTex = float3(0.88f, 0.90f, 0.92f);
    if (HasMaterialTexture(MATERIAL_SLOT_BASECOLOR))
    {
        baseColorTex = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, uv).rgb;
    }

    const bool hasIrisHeightTex = HasMaterialTexture(MATERIAL_SLOT_IRIS_HEIGHT);
    const bool hasIrisMaskTex = HasMaterialTexture(MATERIAL_SLOT_IRIS_MASK);

    float irisHeight = 0.5f;
    if (hasIrisHeightTex)
        irisHeight = g_MaterialTextures[MATERIAL_SLOT_IRIS_HEIGHT].Sample(LinearWrapSampler, uv).r;

    float3 irisMaskTex = float3(0.5f, 0.5f, 0.5f);
    if (hasIrisMaskTex)
        irisMaskTex = g_MaterialTextures[MATERIAL_SLOT_IRIS_MASK].Sample(LinearWrapSampler, uv).rgb;

    // Iris 텍스처 중앙을 원점으로 한다. (0.5 , 0.5) -> (0, 0)
    float2 eyeUV = uv * 2.f - 1.f;

    // 원점으로부터의 거리. 홍채 원형 영역 계산
    float radial = length(eyeUV);

    // 홍채 마스크
    // 두번째 값 낮추면 홍채 작아짐
    float irisArea = 1.f - smoothstep(0.20f, 0.35f, radial);
    irisArea = saturate(irisArea);

    if (!hasIrisHeightTex)
        irisHeight = 1.f - smoothstep(0.06f, 0.34f, radial);

    // 홍채 tint는 기존 BaseColorTint를 재사용하되, 지나친 검정화 방지를 위해 강도를 제한한다.
    float3 irisTint = saturate(g_BaseColorTint.rgb);
    if (irisTint.r > 0.99f && irisTint.g > 0.99f && irisTint.b > 0.99f)
    {
        irisTint = float3(0.30f, 0.22f, 0.14f);
    }

    // 홍채 섬유 무늬, 밝기, 색 변화 강도
    float fiber = saturate(irisHeight);
    // height 텍스처의 0~1 차이를 디테일로 직접 쓰기 위해 대비를 올린다.
    float heightDetail = saturate((fiber - 0.5f) * 2.4f + 0.5f);
    float limbal = smoothstep(0.28f, 0.46f, radial);

    // 범위를 좁히면 띠 얇아짐
    float limbalRing = smoothstep(0.31f, 0.40f, radial) * (1.f - smoothstep(0.40f, 0.50f, radial));

    // 중앙의 검은 동공 영역
    // 둘 다 낮추면 동공 작아짐
    float pupilMask = 1.f - smoothstep(0.05f, 0.11f, radial);
    float angle = atan2(eyeUV.y, eyeUV.x);

    // 홍채의 단색 느낌을 줄이기 위한 방사형 결(iris striation)
    // 과한 나선형 느낌을 줄이고, 섬유질에 가까운 방사형 결로 정리
    float radialBandA = sin(angle * 12.f + radial * 12.f + fiber * 1.5f);
    float radialBandB = sin(angle * 18.f + radial * 20.f + fiber * 2.0f);
    float proceduralDetail = saturate(0.5f + 0.22f * radialBandA + 0.16f * radialBandB);
    proceduralDetail = lerp(0.90f, 1.12f, proceduralDetail);

    // Iris_M은 영역 마스크가 아니라 홍채 디테일 모듈레이터로 사용한다.
    // 채널의 결 무늬를 최대한 살리기 위해 G 채널을 중심으로, R/B를 보조로 섞고 대비를 강화한다.
    float irisMaskBase = saturate(irisMaskTex.g * 1.10f + irisMaskTex.r * 0.18f + irisMaskTex.b * 0.08f);
    float irisMaskContrast = saturate((irisMaskBase - 0.5f) * 1.9f + 0.5f);
    float irisMaskDetail = lerp(0.72f, 1.36f, irisMaskContrast);

    // 최종 디테일은 Height를 주로, Mask를 보조로 사용한다.
    float heightDrivenDetail = lerp(0.68f, 1.42f, heightDetail);
    float irisDetail = heightDrivenDetail * lerp(0.90f, 1.10f, irisMaskContrast);
    irisDetail *= lerp(0.92f, 1.08f, proceduralDetail);

    float3 scleraColor = saturate(baseColorTex * 1.02f);
    float3 irisBaseColor = lerp(float3(0.10f, 0.08f, 0.06f), irisTint, lerp(0.30f, 0.98f, fiber));
    irisBaseColor *= irisDetail * irisMaskDetail;
    irisBaseColor *= lerp(1.0f, 0.68f, limbal); // 홍채 외곽으로 갈수록 기본적으로 어둡게
    float3 irisColor = irisBaseColor;
    // 중심 동공은 반드시 어둡게 고정한다.
    irisColor = lerp(irisColor, float3(0.015f, 0.015f, 0.02f), pupilMask);
    // 경계 띠는 검은자보다 옅어지지 않도록 별도 고리로 강하게 눌러준다.
    irisColor *= lerp(1.0f, 0.42f, limbalRing);
    // 중앙~중간 영역을 살짝 밝게 보정해 "회색 띠" 느낌을 줄인다.
    float innerLift = 1.f - smoothstep(0.10f, 0.36f, radial);
    irisColor *= lerp(0.90f, 1.08f, innerLift);
    irisColor *= lerp(0.88f, 1.0f, irisArea);

    // 홍채-흰자 전이를 더 급하게 만들어 탁한 중간톤 띠를 줄인다.
    float irisBlend = smoothstep(0.32f, 0.88f, irisArea);
    irisBlend = saturate(pow(irisBlend, 0.55f));

    Out.baseColor = lerp(scleraColor, irisColor, irisBlend);
    Out.baseColor = saturate(Out.baseColor);

    Out.opacity = 1.f;
    Out.irisArea = irisBlend;
    Out.pupilMask = pupilMask;

    return Out;
}


float Blender_ColorToValue(float3 color)
{
    return dot(color, float3(0.2126729f, 0.7151522f, 0.0721750f));
}

float3 Blender_MixColor_Mix(float factor, float3 A, float3 B)
{
    factor = saturate(factor);
    return lerp(A, B, factor);
}

float3 RGBToHSV(float3 color)
{
    float r = color.r;
    float g = color.g;
    float b = color.b;

    float maxValue = max(r, max(g, b));
    float minValue = min(r, min(g, b));
    float delta = maxValue - minValue;

    float h = 0.f;
    float s = 0.f;
    float v = maxValue;

    if (delta > 0.00001f)
    {
        s = delta / maxValue;

        if (maxValue == r)
        {
            h = (g - b) / delta;
            if (h < 0.f)
                h += 6.f;
        }
        else if (maxValue == g)
        {
            h = ((b - r) / delta) + 2.f;
        }
        else
        {
            h = ((r - g) / delta) + 4.f;
        }

        h /= 6.f;
    }

    return float3(h, s, v);
}

float3 HSVToRGB(float3 hsv)
{
    float h = frac(hsv.x);
    float s = saturate(hsv.y);
    float v = hsv.z;

    if (s <= 0.00001f)
        return v.xxx;

    float sector = h * 6.f;
    int i = (int) floor(sector);
    float f = sector - (float) i;

    float p = v * (1.f - s);
    float q = v * (1.f - s * f);
    float t = v * (1.f - s * (1.f - f));

    if (i == 0)
        return float3(v, t, p);
    if (i == 1)
        return float3(q, v, p);
    if (i == 2)
        return float3(p, v, t);
    if (i == 3)
        return float3(p, q, v);
    if (i == 4)
        return float3(t, p, v);

    return float3(v, p, q);
}

float SampleHairMask(float2 uv)
{
    return saturate(g_MaterialTextures[MATERIAL_SLOT_OPACITY_MASK].Sample(LinearWrapSampler, uv).r);
}

float3 Blender_HueSaturationValue(float3 color, float hue, float saturation, float value, float factor)
{
    float3 hsv = RGBToHSV(color);

    // Blender Hue/Saturation/Value 노드 기준:
    // Hue 0.5가 원본 색상 유지에 가까움
    float hueOffset = hue - 0.5f;

    hsv.x = frac(hsv.x + hueOffset);
    hsv.y *= saturation;
    hsv.z *= value;

    float3 adjustedColor = HSVToRGB(hsv);

    return lerp(color, adjustedColor, saturate(factor));
}

float3 Apply_PrincipledHairBSDF_Color(float3 color)
{
    // 현재 GBuffer 패스에서는 Blender의 Chiang Hair BSDF 조명 모델을 직접 계산하지 않는다.
    // 우선 Principled Hair BSDF의 Color 입력을 최종 BaseColor로 내보낸다.
    return color;
}

float3 Compute_BlenderHairColor(float2 finalUV)
{
    // Color Gradient Mix
    float4 hairColorTex = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, finalUV);

    // T_Hair_Lune_Color의 Color 출력이 Factor로 들어간다.
    float colorFactor = Blender_ColorToValue(hairColorTex.rgb);

    float3 hairColor = g_BaseColorTint.rgb;

    // Blender_MixColor_Mix(
    //     colorFactor,
    //     g_BaseColorTint.rgb,
    //     g_BaseColorTint.rgb + float3(0.02f, 0.02f, 0.02f)
    // );

    // Per-Strand Color Variation
    float4 hairIDTex = g_MaterialTextures[MATERIAL_SLOT_HAIR_ID].Sample(LinearWrapSampler, finalUV);

    float hairIDValue = Blender_ColorToValue(hairIDTex.rgb);

    float hue = hairIDValue * 0.134f + 0.5f;

    hairColor = Blender_HueSaturationValue(hairColor, hue, 1.f, 1.f, 1.f);

    // Principled Hair BSDF Color
    return Apply_PrincipledHairBSDF_Color(hairColor);
}

// PS_OUT PS_CharacterHair(PS_IN In)
// {
//     PS_OUT Out;
// 
//     float2 finalUV = ApplyUVTransform(In.Texcoord);
// 
//     HairMaterialSample hair = SampleHairMaterial(finalUV);
// 
//     if (hair.opacity < 0.003)
//         discard;
// 
//     float3 worldNormal = ComputeWorldNormal(In.Normal.xyz, In.Tangent, In.Binormal, finalUV);
//         
//     Out.Diffuse = float4(hair.baseColor, 1.f);
//     Out.Normal = float4(EncodeNormal(worldNormal), 1.f);
//     Out.Depth = float4(In.ProjPos.z / In.ProjPos.w, In.ProjPos.w / g_Far, 0.f, 1.f);
// 
//     float3 orm = SampleOrm(finalUV);
//     Out.Material = float4(orm.xyz, 1.f);
// 
//     float3 emissive = SampleEmissive(finalUV);
//     Out.Emissive = float4(emissive.xyz, 1.f);
// 
//     return Out;
// }



#endif
