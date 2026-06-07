#pragma once

#include "Engine_Typedef.h"

NS_BEGIN(Engine)

enum class EffectTextureUVTilingMode : uint32
{
    Wrap = 0,    // distance UV에 tiling을 곱하고 wrap sampler로 반복한다.
    Stretch = 1, // visible length 기준 0..1로 정규화하고 clamp sampler로 펼친다.
    Clamp = 2,   // distance UV에 tiling을 곱하되 clamp sampler로 범위 밖을 고정한다.
    Raw = 3,     // shader가 받은 distance UV를 tiling/stretch 없이 그대로 사용한다.
};

enum class EffectMaterialFamily : uint32
{
    Default = 0,      // 기존 color/coverage 중심 effect material family다.
    SpriteDistortion, // scene color snapshot을 screen-space offset으로 다시 샘플링하는 sprite distortion family다.
    END,
};

// SpriteDistortion family에서 screen UV offset 강도에 곱할 card-local 영향 범위를 고른다.
enum class EffectDistortionShapeMode : uint32
{
    None = 0,      // 기존 square card 전체 distortion 동작이다.
    Radial,        // sprite UV 중심 기준 soft circle/aura 계열 영향 범위다.
    Ring,          // sprite UV 중심 기준 shockwave rim 계열 영향 범위다.
    ShockwaveRing, // 수명 진행률로 확장하며 screen pixel 기준 두께를 유지하는 shockwave ring이다.
    END,
};

enum class EffectMaterialBlendMode : uint32
{
    AlphaBlend = 0,
    Additive,
    Modulate,
    Masked,
    END,
};

enum class EffectMaterialAdditiveColorSource : uint32
{
    MainRGB = 0,
    TintedRGB,
    EmissiveColor,
    ConstantColor,
    END,
};

enum class EffectMaterialAdditiveAmountSource : uint32
{
    Alpha = 0,
    Red,
    Luminance,
    Mask,
    One,
    END,
};

enum class EffectMaterialAdditiveCoveragePolicy : uint32
{
    AmountOnly = 0,
    CoverageAndAmount,
    Independent,
    END,
};

struct EffectMaterialAdditiveContributionData
{
    EffectMaterialAdditiveColorSource colorSource{ EffectMaterialAdditiveColorSource::MainRGB };
    EffectMaterialAdditiveAmountSource amountSource{ EffectMaterialAdditiveAmountSource::Alpha };
    EffectMaterialAdditiveCoveragePolicy coveragePolicy{ EffectMaterialAdditiveCoveragePolicy::AmountOnly };
    float intensityScale{ 1.f };
    bool blackNeutral{ true };
    Color emissiveColor{ 1.f, 1.f, 1.f, 1.f };
    Color constantColor{ 1.f, 1.f, 1.f, 1.f };
};

struct EffectMaterialCoreEmissiveData
{
    bool enabled{ false };
    Color coreColor{ 1.f, 0.85f, 0.45f, 1.f };
    float corePower{ 4.f };
    float coreIntensity{ 2.f };
    float outerPower{ 1.f };
    float outerIntensity{ 1.f };
};

// effect material preset/source에서 복사되어 runtime/authoring 쪽이 함께 보는 최소 material instance 값이다.
// sprite effect 경로에서는 mainTexturePath를 lit base color가 아니라 emissive color + opacity source로 해석한다.
struct EffectMaterialInstanceData
{
    string sourcePresetName{};                                            // UI 표시용 원본 preset 이름이다. 저장 identity나 live reference로 쓰지 않는다.
    EffectMaterialFamily materialFamily{ EffectMaterialFamily::Default }; // material shader/pass family를 고르는 상위 분류다.
    string mainTextureGuid{};                                             // 주 텍스처 asset identity다. resolve 실패 시 path를 fallback/debug hint로 사용한다.
    string mainTexturePath{};                                             // 주 텍스처 resource-relative fallback/debug hint다.
    string noiseTextureGuid{};                                            // 선택 noise 텍스처 asset identity다.
    string noiseTexturePath{};                                            // 선택 noise 텍스처 resource-relative fallback/debug hint다.
    string maskTextureGuid{};                                             // 선택 mask 텍스처 asset identity다.
    string maskTexturePath{};                                             // 선택 mask 텍스처 resource-relative fallback/debug hint다.
    string flowTextureGuid{};                                             // SpriteDistortion에서 screen UV offset source로 사용할 flow texture asset identity다.
    string flowTexturePath{};                                             // SpriteDistortion flow texture의 resource-relative fallback/debug hint다.

    Color tint{ 1.f, 1.f, 1.f, 1.f }; // main texture RGB emissive-like 색상에 곱할 색상 계수다.

