#pragma once

#include "Engine_Typedef.h"

NS_BEGIN(Engine)

// effect material preset/source에서 복사되어 runtime/authoring 쪽이 함께 보는 최소 material instance 값이다.
// sprite effect 경로에서는 mainTexturePath를 lit base color가 아니라 emissive color + opacity source로 해석한다.
struct EffectMaterialInstanceData
{
    string sourcePresetName{}; // UI 표시용 원본 preset 이름이다. 저장 identity나 live reference로 쓰지 않는다.
    string mainTextureGuid{};  // 구버전 파일 복구/현재 registry 진단용 GUID hint다. 저장 identity로 쓰지 않는다.
    string mainTexturePath{};  // emitter가 사용할 주 텍스처 resource-relative 경로다.
    string noiseTextureGuid{}; // 구버전 noise 텍스처 복구/진단용 GUID hint다.
    string noiseTexturePath{}; // 선택 noise 텍스처 resource-relative 경로다.
    string maskTextureGuid{};  // 구버전 mask 텍스처 복구/진단용 GUID hint다.
    string maskTexturePath{};  // 선택 mask 텍스처 resource-relative 경로다.

    Color tint{ 1.f, 1.f, 1.f, 1.f }; // main texture RGB emissive-like 색상에 곱할 색상 계수다.

    float intensity{ 1.f };     // sprite 밝기/발광감 계수다.
    float opacityPower{ 1.f };  // opacity source(alpha/red/luminance)의 sharpening 계수다.
    float noiseStrength{ 0.f }; // noise modulation 강도다.
    float alphaCutoff{ 0.f };   // alpha cutoff 후보 값이다. 소비하지 않는 renderer에서는 보존만 한다.
    float alphaErosion{ 0.f };  // mask/noise 기반 alpha erosion 후보 값이다. 소비하지 않는 renderer에서는 보존만 한다.

    Vec2 mainUvScale{ 1.f, 1.f };       // main texture UV scale 후보 값이다.
    Vec2 mainUvScrollSpeed{ 0.f, 0.f }; // main texture UV scroll speed 후보 값이다.
    Vec2 noiseUvScale{ 1.f, 1.f };      // noise texture UV scale 후보 값이다.
    Vec2 noiseUvScrollSpeed{ 0.f, 0.f }; // noise texture UV scroll speed 후보 값이다.
    Vec2 maskUvScale{ 1.f, 1.f };       // mask texture UV scale 후보 값이다.
    Vec2 maskUvScrollSpeed{ 0.f, 0.f }; // mask texture UV scroll speed 후보 값이다.

    string blendMode{ "AlphaBlend" }; // shader/render path가 해석할 blend mode 힌트다.
    string opacitySource{ "Alpha" };  // main texture에서 opacity를 뽑을 채널/방식이다.
    bool twoSided{ false };           // render state 후보 값이다. 소비하지 않는 renderer에서는 보존만 한다.
    uint32 subUVRows{ 1 };            // atlas row 수다.
    uint32 subUVCols{ 1 };            // atlas column 수다.
};

NS_END
