#pragma once

NS_BEGIN(EffectEditor)

struct EffectMaterialInstanceData
{
    // 원본 material preset 파일명 표시용 값이다. live reference나 저장 identity로 사용하지 않는다.
    string sourcePresetName{};

    string mainTextureId{ "Texture_Effect_DefaultTexture" };
    string noiseTextureId{ "Texture_Effect_Clouds4" };

    Color tint{ 1.f, 1.f, 1.f, 1.f };

    float intensity{ 1.f };
    float opacityPower{ 1.f };
    float noiseStrength{ 0.f };

    string blendMode{ "AlphaBlend" };
    string opacitySource{ "Alpha" };
    uint32 subUvRows{ 1 };
    uint32 subUvCols{ 1 };
};

NS_END
