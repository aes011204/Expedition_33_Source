#pragma once

NS_BEGIN(Engine)

/* ------------------------------------------ */
/*             Material CBuffer               */
/* ------------------------------------------ */
struct alignas(16) MaterialCB
{
    static constexpr ConstantBuffer CBType = ConstantBuffer::Material;

    Vec4 baseColorTint{ 1.f, 1.f, 1.f, 1.f }; // BaseColor 최종 색에 곱하는 tint다. 전체 색감을 바꾸고 싶을 때 만지고, 원본 색을 유지하려면 1,1,1,1로 둔다.
    Vec4 emissiveColor{ 1.f, 1.f, 1.f, 1.f }; // Emissive 텍스처에 입힐 색이다. 발광 색조를 바꾸고 싶을 때만 조정하고, 텍스처 원색을 살리려면 흰색에 가깝게 둔다.

    // emissiveColor가 곱해진 발광 세기다. 텍스처 원색만 쓰고 싶으면 0으로 두고, 색 입힌 발광을 키우고 싶으면 올린다.
    float emissiveIntensity{ 0.f };

    // Emissive 텍스처 원색이 얼마나 직접 더해질지 정한다. 현재 셰이더에서는 0이면 원본 발광이 꺼지고, 1이면 텍스처 기여를 그대로 쓴다.
    float emissiveMaskMul{ 1.f };

    // 노멀맵 굴곡을 얼마나 강하게 보일지 정한다. 1이 기본이고, 0에 가까울수록 평평해지며 너무 크면 표면이 과장돼 보일 수 있다.
    float normalStrength{ 1.f };

    // Masked 머테리얼에서 어느 값부터 보일지 정하는 컷오프다. 값을 올리면 더 많이 잘리고, 내리면 더 많이 남는다.
    float opacityMaskClipValue{ 0.3333f };

    // ORM 보정값.
    float aoScale{ 1.f };           // ORM에서 읽은 AO에 곱하는 배율이다. 낮추면 틈 그림자가 약해지고, 높이면 음영이 더 눌린다.
    float roughnessScale{ 1.f };    // ORM에서 읽은 roughness에 곱한다. 높일수록 더 거칠고 덜 반짝여 보이고, 낮출수록 더 매끈해 보인다.
    float metalnessScale{ 1.f };    // ORM에서 읽은 metalness에 곱한다. 높일수록 금속 느낌이 강해지고, 0에 가까우면 비금속 쪽으로 간다.
    float _pad0{ 0.f };

    Vec2 uvScale{ 1.f, 1.f }; // 텍스처 UV 타일링 배율이다. 2면 두 배 촘촘해지고, 0.5면 두 배 크게 늘어나 보인다.
    Vec2 uvOffset{ 0.f, 0.f }; // 텍스처 UV 시작 위치를 민다. 흐르는 패턴이나 무늬 위치 미세조정에 쓰고, 0이면 원래 위치다.
    float uvRotation{ 0.f }; // UV를 회전시키는 각도다. 기본 0으로 두고, 방향성이 있는 패턴을 돌리고 싶을 때만 조정한다.

    uint32 materialMask{ 0 }; // 머테리얼 기능/슬롯 상태를 셰이더에 알리는 비트마스크다. 사용자가 직접 튜닝하는 값이라기보다 시스템 플래그에 가깝다.
    uint32 ormAoSource{ MaterialChannel_R }; // ORM 텍스처의 어느 채널을 AO로 읽을지 정한다. 보통 R이고, packed 텍스처 구성이 다를 때만 바꾼다.
    uint32 ormRoughnessSource{ MaterialChannel_G }; // ORM 텍스처의 어느 채널을 roughness로 읽을지 정한다. 보통 G를 쓰고, 채널 packing이 다를 때만 변경한다.
    uint32 ormMetalnessSource{ MaterialChannel_B }; // ORM 텍스처의 어느 채널을 metalness로 읽을지 정한다. 보통 B를 쓰고, packed 소스가 다를 때만 변경한다.
    uint32 opacityMaskSource{ MaterialChannel_R }; // 마스크 텍스처의 어느 채널을 clip 기준값으로 볼지 정한다. 흑백 마스크가 다른 채널에 들어 있을 때만 바꾼다.
    uint32 _pad1[2]{};
};

/* ------------------------------------------ */
/*              Hair CBuffer                  */
/* ------------------------------------------ */
struct alignas(16) HairCB
{
    static constexpr ConstantBuffer CBType = ConstantBuffer::Hair;

