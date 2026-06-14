#pragma once

#include "Client_Defines.h"
#include "EffectRuntime_Types.h"

NS_BEGIN(Client)

enum class UISpriteVfxBlendMode : uint8
{
    AlphaBlend,
    Additive,
};

struct UISpriteVfxAsset
{
    static constexpr int32 CurrentVersion = 1;

    string name{ "NewUISpriteVfx" };
    string textureGuid{};
    string texturePath{};
    string maskTextureGuid{};
    string maskTexturePath{};
    string noiseTextureGuid{};
    string noiseTexturePath{};

    uint32 maxParticles{ 64 };
    float duration{ 0.75f };
    bool loop{ false };
    uint32 burstCount{ 24 };
    float spawnRate{ 0.f };
    Vec2 lifetime{ 0.25f, 0.55f };
    Vec2 spawnRange{ 360.f, 80.f };
    Vec2 velocityMin{ -160.f, -20.f };
    Vec2 velocityMax{ 160.f, 20.f };
    float radialVelocityMin{ 0.f };
    float radialVelocityMax{ 0.f };
    Vec2 sizeStart{ 72.f, 18.f };
    Vec2 sizeEnd{ 96.f, 12.f };
    float rotationDegreesMin{ -8.f };
    float rotationDegreesMax{ 8.f };
    float rotationSpeedDegreesMin{ -20.f };
    float rotationSpeedDegreesMax{ 20.f };
    Vec4 colorStart{ 1.f, 1.f, 1.f, 1.f };
    Vec4 colorEnd{ 1.f, 1.f, 1.f, 0.f };
    Vec2 uvScrollSpeed{ 0.f, 0.f };
    float noiseStrength{ 0.f };
    float alphaErosion{ 0.f };
    UISpriteVfxBlendMode blendMode{ UISpriteVfxBlendMode::AlphaBlend };
    int32 sortOrder{ 0 };
};

struct UISpriteVfxParticlePreview
{
    Vec2 position{};
    Vec2 size{};
    float rotationRadians{};
    Vec4 color{};
    float normalizedAge{};
};

class UISpriteVfxSerializer final
{
public:
    static json To_Json(const UISpriteVfxAsset& asset);
    static bool From_Json(const json& root, UISpriteVfxAsset& outAsset);
    static bool Save_ToFile(const UISpriteVfxAsset& asset, const wstring& filePath);
    static bool Load_FromFile(const wstring& filePath, UISpriteVfxAsset& outAsset);
};

class UISpriteVfxRuntimeBuilder final
{
public:
    static Shared<EffectDefinition> Build_EffectDefinition(const UISpriteVfxAsset& asset, const string& debugName);
};

class UISpriteVfxPreviewSampler final
{
public:
    static vector<UISpriteVfxParticlePreview> Sample(
        const UISpriteVfxAsset& asset,
        float timeSeconds,
        const Vec2& origin,
        bool forceLoop);
};

NS_END