    float intensity{ 1.f };       // sprite 밝기/발광감 계수다.
    float opacityPower{ 1.f };    // opacity source(alpha/red/luminance)의 sharpening 계수다.
    float noiseStrength{ 0.f };   // noise modulation 강도다.
    float alphaCutoff{ 0.f };     // alpha cutoff 후보 값이다. 소비하지 않는 renderer에서는 보존만 한다.
    float alphaErosion{ 0.f };    // mask/noise 기반 alpha erosion 후보 값이다. 소비하지 않는 renderer에서는 보존만 한다.
    string noiseSource{ "Red" };  // noise texture에서 scalar를 뽑을 채널/방식이다.
    string maskSource{ "Alpha" }; // mask texture에서 coverage를 뽑을 채널/방식이다.
    bool noiseInvert{ false };    // noise scalar를 1-x로 뒤집어 사용한다.
    bool maskInvert{ false };     // mask coverage를 1-x로 뒤집어 사용한다.

    Vec2 mainUVScale{ 1.f, 1.f };                                                      // main texture UV scale 후보 값이다.
    Vec2 mainUVOffset{ 0.f, 0.f };                                                     // main texture UV 시작 위치를 정적으로 민다.
    Vec2 mainUVScrollSpeed{ 0.f, 0.f };                                                // main texture UV scroll speed 후보 값이다.
    EffectTextureUVTilingMode mainUVTilingMode{ EffectTextureUVTilingMode::Stretch };  // main texture UV tiling 해석 방식이다.
    Vec2 noiseUVScale{ 1.f, 1.f };                                                     // noise texture UV scale 후보 값이다.
    Vec2 noiseUVOffset{ 0.f, 0.f };                                                    // noise texture UV 시작 위치를 정적으로 민다.
    Vec2 noiseUVScrollSpeed{ 0.f, 0.f };                                               // noise texture UV scroll speed 후보 값이다.
    EffectTextureUVTilingMode noiseUVTilingMode{ EffectTextureUVTilingMode::Stretch }; // noise texture UV tiling 해석 방식이다.
    Vec2 maskUVScale{ 1.f, 1.f };                                                      // mask texture UV scale 후보 값이다.
    Vec2 maskUVOffset{ 0.f, 0.f };                                                     // mask texture UV 시작 위치를 정적으로 민다.
    Vec2 maskUVScrollSpeed{ 0.f, 0.f };                                                // mask texture UV scroll speed 후보 값이다.
    EffectTextureUVTilingMode maskUVTilingMode{ EffectTextureUVTilingMode::Stretch };  // mask texture UV tiling 해석 방식이다.
    Vec2 flowUVScale{ 1.f, 1.f };                                                      // flow texture UV scale 후보 값이다.
    Vec2 flowUVOffset{ 0.f, 0.f };                                                     // flow texture UV 시작 위치를 정적으로 민다.
    Vec2 flowUVScrollSpeed{ 0.f, 0.f };                                                // flow texture UV scroll speed 후보 값이다.
    EffectTextureUVTilingMode flowUVTilingMode{ EffectTextureUVTilingMode::Stretch };  // flow texture UV tiling 해석 방식이다.

    EffectMaterialBlendMode blendMode{ EffectMaterialBlendMode::AlphaBlend };         // effect material blend/compose mode다.
    string opacitySource{ "Alpha" };                                                  // main texture에서 opacity를 뽑을 채널/방식이다.
    EffectMaterialAdditiveContributionData additive{};                                // Additive blend에서 더할 contribution 해석 규칙이다.
    EffectMaterialCoreEmissiveData coreEmissive{};                                    // alpha-derived inner core/outer halo shaping 값이다.
    float refractionIntensity{ 0.f };                                                 // SpriteDistortion screen UV offset 강도다.
    float refractionPresence{ 1.f };                                                  // SpriteDistortion refraction 영향도를 켜고 줄이는 authored scalar다.
    EffectDistortionShapeMode distortionShapeMode{ EffectDistortionShapeMode::None }; // SpriteDistortion 영향 범위 weight mode다.
    float distortionShapeRadius{ 0.45f };                                             // sprite UV 중심에서 shape 기준 반경이다.
    float distortionShapeThickness{ 0.10f };                                          // Ring은 UV 폭, ShockwaveRing은 screen pixel half-width다.
    float distortionShapeSoftness{ 0.15f };                                           // Ring은 UV falloff, ShockwaveRing은 screen pixel falloff다.
    bool twoSided{ false };                                                           // render state 후보 값이다. 소비하지 않는 renderer에서는 보존만 한다.
    uint32 subUVRows{ 1 };                                                            // atlas row 수다.
    uint32 subUVCols{ 1 };                                                            // atlas column 수다.
};

NS_END
