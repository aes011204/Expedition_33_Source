#pragma once

#include "Engine_Typedef.h"

NS_BEGIN(Engine)

// effect material preset/source에서 복사되어 runtime/authoring 쪽이 함께 보는 최소 material instance 값이다.
struct EffectMaterialInstanceData
{
    string sourcePresetName{}; // UI 표시용 원본 preset 이름이다. 저장 identity나 live reference로 쓰지 않는다.
    string mainTextureId{};    // emitter가 사용할 주 텍스처 id다.
    string noiseTextureId{};   // 선택 noise 텍스처 id다.

    Color tint{ 1.f, 1.f, 1.f, 1.f }; // 텍스처 샘플에 곱할 색상 계수다.

    float intensity{ 1.f };     // sprite 밝기/발광감 계수다.
    float opacityPower{ 1.f };  // opacity sharpening 계수다.
    float noiseStrength{ 0.f }; // noise modulation 강도다.

    string blendMode{ "AlphaBlend" }; // shader/render path가 해석할 blend mode 힌트다.
    string opacitySource{ "Alpha" };  // opacity를 뽑을 채널/방식이다.
    uint32 subUVRows{ 1 };            // atlas row 수다.
    uint32 subUVCols{ 1 };            // atlas column 수다.
};

NS_END
