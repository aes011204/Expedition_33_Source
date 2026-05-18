#pragma once

NS_BEGIN(Engine)

/* ------------------------------------------ */
/*             Material CBuffer               */
/* ------------------------------------------ */
struct alignas(16) MaterialCB
{
    static constexpr ConstantBuffer CBType = ConstantBuffer::Material;

    Vec4 baseColorTint{ 1.f, 1.f, 1.f, 1.f };
    Vec4 emissiveColor{ 1.f, 1.f, 1.f, 1.f };

    // MI json의 Scalars.Value_Glow 계열
    float emissiveIntensity{ 0.f };

    // MI json의 Scalars.Glow_Mask_Mult 계열
    float emissiveMaskMul{ 1.f };

    // MI json의 Normal/Bump Multiplier 또는 NormalIntensity 계열
    float normalStrength{ 1.f };

    // 셰이더에서 clip 기준으로 사용
    float opacityMaskClipValue{ 0.3333f };

    // ORM 보정값
    float aoScale{ 1.f };           // ORM.r = AO
    float roughnessScale{ 1.f };    // ORM.g = Roughness
    float metalnessScale{ 1.f };    // ORM.b = Metalness
    float _pad0{ 0.f };

    Vec2 uvScale{ 1.f, 1.f };
    Vec2 uvOffset{ 0.f, 0.f };
    float uvRotation{ 0.f };

    uint32 materialMask{ 0 };
    float _pad1[2]{};
};

/* ------------------------------------------ */
/*             Camera CBuffer                 */
/* ------------------------------------------ */
struct alignas(16) CameraCB
{
    static constexpr ConstantBuffer CBType = ConstantBuffer::Camera;

    Matrix  viewMatrix;             // 64
    Matrix  projMatrix;             // 64
    Matrix  viewInverseMatrix;      // 64
    Matrix  projInverseMatrix;      // 64

    Vec4    cameraPosition;         // 16
            
    float   farPlane = 0.f;         // 4
    float   _pad0[3]{};             // 12
};

/* ------------------------------------------ */
/*             Object CBuffer                 */
/* ------------------------------------------ */
struct alignas(16) ObjectCB
{
    static constexpr ConstantBuffer CBType = ConstantBuffer::Object;

    Matrix worldMatrix;
};

/* ------------------------------------------ */
/*             Shadow CBuffer                 */
/* ------------------------------------------ */
struct alignas(16) ShadowCB
{
    static constexpr ConstantBuffer CBType = ConstantBuffer::Shadow;

    Matrix  lightViewMatrix;
    Matrix  lightProjMatrix;

    Matrix  staticLightViewMatrix;
    Matrix  staticLightProjMatrix;

    float   lightFar = 0.f;
    float   bias = 0.f;
    float   intensity = 0.f;
    float   staticShadowEnabled = 0.f;
};

/* ------------------------------------------ */
/*             Deferred CBuffer               */
/* ------------------------------------------ */
struct alignas(16) DeferredCB
{
    static constexpr ConstantBuffer CBType = ConstantBuffer::Deferred;

    Matrix  screenViewMatrix;
    Matrix  screenProjMatrix;

    Matrix  viewInverseMatrix;
    Matrix  projInverseMatrix;

    Vec4    cameraPosition;

    float   farPlane = 0.f;
    Vec2    screenSize = { 1600.f, 900.f };
    float   _pad0;
};

/* ------------------------------------------ */
/*             Light CBuffer                  */
/* ------------------------------------------ */
struct alignas(16) LightCB
{
    static constexpr ConstantBuffer CBType = ConstantBuffer::Light;

    Vec4 direction;
    Vec4 position;

    Vec4 ambient;
    Vec4 diffuse;
    Vec4 specular;

    float range = 0.f;
    float _pad0[3]{};
};

/* ------------------------------------------ */
/*           PostProcess CBuffer              */
/* ------------------------------------------ */
struct alignas(16) PostProcessCB
{
    static constexpr ConstantBuffer CBType = ConstantBuffer::PostProcess;

    float exposure = 1.f;                   // 키우면 밝아짐
    float invWhitePoint = 1.f;              // 어느 밝기 이상을 거의 흰색으로 볼 건지
    float gamma = 2.2f;                     // 선형 색상을 sRGB에 가깝게 변환함
    float bloomRadius = 1.6f;               // 블러 커널의 샘플 간격을 늘리는 배율

    float bloomThreshold = 0.9f;            // Bloom 추출 기준
    float bloomIntensity = 0.4f;            // Bloom 의 강도
    float bloomSoftKnee = 0.7f;             // Bloom Threshold 경계선을 부드럽게 만드는 값
    float bloomClamp = 10.f;                // Bloom 추출 시 너무 강하지 않게 제한 값

    float bloomScatter = 0.7f;              // Bloom 이 얼마나 퍼져 보일지 조정하는 값
    uint32 toneMapMode = 0;                 // Reinhard, ACES, Uncharted2
    Vec2 bloomTexelSize{ 0.f, 0.f };    // Bloom 텍스처의 한 픽셀 크기
        
    uint32 featureMask = 0;                 // 적용할 Feature
    float _pad2[3]{};
};

/* ------------------------------------------ */
/*                 SSAO CBuffer               */
/* ------------------------------------------ */
struct alignas(16) SSAOCB
{
    // SSAO 계산 품질/강도
    float g_SSAORadius = 0.5f;
    float g_SSAOBias = 0.025f;
    float g_SSAOIntensity = 1.0f;
    float g_SSAOPower = 1.2f;

    // Blur / Bilateral Blur
    float g_SSAOBlurRadius = 1.0f;
    float g_SSAODepthSharpness = 100.0f;
    float g_SSAONormalSharpness = 8.0f;
    uint32 g_SSAOSampleCount = 16;

    // 기능 On/Off
    uint32 g_SSAOEnabled = 1;
    uint32 g_SSAOHalfResEnabled = 0;
    uint32 g_SSAOBilateralEnabled = 0;
    uint32 g_SSAOTemporalEnabled = 0;

    // Half-res / Upsampling / Temporal Denoising
    float g_SSAOUpsampleDepthThreshold = 0.01f;
    float g_SSAOTemporalBlendFactor = 0.9f;
    float g_SSAOTemporalDepthReject = 0.02f;
    float g_SSAOTemporalNormalReject = 0.9f;
};

// 패딩이 제대로 됐나 마지막 확인용
static_assert(sizeof(MaterialCB) % 16 == 0);
static_assert(sizeof(CameraCB) % 16 == 0);
static_assert(sizeof(ObjectCB) % 16 == 0);
static_assert(sizeof(ShadowCB) % 16 == 0);
static_assert(sizeof(DeferredCB) % 16 == 0);
static_assert(sizeof(LightCB) % 16 == 0);
static_assert(sizeof(PostProcessCB) % 16 == 0);
static_assert(sizeof(SSAOCB) % 16 == 0);

NS_END

