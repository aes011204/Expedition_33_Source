#include "Engine_Shader_Defines.hlsli"
#include "Engine_Shader_ConstantBuffer.hlsl"
#include "Engine_Shader_Passes.hlsl"
#include "Engine_Shader_RenderState.hlsl"
#include "Engine_Shader_Samplers.hlsl"
#include "Engine_Shader_EffectMaterial.hlsl"

cbuffer cbEffectSpriteMaterial : register(b2)
{
    float4 g_Tint = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectSpriteParams = { 1.f, 1.f, 0.f, 0.f };         // x: emissive-like intensity, y: opacityPower, z: noiseStrength, w: useNoise
    float4 g_EffectSpriteAlphaParams = { 0.f, 0.f, 0.f, 0.f };    // x: alphaCutoff, y: alphaErosion, z: useMask, w: reserved
    float4 g_EffectSpriteAdditiveParams = { 0.f, 0.f, 0.f, 1.f }; // x: colorSource, y: amountSource, z: coveragePolicy, w: intensityScale
    float4 g_EffectSpriteAdditiveEmissiveColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectSpriteAdditiveConstantColor = { 1.f, 1.f, 1.f, 1.f };
    float4 g_EffectSpriteAdditiveFlags = { 1.f, 0.f, 0.f, 0.f }; // x: blackNeutral
    int g_OpacitySource = 0;
    float3 g_EffectSpritePadding;
};

cbuffer cbEffectSpriteRuntime : register(b3)
{
    int g_ScreenAlignmentMode = 0;
    int g_DirectionalAlignmentMode = 1;
    int g_SpriteTextureAxis = 1;
    float g_SpriteRollOffsetRadians = 0.f;
    float4 g_EffectSpriteEmitterCenter = { 0.f, 0.f, 0.f, 1.f };
};

Texture2D g_Texture : register(t0); // sprite effect의 primary texture다. 현재 경로에서는 RGB를 emissive-like color source로 해석한다.
Texture2D g_NoiseTexture : register(t1);
Texture2D g_MaskTexture : register(t2);

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

