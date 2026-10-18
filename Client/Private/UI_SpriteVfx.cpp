#include "UI_SpriteVfx.h"

NS_BEGIN(Client)

namespace
{
    constexpr float kPi = 3.14159265358979323846f;

    string Read_String(const json& item, const char* key, const string& fallback = {})
    {
        const auto iter = item.find(key);
        return iter != item.end() && iter->is_string() ? iter->get<string>() : fallback;
    }

    bool Read_Bool(const json& item, const char* key, bool fallback = false)
    {
        const auto iter = item.find(key);
        return iter != item.end() && iter->is_boolean() ? iter->get<bool>() : fallback;
    }

    uint32 Read_UInt(const json& item, const char* key, uint32 fallback = 0u)
    {
        const auto iter = item.find(key);
        if (iter == item.end())
            return fallback;

        if (iter->is_number_unsigned())
            return iter->get<uint32>();
        if (iter->is_number_integer())
            return static_cast<uint32>(max<int64_t>(0, iter->get<int64_t>()));

        return fallback;
    }

    int32 Read_Int(const json& item, const char* key, int32 fallback = 0)
    {
        const auto iter = item.find(key);
        return iter != item.end() && iter->is_number_integer() ? iter->get<int32>() : fallback;
    }

    float Read_Float(const json& item, const char* key, float fallback = 0.f)
    {
        const auto iter = item.find(key);
        return iter != item.end() && iter->is_number() ? iter->get<float>() : fallback;
    }

    Vec2 Read_Vec2(const json& item, const char* key, const Vec2& fallback)
    {
        const auto iter = item.find(key);
        if (iter == item.end() || !iter->is_array() || iter->size() < 2)
            return fallback;

        return Vec2(
            iter->at(0).is_number() ? iter->at(0).get<float>() : fallback.x,
            iter->at(1).is_number() ? iter->at(1).get<float>() : fallback.y);
    }

    Vec4 Read_Color(const json& item, const char* key, const Vec4& fallback)
    {
        const auto iter = item.find(key);
        if (iter == item.end() || !iter->is_array() || iter->size() < 4)
            return fallback;

        return Vec4(
            iter->at(0).is_number() ? iter->at(0).get<float>() : fallback.x,
            iter->at(1).is_number() ? iter->at(1).get<float>() : fallback.y,
            iter->at(2).is_number() ? iter->at(2).get<float>() : fallback.z,
            iter->at(3).is_number() ? iter->at(3).get<float>() : fallback.w);
    }

    json Vec2_ToJson(const Vec2& value)
    {
        return json::array({ value.x, value.y });
    }

    json Color_ToJson(const Vec4& value)
    {
        return json::array({ value.x, value.y, value.z, value.w });
    }

    const char* BlendMode_ToString(UISpriteVfxBlendMode blendMode)
    {
        switch (blendMode)
        {
        case UISpriteVfxBlendMode::Additive:
            return "Additive";
        case UISpriteVfxBlendMode::AlphaBlend:
        default:
            return "AlphaBlend";
        }
    }

    UISpriteVfxBlendMode Parse_BlendMode(const string& value)
    {
        const string mode = String::ToLowerCopy(value);
        if (mode == "additive" || mode == "add")
            return UISpriteVfxBlendMode::Additive;

        return UISpriteVfxBlendMode::AlphaBlend;
    }

    float Clamp01(float value)
    {
        return min(max(value, 0.f), 1.f);
    }

    float SafeDiv(float numerator, float denominator)
    {
        return fabsf(denominator) <= 0.0001f ? 1.f : numerator / denominator;
    }

    uint32 Hash_UInt(uint32 value)
    {
        value ^= value >> 16;
        value *= 0x7feb352du;
        value ^= value >> 15;
        value *= 0x846ca68bu;
        value ^= value >> 16;
        return value;
    }

    float Hash01(uint32 seed)
    {
        return static_cast<float>(Hash_UInt(seed) & 0x00ffffffu) / static_cast<float>(0x01000000u);
    }

    float Range(float minValue, float maxValue, float t)
    {
        return minValue + (maxValue - minValue) * t;
    }

    Vec2 Lerp_Vec2(const Vec2& a, const Vec2& b, float t)
    {
        return Vec2(Range(a.x, b.x, t), Range(a.y, b.y, t));
    }

