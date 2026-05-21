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

    // Raw SSAO 핵심 파라미터.
    // 현재 구현은 view-space 반구 샘플링이므로 Radius는 "화면 픽셀 반경"이 아니라
    // 현재 픽셀 주변을 3D 공간에서 얼마나 멀리 볼지 정하는 반경으로 해석된다.
    float SSAORadius = 0.2f;                      // view-space 샘플 반경. 값이 커질수록 넓은 홈/바닥까지 AO가 쉽게 들어간다.
    float SSAOBias = 0.1f;                      // 자기 표면을 가려진 것으로 잘못 세지 않도록 밀어주는 깊이 여유값.
    float SSAOIntensity = 0.7f;                   // Combined 단계에서 AO 결과를 얼마나 강하게 섞을지.
    float SSAOPower = 1.2f;                       // Combined 단계에서 AO 대비를 얼마나 더 눌러줄지.

    float SSAODepthRange = 1.0f;                  // sample depth와 center depth 차이가 클 때 AO 기여를 얼마나 빨리 감쇠할지.
    float SSAOBlurRadius = 4.0f;                  // Bilateral Blur 반경. 0에 가까울수록 raw SSAO 형태를 더 많이 유지한다.
    float SSAODepthSharpness = 100.0f;            // Blur에서 깊이 경계를 얼마나 강하게 보존할지.
    float SSAONormalSharpness = 8.0f;             // Blur에서 노멀 경계를 얼마나 강하게 보존할지.

    uint32 SSAOSampleCount = 16;                  // raw SSAO 샘플 개수. 현재 셰이더 커널 최대치는 16개다.
    uint32 SSAOEnabled = 1;                       // SSAO 패스를 실행할지.
    uint32 SSAOBilateralEnabled = 1;              // raw SSAO 뒤에 bilateral blur 패스를 추가로 돌릴지.
    uint32 SSAOPad0 = 0;                          // 16-byte 정렬 유지용 패딩.
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

