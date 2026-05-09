#pragma once

#include <variant>

NS_BEGIN(EffectEditor)

enum class AuthoringModuleType : uint8
{
    Required,
    Spawn,
    Lifetime,
    InitialSize,
    InitialVelocity,
    ColorOverLife,
};

enum class EffectAuthoringSelectionKind : uint8
{
    None,
    Emitter,
    Module,
};

struct RequiredModuleData
{
    string rendererType{ "Sprite" };
    string materialName{ "DefaultSpriteMaterial" };
    string textureId{ "Texture_Snow" };
    float editorColor[4]{ 1.f, 0.f, 0.5f, 1.f };
};

struct SpawnModuleData
{
    uint32 instanceCount{ 64 };
    bool loop{ true };
};

struct LifetimeModuleData
{
    Vec2 lifeTime{ 1.f, 2.f };
};

struct InitialSizeModuleData
{
    Vec2 scale{ 0.15f, 0.35f };
};

struct InitialVelocityModuleData
{
    Vec3 center{ 0.f, 2.f, 0.f };
    Vec3 range{ 2.f, 2.f, 2.f };
    Vec3 pivot{ 0.f, 2.f, 0.f };
    Vec2 speed{ 0.5f, 1.5f };
};

struct ColorOverLifeModuleData
{
    float startColor[4]{ 1.f, 1.f, 1.f, 1.f };
    float endColor[4]{ 1.f, 1.f, 1.f, 0.f };
};

using AuthoringModuleData = std::variant<
    RequiredModuleData,
    SpawnModuleData,
    LifetimeModuleData,
    InitialSizeModuleData,
    InitialVelocityModuleData,
    ColorOverLifeModuleData>;

struct AuthoringModule
{
    uint32 id{};
    AuthoringModuleType type{ AuthoringModuleType::Required };
    string key{};
    wstring displayName{};
    wstring summary{};
    bool enabled{ true };
    bool movable{ true };
    bool removable{ true };
    AuthoringModuleData data{ RequiredModuleData{} };
};

struct AuthoringEmitter
{
    uint32 id{};
    string name{};
    string rendererType{ "Sprite" };
    string supportState{ "Supported" };
    string resourceSummary{};
    string materialName{ "DefaultSpriteMaterial" };
    string textureId{ "Texture_Snow" };
    bool enabled{ true };
    bool previewDirty{ true };
    vector<AuthoringModule> modules{};
};

struct EffectAuthoringSelection
{
    EffectAuthoringSelectionKind kind{ EffectAuthoringSelectionKind::None };
    uint32 emitterId{};
    uint32 moduleId{};
};

NS_END
