// Engine 원본 경로: Engine/Bin/ShaderFiles/Engine_Shader_Material.hlsl
// Client 복사본 경로: Client/Bin/ShaderFiles/Engine_Shader_Material.hlsl - 먼저 이 원본의 의도를 확인하세요.
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
    float3 orm = float3(1.f, 1.f, 0.05f);

    float4 ormTex = HasMaterialTexture(MATERIAL_SLOT_ORM) ? g_MaterialTextures[MATERIAL_SLOT_ORM].Sample(LinearWrapSampler, uv) : float4(0, 0, 0, 0);
    float4 orm2Tex = HasMaterialTexture(MATERIAL_SLOT_ORM2) ? g_MaterialTextures[MATERIAL_SLOT_ORM2].Sample(LinearWrapSampler, uv) : float4(0, 0, 0, 0);
    float4 orm3Tex = HasMaterialTexture(MATERIAL_SLOT_ORM3) ? g_MaterialTextures[MATERIAL_SLOT_ORM3].Sample(LinearWrapSampler, uv) : float4(0, 0, 0, 0);
    
    if (HasMaterialTexture(MATERIAL_SLOT_ORM) || HasMaterialTexture(MATERIAL_SLOT_ORM2) || HasMaterialTexture(MATERIAL_SLOT_ORM3))
    {
        orm.r = ResolveScalarChannel(g_OrmAoSourceSlot == 0 ? ormTex : (g_OrmAoSourceSlot == 1 ? orm2Tex : orm3Tex), g_OrmAoSource);
        orm.g = ResolveScalarChannel(g_OrmRoughnessSourceSlot == 0 ? ormTex : (g_OrmRoughnessSourceSlot == 1 ? orm2Tex : orm3Tex), g_OrmRoughnessSource);
        orm.b = ResolveScalarChannel(g_OrmMetalnessSourceSlot == 0 ? ormTex : (g_OrmMetalnessSourceSlot == 1 ? orm2Tex : orm3Tex), g_OrmMetalnessSource);
    }

    orm.r *= g_AoScale; // O
    orm.g *= g_RoughnessScale; // R
    orm.b *= g_MetalnessScale; // M

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
    float3 baseColor; // 최종 diffuse
    float opacity; // 마스크
    float depth; // hair card 내부 깊이/겹침 보정값
    float id; // 가닥/영역별 variation 값
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