    Vec4 g_HairColorTop{ 0.048f, 0.053f, 0.053f, 1.f };
    Vec4 g_HairColorDown { 0.965f, 0.937f, 0.929f, 1.f};
    Vec4 hairHighlightColor{ 0x6C / 255.f, 0x70 / 255.f, 0x75 / 255.f, 0.f };

    float opacityMaskClipValue{ 0.3333f };
    float roughness{ 0.55f };
    float specular{ 0.1f };
    float scatter{ 0.1f };

    float brightness{ 1.0f };
    float rim{ 1.0f };
    float randomHueVariation{ 0.1338f };
    float randomValueVariation{ 1.0f };

    float bumpStrength{ 0.3f };
    float bumpDistance{ 0.001f };
    float gradientColor{ 0.226f };
    float gradientContrast{ 0.35f };

    float gradientHardness{ 2.0f };
    float gradientHeight{ 0.25f };
    float edgeContrast{ -0.35f };
    float edgeMin{ 6.0f };
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

    Vec2    texelSize = { 1.f/1600.f, 1.f/900.f };
    Vec2    screenSize = { 1600.f, 900.f };
    float   farPlane = 0.f;
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
    float SSAORadius = 0.1f;                      // view-space 샘플 반경. 값이 커질수록 넓은 홈/바닥까지 AO가 쉽게 들어간다.
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

/* ------------------------------------------ */
/*        DeferredLighting CBuffer            */
/* ------------------------------------------ */
struct alignas(16) DeferredLightingCB
{
    static constexpr ConstantBuffer CBType = ConstantBuffer::DeferredLighting;

    float iblIntensity{ 1.f };
    float iblDiffuseIntensity{ 1.f };
    float iblSpecularIntensity{ 1.f };
    float iblPrefilterMaxMip{ 8.f };

    uint32 iblEnabled{ 1u };
    uint32 _pad0[3]{};
};

/* ------------------------------------------ */
/*            DrawFeature CBuffer             */
/* ------------------------------------------ */
struct alignas(16) DrawFeatureCB
{
    static constexpr ConstantBuffer CBType = ConstantBuffer::DrawFeature;

    uint32 activeFeatureMask = 0;       // 이번 Draw에서 켜고 싶은 기능
    uint32 supportFeatureMask = 0;      // 지원가능한 기능
    uint32 textureSlotMask = 0;         // 실제로 바인딩된 슬롯
    uint32 _pad0 = 0;

    // 여러 효과에서 알아서 쓰일 parameters. 기능별 .cpp과 .hlsl 간 규약 필요
    Vec4 param0{ 0.f, 0.f, 0.f, 0.f };  
    Vec4 param1{ 0.f, 0.f, 0.f, 0.f };
};

struct alignas(16) ScreenFxCB
{
    static constexpr ConstantBuffer CBType = ConstantBuffer::ScreenFx;

    Vec3 vignetteColor{ 0.f, 0.f, 0.f };
    float vignetteIntensity{ 1.0f };
    float vignetteRadius{ 0.75f };
    float vignetteSoftness{ 0.5f };
    float ghostingIntensity{ 0.5f };
    float transitionProgress{ 0.0f };

    Vec3 transitionColor{ 0.f, 0.f, 0.f };
    float radialBlurIntensity{ 0.0f };

    // Distortion
    Vec2 radialBlurCenter{ 0.5f, 0.5f }; 
    float radialBlurRadius{ 1.0f };      
    float chromaticAberrationIntensity{ 0.0f }; 
    float bulgeIntensity{ 0.f };
    float pullIntensity{ 0.f };
    Vec2 pullDirection{ -1.f, 1.f };

    // Transition
    float wipeRadius{ 0.f };
    float edgeSoftness{ 0.05f };
    Vec2 pad;
};

// 패딩이 제대로 됐나 마지막 확인용
static_assert(sizeof(MaterialCB) % 16 == 0);
static_assert(sizeof(HairCB) % 16 == 0);
static_assert(sizeof(CameraCB) % 16 == 0);
static_assert(sizeof(ObjectCB) % 16 == 0);
static_assert(sizeof(ShadowCB) % 16 == 0);
static_assert(sizeof(ScreenSpaceCB) % 16 == 0);
static_assert(sizeof(LightCB) % 16 == 0);
static_assert(sizeof(PostProcessCB) % 16 == 0);
static_assert(sizeof(SSAOCB) % 16 == 0);
static_assert(sizeof(DeferredLightingCB) % 16 == 0);
static_assert(sizeof(DrawFeatureCB) % 16 == 0);
static_assert(sizeof(ScreenFxCB) % 16 == 0);

NS_END