    Vec4 Lerp_Color(const Vec4& a, const Vec4& b, float t)
    {
        return Vec4(
            Range(a.x, b.x, t),
            Range(a.y, b.y, t),
            Range(a.z, b.z, t),
            Range(a.w, b.w, t));
    }
}

json UISpriteVfxSerializer::To_Json(const UISpriteVfxAsset& asset)
{
    json root{};
    root["version"] = UISpriteVfxAsset::CurrentVersion;
    root["type"] = "UISpriteParticleVfx";
    root["name"] = asset.name;
    root["textureGuid"] = asset.textureGuid;
    root["texturePath"] = asset.texturePath;
    root["maskTextureGuid"] = asset.maskTextureGuid;
    root["maskTexturePath"] = asset.maskTexturePath;
    root["noiseTextureGuid"] = asset.noiseTextureGuid;
    root["noiseTexturePath"] = asset.noiseTexturePath;
    root["maxParticles"] = asset.maxParticles;
    root["duration"] = asset.duration;
    root["loop"] = asset.loop;
    root["burstCount"] = asset.burstCount;
    root["spawnRate"] = asset.spawnRate;
    root["lifetime"] = Vec2_ToJson(asset.lifetime);
    root["spawnRange"] = Vec2_ToJson(asset.spawnRange);
    root["velocityMin"] = Vec2_ToJson(asset.velocityMin);
    root["velocityMax"] = Vec2_ToJson(asset.velocityMax);
    root["radialVelocityMin"] = asset.radialVelocityMin;
    root["radialVelocityMax"] = asset.radialVelocityMax;
    root["sizeStart"] = Vec2_ToJson(asset.sizeStart);
    root["sizeEnd"] = Vec2_ToJson(asset.sizeEnd);
    root["rotationDegreesMin"] = asset.rotationDegreesMin;
    root["rotationDegreesMax"] = asset.rotationDegreesMax;
    root["rotationSpeedDegreesMin"] = asset.rotationSpeedDegreesMin;
    root["rotationSpeedDegreesMax"] = asset.rotationSpeedDegreesMax;
    root["colorStart"] = Color_ToJson(asset.colorStart);
    root["colorEnd"] = Color_ToJson(asset.colorEnd);
    root["uvScrollSpeed"] = Vec2_ToJson(asset.uvScrollSpeed);
    root["noiseStrength"] = asset.noiseStrength;
    root["alphaErosion"] = asset.alphaErosion;
    root["blendMode"] = BlendMode_ToString(asset.blendMode);
    root["sortOrder"] = asset.sortOrder;
    return root;
}