EyeMaterialSample SampleEyeMaterial(float2 baseUV, float2 irisUV)
{
    EyeMaterialSample Out;

    // 흰자 BaseColor는 원본 눈 메쉬 UV로 샘플링한다.
    float3 scleraColor = float3(0.88f, 0.90f, 0.92f);

    if (HasMaterialTexture(MATERIAL_SLOT_BASECOLOR))
    {
        scleraColor = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, baseUV).rgb;
    }
    scleraColor *= float3(0.88f, 0.84f, 0.82f);

    const bool hasIrisHeightTex = HasMaterialTexture(MATERIAL_SLOT_IRIS_HEIGHT);
    const bool hasIrisMaskTex = HasMaterialTexture(MATERIAL_SLOT_IRIS_MASK);

    // Iris Height 텍스처
    // 홍채 섬유의 밝고 어두운 결을 만드는 주 데이터로 사용한다.
    float irisHeight = 0.5f;

    if (hasIrisHeightTex)
    {
        irisHeight = g_MaterialTextures[MATERIAL_SLOT_IRIS_HEIGHT].Sample(LinearClampSampler, irisUV).r;
    }

    // Iris Mask 텍스처
    // 현재는 RGB 전체를 색으로 사용하지 않고,
    // 채널별 보조 디테일로 사용한다.
    float3 irisMaskTex = float3(0.5f, 0.5f, 0.5f);

    if (hasIrisMaskTex)
    {
        irisMaskTex = g_MaterialTextures[MATERIAL_SLOT_IRIS_MASK].Sample(LinearClampSampler, irisUV).rgb;
    }

    // irisUV는 현재 머테리얼의 UV Scale / Offset이 적용된 상태다.
    // 텍스처 중앙 (0.5, 0.5)을 기준으로 홍채 영역을 판정한다.
    float2 irisCenteredUV = irisUV - float2(0.5f, 0.5f);

    float radial = length(irisCenteredUV);

    // 홍채 전체 영역
    // 홍채 크기는 이 두 값으로 조절한다.
    float irisArea = 1.f - smoothstep(0.43f, 0.50f, radial);

    irisArea = saturate(irisArea);

    // 동공 영역
    float pupilMask = 1.f - smoothstep(0.105f, 0.145f, radial);

    pupilMask = saturate(pupilMask);
    float irisContactShadow = smoothstep(0.46f, 0.50f, radial) * (1.f - smoothstep(0.50f, 0.57f, radial));

    scleraColor *= lerp(1.f, 0.8f, irisContactShadow);

    // 눈의 위아래 가장자리를 어둡게 해서 눈꺼풀 안쪽에 들어가 있는 구 형태를 강조한다.
    float verticalEdge = saturate(abs(baseUV.y - 0.5f) * 2.f);
    float scleraLidShadow = smoothstep(0.7f, 1.f, verticalEdge);
    scleraColor *= lerp(0.9f, 0.7f, scleraLidShadow);

    // BaseColorTint를 홍채의 기본 색으로 사용한다.
    float3 irisTint = saturate(g_BaseColorTint.rgb);

    // Height 텍스처의 대비를 올려 섬유 결로 사용한다.
    float heightDetail = saturate((irisHeight - 0.5f) * 2.f + 0.5f);

    float3 irisColor = irisTint * lerp(0.45f, 1.35f, heightDetail);

    // Iris_M의 G 채널을 보조 섬유 디테일로 사용한다.
    float maskDetail = saturate((irisMaskTex.g - 0.5f) * 1.8f + 0.5f);

    irisColor *= lerp(0.78f, 1.22f, maskDetail);

    // Iris_M의 R 채널은 약한 명암 보조로만 사용한다.
    float innerDetail = saturate((irisMaskTex.r - 0.5f) * 1.5f + 0.5f);

    irisColor *= lerp(0.90f, 1.10f, innerDetail);

    // 홍채 바깥쪽으로 갈수록 어둡게 한다.
    float outerDarkening = smoothstep(0.15f, 0.49f, radial);

    irisColor *= lerp(1.f, 0.3f, outerDarkening);

    // 홍채 외곽의 어두운 테두리
    float limbalRing = smoothstep(0.37f, 0.4f, radial) * (1.f - smoothstep(0.4f, 0.495f, radial));

    irisColor *= lerp(1.f, 0.30f, limbalRing);

    // 동공 주변 섬유를 약간 밝힌다.
    float innerLift = smoothstep(0.13f, 0.23f, radial) * (1.f - smoothstep(0.23f, 0.34f, radial));

    irisColor *= lerp(1.f, 1.12f, innerLift);

    // 중앙 동공
    float pupilEdge = smoothstep(0.095f, 0.135f, radial) * (1.f - smoothstep(0.135f, 0.185f, radial));
    irisColor *= lerp(1.f, 0.48f, pupilEdge);
    irisColor = lerp(irisColor, float3(0.003f, 0.003f, 0.005f), pupilMask);

    // 흰자와 홍채 합성
    Out.baseColor = saturate(lerp(scleraColor, irisColor, irisArea));
    Out.opacity = 1.f;
    Out.irisArea = irisArea;
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

float SampleHairDepth(float2 uv)
{
    if (!HasMaterialTexture(MATERIAL_SLOT_HAIR_DEPTH))
        return 0.5f;

    return saturate(g_MaterialTextures[MATERIAL_SLOT_HAIR_DEPTH].Sample(LinearWrapSampler, uv).r);
}