// emitter 중심에서 바깥쪽을 향하는 정규화 look 방향을 구한다.
float3 ResolveAwayFromCenterLookDirection(float3 particlePosition)
{
    float3 lookDir = particlePosition - g_EffectSpriteEmitterCenter.xyz;
    const float lookLength = length(lookDir);
    return lookLength < 0.0001f ? float3(0.f, 0.f, 1.f) : lookDir / lookLength;
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

// texture 축 하나를 지정 방향에 맞추는 billboard basis를 시도한다.
bool TryResolveTextureAxisBasis(float3 axisDir, out float3 rightDir, out float3 upDir)
{
    ResolveCameraPlaneBasis(rightDir, upDir);

    const float3 planeNormal = normalize(cross(rightDir, upDir));
    float3 projectedAxis = axisDir - planeNormal * dot(axisDir, planeNormal);
    const float projectedLength = length(projectedAxis);
    if (projectedLength < 0.0001f)
        return false;

    projectedAxis /= projectedLength;

    if (g_SpriteTextureAxis == 0)
    {
        rightDir = projectedAxis;
        upDir = normalize(cross(planeNormal, rightDir));
    }
    else
    {
        upDir = projectedAxis;
        rightDir = normalize(cross(upDir, planeNormal));
    }

    return true;
}

[maxvertexcount(4)]
void GS_MAIN(point GS_IN In[1], inout TriangleStream<GS_OUT> OutStream)
{
    const float3 particlePosition = In[0].worldPosition.xyz;
    float3 rightDir = float3(1.f, 0.f, 0.f);
    float3 upDir = float3(0.f, 1.f, 0.f);
    if (g_ScreenAlignmentMode == 1 || g_ScreenAlignmentMode == 2)
        ResolveCameraPlaneBasis(rightDir, upDir);
    else if (g_ScreenAlignmentMode == 3)
    {
        const float3 lookDir = ResolveAwayFromCenterLookDirection(particlePosition);

        if (g_DirectionalAlignmentMode == 1) // 스프라이트의 X 또는 Y 가 바라보도록
        {
            if (!TryResolveTextureAxisBasis(lookDir, rightDir, upDir))
                ResolveCameraPlaneBasis(rightDir, upDir);
        }
        else // 스프라이트의 look 이 바라보도록
            ResolveLookAlignedBasis(lookDir, rightDir, upDir);
    }
    else if (g_ScreenAlignmentMode == 4)
    {
        const float velocityLength = length(In[0].velocityDirection);
        const float3 lookDir = velocityLength < 0.0001f
                               ? ResolveFacingLookDirection(particlePosition)
                               : In[0].velocityDirection / velocityLength;

        if (g_DirectionalAlignmentMode == 1 && velocityLength >= 0.0001f)
        {
            if (!TryResolveTextureAxisBasis(lookDir, rightDir, upDir))
                ResolveLookAlignedBasis(lookDir, rightDir, upDir);
        }
        else
            ResolveLookAlignedBasis(lookDir, rightDir, upDir);
    }
    else
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
    GS_OUT Out[4];

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

EffectMaterialSurface Resolve_EffectMaterialSurface(PS_IN In)
{
    EffectMaterialSurface surface = (EffectMaterialSurface)0;

    const float lifeMax = max(In.lifeTime.x, 0.0001f);
    const float lifeProgress = saturate(In.lifeTime.y / lifeMax);
    const float4 particleColor = lerp(In.startColor, In.endColor, lifeProgress);
    const float2 sampleUV = lerp(In.subUVRect.xy, In.subUVRect.zw, In.texCoord);
    const float4 symbol = g_Texture.Sample(LinearClampSampler, sampleUV);
    const float opacityPower = max(g_EffectSpriteParams.y, 0.0001f);
    const float alphaCutoff = saturate(g_EffectSpriteAlphaParams.x);
    const float alphaErosion = saturate(g_EffectSpriteAlphaParams.y);
    const float selectedOpacity = Effect_SelectOpacity(symbol, g_OpacitySource);
    float noiseFactor = 1.f;
    float maskAlpha = 1.f;

    if (0 != g_EffectSpriteParams.w && g_EffectSpriteParams.z > 0.f)
    {
        const float noise = g_NoiseTexture.Sample(LinearClampSampler, sampleUV).r;
        noiseFactor = lerp(1.f, noise, saturate(g_EffectSpriteParams.z));
    }

    if (0 != g_EffectSpriteAlphaParams.z)
        maskAlpha = g_MaskTexture.Sample(LinearClampSampler, sampleUV).a;

    const float maskCoverage = Effect_ApplyAlphaErosion(maskAlpha, maskAlpha * noiseFactor, alphaErosion);
    surface.color = symbol.rgb * particleColor.rgb * g_Tint.rgb * noiseFactor;
    surface.coverage = Effect_BuildCoverage(selectedOpacity, opacityPower, maskCoverage, noiseFactor, particleColor.a * g_Tint.a);

    if (surface.coverage < max(alphaCutoff, 0.01f))
        discard;

    return surface;
}

PS_OUT PS_MAIN(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;
    const EffectMaterialSurface surface = Resolve_EffectMaterialSurface(In);
    Out.color = Effect_ComposeAlphaBlend(surface, g_EffectSpriteParams.x);
    return Out;
}

PS_OUT PS_ADDITIVE(PS_IN In)
{
    PS_OUT Out = (PS_OUT)0;

    const float lifeMax = max(In.lifeTime.x, 0.0001f);
    const float lifeProgress = saturate(In.lifeTime.y / lifeMax);
    const float4 particleColor = lerp(In.startColor, In.endColor, lifeProgress);
    const float2 sampleUV = lerp(In.subUVRect.xy, In.subUVRect.zw, In.texCoord);
    const float4 symbol = g_Texture.Sample(LinearClampSampler, sampleUV);
    const float alphaCutoff = saturate(g_EffectSpriteAlphaParams.x);
    const float alphaErosion = saturate(g_EffectSpriteAlphaParams.y);
    float noiseFactor = 1.f;
    float maskAlpha = 1.f;

    if (0 != g_EffectSpriteParams.w && g_EffectSpriteParams.z > 0.f)
    {
        const float noise = g_NoiseTexture.Sample(LinearClampSampler, sampleUV).r;
        noiseFactor = lerp(1.f, noise, saturate(g_EffectSpriteParams.z));
    }

    if (0 != g_EffectSpriteAlphaParams.z)
        maskAlpha = g_MaskTexture.Sample(LinearClampSampler, sampleUV).a;

    const float maskCoverage = Effect_ApplyAlphaErosion(maskAlpha, maskAlpha * noiseFactor, alphaErosion);
    const float coverageBase = saturate(maskCoverage * noiseFactor * particleColor.a * g_Tint.a);
    const int colorSource = (int)g_EffectSpriteAdditiveParams.x;
    const int amountSource = (int)g_EffectSpriteAdditiveParams.y;
    const int coveragePolicy = (int)g_EffectSpriteAdditiveParams.z;
    const float amount = Effect_SelectAdditiveAmount(symbol, maskAlpha, amountSource);
    const float policyAmount = Effect_ResolveAdditivePolicyAmount(amount, coverageBase, coveragePolicy);
    const float discardSource = Effect_ResolveAdditiveDiscardSource(amount, coverageBase, coveragePolicy);
    const float blackGate = Effect_ResolveAdditiveBlackGate(symbol.rgb, colorSource, g_EffectSpriteAdditiveFlags.x);

    if (discardSource < max(alphaCutoff, 0.01f))
        discard;

    const float3 materialColor = Effect_ResolveAdditiveColor(
        symbol.rgb,
        symbol.rgb * g_Tint.rgb,
        g_EffectSpriteAdditiveEmissiveColor,
        g_EffectSpriteAdditiveConstantColor,
        colorSource
    );
    const float intensity = max(g_EffectSpriteParams.x, 0.f) * max(g_EffectSpriteAdditiveParams.w, 0.f);
    const float3 contribution = materialColor * particleColor.rgb * noiseFactor * policyAmount * intensity * blackGate;
    Out.color = float4(contribution, 1.f);
    return Out;
}

technique11 DefaultTechnique
{
    PASS_RS_DS_BS_VGP(AlphaBlendPass, RS_CullNone, DSS_DepthRead, BS_EffectAlphaBlend, VS_MAIN, GS_MAIN, PS_MAIN) // 0
    PASS_RS_DS_BS_VGP(AdditivePass, RS_CullNone, DSS_DepthRead, BS_EffectAdditive, VS_MAIN, GS_MAIN, PS_ADDITIVE) // 1
}