bool UISpriteVfxSerializer::From_Json(const json& root, UISpriteVfxAsset& outAsset)
{
    if (!root.is_object())
        return false;

    UISpriteVfxAsset asset{};
    asset.name = Read_String(root, "name", asset.name);
    asset.textureGuid = Read_String(root, "textureGuid");
    asset.texturePath = Read_String(root, "texturePath");
    asset.maskTextureGuid = Read_String(root, "maskTextureGuid");
    asset.maskTexturePath = Read_String(root, "maskTexturePath");
    asset.noiseTextureGuid = Read_String(root, "noiseTextureGuid");
    asset.noiseTexturePath = Read_String(root, "noiseTexturePath");
    asset.maxParticles = min(max(1u, Read_UInt(root, "maxParticles", asset.maxParticles)), 4096u);
    asset.duration = max(0.001f, Read_Float(root, "duration", asset.duration));
    asset.loop = Read_Bool(root, "loop", asset.loop);
    asset.burstCount = min(Read_UInt(root, "burstCount", asset.burstCount), asset.maxParticles);
    asset.spawnRate = max(0.f, Read_Float(root, "spawnRate", asset.spawnRate));
    asset.lifetime = Read_Vec2(root, "lifetime", asset.lifetime);
    asset.lifetime.x = max(0.001f, asset.lifetime.x);
    asset.lifetime.y = max(asset.lifetime.x, asset.lifetime.y);
    asset.spawnRange = Read_Vec2(root, "spawnRange", asset.spawnRange);
    asset.spawnRange.x = max(0.f, asset.spawnRange.x);
    asset.spawnRange.y = max(0.f, asset.spawnRange.y);
    asset.velocityMin = Read_Vec2(root, "velocityMin", asset.velocityMin);
    asset.velocityMax = Read_Vec2(root, "velocityMax", asset.velocityMax);
    asset.radialVelocityMin = Read_Float(root, "radialVelocityMin", asset.radialVelocityMin);
    asset.radialVelocityMax = Read_Float(root, "radialVelocityMax", asset.radialVelocityMax);
    asset.sizeStart = Read_Vec2(root, "sizeStart", asset.sizeStart);
    asset.sizeEnd = Read_Vec2(root, "sizeEnd", asset.sizeEnd);
    asset.sizeStart.x = max(0.01f, asset.sizeStart.x);
    asset.sizeStart.y = max(0.01f, asset.sizeStart.y);
    asset.sizeEnd.x = max(0.01f, asset.sizeEnd.x);
    asset.sizeEnd.y = max(0.01f, asset.sizeEnd.y);
    asset.rotationDegreesMin = Read_Float(root, "rotationDegreesMin", asset.rotationDegreesMin);
    asset.rotationDegreesMax = Read_Float(root, "rotationDegreesMax", asset.rotationDegreesMax);
    asset.rotationSpeedDegreesMin = Read_Float(root, "rotationSpeedDegreesMin", asset.rotationSpeedDegreesMin);
    asset.rotationSpeedDegreesMax = Read_Float(root, "rotationSpeedDegreesMax", asset.rotationSpeedDegreesMax);
    asset.colorStart = Read_Color(root, "colorStart", asset.colorStart);
    asset.colorEnd = Read_Color(root, "colorEnd", asset.colorEnd);
    asset.uvScrollSpeed = Read_Vec2(root, "uvScrollSpeed", asset.uvScrollSpeed);
    asset.noiseStrength = max(0.f, Read_Float(root, "noiseStrength", asset.noiseStrength));
    asset.alphaErosion = Clamp01(Read_Float(root, "alphaErosion", asset.alphaErosion));
    asset.blendMode = Parse_BlendMode(Read_String(root, "blendMode", BlendMode_ToString(asset.blendMode)));
    asset.sortOrder = Read_Int(root, "sortOrder", asset.sortOrder);

    outAsset = move(asset);
    return true;
}

bool UISpriteVfxSerializer::Save_ToFile(const UISpriteVfxAsset& asset, const wstring& filePath)
{
    if (filePath.empty())
        return false;

    try
    {
        ofstream file{ fs::path(filePath) };
        if (!file.is_open())
            return false;

        file << To_Json(asset).dump(4);
        return file.good();
    }
    catch (const exception& exception)
    {
        LOG_ERROR("UI Sprite VFX save failed: {} ({})", exception.what(), String::ToString(filePath));
        return false;
    }
}

bool UISpriteVfxSerializer::Load_FromFile(const wstring& filePath, UISpriteVfxAsset& outAsset)
{
    if (filePath.empty())
        return false;

    try
    {
        ifstream file{ fs::path(filePath) };
        if (!file.is_open())
            return false;

        json root{};
        file >> root;
        return From_Json(root, outAsset);
    }
    catch (const exception& exception)
    {
        LOG_ERROR("UI Sprite VFX load failed: {} ({})", exception.what(), String::ToString(filePath));
        return false;
    }
}