float SampleHairIDValue(float2 uv)
{
    if (!HasMaterialTexture(MATERIAL_SLOT_HAIR_ID))
        return 0.5f;

    float3 hairIDTex = g_MaterialTextures[MATERIAL_SLOT_HAIR_ID].Sample(LinearWrapSampler, uv).rgb;
    return saturate(Blender_ColorToValue(hairIDTex));
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

float SampleHairMaskDensity(float2 uv)
{
    if (!HasMaterialTexture(MATERIAL_SLOT_OPACITY_MASK))
        return 1.f;

    uint textureWidth = 1u;
    uint textureHeight = 1u;

    g_MaterialTextures[MATERIAL_SLOT_OPACITY_MASK].GetDimensions(textureWidth, textureHeight);

    float2 texelSize = 1.f / float2(max(textureWidth, 1u), max(textureHeight, 1u));

    float center = SampleHairMask(uv);
    float left = SampleHairMask(uv + float2(-texelSize.x, 0.f));
    float right = SampleHairMask(uv + float2(texelSize.x, 0.f));
    float up = SampleHairMask(uv + float2(0.f, -texelSize.y));
    float down = SampleHairMask(uv + float2(0.f, texelSize.y));
    float upperLeft = SampleHairMask(uv + float2(-texelSize.x, -texelSize.y));
    float upperRight = SampleHairMask(uv + float2(texelSize.x, -texelSize.y));
    float lowerLeft = SampleHairMask(uv + float2(-texelSize.x, texelSize.y));
    float lowerRight = SampleHairMask(uv + float2(texelSize.x, texelSize.y));

    // 중심 픽셀에는 조금 더 높은 비중을 준다.
    float density = center * 2.f + left + right + up + down + upperLeft + upperRight + lowerLeft + lowerRight;

    return saturate(density / 10.f);
}

float3 Compute_BlenderHairColor(float2 finalUV)
{
    float3 hairColorTex = float3(1.f, 1.f, 1.f);

    if (HasMaterialTexture(MATERIAL_SLOT_BASECOLOR))
        hairColorTex = g_MaterialTextures[MATERIAL_SLOT_BASECOLOR].Sample(LinearWrapSampler, finalUV).rgb;

    float hairMask = SampleHairMask(finalUV);
    float hairDepth = SampleHairDepth(finalUV);
    float hairIDValue = SampleHairIDValue(finalUV);
    float hairMaskDensity = SampleHairMaskDensity(finalUV);

    float3 hairColor = saturate(g_BaseColorTint.rgb);

    // Color 텍스처의 결/밝기 차이를 실제 BaseColor에 반영한다.
    hairColor *= lerp(float3(0.62f, 0.62f, 0.62f), float3(1.12f, 1.12f, 1.12f), hairColorTex);

    // Depth 텍스처로 카드 내부/외부 층 차이를 준다.
    hairColor *= lerp(0.72f, 1.06f, hairDepth);

    // ID 텍스처로 클럼프/가닥 단위 색 편차를 준다.
    float hue = hairIDValue * 0.10f + 0.5f;
    hairColor = Blender_HueSaturationValue(hairColor, hue, 0.92f, 1.00f, 1.f);
    hairColor *= lerp(0.92f, 1.08f, hairIDValue);

    float strandInterior = smoothstep(0.12f, 0.62f, hairMask);
    float clumpDensity = smoothstep(0.08f, 0.24f, hairMaskDensity);

    // Mask가 약한 가장자리는 어둡게 눌러 리본 외곽을 줄인다.
    hairColor *= lerp(0.72f, 1.0f, strandInterior);

    // 주변에 혼자 떨어진 가닥은 본체보다 약간 어둡게 눌러 검은 전선처럼 튀는 대비를 줄인다.
    hairColor *= lerp(0.82f, 1.0f, clumpDensity);

    return saturate(Apply_PrincipledHairBSDF_Color(hairColor));
}


#endif
