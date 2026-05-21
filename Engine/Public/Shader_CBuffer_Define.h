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
/*             ScreenSpaceCB CBuffer          */
/* ------------------------------------------ */
struct alignas(16) ScreenSpaceCB
{
    static constexpr ConstantBuffer CBType = ConstantBuffer::ScreenSpace;

    Matrix  screenViewMatrix;
    Matrix  screenProjMatrix;

    Matrix  viewInverseMatrix;
    Matrix  projInverseMatrix;

    Vec4    cameraPosition;

    float   farPlane = 0.f;
    Vec2    texelSize = { 1.f/1600.f, 1.f/900.f };
    Vec2    screenSize = { 1600.f, 900.f };
    float   pad[3];
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
    uint32 toneMapMode = 2;                 // 0: Reinhard, 1: ACES, 2: Uncharted2
        
    uint32 featureMask = 0;                 // 적용할 Feature
    uint32 useBackgroundFallback = 0;       // 0이면 비활성, 1이면 빈 픽셀 fallback 색 적용
    float _pad2[4]{};
    Vec4 backgroundFallbackColor{ 0.f, 0.f, 0.f, 1.f }; // fallback 배경색
};

/* ------------------------------------------ */
/*                 SSAO CBuffer               */
/* ------------------------------------------ */
struct alignas(16) SSAOCB
{
    static constexpr ConstantBuffer CBType = ConstantBuffer::SSAO;

    // SSAO 계산 품질/강도
    float SSAORadius = 4.0f;                      // 화면상 주변을 얼마나 멀리 샘플링할지.
    float SSAOBias = 0.025f;                      // 자기 자신과 거의 같은 깊이는 ao 로 안 넘김.
    float SSAOIntensity = 1.0f;                   // AO 강도. 클수록 더 어두워짐.
    float SSAOPower = 1.2f;                       // AO 대비 조절. 클수록 어두운 부분이 더 강조됨.

    float SSAODepthRange = 0.5f;                  // viewZ 기준으로 AO로 인정할 최대 깊이 차이.

    // Blur / Bilateral Blur
    float SSAOBlurRadius = 1.0f;                  // Blur 단계에서 주변 몇 픽셀까지 흐릴지.
    float SSAODepthSharpness = 100.0f;            // 깊이 차이가 클 때 AO 영향을 얼마나 빨리 줄일지.
    float SSAONormalSharpness = 8.0f;             // Normal 기반 Bilateral Blur에서 노멀 차이를 얼마나 민감하게 볼지.
    uint32 SSAOSampleCount = 16;                  // 샘플 개수, 최대 16개.

    // 기능 On/Off
    uint32 SSAOEnabled = 1;                       // SSAO On.Off
    uint32 SSAOHalfResEnabled = 0;                // SSAO를 절반 해상도로 계산할지.
    uint32 SSAOBilateralEnabled = 0;              // Bilateral Blur를 쓸지.
    uint32 SSAOTemporalEnabled = 0;               // Temporal SSAO를 쓸지.

    // Half-res / Upsampling / Temporal Denoising
    float SSAOUpsampleDepthThreshold = 0.01f;     // Half-res SSAO를 원본 해상도로 키울 때 깊이 차이 허용값.
    float SSAOTemporalBlendFactor = 0.9f;         // 이전 프레임 AO와 현재 프레임 AO를 얼마나 섞을지.
    float SSAOTemporalDepthReject = 0.02f;        // Temporal에서 깊이 차이가 크면 이전 프레임을 버리는 기준.
    float SSAOTemporalNormalReject = 0.9f;        // Temporal에서 노멀 차이가 크면 이전 프레임을 버리는 기준.

    float pad0[3];
};

// 패딩이 제대로 됐나 마지막 확인용
static_assert(sizeof(MaterialCB) % 16 == 0);
static_assert(sizeof(CameraCB) % 16 == 0);
static_assert(sizeof(ObjectCB) % 16 == 0);
static_assert(sizeof(ShadowCB) % 16 == 0);
static_assert(sizeof(ScreenSpaceCB) % 16 == 0);
static_assert(sizeof(LightCB) % 16 == 0);
static_assert(sizeof(PostProcessCB) % 16 == 0);
static_assert(sizeof(SSAOCB) % 16 == 0);

NS_END