Shared<EffectDefinition> UISpriteVfxRuntimeBuilder::Build_EffectDefinition(
    const UISpriteVfxAsset& asset,
    const string& debugName)
{
    auto definition = make_shared<EffectDefinition>();
    definition->name = debugName.empty() ? asset.name : debugName;

    EffectEmitterDefinition emitter{};
    emitter.id = 1u;
    emitter.name = asset.name.empty() ? "UISpriteEmitter" : asset.name;
    emitter.kind = EffectEmitterKind::Sprite;
    emitter.enabled = true;
    emitter.useLocalSpace = true;
    emitter.renderLayerOverride = EffectEmitterRenderLayerOverride::UIEffect;

    ComputeSpriteEmitterDesc desc{};
    desc.renderLayerOverride = EffectEmitterRenderLayerOverride::UIEffect;
    desc.spawn.instanceCount = max(1u, asset.maxParticles);
    desc.spawn.particleSpawn.maxActiveCount = max(1u, asset.maxParticles);
    desc.spawn.particleSpawn.processBurstList = asset.burstCount > 0u;
    desc.spawn.particleSpawn.processSpawnRate = asset.spawnRate > 0.f;
    desc.spawn.particleSpawn.spawnRate = asset.spawnRate;
    desc.spawn.particleSpawn.spawnRateRange = Vec2(asset.spawnRate, asset.spawnRate);
    desc.spawn.particleSpawn.bursts.clear();
    if (asset.burstCount > 0u)
        desc.spawn.particleSpawn.bursts.push_back(PointParticleBurstDesc{ 0.f, min(asset.burstCount, asset.maxParticles) });

    desc.lifetime.lifeTime = Vec2(max(0.001f, asset.lifetime.x), max(asset.lifetime.x, asset.lifetime.y));
    desc.initialSize.enabled = true;
    desc.initialSize.sizeMin = asset.sizeStart;
    desc.initialSize.sizeMax = asset.sizeStart;
    desc.sizeByLife.enabled = true;
    desc.sizeByLife.multiplyXStart = 1.f;
    desc.sizeByLife.multiplyYStart = 1.f;
    desc.sizeByLife.multiplyXEnd = SafeDiv(asset.sizeEnd.x, asset.sizeStart.x);
    desc.sizeByLife.multiplyYEnd = SafeDiv(asset.sizeEnd.y, asset.sizeStart.y);

    desc.initialColor.startColorMin = asset.colorStart;
    desc.initialColor.startColorMax = asset.colorStart;
    desc.colorOverLife.endColorMin = asset.colorEnd;
    desc.colorOverLife.endColorMax = asset.colorEnd;
    desc.colorOverLife.curve.colorCurveEnabled = true;
    desc.colorOverLife.curve.alphaCurveEnabled = true;
    desc.colorOverLife.curve.colorCurveValuesR = Vec4(asset.colorStart.x, asset.colorEnd.x, 0.f, 0.f);
    desc.colorOverLife.curve.colorCurveValuesG = Vec4(asset.colorStart.y, asset.colorEnd.y, 0.f, 0.f);
    desc.colorOverLife.curve.colorCurveValuesB = Vec4(asset.colorStart.z, asset.colorEnd.z, 0.f, 0.f);
    desc.colorOverLife.curve.alphaCurveValues = Vec4(asset.colorStart.w, asset.colorEnd.w, 0.f, 0.f);

    desc.initialLocation.enabled = true;
    desc.initialLocation.minOffset = Vec3(-asset.spawnRange.x * 0.5f, -asset.spawnRange.y * 0.5f, 0.f);
    desc.initialLocation.maxOffset = Vec3(asset.spawnRange.x * 0.5f, asset.spawnRange.y * 0.5f, 0.f);

    desc.motion.enabled = true;
    desc.motion.initialVelocityEnabled = true;
    desc.motion.initialVelocityInWorldSpace = false;
    desc.motion.initialVelocityMin = Vec3(asset.velocityMin.x, asset.velocityMin.y, 0.f);
    desc.motion.initialVelocityMax = Vec3(asset.velocityMax.x, asset.velocityMax.y, 0.f);
    desc.motion.initialRadialVelocityEnabled = fabsf(asset.radialVelocityMin) > 0.001f || fabsf(asset.radialVelocityMax) > 0.001f;
    desc.motion.radialSpeed = Vec2(asset.radialVelocityMin, asset.radialVelocityMax);
    desc.motion.initialRadialVelocityCenterDirectionMode =
        PointParticleInitialRadialVelocityCenterDirectionMode::PlaneRadial;

    desc.rotation.enabled = true;
    desc.rotation.initialRotationDegrees = Vec2(asset.rotationDegreesMin, asset.rotationDegreesMax);
    desc.rotation.initialRotationRateDegrees = Vec2(asset.rotationSpeedDegreesMin, asset.rotationSpeedDegreesMax);

    desc.required.material.mainTextureGuid = asset.textureGuid;
    desc.required.material.mainTexturePath = asset.texturePath;
    desc.required.material.maskTextureGuid = asset.maskTextureGuid;
    desc.required.material.maskTexturePath = asset.maskTexturePath;
    desc.required.material.noiseTextureGuid = asset.noiseTextureGuid;
    desc.required.material.noiseTexturePath = asset.noiseTexturePath;
    desc.required.material.blendMode = asset.blendMode == UISpriteVfxBlendMode::Additive
        ? EffectMaterialBlendMode::Additive
        : EffectMaterialBlendMode::AlphaBlend;
    desc.required.material.tint = Vec4(1.f, 1.f, 1.f, 1.f);
    desc.required.material.opacitySource = "Alpha";
    desc.required.material.noiseStrength = asset.noiseStrength;
    desc.required.material.alphaErosion = asset.alphaErosion;
    desc.required.material.mainUVScrollSpeed = asset.uvScrollSpeed;
    desc.required.material.noiseUVScrollSpeed = asset.uvScrollSpeed;
    desc.required.spriteRender.screenAlignment = EffectScreenAlignment::Rectangle;
    desc.required.playback.duration = max(0.001f, asset.duration);
    desc.required.playback.loopCount = asset.loop ? 0u : 1u;
    desc.required.playback.killOnCompleted = !asset.loop;
    desc.required.drawLimit.maxDrawCount = max(1u, asset.maxParticles);
    desc.required.sort.sortPolicy = EffectSortPolicy::None;
    desc.required.sort.sortLayer = asset.sortOrder;

    emitter.concreteDesc = desc;
    definition->emitters.push_back(move(emitter));
    return definition;
}

