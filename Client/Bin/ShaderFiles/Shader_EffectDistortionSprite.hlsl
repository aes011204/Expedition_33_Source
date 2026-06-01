#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_EffectMaterial.hlsl"
#include "Shader_EffectSpriteCommon.hlsli"
#include "Shader_EffectDistortionCommon.hlsli"

cbuffer cbEffectDistortionSpriteMaterial : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectSpriteParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectSpriteAlphaParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectSpriteMainUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectSpriteNoiseUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectSpriteMaskUVParams = { 1.f, 1.f, 0.f, 0.f };
    float4 g_EffectSpriteUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectSpriteMaskUVOffsetParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectSpriteUVModeParams = { 0.f, 0.f, 0.f, 0.f };
    float4 g_EffectDistortionParams = { 0.f, 1.f, 0.f, 0.f };            // x:intensity, y:presence, z:useFlow, w:emitterElapsedTime
    float4 g_EffectDistortionShapeParams = { 0.f, 0.45f, 0.10f, 0.15f }; // x:mode, y:radius, z:thickness, w:softness
    float4 g_EffectInfluenceDebugParams = { 0.f, 0.5f, 2.f, 0.f };       // x: influence outline enabled, y: reserved, z: pixel width scale, w: reserved
    float4 g_DistortionScreenSize = { 1.f, 1.f, 0.f, 0.f };
    float2 g_FlowUVScale = { 1.f, 1.f };
    float2 g_FlowUVOffset = { 0.f, 0.f };
    float2 g_FlowUVScrollSpeed = { 0.f, 0.f };
    float g_FlowUVTilingMode = 0.f;
    int g_OpacitySource = 0;
    float g_EffectSpriteMaterialTime = 0.f;
    float g_DistortionPadding;
};

Texture2D g_Texture : register(t0);
Texture2D g_NoiseTexture : register(t1);
Texture2D g_MaskTexture : register(t2);
Texture2D g_FlowTexture : register(t3);

static const float3 kInfluenceOutlineColor = float3(1.f, 0.f, 0.f);

bool Use_EffectSpriteClampSampler(float mode)
{
    return mode >= 0.5f && mode < 2.5f;
}

float2 Build_EffectSpriteUV(float2 baseUV, float4 uvParams, float2 uvOffset, float mode)
{
    const float2 transformedUV = baseUV * uvParams.xy + uvOffset + uvParams.zw * g_EffectSpriteMaterialTime;
    return mode >= 2.5f ? baseUV : transformedUV;
}

float4 Sample_EffectSpriteMainTexture(float2 uv, float mode)
{
    return Use_EffectSpriteClampSampler(mode)
           ? g_Texture.Sample(LinearClampSampler, uv)
           : g_Texture.Sample(LinearWrapSampler, uv);
}

float Sample_EffectSpriteNoiseTexture(float2 uv, float mode)
{
    return Use_EffectSpriteClampSampler(mode)
           ? g_NoiseTexture.Sample(LinearClampSampler, uv).r
           : g_NoiseTexture.Sample(LinearWrapSampler, uv).r;
}

float Sample_EffectSpriteMaskTexture(float2 uv, float mode)
{
    return Use_EffectSpriteClampSampler(mode)
           ? g_MaskTexture.Sample(LinearClampSampler, uv).a
           : g_MaskTexture.Sample(LinearWrapSampler, uv).a;
}

float2 Sample_EffectSpriteFlowTexture(float2 uv, float mode)
{
    return Use_EffectSpriteClampSampler(mode)
           ? g_FlowTexture.Sample(LinearClampSampler, uv).rg
           : g_FlowTexture.Sample(LinearWrapSampler, uv).rg;
}

float Compute_InfluenceOutlineWidth(float2 cardUV)
{
    const float pixelUvWidth = max(fwidth(cardUV.x), fwidth(cardUV.y));
    const float pixelScale = max(g_EffectInfluenceDebugParams.z, 0.0001f);
    return clamp(pixelUvWidth * pixelScale, 0.0001f, 0.035f);
}

struct VS_IN
{
    float3 position : POSITION;
    row_major float4x4 InstanceMatrix : WORLD;
    float2 lifeTime : TEXCOORD0;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD1;
};

struct VS_OUT
{
    float4 worldPosition : POSITION;
    float2 size : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD2;
    float rotationRadians : TEXCOORD3;
    float3 velocityDirection : TEXCOORD4;
};

