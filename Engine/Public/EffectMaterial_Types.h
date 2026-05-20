#pragma once

#include "Engine_Typedef.h"

NS_BEGIN(Engine)

// effect material preset/source에서 복사되어 runtime/authoring 쪽이 함께 보는 최소 material instance 값이다.
// sprite effect 경로에서는 mainTextureGuid/path를 lit base color가 아니라 emissive color + opacity source로 해석한다.
struct EffectMaterialInstanceData
{
    string sourcePresetName{}; // UI 표시용 원본 preset 이름이다. 저장 identity나 live reference로 쓰지 않는다.
    string mainTextureGuid{};  // emitter가 사용할 주 텍스처 asset GUID다.
    string mainTexturePath{};  // mainTextureGuid resolve 실패 시 사용할 fallback 경로다.
    string noiseTextureGuid{}; // 선택 noise 텍스처 asset GUID다.
    string noiseTexturePath{}; // noiseTextureGuid resolve 실패 시 사용할 fallback 경로다.

    Color tint{ 1.f, 1.f, 1.f, 1.f }; // main texture RGB emissive-like 색상에 곱할 색상 계수다.

    float intensity{ 1.f };     // sprite 밝기/발광감 계수다.
    float opacityPower{ 1.f };  // opacity source(alpha/red/luminance)의 sharpening 계수다.
    float noiseStrength{ 0.f }; // noise modulation 강도다.

    string blendMode{ "AlphaBlend" }; // shader/render path가 해석할 blend mode 힌트다.
    string opacitySource{ "Alpha" };  // main texture에서 opacity를 뽑을 채널/방식이다.
    uint32 subUVRows{ 1 };            // atlas row 수다.
    uint32 subUVCols{ 1 };            // atlas column 수다.
};

NS_END