vector<UISpriteVfxParticlePreview> UISpriteVfxPreviewSampler::Sample(
    const UISpriteVfxAsset& asset,
    float timeSeconds,
    const Vec2& origin,
    bool forceLoop)
{
    vector<UISpriteVfxParticlePreview> particles{};
    if (asset.maxParticles == 0u)
        return particles;

    const float duration = max(0.001f, asset.duration);
    const bool loop = forceLoop || asset.loop;
    float localTime = timeSeconds;
    if (loop)
        localTime = fmodf(max(0.f, timeSeconds), duration);
    else
        localTime = min(max(0.f, timeSeconds), duration);

    const uint32 spawnRateCount = asset.spawnRate > 0.f
        ? static_cast<uint32>(floorf(localTime * asset.spawnRate))
        : 0u;
    const uint32 totalPotentialCount = min(asset.maxParticles, asset.burstCount + spawnRateCount);
    particles.reserve(totalPotentialCount);

    for (uint32 index = 0; index < totalPotentialCount; ++index)
    {
        const bool isBurst = index < asset.burstCount;
        const uint32 rateIndex = isBurst ? 0u : index - asset.burstCount + 1u;
        const float spawnTime = isBurst ? 0.f : static_cast<float>(rateIndex) / max(0.001f, asset.spawnRate);
        if (localTime < spawnTime)
            continue;

        const float lifetime = Range(asset.lifetime.x, asset.lifetime.y, Hash01(index * 17u + 3u));
        if (lifetime <= 0.f)
            continue;

        const float age = localTime - spawnTime;
        if (age > lifetime)
            continue;

        const float normalizedAge = Clamp01(age / lifetime);
        const float offsetX = Range(-asset.spawnRange.x * 0.5f, asset.spawnRange.x * 0.5f, Hash01(index * 31u + 5u));
        const float offsetY = Range(-asset.spawnRange.y * 0.5f, asset.spawnRange.y * 0.5f, Hash01(index * 47u + 7u));
        const Vec2 velocity(
            Range(asset.velocityMin.x, asset.velocityMax.x, Hash01(index * 61u + 11u)),
            Range(asset.velocityMin.y, asset.velocityMax.y, Hash01(index * 73u + 13u)));
        const float angle = Hash01(index * 89u + 19u) * kPi * 2.f;
        const float radialSpeed = Range(asset.radialVelocityMin, asset.radialVelocityMax, Hash01(index * 97u + 23u));
        const Vec2 radialVelocity(cosf(angle) * radialSpeed, sinf(angle) * radialSpeed);
        const float rotationDegrees = Range(asset.rotationDegreesMin, asset.rotationDegreesMax, Hash01(index * 109u + 29u));
        const float rotationSpeedDegrees = Range(asset.rotationSpeedDegreesMin, asset.rotationSpeedDegreesMax, Hash01(index * 127u + 31u));

        UISpriteVfxParticlePreview particle{};
        particle.position = Vec2(
            origin.x + offsetX + (velocity.x + radialVelocity.x) * age,
            origin.y + offsetY + (velocity.y + radialVelocity.y) * age);
        particle.size = Lerp_Vec2(asset.sizeStart, asset.sizeEnd, normalizedAge);
        particle.rotationRadians = (rotationDegrees + rotationSpeedDegrees * age) * kPi / 180.f;
        particle.color = Lerp_Color(asset.colorStart, asset.colorEnd, normalizedAge);
        particle.normalizedAge = normalizedAge;
        particles.push_back(particle);
    }

    return particles;
}

NS_END