VS_OUT VS_MAIN(VS_IN In)
{
    VS_OUT Out = (VS_OUT)0;

    const float4 worldPosition = mul(float4(In.position, 1.f), In.InstanceMatrix);
    Out.worldPosition = worldPosition;
    Out.size = float2(length(In.InstanceMatrix[0].xyz), length(In.InstanceMatrix[1].xyz));
    Out.lifeTime = In.lifeTime;
    Out.startColor = In.startColor;
    Out.endColor = In.endColor;
    Out.subUVRect = In.subUVRect;
    Out.rotationRadians = atan2(In.InstanceMatrix[0].y, In.InstanceMatrix[0].x);
    Out.velocityDirection = In.InstanceMatrix[2].xyz;

    return Out;
}

struct GS_IN
{
    float4 worldPosition : POSITION;
    float2 size : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD2;
    float rotationRadians : TEXCOORD3;
    float3 velocityDirection : TEXCOORD4;
};

struct GS_OUT
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD2;
};

// 파티클 위치에서 카메라를 향하는 정규화 look 방향을 구한다.
float3 ResolveFacingLookDirection(float3 particlePosition)
{
    float3 lookDir = g_CamPosition.xyz - particlePosition;
    const float lookLength = length(lookDir);
    return lookLength < 0.0001f ? float3(0.f, 0.f, 1.f) : lookDir / lookLength;
}

bool TryNormalizeDirection(float3 direction, out float3 normalizedDirection)
{
    normalizedDirection = float3(0.f, 0.f, 0.f);

    const float directionLength = length(direction);
    if (directionLength < 0.0001f)
        return false;

    normalizedDirection = direction / directionLength;
    return true;
}

bool TryResolveAwayFromCenterLookDirection(float3 particlePosition, out float3 lookDir)
{
    return TryNormalizeDirection(particlePosition - g_EffectSpriteEmitterCenter.xyz, lookDir);
}

bool TryResolveVelocityLookDirection(float3 velocityDirection, out float3 lookDir)
{
    return TryNormalizeDirection(velocityDirection, lookDir);
}

bool TryResolveDirectionalLookDirection(float3 particlePosition, float3 velocityDirection, out float3 lookDir)
{
    lookDir = float3(0.f, 0.f, 0.f);

    if (g_ScreenAlignmentMode == 3)
    {
        if (TryResolveAwayFromCenterLookDirection(particlePosition, lookDir))
            return true;

        return TryResolveVelocityLookDirection(velocityDirection, lookDir);
    }

    if (g_ScreenAlignmentMode == 4)
    {
        if (TryResolveVelocityLookDirection(velocityDirection, lookDir))
            return true;

        return TryResolveAwayFromCenterLookDirection(particlePosition, lookDir);
    }

    return false;
}

// look 방향을 기준으로 billboard right/up 축을 만든다.
void ResolveLookAlignedBasis(float3 lookDir, out float3 rightDir, out float3 upDir)
{
    float3 worldUp = float3(0.f, 1.f, 0.f);
    rightDir = cross(worldUp, lookDir);

    if (length(rightDir) < 0.0001f)
        rightDir = cross(float3(0.f, 0.f, 1.f), lookDir);

    rightDir = normalize(rightDir);
    upDir = normalize(cross(lookDir, rightDir));
}

// 현재 카메라 평면 기준의 right/up 축을 가져온다.
void ResolveCameraPlaneBasis(out float3 rightDir, out float3 upDir)
{
    rightDir = normalize(float3(g_ViewMatrix[0].x, g_ViewMatrix[1].x, g_ViewMatrix[2].x));
    upDir = normalize(float3(g_ViewMatrix[0].y, g_ViewMatrix[1].y, g_ViewMatrix[2].y));
}

// texture 축 하나를 지정 방향에 맞추고, 나머지 축은 카메라 가시성을 유지하도록 만든다.
bool TryResolveTextureAxisBasis(float3 axisDir, float3 particlePosition, out float3 rightDir, out float3 upDir)
{
    ResolveCameraPlaneBasis(rightDir, upDir);

    float3 axis = float3(0.f, 0.f, 0.f);
    if (!TryNormalizeDirection(axisDir, axis))
        return false;

    float3 cameraLook = float3(0.f, 0.f, 0.f);
    if (!TryNormalizeDirection(g_CamPosition.xyz - particlePosition, cameraLook))
        return false;

    if (g_SpriteTextureAxis == 0)
    {
        rightDir = axis;
        if (!TryNormalizeDirection(cross(rightDir, cameraLook), upDir))
            return false;
    }
    else
    {
        upDir = axis;
        if (!TryNormalizeDirection(cross(cameraLook, upDir), rightDir))
            return false;
    }

    return true;
}

