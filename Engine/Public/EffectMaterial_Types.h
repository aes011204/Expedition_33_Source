#pragma once

#include "Engine_Typedef.h"

NS_BEGIN(Engine)

// effect material preset/source에서 복사되어 runtime/authoring 쪽이 함께 보는 최소 material instance 값이다.
// sprite effect 경로에서는 mainTexturePath를 lit base color가 아니라 emissive color + opacity source로 해석한다.
struct EffectMaterialInstanceData
{
    string sourcePresetName{}; // UI 표시용 원본 preset 이름이다. 저장 identity나 live reference로 쓰지 않는다.
    string mainTextureGuid{};  // 주 텍스처 asset identity다. resolve 실패 시 path를 fallback/debug hint로 사용한다.
    string mainTexturePath{};  // 주 텍스처 resource-relative fallback/debug hint다.
    string noiseTextureGuid{}; // 선택 noise 텍스처 asset identity다.
    string noiseTexturePath{}; // 선택 noise 텍스처 resource-relative fallback/debug hint다.
    string maskTextureGuid{};  // 선택 mask 텍스처 asset identity다.
    string maskTexturePath{};  // 선택 mask 텍스처 resource-relative fallback/debug hint다.

    Color tint{ 1.f, 1.f, 1.f, 1.f }; // main texture RGB emissive-like 색상에 곱할 색상 계수다.

    float intensity{ 1.f };     // sprite 밝기/발광감 계수다.
    float opacityPower{ 1.f };  // opacity source(alpha/red/luminance)의 sharpening 계수다.
    float noiseStrength{ 0.f }; // noise modulation 강도다.
    float alphaCutoff{ 0.f };   // alpha cutoff 후보 값이다. 소비하지 않는 renderer에서는 보존만 한다.
    float alphaErosion{ 0.f };  // mask/noise 기반 alpha erosion 후보 값이다. 소비하지 않는 renderer에서는 보존만 한다.

    Vec2 mainUVScale{ 1.f, 1.f };       // main texture UV scale 후보 값이다.
    Vec2 mainUVScrollSpeed{ 0.f, 0.f }; // main texture UV scroll speed 후보 값이다.
    Vec2 noiseUVScale{ 1.f, 1.f };      // noise texture UV scale 후보 값이다.
    Vec2 noiseUVScrollSpeed{ 0.f, 0.f }; // noise texture UV scroll speed 후보 값이다.
    Vec2 maskUVScale{ 1.f, 1.f };       // mask texture UV scale 후보 값이다.
    Vec2 maskUVScrollSpeed{ 0.f, 0.f }; // mask texture UV scroll speed 후보 값이다.

    string blendMode{ "AlphaBlend" }; // shader/render path가 해석할 blend mode 힌트다.
    string opacitySource{ "Alpha" };  // main texture에서 opacity를 뽑을 채널/방식이다.
    bool twoSided{ false };           // render state 후보 값이다. 소비하지 않는 renderer에서는 보존만 한다.
    uint32 subUVRows{ 1 };            // atlas row 수다.
    uint32 subUVCols{ 1 };            // atlas column 수다.
};

NS_END