[maxvertexcount(4)]
void GS_MAIN(point GS_IN In[1], inout TriangleStream<GS_OUT> OutStream)
{
    const float3 particlePosition = In[0].worldPosition.xyz;
    float3 rightDir = float3(0.f, 0.f, 0.f);
    float3 upDir = float3(0.f, 0.f, 0.f);
    ResolveCameraPlaneBasis(rightDir, upDir);
    if (g_ScreenAlignmentMode == 5)
    {
        rightDir = float3(1.f, 0.f, 0.f);
        upDir = float3(0.f, 1.f, 0.f);
    }
    else if (g_ScreenAlignmentMode == 6)
    {
        rightDir = float3(1.f, 0.f, 0.f);
        upDir = float3(0.f, 0.f, 1.f);
    }
    else if (g_ScreenAlignmentMode == 3 || g_ScreenAlignmentMode == 4)
    {
        float3 lookDir = float3(0.f, 0.f, 0.f);
        const bool hasLookDir = TryResolveDirectionalLookDirection(particlePosition, In[0].velocityDirection, lookDir);

        if (g_DirectionalAlignmentMode == 1) // 스프라이트의 X 또는 Y 가 바라보도록
        {
            if (hasLookDir)
                TryResolveTextureAxisBasis(lookDir, particlePosition, rightDir, upDir);
        }
        else // 스프라이트의 look 이 바라보도록
        {
            if (!hasLookDir)
                lookDir = ResolveFacingLookDirection(particlePosition);
            ResolveLookAlignedBasis(lookDir, rightDir, upDir);
        }
    }
    else if (g_ScreenAlignmentMode != 1 && g_ScreenAlignmentMode != 2)
    {
        const float3 lookDir = ResolveFacingLookDirection(particlePosition);
        ResolveLookAlignedBasis(lookDir, rightDir, upDir);
    }

    const float rotationRadians = In[0].rotationRadians + g_SpriteRollOffsetRadians;
    const float rotationCos = cos(rotationRadians);
    const float rotationSin = sin(rotationRadians);
    const float3 billboardRight = rightDir;
    const float3 billboardUp = upDir;
    rightDir = billboardRight * rotationCos + billboardUp * rotationSin;
    upDir = -billboardRight * rotationSin + billboardUp * rotationCos;
    float2 spriteSize = In[0].size;
    if (g_ScreenAlignmentMode == 2)
    {
        const float squareExtent = max(spriteSize.x, spriteSize.y);
        spriteSize = float2(squareExtent, squareExtent);
    }

    rightDir *= spriteSize.x * 0.5f;
    upDir *= spriteSize.y * 0.5f;

    const matrix ViewProjMatrix = mul(g_ViewMatrix, g_ProjMatrix);
    GS_OUT Out[4] = (GS_OUT[4])0;

    Out[0].position = mul(float4(particlePosition - rightDir + upDir, 1.f), ViewProjMatrix);
    Out[0].texCoord = float2(0.f, 0.f);
    Out[0].lifeTime = In[0].lifeTime;
    Out[0].startColor = In[0].startColor;
    Out[0].endColor = In[0].endColor;
    Out[0].subUVRect = In[0].subUVRect;

    Out[1].position = mul(float4(particlePosition + rightDir + upDir, 1.f), ViewProjMatrix);
    Out[1].texCoord = float2(1.f, 0.f);
    Out[1].lifeTime = In[0].lifeTime;
    Out[1].startColor = In[0].startColor;
    Out[1].endColor = In[0].endColor;
    Out[1].subUVRect = In[0].subUVRect;

    Out[2].position = mul(float4(particlePosition - rightDir - upDir, 1.f), ViewProjMatrix);
    Out[2].texCoord = float2(0.f, 1.f);
    Out[2].lifeTime = In[0].lifeTime;
    Out[2].startColor = In[0].startColor;
    Out[2].endColor = In[0].endColor;
    Out[2].subUVRect = In[0].subUVRect;

    Out[3].position = mul(float4(particlePosition + rightDir - upDir, 1.f), ViewProjMatrix);
    Out[3].texCoord = float2(1.f, 1.f);
    Out[3].lifeTime = In[0].lifeTime;
    Out[3].startColor = In[0].startColor;
    Out[3].endColor = In[0].endColor;
    Out[3].subUVRect = In[0].subUVRect;

    OutStream.Append(Out[0]);
    OutStream.Append(Out[1]);
    OutStream.Append(Out[2]);
    OutStream.Append(Out[3]);
}

struct PS_IN
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float2 lifeTime : TEXCOORD1;
    float4 startColor : COLOR0;
    float4 endColor : COLOR1;
    float4 subUVRect : TEXCOORD2;
};

struct PS_OUT
{
    float4 color : SV_TARGET0;
};

float Compute_DistortionInfluenceOutline(float2 cardUV, float lifeProgress)
{
    const float mode = g_EffectDistortionShapeParams.x;
    const float outlineWidth = Compute_InfluenceOutlineWidth(cardUV);
    const float distanceFromCenter = distance(cardUV, float2(0.5f, 0.5f));
    float edgeDistance = min(min(cardUV.x, 1.f - cardUV.x), min(cardUV.y, 1.f - cardUV.y));

    if (mode >= 0.5f && mode < 1.5f)
        edgeDistance = abs(distanceFromCenter - max(g_EffectDistortionShapeParams.y, 0.f));
    else if (mode >= 1.5f && mode < 2.5f)
    {
        const float radius = max(g_EffectDistortionShapeParams.y, 0.f);
        const float thickness = max(g_EffectDistortionShapeParams.z, 0.f);
        edgeDistance = abs(abs(distanceFromCenter - radius) - thickness);
    }
    else if (mode >= 2.5f)
    {
        const float pixelUvWidth = EffectDistortion_ComputeShockwavePixelUvWidth(cardUV);
        const float thickness = max(g_EffectDistortionShapeParams.z, 0.f) * pixelUvWidth;
        const float softness = max(g_EffectDistortionShapeParams.w, 0.0001f) * pixelUvWidth;
        const float radiusMax = max(0.f, 0.5f - thickness - softness);
        const float radius = saturate(lifeProgress) * radiusMax;
        edgeDistance = abs(abs(distanceFromCenter - radius) - thickness);
    }

    return 1.f - smoothstep(outlineWidth, outlineWidth * 1.5f, edgeDistance);
}

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;

    const float lifeMax = max(In.lifeTime.x, 0.0001f);
    const float lifeProgress = saturate(In.lifeTime.y / lifeMax);
    const float4 particleColor = lerp(In.startColor, In.endColor, lifeProgress);
    const float2 sampleUV = lerp(In.subUVRect.xy, In.subUVRect.zw, In.texCoord);
    const float2 mainUV = Build_EffectSpriteUV(sampleUV, g_EffectSpriteMainUVParams, g_EffectSpriteUVOffsetParams.xy, g_EffectSpriteUVModeParams.x);
    const float2 noiseUV = Build_EffectSpriteUV(sampleUV, g_EffectSpriteNoiseUVParams, g_EffectSpriteUVOffsetParams.zw, g_EffectSpriteUVModeParams.y);
    const float2 maskUV = Build_EffectSpriteUV(sampleUV, g_EffectSpriteMaskUVParams, g_EffectSpriteMaskUVOffsetParams.xy, g_EffectSpriteUVModeParams.z);
    const float4 symbol = Sample_EffectSpriteMainTexture(mainUV, g_EffectSpriteUVModeParams.x);
    const float selectedOpacity = Effect_SelectOpacity(symbol, g_OpacitySource);
    const float opacityPower = max(g_EffectSpriteParams.y, 0.0001f);
    const float alphaCutoff = saturate(g_EffectSpriteAlphaParams.x);
    const float alphaErosion = saturate(g_EffectSpriteAlphaParams.y);
    float noiseFactor = 1.f;
    float maskAlpha = 1.f;

    if (0 != g_EffectSpriteParams.w && g_EffectSpriteParams.z > 0.f)
    {
        const float noise = Sample_EffectSpriteNoiseTexture(noiseUV, g_EffectSpriteUVModeParams.y);
        noiseFactor = lerp(1.f, noise, saturate(g_EffectSpriteParams.z));
    }

    if (0 != g_EffectSpriteAlphaParams.z)
        maskAlpha = Sample_EffectSpriteMaskTexture(maskUV, g_EffectSpriteUVModeParams.z);

    const float maskCoverage = Effect_ApplyAlphaErosion(maskAlpha, maskAlpha * noiseFactor, alphaErosion);
    const float materialCoverage = Effect_BuildCoverage(selectedOpacity, opacityPower, maskCoverage, noiseFactor, particleColor.a * g_Tint.a);
    if (materialCoverage < max(alphaCutoff, 0.01f))
        discard;

    const float2 flowUV = Build_EffectSpriteUV(sampleUV, float4(g_FlowUVScale, g_FlowUVScrollSpeed), g_FlowUVOffset, g_FlowUVTilingMode);
    const float2 flow = 0 != g_EffectDistortionParams.z
                        ? Sample_EffectSpriteFlowTexture(flowUV, g_FlowUVTilingMode) * 2.f - 1.f
                        : float2(0.f, 0.f);

    const float shapeCoverage = EffectDistortion_ComputeShapeCoverage(g_EffectDistortionShapeParams, In.texCoord, lifeProgress);
    const float lensCoverage = EffectDistortion_ComputeLensCoverage(materialCoverage, shapeCoverage, g_EffectDistortionParams.y);
    const float2 offset = EffectDistortion_ComputeScreenOffset(flow, g_EffectDistortionParams.x);
    Out.color = EffectDistortion_PackField(offset, lensCoverage);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VGP(DistortionPass, RS_CullNone, DSS_DepthRead, BS_Blend, VS_MAIN, GS_MAIN, PS_MAIN)
}
