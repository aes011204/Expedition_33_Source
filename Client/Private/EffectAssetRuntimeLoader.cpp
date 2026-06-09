#include "pch.h"
#include "EffectAssetRuntimeLoader.h"

#include "EffectRuntime_Lowering.h"
#include "GameInstance.h"

NS_BEGIN(Client)

namespace Internal
{
    constexpr uint32 kMaxSubUVFrameCurveKeys{ ::Engine::kEffectDistributionCurveMaxKeys };

    string To_LogPath(const fs::path& path)
    {
        return String::ToString(path.wstring());
    }

    string Read_String(const json& node, const char* key, const string& fallbackValue = {})
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_string())
            return fallbackValue;

        return iter->get<string>();
    }

    bool Read_Bool(const json& node, const char* key, bool fallbackValue = false)
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_boolean())
            return fallbackValue;

        return iter->get<bool>();
    }

    uint32 Read_UInt(const json& node, const char* key, uint32 fallbackValue = 0u)
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_number_unsigned())
            return fallbackValue;

        return iter->get<uint32>();
    }

    int32 Read_Int(const json& node, const char* key, int32 fallbackValue = 0)
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_number_integer())
            return fallbackValue;

        return iter->get<int32>();
    }

    float Read_Float(const json& node, const char* key, float fallbackValue = 0.f)
    {
        const auto iter = node.find(key);
        if (iter == node.end() || !iter->is_number())
            return fallbackValue;

        return iter->get<float>();
    }

    void Read_Vec2(const json& node, Vec2& outValue)
    {
        if (!node.is_array() || node.size() < 2 || !node[0].is_number() || !node[1].is_number())
            return;

        outValue.x = node[0].get<float>();
        outValue.y = node[1].get<float>();
    }

    void Read_Vec3(const json& node, Vec3& outValue)
    {
        if (!node.is_array() || node.size() < 3 || !node[0].is_number() || !node[1].is_number() || !node[2].is_number())
            return;

        outValue.x = node[0].get<float>();
        outValue.y = node[1].get<float>();
        outValue.z = node[2].get<float>();
    }

    void Read_Vec4(const json& node, Vec4& outValue)
    {
        if (!node.is_array() || node.size() < 4 ||
            !node[0].is_number() || !node[1].is_number() || !node[2].is_number() || !node[3].is_number())
            return;

        outValue.x = node[0].get<float>();
        outValue.y = node[1].get<float>();
        outValue.z = node[2].get<float>();
        outValue.w = node[3].get<float>();
    }

    void Read_Color(const json& node, Color& outValue)
    {
        if (!node.is_array() || node.size() < 4 ||
            !node[0].is_number() || !node[1].is_number() || !node[2].is_number() || !node[3].is_number())
            return;

        outValue.x = node[0].get<float>();
        outValue.y = node[1].get<float>();
        outValue.z = node[2].get<float>();
        outValue.w = node[3].get<float>();
    }

    template <typename EnumType>
    void Read_Enum(const json& node, const char* key, EnumType& outValue)
    {
        const string token = Read_String(node, key);
        if (token.empty())
            return;

        const optional<EnumType> parsed = magic_enum::enum_cast<EnumType>(token);
        if (parsed.has_value())
            outValue = parsed.value();
    }

    EffectSortPolicy Read_SortPolicy(const json& node, EffectSortPolicy fallbackValue)
    {
        string token = Read_String(node, "sortPolicy");
        if (!token.empty())
        {
            const optional<EffectSortPolicy> parsed = magic_enum::enum_cast<EffectSortPolicy>(token);
            if (parsed.has_value())
                return parsed.value();
        }

        token = Read_String(node, "sortMode");
        if (token == "ViewProjDepth" || token == "DistanceToView")
            return EffectSortPolicy::EmitterDepth;
        if (token == "None")
            return EffectSortPolicy::None;

        return fallbackValue;
    }

    void Apply_SortPayload(const json& data, EffectRequiredSortRuntimeDesc& sortDesc)
    {
        sortDesc.sortPolicy = Read_SortPolicy(data, sortDesc.sortPolicy);
        sortDesc.sortLayer = Read_Int(data, "sortLayer", sortDesc.sortLayer);
        sortDesc.artistSortBias = Read_Float(data, "sortBias", sortDesc.artistSortBias);
    }

    PointParticleRandomSeedRuntimeDesc Read_RandomSeedDesc(const json& moduleData)
    {
        const auto randomSeedIter = moduleData.find("randomSeed");
        if (randomSeedIter == moduleData.end() || !randomSeedIter->is_object())
            return {};

        const string mode = Read_String(*randomSeedIter, "mode");
        uint32 manualSeed = 0u;
        if (const auto seedIter = randomSeedIter->find("manualSeed"); seedIter != randomSeedIter->end())
        {
            if (seedIter->is_number_unsigned())
                manualSeed = seedIter->get<uint32>();
            else if (seedIter->is_number_integer())
                manualSeed = static_cast<uint32>(max<int64_t>(0, seedIter->get<int64_t>()));
        }

        return PointParticleRandomSeedRuntimeDesc{
            .manualSeedEnabled = mode == "Manual",
            .seed = mode == "Manual" ? manualSeed : 0u,
            .useInstanceSeed = Read_Bool(*randomSeedIter, "useInstanceSeed", false)
        };
    }

    PointParticleRandomSeedRuntimeDesc Read_DistributionRandomSeedDesc(const json& distribution, const json* legacyModuleData = nullptr)
    {
        if (distribution.is_object())
        {
            const auto payloadIter = distribution.find("payload");
            if (payloadIter != distribution.end() && payloadIter->is_object())
            {
                const auto randomSeedIter = payloadIter->find("randomSeed");
                if (randomSeedIter != payloadIter->end() && randomSeedIter->is_object())
                    return Read_RandomSeedDesc(*payloadIter);
            }
        }

        return legacyModuleData != nullptr ? Read_RandomSeedDesc(*legacyModuleData) : PointParticleRandomSeedRuntimeDesc{};
    }
}

using Internal::Read_Bool;
using Internal::Read_Enum;
using Internal::Read_Float;
using Internal::Read_String;
using Internal::Read_UInt;
using Internal::Read_Vec2;
using Internal::Read_Vec3;
using Internal::Read_Vec4;
using Internal::Read_Color;
using Internal::To_LogPath;
using Internal::kMaxSubUVFrameCurveKeys;

Shared<const EffectDefinition> EffectAssetRuntimeLoader::Load_Definition(const wstring& effectAssetPath)
{
    json root{};
    if (!Load_JsonRoot(effectAssetPath, root))
        return nullptr;

    if (!root.is_object())
    {
        LOG_ERROR("Root is not object. path='{}'", String::ToString(effectAssetPath));
        return nullptr;
    }

    const auto emittersIter = root.find("emitters");
    if (emittersIter == root.end() || !emittersIter->is_array())
    {
        LOG_ERROR("Emitters array is missing. path='{}'", String::ToString(effectAssetPath));
        return nullptr;
    }

    auto definition = make_shared<EffectDefinition>();
    definition->name = Read_String(root, "name");
    if (definition->name.empty())
        definition->name = String::ToString(fs::path(effectAssetPath).stem().wstring());

    for (const json& emitter : *emittersIter)
    {
        if (!emitter.is_object())
        {
            LOG_WARN("Skipped invalid emitter node. effect='{}'", definition->name);
            continue;
        }

        if (!Read_Bool(emitter, "enabled", true))
            continue;

        const string rendererType = Resolve_RendererType(emitter);
        const string rendererTypeLower = String::ToLowerCopy(rendererType);

        EffectEmitterDefinition emitterDefinition{};
        bool converted = false;

        if (rendererTypeLower == "sprite")
            converted = Build_EmitterDefinition(emitter, emitterDefinition);
        else if (rendererTypeLower == "trail")
            converted = Build_TrailEmitterDefinition(emitter, emitterDefinition);
        else if (rendererTypeLower == "sourcehistoryribbon")
            converted = Build_SourceHistoryRibbonEmitterDefinition(emitter, emitterDefinition);
        else if (rendererTypeLower == "mesh")
            converted = Build_MeshEmitterDefinition(emitter, emitterDefinition);
        else
        {
            LOG_INFO(
                "Skipped unsupported renderer. effect='{}', emitter='{}', rendererType='{}'",
                definition->name,
                Internal::Read_String(emitter, "name"),
                rendererType
            );
            continue;
        }

        if (!converted)
        {
            LOG_WARN(
                "EffectAssetRuntimeLoader skipped emitter conversion failure. effect='{}', emitter='{}'",
                definition->name,
                Internal::Read_String(emitter, "name")
            );
            continue;
        }

        definition->emitters.push_back(emitterDefinition);
    }

    if (definition->emitters.empty())
    {
        LOG_ERROR("No runtime emitter converted. path='{}'", String::ToString(effectAssetPath));
        return nullptr;
    }

    return definition;
}

Shared<const EffectDefinition> EffectAssetRuntimeLoader::Load_DefinitionByName(const wstring& effectName)
{
    string effectAssetGuid{};
    if (!Find_EffectAssetGuidByName(effectName, effectAssetGuid))
        return nullptr;

    wstring resolvedEffectAssetPath{};
    if (!Resolve_EffectAssetPathByGuid(effectAssetGuid, resolvedEffectAssetPath))
        return nullptr;

    return Load_Definition(resolvedEffectAssetPath);
}

const json* EffectAssetRuntimeLoader::Find_ModuleData(const json& emitter, const string& moduleType)
{
    const auto modulesIter = emitter.find("modules");
    if (modulesIter == emitter.end() || !modulesIter->is_array())
        return nullptr;

    for (const json& module : *modulesIter)
    {
        if (!module.is_object())
            continue;

        if (!Read_Bool(module, "enabled", true))
            continue;

        if (Read_String(module, "type") != moduleType)
            continue;

        const auto dataIter = module.find("data");
        if (dataIter == module.end() || !dataIter->is_object())
            return nullptr;

        return &*dataIter;
    }

    return nullptr;
}

float EffectAssetRuntimeLoader::Evaluate_FloatCurveSegmentLinear(const json& leftKey, const json& rightKey, float x)
{
    const float leftTime = Read_Float(leftKey, "time");
    const float rightTime = Read_Float(rightKey, "time", 1.f);
    const float width = max(0.0001f, rightTime - leftTime);
    const float t = clamp((x - leftTime) / width, 0.f, 1.f);
    return lerp(Read_Float(leftKey, "value"), Read_Float(rightKey, "value"), t);
}

float EffectAssetRuntimeLoader::Evaluate_FloatCurveSegmentAutoClamped(const json& leftKey, const json& rightKey, float x)
{
    const float leftTime = Read_Float(leftKey, "time");
    const float rightTime = Read_Float(rightKey, "time", 1.f);
    const float width = max(0.0001f, rightTime - leftTime);
    const float t = clamp((x - leftTime) / width, 0.f, 1.f);
    const float m0 = Read_Float(leftKey, "leaveTangent") * width;
    const float m1 = Read_Float(rightKey, "arriveTangent") * width;
    const float t2 = t * t;
    const float t3 = t2 * t;

    return (2.f * t3 - 3.f * t2 + 1.f) * Read_Float(leftKey, "value") +
           (t3 - 2.f * t2 + t) * m0 +
           (-2.f * t3 + 3.f * t2) * Read_Float(rightKey, "value") +
           (t3 - t2) * m1;
}

float EffectAssetRuntimeLoader::Evaluate_FloatConstantCurve(const json& keys, float x, float fallbackValue)
{
    if (!keys.is_array() || keys.empty())
        return fallbackValue;

    if (keys.size() == 1)
        return Read_Float(keys.front(), "value", fallbackValue);

    const float clampedX = clamp(x, 0.f, 1.f);
    for (size_t index = 1; index < keys.size(); ++index)
    {
        const json& leftKey = keys[index - 1];
        const json& rightKey = keys[index];
        if (clampedX > Read_Float(rightKey, "time", 1.f))
            continue;

        const string interpolationMode = Read_String(leftKey, "interpolationMode", "Linear");
        if ("Constant" == interpolationMode)
            return Read_Float(leftKey, "value", fallbackValue);

        if ("CurveAutoClamped" == interpolationMode)
            return Evaluate_FloatCurveSegmentAutoClamped(leftKey, rightKey, clampedX);

        return Evaluate_FloatCurveSegmentLinear(leftKey, rightKey, clampedX);
    }

    return Read_Float(keys.back(), "value", fallbackValue);
}

Vec2 EffectAssetRuntimeLoader::Evaluate_Vector2CurveSegmentLinear(const json& leftKey, const json& rightKey, float x)
{
    const float leftTime = Read_Float(leftKey, "time");
    const float rightTime = Read_Float(rightKey, "time", 1.f);
    const float width = max(0.0001f, rightTime - leftTime);
    const float t = clamp((x - leftTime) / width, 0.f, 1.f);

    Vec2 leftValue{};
    Vec2 rightValue{};
    if (const auto iter = leftKey.find("value"); iter != leftKey.end())
        Read_Vec2(*iter, leftValue);
    if (const auto iter = rightKey.find("value"); iter != rightKey.end())
        Read_Vec2(*iter, rightValue);

    return Vec2{ lerp(leftValue.x, rightValue.x, t), lerp(leftValue.y, rightValue.y, t) };
}

Vec2 EffectAssetRuntimeLoader::Evaluate_Vector2CurveSegmentAutoClamped(const json& leftKey, const json& rightKey, float x)
{
    const float leftTime = Read_Float(leftKey, "time");
    const float rightTime = Read_Float(rightKey, "time", 1.f);
    const float width = max(0.0001f, rightTime - leftTime);
    const float t = clamp((x - leftTime) / width, 0.f, 1.f);
    const float t2 = t * t;
    const float t3 = t2 * t;

    Vec2 leftValue{};
    Vec2 rightValue{};
    Vec2 leftTangent{};
    Vec2 rightTangent{};
    if (const auto iter = leftKey.find("value"); iter != leftKey.end())
        Read_Vec2(*iter, leftValue);
    if (const auto iter = rightKey.find("value"); iter != rightKey.end())
        Read_Vec2(*iter, rightValue);
    if (const auto iter = leftKey.find("leaveTangent"); iter != leftKey.end())
        Read_Vec2(*iter, leftTangent);
    if (const auto iter = rightKey.find("arriveTangent"); iter != rightKey.end())
        Read_Vec2(*iter, rightTangent);

    const Vec2 m0{ leftTangent.x * width, leftTangent.y * width };
    const Vec2 m1{ rightTangent.x * width, rightTangent.y * width };
    return Vec2{
        (2.f * t3 - 3.f * t2 + 1.f) * leftValue.x +
        (t3 - 2.f * t2 + t) * m0.x +
        (-2.f * t3 + 3.f * t2) * rightValue.x +
        (t3 - t2) * m1.x,
        (2.f * t3 - 3.f * t2 + 1.f) * leftValue.y +
        (t3 - 2.f * t2 + t) * m0.y +
        (-2.f * t3 + 3.f * t2) * rightValue.y +
        (t3 - t2) * m1.y
    };
}

Vec2 EffectAssetRuntimeLoader::Evaluate_Vector2ConstantCurve(const json& keys, float x, const Vec2& fallbackValue)
{
    if (!keys.is_array() || keys.empty())
        return fallbackValue;

    auto read_value = [](const json& key, const Vec2& fallback)
    {
        Vec2 value = fallback;
        if (const auto iter = key.find("value"); iter != key.end())
            Read_Vec2(*iter, value);
        return value;
    };

    if (keys.size() == 1)
        return read_value(keys.front(), fallbackValue);

    const float clampedX = clamp(x, 0.f, 1.f);
    for (size_t index = 1; index < keys.size(); ++index)
    {
        const json& leftKey = keys[index - 1];
        const json& rightKey = keys[index];
        if (clampedX > Read_Float(rightKey, "time", 1.f))
            continue;

        const string interpolationMode = Read_String(leftKey, "interpolationMode", "Linear");
        if ("Constant" == interpolationMode)
            return read_value(leftKey, fallbackValue);

        if ("CurveAutoClamped" == interpolationMode)
            return Evaluate_Vector2CurveSegmentAutoClamped(leftKey, rightKey, clampedX);

        return Evaluate_Vector2CurveSegmentLinear(leftKey, rightKey, clampedX);
    }

    return read_value(keys.back(), fallbackValue);
}

Vec3 EffectAssetRuntimeLoader::Evaluate_Vector3ConstantCurve(const json& keys, float x, const Vec3& fallbackValue)
{
    if (!keys.is_array() || keys.empty())
        return fallbackValue;

    auto read_value = [](const json& key, const Vec3& fallback)
    {
        Vec3 value = fallback;
        if (const auto iter = key.find("value"); iter != key.end())
            Read_Vec3(*iter, value);
        return value;
    };

    if (keys.size() == 1)
        return read_value(keys.front(), fallbackValue);

    const float clampedX = clamp(x, 0.f, 1.f);
    for (size_t index = 1; index < keys.size(); ++index)
    {
        const json& leftKey = keys[index - 1];
        const json& rightKey = keys[index];
        if (clampedX > Read_Float(rightKey, "time", 1.f))
            continue;

        const string interpolationMode = Read_String(leftKey, "interpolationMode", "Linear");
        if ("Constant" == interpolationMode)
            return read_value(leftKey, fallbackValue);

        const float leftTime = Read_Float(leftKey, "time");
        const float rightTime = Read_Float(rightKey, "time", 1.f);
        const float width = max(0.0001f, rightTime - leftTime);
        const float t = clamp((clampedX - leftTime) / width, 0.f, 1.f);
        const Vec3 leftValue = read_value(leftKey, fallbackValue);
        const Vec3 rightValue = read_value(rightKey, fallbackValue);
        return Vec3{
            lerp(leftValue.x, rightValue.x, t),
            lerp(leftValue.y, rightValue.y, t),
            lerp(leftValue.z, rightValue.z, t)
        };
    }

    return read_value(keys.back(), fallbackValue);
}

Vec4 EffectAssetRuntimeLoader::Evaluate_ColorConstantCurve(const json& keys, float x, const Vec4& fallbackValue)
{
    if (!keys.is_array() || keys.empty())
        return fallbackValue;

    auto read_value = [](const json& key, const Vec4& fallback)
    {
        Vec3 rgb{ fallback.x, fallback.y, fallback.z };
        if (const auto iter = key.find("value"); iter != key.end())
            Read_Vec3(*iter, rgb);
        return Vec4{ rgb.x, rgb.y, rgb.z, fallback.w };
    };

    if (keys.size() == 1)
        return read_value(keys.front(), fallbackValue);

    const float clampedX = clamp(x, 0.f, 1.f);
    for (size_t index = 1; index < keys.size(); ++index)
    {
        const json& leftKey = keys[index - 1];
        const json& rightKey = keys[index];
        if (clampedX > Read_Float(rightKey, "time", 1.f))
            continue;

        const string interpolationMode = Read_String(leftKey, "interpolationMode", "Linear");
        if ("Constant" == interpolationMode)
            return read_value(leftKey, fallbackValue);

        const float leftTime = Read_Float(leftKey, "time");
        const float rightTime = Read_Float(rightKey, "time", 1.f);
        const float width = max(0.0001f, rightTime - leftTime);
        const float t = clamp((clampedX - leftTime) / width, 0.f, 1.f);
        const Vec4 leftValue = read_value(leftKey, fallbackValue);
        const Vec4 rightValue = read_value(rightKey, fallbackValue);
        return Vec4{
            lerp(leftValue.x, rightValue.x, t),
            lerp(leftValue.y, rightValue.y, t),
            lerp(leftValue.z, rightValue.z, t),
            fallbackValue.w
        };
    }

    return read_value(keys.back(), fallbackValue);
}

const json* EffectAssetRuntimeLoader::Find_DistributionPayload(const json& distribution)
{
    const auto payloadIter = distribution.find("payload");
    if (payloadIter == distribution.end() || !payloadIter->is_object())
        return nullptr;

    return &*payloadIter;
}

float EffectAssetRuntimeLoader::Evaluate_FloatDistributionMin(const json& distribution, float fallbackValue)
{
    const json* payload = Find_DistributionPayload(distribution);
    if (nullptr == payload)
        return fallbackValue;

    const string mode = Read_String(distribution, "mode");
    if ("Constant" == mode)
        return Read_Float(*payload, "value", fallbackValue);

    if ("Uniform" == mode)
        return Read_Float(*payload, "minValue", fallbackValue);

    if ("ConstantCurve" == mode)
    {
        const auto keysIter = payload->find("keys");
        return keysIter != payload->end() ? Evaluate_FloatConstantCurve(*keysIter, 0.f, fallbackValue) : fallbackValue;
    }

    return fallbackValue;
}

float EffectAssetRuntimeLoader::Evaluate_FloatDistributionMax(const json& distribution, float fallbackValue)
{
    const json* payload = Find_DistributionPayload(distribution);
    if (nullptr == payload)
        return fallbackValue;

    const string mode = Read_String(distribution, "mode");
    if ("Constant" == mode)
        return Read_Float(*payload, "value", fallbackValue);

    if ("Uniform" == mode)
        return Read_Float(*payload, "maxValue", fallbackValue);

    if ("ConstantCurve" == mode)
    {
        const auto keysIter = payload->find("keys");
        return keysIter != payload->end() ? Evaluate_FloatConstantCurve(*keysIter, 1.f, fallbackValue) : fallbackValue;
    }

    return fallbackValue;
}

Vec2 EffectAssetRuntimeLoader::Evaluate_Vector2DistributionMin(const json& distribution, const Vec2& fallbackValue)
{
    const json* payload = Find_DistributionPayload(distribution);
    if (nullptr == payload)
        return fallbackValue;

    Vec2 value = fallbackValue;
    if ("Uniform" == Read_String(distribution, "mode"))
    {
        const auto iter = payload->find("minValue");
        if (iter != payload->end())
            Read_Vec2(*iter, value);
        return value;
    }

    if ("ConstantCurve" == Read_String(distribution, "mode"))
    {
        const auto keysIter = payload->find("keys");
        return keysIter != payload->end() ? Evaluate_Vector2ConstantCurve(*keysIter, 0.f, fallbackValue) : fallbackValue;
    }

    const auto iter = payload->find("value");
    if (iter != payload->end())
        Read_Vec2(*iter, value);
    return value;
}

Vec2 EffectAssetRuntimeLoader::Evaluate_Vector2DistributionMax(const json& distribution, const Vec2& fallbackValue)
{
    const json* payload = Find_DistributionPayload(distribution);
    if (nullptr == payload)
        return fallbackValue;

    Vec2 value = fallbackValue;
    if ("Uniform" == Read_String(distribution, "mode"))
    {
        const auto iter = payload->find("maxValue");
        if (iter != payload->end())
            Read_Vec2(*iter, value);
        return value;
    }

    if ("ConstantCurve" == Read_String(distribution, "mode"))
    {
        const auto keysIter = payload->find("keys");
        return keysIter != payload->end() ? Evaluate_Vector2ConstantCurve(*keysIter, 1.f, fallbackValue) : fallbackValue;
    }

    const auto iter = payload->find("value");
    if (iter != payload->end())
        Read_Vec2(*iter, value);
    return value;
}

Vec3 EffectAssetRuntimeLoader::Evaluate_Vector3DistributionMin(const json& distribution, const Vec3& fallbackValue)
{
    const json* payload = Find_DistributionPayload(distribution);
    if (nullptr == payload)
        return fallbackValue;

    Vec3 value = fallbackValue;
    if ("Uniform" == Read_String(distribution, "mode"))
    {
        const auto iter = payload->find("minValue");
        if (iter != payload->end())
            Read_Vec3(*iter, value);
        return value;
    }

    if ("ConstantCurve" == Read_String(distribution, "mode"))
    {
        const auto keysIter = payload->find("keys");
        return keysIter != payload->end() ? Evaluate_Vector3ConstantCurve(*keysIter, 0.f, fallbackValue) : fallbackValue;
    }

    const auto iter = payload->find("value");
    if (iter != payload->end())
        Read_Vec3(*iter, value);
    return value;
}

Vec3 EffectAssetRuntimeLoader::Evaluate_Vector3DistributionMax(const json& distribution, const Vec3& fallbackValue)
{
    const json* payload = Find_DistributionPayload(distribution);
    if (nullptr == payload)
        return fallbackValue;

    Vec3 value = fallbackValue;
    if ("Uniform" == Read_String(distribution, "mode"))
    {
        const auto iter = payload->find("maxValue");
        if (iter != payload->end())
            Read_Vec3(*iter, value);
        return value;
    }

    if ("ConstantCurve" == Read_String(distribution, "mode"))
    {
        const auto keysIter = payload->find("keys");
        return keysIter != payload->end() ? Evaluate_Vector3ConstantCurve(*keysIter, 1.f, fallbackValue) : fallbackValue;
    }

    const auto iter = payload->find("value");
    if (iter != payload->end())
        Read_Vec3(*iter, value);
    return value;
}

Vec4 EffectAssetRuntimeLoader::Clamp_Color(Vec4 value)
{
    value.x = clamp(value.x, 0.f, 1.f);
    value.y = clamp(value.y, 0.f, 1.f);
    value.z = clamp(value.z, 0.f, 1.f);
    value.w = clamp(value.w, 0.f, 1.f);
    return value;
}

Vec4 EffectAssetRuntimeLoader::Evaluate_ColorDistributionMin(const json& distribution, const Vec4& fallbackValue)
{
    const json* payload = Find_DistributionPayload(distribution);
    if (nullptr == payload)
        return Clamp_Color(fallbackValue);

    Vec4 value = fallbackValue;
    if ("Uniform" == Read_String(distribution, "mode"))
    {
        const auto iter = payload->find("minValue");
        if (iter != payload->end())
            Read_Vec4(*iter, value);
        return Clamp_Color(value);
    }

    if ("ConstantCurve" == Read_String(distribution, "mode"))
    {
        const auto keysIter = payload->find("keys");
        return Clamp_Color(keysIter != payload->end() ? Evaluate_ColorConstantCurve(*keysIter, 0.f, fallbackValue) : fallbackValue);
    }

    const auto iter = payload->find("value");
    if (iter != payload->end())
        Read_Vec4(*iter, value);
    return Clamp_Color(value);
}

Vec4 EffectAssetRuntimeLoader::Evaluate_ColorDistributionMax(const json& distribution, const Vec4& fallbackValue)
{
    const json* payload = Find_DistributionPayload(distribution);
    if (nullptr == payload)
        return Clamp_Color(fallbackValue);

    Vec4 value = fallbackValue;
    if ("Uniform" == Read_String(distribution, "mode"))
    {
        const auto iter = payload->find("maxValue");
        if (iter != payload->end())
            Read_Vec4(*iter, value);
        return Clamp_Color(value);
    }

    if ("ConstantCurve" == Read_String(distribution, "mode"))
    {
        const auto keysIter = payload->find("keys");
        return Clamp_Color(keysIter != payload->end() ? Evaluate_ColorConstantCurve(*keysIter, 1.f, fallbackValue) : fallbackValue);
    }

    const auto iter = payload->find("value");
    if (iter != payload->end())
        Read_Vec4(*iter, value);
    return Clamp_Color(value);
}

Vec4 EffectAssetRuntimeLoader::Evaluate_ColorOverLifeEndpointColorMin(const json& distribution, const Vec4& fallbackValue)
{
    const json* payload = Find_DistributionPayload(distribution);
    if (nullptr == payload)
        return Clamp_Color(fallbackValue);

    if ("ConstantCurve" == Read_String(distribution, "mode"))
    {
        const auto keysIter = payload->find("keys");
        return Clamp_Color(keysIter != payload->end() ? Evaluate_ColorConstantCurve(*keysIter, 1.f, fallbackValue) : fallbackValue);
    }

    return Evaluate_ColorDistributionMin(distribution, fallbackValue);
}

Vec4 EffectAssetRuntimeLoader::Evaluate_ColorOverLifeEndpointColorMax(const json& distribution, const Vec4& fallbackValue)
{
    const json* payload = Find_DistributionPayload(distribution);
    if (nullptr == payload)
        return Clamp_Color(fallbackValue);

    if ("ConstantCurve" == Read_String(distribution, "mode"))
    {
        const auto keysIter = payload->find("keys");
        return Clamp_Color(keysIter != payload->end() ? Evaluate_ColorConstantCurve(*keysIter, 1.f, fallbackValue) : fallbackValue);
    }

    return Evaluate_ColorDistributionMax(distribution, fallbackValue);
}

float EffectAssetRuntimeLoader::Evaluate_ColorOverLifeEndpointAlphaMin(const json& distribution, float fallbackValue)
{
    const json* payload = Find_DistributionPayload(distribution);
    if (nullptr == payload)
        return fallbackValue;

    if ("ConstantCurve" == Read_String(distribution, "mode"))
    {
        const auto keysIter = payload->find("keys");
        return keysIter != payload->end() ? Evaluate_FloatConstantCurve(*keysIter, 1.f, fallbackValue) : fallbackValue;
    }

    return Evaluate_FloatDistributionMin(distribution, fallbackValue);
}

float EffectAssetRuntimeLoader::Evaluate_ColorOverLifeEndpointAlphaMax(const json& distribution, float fallbackValue)
{
    const json* payload = Find_DistributionPayload(distribution);
    if (nullptr == payload)
        return fallbackValue;

    if ("ConstantCurve" == Read_String(distribution, "mode"))
    {
        const auto keysIter = payload->find("keys");
        return keysIter != payload->end() ? Evaluate_FloatConstantCurve(*keysIter, 1.f, fallbackValue) : fallbackValue;
    }

    return Evaluate_FloatDistributionMax(distribution, fallbackValue);
}

float EffectAssetRuntimeLoader::Resolve_CurveInterpolationMode(const json& key)
{
    const string interpolationMode = Read_String(key, "interpolationMode", "Linear");
    if ("Constant" == interpolationMode)
        return 0.f;
    if ("CurveAutoClamped" == interpolationMode)
        return 2.f;
    return 1.f;
}

void EffectAssetRuntimeLoader::Fill_ColorOverLifeCurvePayload(
    PointParticleColorOverLifeCurveDesc& desc,
    const json& colorOverLife)
{
    constexpr uint32 maxColorOverLifeCurveKeys{ ::Engine::kEffectDistributionCurveMaxKeys };

    if (const auto colorIter = colorOverLife.find("colorOverLife"); colorIter != colorOverLife.end())
    {
        const json* payload = Find_DistributionPayload(*colorIter);
        if (payload != nullptr && "ConstantCurve" == Read_String(*colorIter, "mode"))
        {
            const auto keysIter = payload->find("keys");
            if (keysIter != payload->end() && keysIter->is_array() && !keysIter->empty())
            {
                vector<const json*> keys{};
                keys.reserve(keysIter->size());
                for (const json& key : *keysIter)
                    keys.push_back(&key);
                ranges::sort(keys, [](const json* lhs, const json* rhs) { return Read_Float(*lhs, "time") < Read_Float(*rhs, "time"); });

                const uint32 keyCount = min(maxColorOverLifeCurveKeys, static_cast<uint32>(keys.size()));
                desc.colorCurveEnabled = true;
                desc.colorCurveKeyCount = keyCount;
                desc.colorCurveTimes = Vec4{};
                desc.colorCurveTimesBlock1 = Vec4{};
                desc.colorCurveValuesR = Vec4{};
                desc.colorCurveValuesRBlock1 = Vec4{};
                desc.colorCurveValuesG = Vec4{};
                desc.colorCurveValuesGBlock1 = Vec4{};
                desc.colorCurveValuesB = Vec4{};
                desc.colorCurveValuesBBlock1 = Vec4{};
                desc.colorCurveArriveR = Vec4{};
                desc.colorCurveArriveRBlock1 = Vec4{};
                desc.colorCurveArriveG = Vec4{};
                desc.colorCurveArriveGBlock1 = Vec4{};
                desc.colorCurveArriveB = Vec4{};
                desc.colorCurveArriveBBlock1 = Vec4{};
                desc.colorCurveLeaveR = Vec4{};
                desc.colorCurveLeaveRBlock1 = Vec4{};
                desc.colorCurveLeaveG = Vec4{};
                desc.colorCurveLeaveGBlock1 = Vec4{};
                desc.colorCurveLeaveB = Vec4{};
                desc.colorCurveLeaveBBlock1 = Vec4{};
                desc.colorCurveModes = Vec4{};
                desc.colorCurveModesBlock1 = Vec4{};
                for (uint32 index = 0; index < keyCount; ++index)
                {
                    const json& key = *keys[index];
                    Vec3 value{ 1.f, 1.f, 1.f };
                    Vec3 arriveTangent{};
                    Vec3 leaveTangent{};
                    if (const auto valueIter = key.find("value"); valueIter != key.end())
                        Read_Vec3(*valueIter, value);
                    if (const auto arriveIter = key.find("arriveTangent"); arriveIter != key.end())
                        Read_Vec3(*arriveIter, arriveTangent);
                    if (const auto leaveIter = key.find("leaveTangent"); leaveIter != key.end())
                        Read_Vec3(*leaveIter, leaveTangent);

                    Set_CurveComponent(desc.colorCurveTimes, desc.colorCurveTimesBlock1, index, clamp(Read_Float(key, "time"), 0.f, 1.f));
                    Set_CurveComponent(desc.colorCurveValuesR, desc.colorCurveValuesRBlock1, index, value.x);
                    Set_CurveComponent(desc.colorCurveValuesG, desc.colorCurveValuesGBlock1, index, value.y);
                    Set_CurveComponent(desc.colorCurveValuesB, desc.colorCurveValuesBBlock1, index, value.z);
                    Set_CurveComponent(desc.colorCurveArriveR, desc.colorCurveArriveRBlock1, index, arriveTangent.x);
                    Set_CurveComponent(desc.colorCurveArriveG, desc.colorCurveArriveGBlock1, index, arriveTangent.y);
                    Set_CurveComponent(desc.colorCurveArriveB, desc.colorCurveArriveBBlock1, index, arriveTangent.z);
                    Set_CurveComponent(desc.colorCurveLeaveR, desc.colorCurveLeaveRBlock1, index, leaveTangent.x);
                    Set_CurveComponent(desc.colorCurveLeaveG, desc.colorCurveLeaveGBlock1, index, leaveTangent.y);
                    Set_CurveComponent(desc.colorCurveLeaveB, desc.colorCurveLeaveBBlock1, index, leaveTangent.z);
                    Set_CurveComponent(desc.colorCurveModes, desc.colorCurveModesBlock1, index, Resolve_CurveInterpolationMode(key));
                }
            }
        }
    }

    if (const auto alphaIter = colorOverLife.find("alphaOverLife"); alphaIter != colorOverLife.end())
    {
        const json* payload = Find_DistributionPayload(*alphaIter);
        if (payload != nullptr && "ConstantCurve" == Read_String(*alphaIter, "mode"))
        {
            const auto keysIter = payload->find("keys");
            if (keysIter != payload->end() && keysIter->is_array() && !keysIter->empty())
            {
                vector<const json*> keys{};
                keys.reserve(keysIter->size());
                for (const json& key : *keysIter)
                    keys.push_back(&key);
                ranges::sort(keys, [](const json* lhs, const json* rhs) { return Read_Float(*lhs, "time") < Read_Float(*rhs, "time"); });

                const uint32 keyCount = min(maxColorOverLifeCurveKeys, static_cast<uint32>(keys.size()));
                desc.alphaCurveEnabled = true;
                desc.alphaCurveKeyCount = keyCount;
                desc.alphaCurveTimes = Vec4{};
                desc.alphaCurveTimesBlock1 = Vec4{};
                desc.alphaCurveValues = Vec4{};
                desc.alphaCurveValuesBlock1 = Vec4{};
                desc.alphaCurveArrive = Vec4{};
                desc.alphaCurveArriveBlock1 = Vec4{};
                desc.alphaCurveLeave = Vec4{};
                desc.alphaCurveLeaveBlock1 = Vec4{};
                desc.alphaCurveModes = Vec4{};
                desc.alphaCurveModesBlock1 = Vec4{};
                for (uint32 index = 0; index < keyCount; ++index)
                {
                    const json& key = *keys[index];
                    Set_CurveComponent(desc.alphaCurveTimes, desc.alphaCurveTimesBlock1, index, clamp(Read_Float(key, "time"), 0.f, 1.f));
                    Set_CurveComponent(desc.alphaCurveValues, desc.alphaCurveValuesBlock1, index, Read_Float(key, "value", 1.f));
                    Set_CurveComponent(desc.alphaCurveArrive, desc.alphaCurveArriveBlock1, index, Read_Float(key, "arriveTangent"));
                    Set_CurveComponent(desc.alphaCurveLeave, desc.alphaCurveLeaveBlock1, index, Read_Float(key, "leaveTangent"));
                    Set_CurveComponent(desc.alphaCurveModes, desc.alphaCurveModesBlock1, index, Resolve_CurveInterpolationMode(key));
                }
            }
        }
    }
}

void EffectAssetRuntimeLoader::Set_SubUVFrameCurveComponent(Vec4& values, uint32 index, float value)
{
    switch (index)
    {
    case 0:
        values.x = value;
        break;
    case 1:
        values.y = value;
        break;
    case 2:
        values.z = value;
        break;
    case 3:
        values.w = value;
        break;
    default:
        break;
    }
}

void EffectAssetRuntimeLoader::Set_CurveComponent(Vec4& valuesBlock0, Vec4& valuesBlock1, uint32 index, float value)
{
    if (index < 4u)
    {
        Set_SubUVFrameCurveComponent(valuesBlock0, index, value);
        return;
    }

    Set_SubUVFrameCurveComponent(valuesBlock1, index - 4u, value);
}

void EffectAssetRuntimeLoader::Fill_SubUVFrameCurvePayload(PointParticleSubUVFrameOverLifeDesc& desc, const json& frameIndex)
{
    desc.frameCurveKeyCount = 1;
    desc.frameCurveTimes = Vec4{};
    desc.frameCurveTimesBlock1 = Vec4{};
    desc.frameCurveValues = Vec4{};
    desc.frameCurveValuesBlock1 = Vec4{};

    const json* payload = Find_DistributionPayload(frameIndex);
    if (nullptr == payload)
        return;

    if ("Constant" == Read_String(frameIndex, "mode"))
    {
        desc.frameCurveValues.x = Read_Float(*payload, "value");
        return;
    }

    if ("ConstantCurve" != Read_String(frameIndex, "mode"))
        return;

    const auto keysIter = payload->find("keys");
    if (keysIter == payload->end() || !keysIter->is_array() || keysIter->empty())
        return;

    const uint32 keyCount = min(kMaxSubUVFrameCurveKeys, static_cast<uint32>(keysIter->size()));
    desc.frameCurveKeyCount = keyCount;

    for (uint32 index = 0; index < keyCount; ++index)
    {
        const json& key = (*keysIter)[index];
        Set_CurveComponent(desc.frameCurveTimes, desc.frameCurveTimesBlock1, index, clamp(Read_Float(key, "time"), 0.f, 1.f));
        Set_CurveComponent(desc.frameCurveValues, desc.frameCurveValuesBlock1, index, Read_Float(key, "value"));
    }
}

void EffectAssetRuntimeLoader::Fill_SizeByLifePayload(PointParticleSizeByLifeDesc& desc, const json& sizeByLife)
{
    desc.enabled = true;
    desc.multiplyX = Read_Bool(sizeByLife, "multiplyX", desc.multiplyX);
    desc.multiplyY = Read_Bool(sizeByLife, "multiplyY", desc.multiplyY);
    Read_Enum(sizeByLife, "axisLock", desc.axisLock);

    const auto scaleOverLifeIter = sizeByLife.find("scaleOverLife");
    if (scaleOverLifeIter == sizeByLife.end())
    {
        desc.multiplyXStart = Read_Float(sizeByLife, "multiplyXStart", desc.multiplyXStart);
        desc.multiplyXEnd = Read_Float(sizeByLife, "multiplyXEnd", desc.multiplyXEnd);
        desc.multiplyYStart = Read_Float(sizeByLife, "multiplyYStart", desc.multiplyYStart);
        desc.multiplyYEnd = Read_Float(sizeByLife, "multiplyYEnd", desc.multiplyYEnd);
        desc.curveKeyCount = 2;
        desc.curveKeyTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
        desc.curveKeyTimesBlock1 = Vec4{};
        desc.curveKeyValuesX = Vec4{ desc.multiplyXStart, desc.multiplyXEnd, 0.f, 0.f };
        desc.curveKeyValuesXBlock1 = Vec4{};
        desc.curveKeyValuesY = Vec4{ desc.multiplyYStart, desc.multiplyYEnd, 0.f, 0.f };
        desc.curveKeyValuesYBlock1 = Vec4{};
        return;
    }

    const Vec2 startValue = Evaluate_Vector2DistributionMin(*scaleOverLifeIter, Vec2{ 1.f, 1.f });
    const Vec2 endValue = Evaluate_Vector2DistributionMax(*scaleOverLifeIter, Vec2{ 1.f, 1.f });
    desc.multiplyXStart = startValue.x;
    desc.multiplyXEnd = endValue.x;
    desc.multiplyYStart = startValue.y;
    desc.multiplyYEnd = endValue.y;
    desc.curveKeyCount = 2;
    desc.curveKeyTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
    desc.curveKeyTimesBlock1 = Vec4{};
    desc.curveKeyValuesX = Vec4{ startValue.x, endValue.x, 0.f, 0.f };
    desc.curveKeyValuesXBlock1 = Vec4{};
    desc.curveKeyValuesY = Vec4{ startValue.y, endValue.y, 0.f, 0.f };
    desc.curveKeyValuesYBlock1 = Vec4{};

    const json* payload = Find_DistributionPayload(*scaleOverLifeIter);
    if (payload == nullptr || "ConstantCurve" != Read_String(*scaleOverLifeIter, "mode"))
        return;

    const auto keysIter = payload->find("keys");
    if (keysIter == payload->end() || !keysIter->is_array() || keysIter->empty())
        return;

    const uint32 keyCount = min(kMaxSubUVFrameCurveKeys, static_cast<uint32>(keysIter->size()));
    desc.curveKeyCount = keyCount;
    desc.curveKeyTimes = Vec4{};
    desc.curveKeyTimesBlock1 = Vec4{};
    desc.curveKeyValuesX = Vec4{};
    desc.curveKeyValuesXBlock1 = Vec4{};
    desc.curveKeyValuesY = Vec4{};
    desc.curveKeyValuesYBlock1 = Vec4{};

    for (uint32 index = 0; index < keyCount; ++index)
    {
        const json& key = (*keysIter)[index];
        Vec2 value{ 1.f, 1.f };
        if (const auto valueIter = key.find("value"); valueIter != key.end())
            Read_Vec2(*valueIter, value);

        Set_CurveComponent(desc.curveKeyTimes, desc.curveKeyTimesBlock1, index, clamp(Read_Float(key, "time"), 0.f, 1.f));
        Set_CurveComponent(desc.curveKeyValuesX, desc.curveKeyValuesXBlock1, index, value.x);
        Set_CurveComponent(desc.curveKeyValuesY, desc.curveKeyValuesYBlock1, index, value.y);
    }
}

void EffectAssetRuntimeLoader::Fill_MaterialScalarModulationPayload(
    EffectMaterialScalarModulationRuntimeDesc& desc,
    const json& materialScalarModulation)
{
    desc = EffectMaterialScalarModulationRuntimeDesc{};

    const auto modulatorsIter = materialScalarModulation.find("modulators");
    if (modulatorsIter == materialScalarModulation.end() || !modulatorsIter->is_array())
        return;

    const uint32 count = min(
        EffectMaterialScalarModulationRuntimeDesc::kMaxModulators,
        static_cast<uint32>(modulatorsIter->size())
    );
    desc.count = count;

    for (uint32 index = 0; index < count; ++index)
    {
        const json& source = (*modulatorsIter)[index];
        EffectMaterialScalarModulatorRuntimeDesc& target = desc.modulators[index];
        target.enabled = Read_Bool(source, "enabled", target.enabled);
        Read_Enum(source, "targetField", target.targetField);
        Read_Enum(source, "operation", target.operation);
        Read_Enum(source, "timeSource", target.timeSource);

        const auto distributionIter = source.find("distribution");
        if (distributionIter == source.end() || !distributionIter->is_object())
            continue;

        target.curve = EffectMaterialScalarModulationCurveDesc{};
        target.curve.enabled = true;

        const string mode = Read_String(*distributionIter, "mode");
        const json* payload = Find_DistributionPayload(*distributionIter);
        if (payload != nullptr && mode == "ConstantCurve")
        {
            const auto keysIter = payload->find("keys");
            if (keysIter != payload->end() && keysIter->is_array() && !keysIter->empty())
            {
                const uint32 keyCount = min(kMaxSubUVFrameCurveKeys, static_cast<uint32>(keysIter->size()));
                target.curve.keyCount = keyCount;
                target.curve.keyTimes = Vec4{};
                target.curve.keyTimesBlock1 = Vec4{};
                target.curve.keyValues = Vec4{};
                target.curve.keyValuesBlock1 = Vec4{};
                target.curve.keyArriveTangents = Vec4{};
                target.curve.keyArriveTangentsBlock1 = Vec4{};
                target.curve.keyLeaveTangents = Vec4{};
                target.curve.keyLeaveTangentsBlock1 = Vec4{};
                target.curve.keyModes = Vec4{};
                target.curve.keyModesBlock1 = Vec4{};

                for (uint32 keyIndex = 0; keyIndex < keyCount; ++keyIndex)
                {
                    const json& key = (*keysIter)[keyIndex];
                    Set_CurveComponent(target.curve.keyTimes, target.curve.keyTimesBlock1, keyIndex, clamp(Read_Float(key, "time"), 0.f, 1.f));
                    Set_CurveComponent(target.curve.keyValues, target.curve.keyValuesBlock1, keyIndex, Read_Float(key, "value", 1.f));
                    Set_CurveComponent(target.curve.keyArriveTangents, target.curve.keyArriveTangentsBlock1, keyIndex, Read_Float(key, "arriveTangent"));
                    Set_CurveComponent(target.curve.keyLeaveTangents, target.curve.keyLeaveTangentsBlock1, keyIndex, Read_Float(key, "leaveTangent"));
                    Set_CurveComponent(target.curve.keyModes, target.curve.keyModesBlock1, keyIndex, Resolve_CurveInterpolationMode(key));
                }
                continue;
            }
        }

        float value = 1.f;
        if (payload != nullptr && mode == "Constant")
            value = Read_Float(*payload, "value", value);
        else if (payload != nullptr && mode == "Uniform")
            value = (Read_Float(*payload, "minValue", value) + Read_Float(*payload, "maxValue", value)) * 0.5f;

        target.curve.keyCount = 2;
        target.curve.keyValues = Vec4{ value, value, 0.f, 0.f };
        target.curve.keyValuesBlock1 = Vec4{};
    }
}

void EffectAssetRuntimeLoader::Fill_CoreColorRgbModulationPayload(
    EffectMaterialCoreColorRgbModulationRuntimeDesc& desc,
    const json& materialScalarModulation)
{
    desc = EffectMaterialCoreColorRgbModulationRuntimeDesc{};

    const auto modulatorsIter = materialScalarModulation.find("coreColorRgbModulators");
    if (modulatorsIter == materialScalarModulation.end() || !modulatorsIter->is_array())
        return;

    const uint32 count = min(
        EffectMaterialCoreColorRgbModulationRuntimeDesc::kMaxModulators,
        static_cast<uint32>(modulatorsIter->size())
    );
    desc.count = count;

    for (uint32 index = 0; index < count; ++index)
    {
        const json& source = (*modulatorsIter)[index];
        EffectMaterialCoreColorRgbModulatorRuntimeDesc& target = desc.modulators[index];
        target.enabled = Read_Bool(source, "enabled", target.enabled);
        Read_Enum(source, "timeSource", target.timeSource);

        const auto distributionIter = source.find("distribution");
        if (distributionIter == source.end() || !distributionIter->is_object())
            continue;

        target.curve = EffectMaterialCoreColorRgbModulationCurveDesc{};
        target.curve.enabled = true;

        const string mode = Read_String(*distributionIter, "mode");
        const json* payload = Find_DistributionPayload(*distributionIter);
        if (payload != nullptr && mode == "ConstantCurve")
        {
            const auto keysIter = payload->find("keys");
            if (keysIter != payload->end() && keysIter->is_array() && !keysIter->empty())
            {
                const uint32 keyCount = min(kMaxSubUVFrameCurveKeys, static_cast<uint32>(keysIter->size()));
                target.curve.keyCount = keyCount;
                target.curve.keyTimes = Vec4{};
                target.curve.keyTimesBlock1 = Vec4{};
                target.curve.keyValuesR = Vec4{};
                target.curve.keyValuesRBlock1 = Vec4{};
                target.curve.keyValuesG = Vec4{};
                target.curve.keyValuesGBlock1 = Vec4{};
                target.curve.keyValuesB = Vec4{};
                target.curve.keyValuesBBlock1 = Vec4{};
                target.curve.keyArriveTangentsR = Vec4{};
                target.curve.keyArriveTangentsRBlock1 = Vec4{};
                target.curve.keyArriveTangentsG = Vec4{};
                target.curve.keyArriveTangentsGBlock1 = Vec4{};
                target.curve.keyArriveTangentsB = Vec4{};
                target.curve.keyArriveTangentsBBlock1 = Vec4{};
                target.curve.keyLeaveTangentsR = Vec4{};
                target.curve.keyLeaveTangentsRBlock1 = Vec4{};
                target.curve.keyLeaveTangentsG = Vec4{};
                target.curve.keyLeaveTangentsGBlock1 = Vec4{};
                target.curve.keyLeaveTangentsB = Vec4{};
                target.curve.keyLeaveTangentsBBlock1 = Vec4{};
                target.curve.keyModes = Vec4{};
                target.curve.keyModesBlock1 = Vec4{};

                for (uint32 keyIndex = 0; keyIndex < keyCount; ++keyIndex)
                {
                    const json& key = (*keysIter)[keyIndex];
                    Vec3 value{ 1.f, 0.85f, 0.45f };
                    Vec3 arriveTangent{};
                    Vec3 leaveTangent{};
                    if (const auto valueIter = key.find("value"); valueIter != key.end())
                        Read_Vec3(*valueIter, value);
                    if (const auto arriveIter = key.find("arriveTangent"); arriveIter != key.end())
                        Read_Vec3(*arriveIter, arriveTangent);
                    if (const auto leaveIter = key.find("leaveTangent"); leaveIter != key.end())
                        Read_Vec3(*leaveIter, leaveTangent);

                    Set_CurveComponent(target.curve.keyTimes, target.curve.keyTimesBlock1, keyIndex, clamp(Read_Float(key, "time"), 0.f, 1.f));
                    Set_CurveComponent(target.curve.keyValuesR, target.curve.keyValuesRBlock1, keyIndex, value.x);
                    Set_CurveComponent(target.curve.keyValuesG, target.curve.keyValuesGBlock1, keyIndex, value.y);
                    Set_CurveComponent(target.curve.keyValuesB, target.curve.keyValuesBBlock1, keyIndex, value.z);
                    Set_CurveComponent(target.curve.keyArriveTangentsR, target.curve.keyArriveTangentsRBlock1, keyIndex, arriveTangent.x);
                    Set_CurveComponent(target.curve.keyArriveTangentsG, target.curve.keyArriveTangentsGBlock1, keyIndex, arriveTangent.y);
                    Set_CurveComponent(target.curve.keyArriveTangentsB, target.curve.keyArriveTangentsBBlock1, keyIndex, arriveTangent.z);
                    Set_CurveComponent(target.curve.keyLeaveTangentsR, target.curve.keyLeaveTangentsRBlock1, keyIndex, leaveTangent.x);
                    Set_CurveComponent(target.curve.keyLeaveTangentsG, target.curve.keyLeaveTangentsGBlock1, keyIndex, leaveTangent.y);
                    Set_CurveComponent(target.curve.keyLeaveTangentsB, target.curve.keyLeaveTangentsBBlock1, keyIndex, leaveTangent.z);
                    Set_CurveComponent(target.curve.keyModes, target.curve.keyModesBlock1, keyIndex, Resolve_CurveInterpolationMode(key));
                }
                continue;
            }
        }

        Vec3 value{ 1.f, 0.85f, 0.45f };
        if (payload != nullptr && mode == "Constant")
        {
            if (const auto valueIter = payload->find("value"); valueIter != payload->end())
                Read_Vec3(*valueIter, value);
        }
        else if (payload != nullptr && mode == "Uniform")
        {
            Vec3 minValue = value;
            Vec3 maxValue = value;
            if (const auto minIter = payload->find("minValue"); minIter != payload->end())
                Read_Vec3(*minIter, minValue);
            if (const auto maxIter = payload->find("maxValue"); maxIter != payload->end())
                Read_Vec3(*maxIter, maxValue);
            value = (minValue + maxValue) * 0.5f;
        }

        target.curve.keyCount = 2;
        target.curve.keyValuesR = Vec4{ value.x, value.x, 0.f, 0.f };
        target.curve.keyValuesRBlock1 = Vec4{};
        target.curve.keyValuesG = Vec4{ value.y, value.y, 0.f, 0.f };
        target.curve.keyValuesGBlock1 = Vec4{};
        target.curve.keyValuesB = Vec4{ value.z, value.z, 0.f, 0.f };
        target.curve.keyValuesBBlock1 = Vec4{};
    }
}

void EffectAssetRuntimeLoader::Fill_RotationOverLifePayload(PointParticleRotationDesc& desc, const json& rotationOverLife)
{
    const auto rotationOverLifeIter = rotationOverLife.find("rotationOverLife");
    if (rotationOverLifeIter == rotationOverLife.end())
    {
        desc.rotationOverLifeDegrees = Vec2{
            Read_Float(rotationOverLife, "rotationStartDegrees", desc.rotationOverLifeDegrees.x),
            Read_Float(rotationOverLife, "rotationEndDegrees", desc.rotationOverLifeDegrees.y)
        };
        desc.rotationOverLifeCurveKeyCount = 2;
        desc.rotationOverLifeCurveTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
        desc.rotationOverLifeCurveTimesBlock1 = Vec4{};
        desc.rotationOverLifeCurveValues = Vec4{ desc.rotationOverLifeDegrees.x, desc.rotationOverLifeDegrees.y, 0.f, 0.f };
        desc.rotationOverLifeCurveValuesBlock1 = Vec4{};
        return;
    }

    const float startValue = Evaluate_FloatDistributionMin(*rotationOverLifeIter, 0.f);
    const float endValue = Evaluate_FloatDistributionMax(*rotationOverLifeIter, 0.f);
    desc.rotationOverLifeDegrees = Vec2{ startValue, endValue };
    desc.rotationOverLifeCurveKeyCount = 2;
    desc.rotationOverLifeCurveTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
    desc.rotationOverLifeCurveTimesBlock1 = Vec4{};
    desc.rotationOverLifeCurveValues = Vec4{ startValue, endValue, 0.f, 0.f };
    desc.rotationOverLifeCurveValuesBlock1 = Vec4{};

    const json* payload = Find_DistributionPayload(*rotationOverLifeIter);
    if (payload == nullptr || "ConstantCurve" != Read_String(*rotationOverLifeIter, "mode"))
        return;

    const auto keysIter = payload->find("keys");
    if (keysIter == payload->end() || !keysIter->is_array() || keysIter->empty())
        return;

    vector<const json*> keys{};
    keys.reserve(keysIter->size());
    for (const json& key : *keysIter)
        keys.push_back(&key);
    ranges::sort(
        keys,
        [](const json* lhs, const json* rhs)
        {
            return Read_Float(*lhs, "time") < Read_Float(*rhs, "time");
        }
    );

    const uint32 keyCount = min(kMaxSubUVFrameCurveKeys, static_cast<uint32>(keys.size()));
    desc.rotationOverLifeCurveKeyCount = keyCount;
    desc.rotationOverLifeCurveTimes = Vec4{};
    desc.rotationOverLifeCurveTimesBlock1 = Vec4{};
    desc.rotationOverLifeCurveValues = Vec4{};
    desc.rotationOverLifeCurveValuesBlock1 = Vec4{};

    for (uint32 index = 0; index < keyCount; ++index)
    {
        const json& key = *keys[index];
        Set_CurveComponent(
            desc.rotationOverLifeCurveTimes,
            desc.rotationOverLifeCurveTimesBlock1,
            index,
            clamp(Read_Float(key, "time"), 0.f, 1.f)
        );
        Set_CurveComponent(
            desc.rotationOverLifeCurveValues,
            desc.rotationOverLifeCurveValuesBlock1,
            index,
            Read_Float(key, "value")
        );
    }
}

void EffectAssetRuntimeLoader::Fill_RotationRateScaleByLifePayload(PointParticleRotationDesc& desc, const json& rotationRateScaleByLife)
{
    const auto scaleOverLifeIter = rotationRateScaleByLife.find("scaleOverLife");
    if (scaleOverLifeIter == rotationRateScaleByLife.end())
    {
        desc.rotationRateScaleByLife = Vec2{
            Read_Float(rotationRateScaleByLife, "scaleStart", desc.rotationRateScaleByLife.x),
            Read_Float(rotationRateScaleByLife, "scaleEnd", desc.rotationRateScaleByLife.y)
        };
        desc.rotationRateScaleByLifeCurveKeyCount = 2;
        desc.rotationRateScaleByLifeCurveTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
        desc.rotationRateScaleByLifeCurveTimesBlock1 = Vec4{};
        desc.rotationRateScaleByLifeCurveValues = Vec4{ desc.rotationRateScaleByLife.x, desc.rotationRateScaleByLife.y, 0.f, 0.f };
        desc.rotationRateScaleByLifeCurveValuesBlock1 = Vec4{};
        return;
    }

    const float startValue = Evaluate_FloatDistributionMin(*scaleOverLifeIter, 1.f);
    const float endValue = Evaluate_FloatDistributionMax(*scaleOverLifeIter, 1.f);
    desc.rotationRateScaleByLife = Vec2{ startValue, endValue };
    desc.rotationRateScaleByLifeCurveKeyCount = 2;
    desc.rotationRateScaleByLifeCurveTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
    desc.rotationRateScaleByLifeCurveTimesBlock1 = Vec4{};
    desc.rotationRateScaleByLifeCurveValues = Vec4{ startValue, endValue, 0.f, 0.f };
    desc.rotationRateScaleByLifeCurveValuesBlock1 = Vec4{};

    const json* payload = Find_DistributionPayload(*scaleOverLifeIter);
    if (payload == nullptr || "ConstantCurve" != Read_String(*scaleOverLifeIter, "mode"))
        return;

    const auto keysIter = payload->find("keys");
    if (keysIter == payload->end() || !keysIter->is_array() || keysIter->empty())
        return;

    vector<const json*> keys{};
    keys.reserve(keysIter->size());
    for (const json& key : *keysIter)
        keys.push_back(&key);
    ranges::sort(
        keys,
        [](const json* lhs, const json* rhs)
        {
            return Read_Float(*lhs, "time") < Read_Float(*rhs, "time");
        }
    );

    const uint32 keyCount = min(kMaxSubUVFrameCurveKeys, static_cast<uint32>(keys.size()));
    desc.rotationRateScaleByLifeCurveKeyCount = keyCount;
    desc.rotationRateScaleByLifeCurveTimes = Vec4{};
    desc.rotationRateScaleByLifeCurveTimesBlock1 = Vec4{};
    desc.rotationRateScaleByLifeCurveValues = Vec4{};
    desc.rotationRateScaleByLifeCurveValuesBlock1 = Vec4{};

    for (uint32 index = 0; index < keyCount; ++index)
    {
        const json& key = *keys[index];
        Set_CurveComponent(
            desc.rotationRateScaleByLifeCurveTimes,
            desc.rotationRateScaleByLifeCurveTimesBlock1,
            index,
            clamp(Read_Float(key, "time"), 0.f, 1.f)
        );
        Set_CurveComponent(
            desc.rotationRateScaleByLifeCurveValues,
            desc.rotationRateScaleByLifeCurveValuesBlock1,
            index,
            Read_Float(key, "value", 1.f)
        );
    }
}

void EffectAssetRuntimeLoader::Fill_VelocityScaleByLifePayload(PointParticleMotionDesc& desc, const json& velocityOverLife)
{
    const auto scaleOverLifeIter = velocityOverLife.find("scaleOverLife");
    if (scaleOverLifeIter == velocityOverLife.end())
    {
        desc.velocityScaleByLife = Vec2{
            Read_Float(velocityOverLife, "scaleStart", desc.velocityScaleByLife.x),
            Read_Float(velocityOverLife, "scaleEnd", desc.velocityScaleByLife.y)
        };
        desc.velocityScaleByLifeCurveKeyCount = 2;
        desc.velocityScaleByLifeCurveTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
        desc.velocityScaleByLifeCurveTimesBlock1 = Vec4{};
        desc.velocityScaleByLifeCurveValues = Vec4{ desc.velocityScaleByLife.x, desc.velocityScaleByLife.y, 0.f, 0.f };
        desc.velocityScaleByLifeCurveValuesBlock1 = Vec4{};
        return;
    }

    const float startValue = Evaluate_FloatDistributionMin(*scaleOverLifeIter, 1.f);
    const float endValue = Evaluate_FloatDistributionMax(*scaleOverLifeIter, 1.f);
    desc.velocityScaleByLife = Vec2{ startValue, endValue };
    desc.velocityScaleByLifeCurveKeyCount = 2;
    desc.velocityScaleByLifeCurveTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
    desc.velocityScaleByLifeCurveTimesBlock1 = Vec4{};
    desc.velocityScaleByLifeCurveValues = Vec4{ startValue, endValue, 0.f, 0.f };
    desc.velocityScaleByLifeCurveValuesBlock1 = Vec4{};

    const json* payload = Find_DistributionPayload(*scaleOverLifeIter);
    if (payload == nullptr || "ConstantCurve" != Read_String(*scaleOverLifeIter, "mode"))
        return;

    const auto keysIter = payload->find("keys");
    if (keysIter == payload->end() || !keysIter->is_array() || keysIter->empty())
        return;

    vector<const json*> keys{};
    keys.reserve(keysIter->size());
    for (const json& key : *keysIter)
        keys.push_back(&key);
    ranges::sort(
        keys,
        [](const json* lhs, const json* rhs)
        {
            return Read_Float(*lhs, "time") < Read_Float(*rhs, "time");
        }
    );

    const uint32 keyCount = min(kMaxSubUVFrameCurveKeys, static_cast<uint32>(keys.size()));
    desc.velocityScaleByLifeCurveKeyCount = keyCount;
    desc.velocityScaleByLifeCurveTimes = Vec4{};
    desc.velocityScaleByLifeCurveTimesBlock1 = Vec4{};
    desc.velocityScaleByLifeCurveValues = Vec4{};
    desc.velocityScaleByLifeCurveValuesBlock1 = Vec4{};

    for (uint32 index = 0; index < keyCount; ++index)
    {
        const json& key = *keys[index];
        Set_CurveComponent(
            desc.velocityScaleByLifeCurveTimes,
            desc.velocityScaleByLifeCurveTimesBlock1,
            index,
            clamp(Read_Float(key, "time"), 0.f, 1.f)
        );
        Set_CurveComponent(
            desc.velocityScaleByLifeCurveValues,
            desc.velocityScaleByLifeCurveValuesBlock1,
            index,
            Read_Float(key, "value", 1.f)
        );
    }
}

void EffectAssetRuntimeLoader::Fill_VelocityConePayload(PointParticleMotionDesc& desc, const json& velocityCone)
{
    desc.velocityConeEnabled = true;
    desc.velocityConeInWorldSpace = Read_Bool(velocityCone, "inWorldSpace", desc.velocityConeInWorldSpace);
    desc.velocityConeAngleDegrees = clamp(Read_Float(velocityCone, "angleDegrees", desc.velocityConeAngleDegrees), 0.f, 180.f);

    const auto axisIter = velocityCone.find("axis");
    if (axisIter != velocityCone.end())
        Read_Vec3(*axisIter, desc.velocityConeAxis);

    const auto speedIter = velocityCone.find("speed");
    if (speedIter != velocityCone.end())
    {
        desc.velocityConeSpeed = Vec2{
            Evaluate_FloatDistributionMin(*speedIter, desc.velocityConeSpeed.x),
            Evaluate_FloatDistributionMax(*speedIter, desc.velocityConeSpeed.y)
        };
    }
}

void EffectAssetRuntimeLoader::Fill_AccelerationCurvePayload(PointParticleMotionDesc& desc, const json& distribution)
{
    desc.accelerationCurveEnabled = false;
    desc.accelerationCurveKeyCount = 2;
    desc.accelerationCurveTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
    desc.accelerationCurveTimesBlock1 = Vec4{};
    desc.accelerationCurveValuesX = Vec4{ desc.accelerationMin.x, desc.accelerationMax.x, 0.f, 0.f };
    desc.accelerationCurveValuesXBlock1 = Vec4{};
    desc.accelerationCurveValuesY = Vec4{ desc.accelerationMin.y, desc.accelerationMax.y, 0.f, 0.f };
    desc.accelerationCurveValuesYBlock1 = Vec4{};
    desc.accelerationCurveValuesZ = Vec4{ desc.accelerationMin.z, desc.accelerationMax.z, 0.f, 0.f };
    desc.accelerationCurveValuesZBlock1 = Vec4{};

    const json* payload = Find_DistributionPayload(distribution);
    if (payload == nullptr || "ConstantCurve" != Read_String(distribution, "mode"))
        return;

    const auto keysIter = payload->find("keys");
    if (keysIter == payload->end() || !keysIter->is_array() || keysIter->empty())
        return;

    vector<const json*> keys{};
    keys.reserve(keysIter->size());
    for (const json& key : *keysIter)
        keys.push_back(&key);
    ranges::sort(keys, [](const json* lhs, const json* rhs) { return Read_Float(*lhs, "time") < Read_Float(*rhs, "time"); });

    const uint32 keyCount = min(kMaxSubUVFrameCurveKeys, static_cast<uint32>(keys.size()));
    desc.accelerationCurveEnabled = true;
    desc.accelerationCurveKeyCount = keyCount;
    desc.accelerationCurveTimes = Vec4{};
    desc.accelerationCurveTimesBlock1 = Vec4{};
    desc.accelerationCurveValuesX = Vec4{};
    desc.accelerationCurveValuesXBlock1 = Vec4{};
    desc.accelerationCurveValuesY = Vec4{};
    desc.accelerationCurveValuesYBlock1 = Vec4{};
    desc.accelerationCurveValuesZ = Vec4{};
    desc.accelerationCurveValuesZBlock1 = Vec4{};

    for (uint32 index = 0; index < keyCount; ++index)
    {
        const json& key = *keys[index];
        Vec3 value{};
        if (const auto valueIter = key.find("value"); valueIter != key.end())
            Read_Vec3(*valueIter, value);

        Set_CurveComponent(desc.accelerationCurveTimes, desc.accelerationCurveTimesBlock1, index, clamp(Read_Float(key, "time"), 0.f, 1.f));
        Set_CurveComponent(desc.accelerationCurveValuesX, desc.accelerationCurveValuesXBlock1, index, value.x);
        Set_CurveComponent(desc.accelerationCurveValuesY, desc.accelerationCurveValuesYBlock1, index, value.y);
        Set_CurveComponent(desc.accelerationCurveValuesZ, desc.accelerationCurveValuesZBlock1, index, value.z);
    }
}

void EffectAssetRuntimeLoader::Fill_OrbitOverLifePayload(EffectOrbitOverLifeRuntimeDesc& desc, const json& orbitOverLife)
{
    desc.enabled = Read_Bool(orbitOverLife, "enabled", desc.enabled);
    Read_Enum(orbitOverLife, "plane", desc.plane);

    auto fill_float_curve = [](
        Vec2& range,
        uint32& keyCount,
        Vec4& times,
        Vec4& timesBlock1,
        Vec4& values,
        Vec4& valuesBlock1,
        const json& distribution,
        float fallbackValue)
    {
        const float startValue = Evaluate_FloatDistributionMin(distribution, fallbackValue);
        const float endValue = Evaluate_FloatDistributionMax(distribution, fallbackValue);
        range = Vec2{ startValue, endValue };
        keyCount = 2;
        times = Vec4{ 0.f, 1.f, 0.f, 0.f };
        timesBlock1 = Vec4{};
        values = Vec4{ startValue, endValue, 0.f, 0.f };
        valuesBlock1 = Vec4{};

        const json* payload = Find_DistributionPayload(distribution);
        if (payload == nullptr || "ConstantCurve" != Read_String(distribution, "mode"))
            return;

        const auto keysIter = payload->find("keys");
        if (keysIter == payload->end() || !keysIter->is_array() || keysIter->empty())
            return;

        vector<const json*> keys{};
        keys.reserve(keysIter->size());
        for (const json& key : *keysIter)
            keys.push_back(&key);
        ranges::sort(keys, [](const json* lhs, const json* rhs) { return Read_Float(*lhs, "time") < Read_Float(*rhs, "time"); });

        const uint32 clampedKeyCount = min(kMaxSubUVFrameCurveKeys, static_cast<uint32>(keys.size()));
        keyCount = clampedKeyCount;
        times = Vec4{};
        timesBlock1 = Vec4{};
        values = Vec4{};
        valuesBlock1 = Vec4{};
        for (uint32 index = 0; index < clampedKeyCount; ++index)
        {
            const json& key = *keys[index];
            Set_CurveComponent(times, timesBlock1, index, clamp(Read_Float(key, "time"), 0.f, 1.f));
            Set_CurveComponent(values, valuesBlock1, index, Read_Float(key, "value", fallbackValue));
        }
    };

    if (const auto angleIter = orbitOverLife.find("angleDegreesOverLife"); angleIter != orbitOverLife.end())
    {
        fill_float_curve(
            desc.angleDegreesOverLife,
            desc.angleCurveKeyCount,
            desc.angleCurveTimes,
            desc.angleCurveTimesBlock1,
            desc.angleCurveValues,
            desc.angleCurveValuesBlock1,
            *angleIter,
            0.f
        );
    }

    if (const auto radiusIter = orbitOverLife.find("radiusScaleOverLife"); radiusIter != orbitOverLife.end())
    {
        fill_float_curve(
            desc.radiusScaleOverLife,
            desc.radiusScaleCurveKeyCount,
            desc.radiusScaleCurveTimes,
            desc.radiusScaleCurveTimesBlock1,
            desc.radiusScaleCurveValues,
            desc.radiusScaleCurveValuesBlock1,
            *radiusIter,
            1.f
        );
    }
}

namespace
{
    Vec2 Read_LegacyFloatRange(const json& node, const Vec2& fallbackValue)
    {
        Vec2 range = fallbackValue;
        Read_Vec2(node, range);
        return Vec2{ min(range.x, range.y), max(range.x, range.y) };
    }
}

void EffectAssetRuntimeLoader::Fill_PlaneRadialLocationPayload(PointParticlePlaneRadialLocationDesc& desc, const json& planeRadialLocation)
{
    desc.enabled = Read_Bool(planeRadialLocation, "enabled", desc.enabled);
    Read_Enum(planeRadialLocation, "plane", desc.plane);
    Read_Enum(planeRadialLocation, "shape", desc.shape);
    Read_Enum(planeRadialLocation, "placementMode", desc.placementMode);

    if (const auto offsetIter = planeRadialLocation.find("offset"); offsetIter != planeRadialLocation.end())
        Read_Vec3(*offsetIter, desc.offset);

    auto read_range = [&](const char* distributionKey, const char* legacyRangeKey, const Vec2& fallbackValue)
    {
        if (const auto distributionIter = planeRadialLocation.find(distributionKey);
            distributionIter != planeRadialLocation.end() && distributionIter->is_object())
        {
            const float minValue = Evaluate_FloatDistributionMin(*distributionIter, fallbackValue.x);
            const float maxValue = Evaluate_FloatDistributionMax(*distributionIter, fallbackValue.y);
            return Vec2{ min(minValue, maxValue), max(minValue, maxValue) };
        }

        if (const auto legacyIter = planeRadialLocation.find(legacyRangeKey); legacyIter != planeRadialLocation.end())
            return Read_LegacyFloatRange(*legacyIter, fallbackValue);

        return fallbackValue;
    };

    desc.uRange = read_range("uDistribution", "rangeU", desc.uRange);
    if (const auto distributionIter = planeRadialLocation.find("uDistribution");
        distributionIter != planeRadialLocation.end() && distributionIter->is_object())
        desc.uSeed = Internal::Read_DistributionRandomSeedDesc(*distributionIter, &planeRadialLocation);
    desc.vRange = read_range("vDistribution", "rangeV", desc.vRange);
    if (const auto distributionIter = planeRadialLocation.find("vDistribution");
        distributionIter != planeRadialLocation.end() && distributionIter->is_object())
        desc.vSeed = Internal::Read_DistributionRandomSeedDesc(*distributionIter, &planeRadialLocation);

    const Vec2 radiusRange = read_range("radiusDistribution", "radiusRange", desc.radiusRange);
    desc.radiusRange = Vec2{ max(0.f, radiusRange.x), max(0.f, radiusRange.y) };
    if (const auto distributionIter = planeRadialLocation.find("radiusDistribution");
        distributionIter != planeRadialLocation.end() && distributionIter->is_object())
        desc.radiusSeed = Internal::Read_DistributionRandomSeedDesc(*distributionIter, &planeRadialLocation);
    desc.angleDegreesRange = read_range("angleDegreesDistribution", "angleDegreesRange", desc.angleDegreesRange);
    if (const auto distributionIter = planeRadialLocation.find("angleDegreesDistribution");
        distributionIter != planeRadialLocation.end() && distributionIter->is_object())
        desc.angleDegreesSeed = Internal::Read_DistributionRandomSeedDesc(*distributionIter, &planeRadialLocation);
    desc.thickness = max(0.f, Read_Float(planeRadialLocation, "thickness", desc.thickness));
}

void EffectAssetRuntimeLoader::Fill_SphereRadialOrientationPayload(
    PointParticleSphereRadialOrientationDesc& desc,
    const json& sphereRadialOrientation)
{
    desc.enabled = Read_Bool(sphereRadialOrientation, "enabled", desc.enabled);
    Read_Enum(sphereRadialOrientation, "orientationMode", desc.orientationMode);
    Read_Enum(sphereRadialOrientation, "meshForwardAxis", desc.meshForwardAxis);
    Read_Enum(sphereRadialOrientation, "meshUpAxis", desc.meshUpAxis);
    desc.tiltDegrees = Read_Float(sphereRadialOrientation, "tiltDegrees", desc.tiltDegrees);
    desc.rollOffsetDegrees = Read_Float(sphereRadialOrientation, "rollOffsetDegrees", desc.rollOffsetDegrees);
}

void EffectAssetRuntimeLoader::Fill_PlaneRadialOrientationPayload(
    PointParticlePlaneRadialOrientationDesc& desc,
    const json& planeRadialOrientation)
{
    desc.enabled = Read_Bool(planeRadialOrientation, "enabled", desc.enabled);
    Read_Enum(planeRadialOrientation, "targetKind", desc.targetKind);
    Read_Enum(planeRadialOrientation, "orientationMode", desc.orientationMode);
    Read_Enum(planeRadialOrientation, "meshForwardAxis", desc.meshForwardAxis);
    Read_Enum(planeRadialOrientation, "meshUpAxis", desc.meshUpAxis);
    desc.tiltDegrees = Read_Float(planeRadialOrientation, "tiltDegrees", desc.tiltDegrees);
    desc.rollOffsetDegrees = Read_Float(planeRadialOrientation, "rollOffsetDegrees", desc.rollOffsetDegrees);
}

void EffectAssetRuntimeLoader::Fill_MeshVector3CurvePayload(
    MeshVector3CurveRuntimeDesc& desc,
    const json& distribution,
    const Vec3& fallbackValue)
{
    desc.enabled = true;
    const Vec3 startValue = Evaluate_Vector3DistributionMin(distribution, fallbackValue);
    const Vec3 endValue = Evaluate_Vector3DistributionMax(distribution, fallbackValue);
    desc.start = startValue;
    desc.end = endValue;
    desc.curveKeyCount = 2;
    desc.curveKeyTimes = Vec4{ 0.f, 1.f, 0.f, 0.f };
    desc.curveKeyTimesBlock1 = Vec4{};
    desc.curveKeyValuesX = Vec4{ startValue.x, endValue.x, 0.f, 0.f };
    desc.curveKeyValuesXBlock1 = Vec4{};
    desc.curveKeyValuesY = Vec4{ startValue.y, endValue.y, 0.f, 0.f };
    desc.curveKeyValuesYBlock1 = Vec4{};
    desc.curveKeyValuesZ = Vec4{ startValue.z, endValue.z, 0.f, 0.f };
    desc.curveKeyValuesZBlock1 = Vec4{};

    const json* payload = Find_DistributionPayload(distribution);
    if (payload == nullptr || "ConstantCurve" != Read_String(distribution, "mode"))
        return;

    const auto keysIter = payload->find("keys");
    if (keysIter == payload->end() || !keysIter->is_array() || keysIter->empty())
        return;

    vector<const json*> keys{};
    keys.reserve(keysIter->size());
    for (const json& key : *keysIter)
        keys.push_back(&key);
    ranges::sort(keys, [](const json* lhs, const json* rhs) { return Read_Float(*lhs, "time") < Read_Float(*rhs, "time"); });

    const uint32 keyCount = min(kMaxSubUVFrameCurveKeys, static_cast<uint32>(keys.size()));
    desc.curveKeyCount = keyCount;
    desc.curveKeyTimes = Vec4{};
    desc.curveKeyTimesBlock1 = Vec4{};
    desc.curveKeyValuesX = Vec4{};
    desc.curveKeyValuesXBlock1 = Vec4{};
    desc.curveKeyValuesY = Vec4{};
    desc.curveKeyValuesYBlock1 = Vec4{};
    desc.curveKeyValuesZ = Vec4{};
    desc.curveKeyValuesZBlock1 = Vec4{};

    for (uint32 index = 0; index < keyCount; ++index)
    {
        const json& key = *keys[index];
        Vec3 value = fallbackValue;
        if (const auto valueIter = key.find("value"); valueIter != key.end())
            Read_Vec3(*valueIter, value);

        Set_CurveComponent(desc.curveKeyTimes, desc.curveKeyTimesBlock1, index, clamp(Read_Float(key, "time"), 0.f, 1.f));
        Set_CurveComponent(desc.curveKeyValuesX, desc.curveKeyValuesXBlock1, index, value.x);
        Set_CurveComponent(desc.curveKeyValuesY, desc.curveKeyValuesYBlock1, index, value.y);
        Set_CurveComponent(desc.curveKeyValuesZ, desc.curveKeyValuesZBlock1, index, value.z);
    }
}

string EffectAssetRuntimeLoader::Resolve_RendererType(const json& emitter)
{
    string rendererType = Read_String(emitter, "rendererType");
    if (!rendererType.empty())
        return rendererType;

    if (const json* required = Find_ModuleData(emitter, "Required"))
    {
        rendererType = Read_String(*required, "rendererType");
        if (!rendererType.empty())
            return rendererType;
    }

    const auto typeDataIter = emitter.find("typeData");
    if (typeDataIter != emitter.end() && typeDataIter->is_object())
        return Read_String(*typeDataIter, "kind");

    return {};
}

void EffectAssetRuntimeLoader::Apply_MaterialPayload(
    const json& material,
    EffectRequiredMaterialRuntimeDesc& materialDesc)
{
    const EffectMaterialInstanceData defaultMaterial{};

    Read_Enum(material, "materialFamily", materialDesc.materialFamily);
    materialDesc.mainTextureGuid = Read_String(material, "mainTextureGuid", materialDesc.mainTextureGuid);
    materialDesc.mainTexturePath = Read_String(material, "mainTexturePath", materialDesc.mainTexturePath);
    materialDesc.noiseTextureGuid = Read_String(material, "noiseTextureGuid", materialDesc.noiseTextureGuid);
    materialDesc.noiseTexturePath = Read_String(material, "noiseTexturePath", materialDesc.noiseTexturePath);
    materialDesc.maskTextureGuid = Read_String(material, "maskTextureGuid", materialDesc.maskTextureGuid);
    materialDesc.maskTexturePath = Read_String(material, "maskTexturePath", materialDesc.maskTexturePath);
    materialDesc.flowTextureGuid = Read_String(material, "flowTextureGuid", materialDesc.flowTextureGuid);
    materialDesc.flowTexturePath = Read_String(material, "flowTexturePath", materialDesc.flowTexturePath);

    Read_Enum(material, "blendMode", materialDesc.blendMode);

    const string opacitySource = Read_String(material, "opacitySource");
    if (!opacitySource.empty())
        materialDesc.opacitySource = opacitySource;

    const auto tintIter = material.find("tint");
    if (tintIter != material.end())
        Read_Vec4(*tintIter, materialDesc.tint);

    materialDesc.intensity = Read_Float(material, "intensity", materialDesc.intensity);
    materialDesc.opacityPower = Read_Float(material, "opacityPower", materialDesc.opacityPower);
    materialDesc.noiseStrength = Read_Float(material, "noiseStrength", materialDesc.noiseStrength);
    materialDesc.alphaCutoff = Read_Float(material, "alphaCutoff", materialDesc.alphaCutoff);
    materialDesc.alphaErosion = Read_Float(material, "alphaErosion", materialDesc.alphaErosion);
    materialDesc.noiseSource = Read_String(material, "noiseSource", materialDesc.noiseSource);
    materialDesc.maskSource = Read_String(material, "maskSource", materialDesc.maskSource);
    materialDesc.noiseInvert = Read_Bool(material, "noiseInvert", materialDesc.noiseInvert);
    materialDesc.maskInvert = Read_Bool(material, "maskInvert", materialDesc.maskInvert);

    if (const auto coreEmissiveIter = material.find("coreEmissive"); coreEmissiveIter != material.end() && coreEmissiveIter->is_object())
    {
        const json& coreEmissive = *coreEmissiveIter;
        materialDesc.coreEmissive.enabled = Read_Bool(coreEmissive, "enabled", materialDesc.coreEmissive.enabled);
        if (const auto coreColorIter = coreEmissive.find("coreColor"); coreColorIter != coreEmissive.end())
            Read_Color(*coreColorIter, materialDesc.coreEmissive.coreColor);
        materialDesc.coreEmissive.corePower = Read_Float(coreEmissive, "corePower", materialDesc.coreEmissive.corePower);
        materialDesc.coreEmissive.coreIntensity = Read_Float(coreEmissive, "coreIntensity", materialDesc.coreEmissive.coreIntensity);
        materialDesc.coreEmissive.outerPower = Read_Float(coreEmissive, "outerPower", materialDesc.coreEmissive.outerPower);
        materialDesc.coreEmissive.outerIntensity = Read_Float(coreEmissive, "outerIntensity", materialDesc.coreEmissive.outerIntensity);
    }

    if (const auto mainUVOffsetIter = material.find("mainUVOffset"); mainUVOffsetIter != material.end())
        Read_Vec2(*mainUVOffsetIter, materialDesc.mainUVOffset);

    if (const auto mainUVScaleIter = material.find("mainUVScale"); mainUVScaleIter != material.end())
        Read_Vec2(*mainUVScaleIter, materialDesc.mainUVScale);

    if (const auto mainUVScrollSpeedIter = material.find("mainUVScrollSpeed"); mainUVScrollSpeedIter != material.end())
        Read_Vec2(*mainUVScrollSpeedIter, materialDesc.mainUVScrollSpeed);

    if (const auto noiseUVOffsetIter = material.find("noiseUVOffset"); noiseUVOffsetIter != material.end())
        Read_Vec2(*noiseUVOffsetIter, materialDesc.noiseUVOffset);

    if (const auto noiseUVScaleIter = material.find("noiseUVScale"); noiseUVScaleIter != material.end())
        Read_Vec2(*noiseUVScaleIter, materialDesc.noiseUVScale);

    if (const auto noiseUVScrollSpeedIter = material.find("noiseUVScrollSpeed"); noiseUVScrollSpeedIter != material.end())
        Read_Vec2(*noiseUVScrollSpeedIter, materialDesc.noiseUVScrollSpeed);

    if (const auto maskUVOffsetIter = material.find("maskUVOffset"); maskUVOffsetIter != material.end())
        Read_Vec2(*maskUVOffsetIter, materialDesc.maskUVOffset);

    if (const auto maskUVScaleIter = material.find("maskUVScale"); maskUVScaleIter != material.end())
        Read_Vec2(*maskUVScaleIter, materialDesc.maskUVScale);

    if (const auto maskUVScrollSpeedIter = material.find("maskUVScrollSpeed"); maskUVScrollSpeedIter != material.end())
        Read_Vec2(*maskUVScrollSpeedIter, materialDesc.maskUVScrollSpeed);

    if (const auto flowUVScaleIter = material.find("flowUVScale"); flowUVScaleIter != material.end())
        Read_Vec2(*flowUVScaleIter, materialDesc.flowUVScale);

    if (const auto flowUVOffsetIter = material.find("flowUVOffset"); flowUVOffsetIter != material.end())
        Read_Vec2(*flowUVOffsetIter, materialDesc.flowUVOffset);

    if (const auto flowUVScrollSpeedIter = material.find("flowUVScrollSpeed"); flowUVScrollSpeedIter != material.end())
        Read_Vec2(*flowUVScrollSpeedIter, materialDesc.flowUVScrollSpeed);

    materialDesc.mainUVTilingMode = defaultMaterial.mainUVTilingMode;
    materialDesc.noiseUVTilingMode = defaultMaterial.noiseUVTilingMode;
    materialDesc.maskUVTilingMode = defaultMaterial.maskUVTilingMode;
    materialDesc.flowUVTilingMode = defaultMaterial.flowUVTilingMode;
    Read_Enum(material, "mainUVTilingMode", materialDesc.mainUVTilingMode);
    Read_Enum(material, "noiseUVTilingMode", materialDesc.noiseUVTilingMode);
    Read_Enum(material, "maskUVTilingMode", materialDesc.maskUVTilingMode);
    Read_Enum(material, "flowUVTilingMode", materialDesc.flowUVTilingMode);
    materialDesc.refractionIntensity = Read_Float(material, "refractionIntensity", materialDesc.refractionIntensity);
    materialDesc.refractionPresence = Read_Float(material, "refractionPresence", materialDesc.refractionPresence);
    Read_Enum(material, "distortionShapeMode", materialDesc.distortionShapeMode);
    materialDesc.distortionShapeRadius = Read_Float(material, "distortionShapeRadius", materialDesc.distortionShapeRadius);
    materialDesc.distortionShapeThickness = Read_Float(material, "distortionShapeThickness", materialDesc.distortionShapeThickness);
    materialDesc.distortionShapeSoftness = Read_Float(material, "distortionShapeSoftness", materialDesc.distortionShapeSoftness);
    materialDesc.subUVRows = max(1u, Read_UInt(material, "subUVRows", materialDesc.subUVRows));
    materialDesc.subUVCols = max(1u, Read_UInt(material, "subUVCols", materialDesc.subUVCols));
    materialDesc.twoSided = Read_Bool(material, "twoSided", materialDesc.twoSided);

    const auto additiveIter = material.find("additive");
    if (additiveIter != material.end() && additiveIter->is_object())
    {
        const json& additive = *additiveIter;
        Read_Enum(additive, "colorSource", materialDesc.additive.colorSource);
        Read_Enum(additive, "amountSource", materialDesc.additive.amountSource);
        Read_Enum(additive, "coveragePolicy", materialDesc.additive.coveragePolicy);
        materialDesc.additive.intensityScale =
            Read_Float(additive, "intensityScale", materialDesc.additive.intensityScale);
        materialDesc.additive.blackNeutral =
            Read_Bool(additive, "blackNeutral", materialDesc.additive.blackNeutral);

        const auto emissiveColorIter = additive.find("emissiveColor");
        if (emissiveColorIter != additive.end())
            Read_Color(*emissiveColorIter, materialDesc.additive.emissiveColor);
        const auto constantColorIter = additive.find("constantColor");
        if (constantColorIter != additive.end())
            Read_Color(*constantColorIter, materialDesc.additive.constantColor);
    }
}

void EffectAssetRuntimeLoader::Apply_RequiredModule(const json& data, EffectEmitterDefinition& definition, ComputeSpriteEmitterDesc& desc)
{
    const auto materialIter = data.find("material");
    if (materialIter != data.end() && materialIter->is_object())
        Apply_MaterialPayload(*materialIter, desc.required.material);

    Read_Enum(data, "screenAlignment", desc.required.spriteRender.screenAlignment);
    Read_Enum(data, "directionalAlignmentMode", desc.required.spriteRender.directionalAlignmentMode);
    Read_Enum(data, "spriteTextureAxis", desc.required.spriteRender.spriteTextureAxis);
    desc.required.spriteRender.spriteRollOffsetDegrees =
        Read_Float(data, "spriteRollOffsetDegrees", desc.required.spriteRender.spriteRollOffsetDegrees);
    Internal::Apply_SortPayload(data, desc.required.sort);
    desc.required.playback.duration = max(0.0001f, Read_Float(data, "duration", desc.required.playback.duration));
    desc.required.playback.loopCount = Read_UInt(data, "loopCount", desc.required.playback.loopCount);
    desc.required.playback.delay = max(0.f, Read_Float(data, "delay", desc.required.playback.delay));
    desc.required.playback.delayFirstLoopOnly = Read_Bool(data, "delayFirstLoopOnly", desc.required.playback.delayFirstLoopOnly);
    desc.required.playback.killOnDeactivate = Read_Bool(data, "killOnDeactivate", desc.required.playback.killOnDeactivate);
    desc.required.playback.killOnCompleted = Read_Bool(data, "killOnCompleted", desc.required.playback.killOnCompleted);
    desc.required.drawLimit.useMaxDrawCount = Read_Bool(data, "useMaxDrawCount", desc.required.drawLimit.useMaxDrawCount);
    desc.required.drawLimit.maxDrawCount = max(1u, Read_UInt(data, "maxDrawCount", desc.required.drawLimit.maxDrawCount));

    const auto originIter = data.find("emitterOrigin");
    if (originIter != data.end())
        Read_Vec3(*originIter, definition.localPosition);

    const auto rotationIter = data.find("emitterRotationDegrees");
    if (rotationIter != data.end())
        Read_Vec3(*rotationIter, definition.localRotationDegrees);

    definition.useLocalSpace = Read_Bool(data, "useLocalSpace", definition.useLocalSpace);
}

void EffectAssetRuntimeLoader::Apply_RequiredModule(const json& data, EffectEmitterDefinition& definition, ComputeTrailEmitterDesc& desc)
{
    const auto materialIter = data.find("material");
    if (materialIter != data.end() && materialIter->is_object())
        Apply_MaterialPayload(*materialIter, desc.material);
    Engine::EffectRuntime::Apply_TrailMaterialRuntimePolicy(desc.material);

    desc.playback.duration = max(0.0001f, Read_Float(data, "duration", desc.playback.duration));
    desc.playback.loopCount = Read_UInt(data, "loopCount", desc.playback.loopCount);
    desc.playback.delay = max(0.f, Read_Float(data, "delay", desc.playback.delay));
    desc.playback.delayFirstLoopOnly = Read_Bool(data, "delayFirstLoopOnly", desc.playback.delayFirstLoopOnly);
    desc.playback.killOnDeactivate = Read_Bool(data, "killOnDeactivate", desc.playback.killOnDeactivate);
    desc.playback.killOnCompleted = Read_Bool(data, "killOnCompleted", desc.playback.killOnCompleted);

    const auto originIter = data.find("emitterOrigin");
    if (originIter != data.end())
        Read_Vec3(*originIter, definition.localPosition);

    const auto rotationIter = data.find("emitterRotationDegrees");
    if (rotationIter != data.end())
        Read_Vec3(*rotationIter, definition.localRotationDegrees);

    definition.useLocalSpace = Read_Bool(data, "useLocalSpace", definition.useLocalSpace);
}

void EffectAssetRuntimeLoader::Apply_RequiredModule(
    const json& data,
    EffectEmitterDefinition& definition,
    ComputeSourceHistoryRibbonEmitterDesc& desc)
{
    const auto materialIter = data.find("material");
    if (materialIter != data.end() && materialIter->is_object())
        Apply_MaterialPayload(*materialIter, desc.material);

    Internal::Apply_SortPayload(data, desc.sort);
    desc.playback.duration = max(0.0001f, Read_Float(data, "duration", desc.playback.duration));
    desc.playback.loopCount = Read_UInt(data, "loopCount", desc.playback.loopCount);
    desc.playback.delay = max(0.f, Read_Float(data, "delay", desc.playback.delay));
    desc.playback.delayFirstLoopOnly = Read_Bool(data, "delayFirstLoopOnly", desc.playback.delayFirstLoopOnly);
    desc.playback.killOnDeactivate = Read_Bool(data, "killOnDeactivate", desc.playback.killOnDeactivate);
    desc.playback.killOnCompleted = Read_Bool(data, "killOnCompleted", desc.playback.killOnCompleted);

    const auto originIter = data.find("emitterOrigin");
    if (originIter != data.end())
        Read_Vec3(*originIter, definition.localPosition);

    const auto rotationIter = data.find("emitterRotationDegrees");
    if (rotationIter != data.end())
        Read_Vec3(*rotationIter, definition.localRotationDegrees);

    definition.useLocalSpace = Read_Bool(data, "useLocalSpace", definition.useLocalSpace);
}

void EffectAssetRuntimeLoader::Apply_RequiredModule(const json& data, EffectEmitterDefinition& definition, MeshEmitterDesc& desc)
{
    const auto materialIter = data.find("material");
    if (materialIter != data.end() && materialIter->is_object())
        Apply_MaterialPayload(*materialIter, desc.material);

    Read_Enum(data, "screenAlignment", desc.meshTransform.alignment);
    Internal::Apply_SortPayload(data, desc.sort);
    desc.playback.duration = max(0.0001f, Read_Float(data, "duration", desc.playback.duration));
    desc.playback.loopCount = Read_UInt(data, "loopCount", desc.playback.loopCount);
    desc.playback.delay = max(0.f, Read_Float(data, "delay", desc.playback.delay));
    desc.playback.delayFirstLoopOnly = Read_Bool(data, "delayFirstLoopOnly", desc.playback.delayFirstLoopOnly);
    desc.playback.killOnDeactivate = Read_Bool(data, "killOnDeactivate", desc.playback.killOnDeactivate);
    desc.playback.killOnCompleted = Read_Bool(data, "killOnCompleted", desc.playback.killOnCompleted);
    desc.drawLimit.useMaxDrawCount = Read_Bool(data, "useMaxDrawCount", desc.drawLimit.useMaxDrawCount);
    desc.drawLimit.maxDrawCount = max(1u, Read_UInt(data, "maxDrawCount", desc.drawLimit.maxDrawCount));

    const auto originIter = data.find("emitterOrigin");
    if (originIter != data.end())
        Read_Vec3(*originIter, definition.localPosition);

    const auto rotationIter = data.find("emitterRotationDegrees");
    if (rotationIter != data.end())
        Read_Vec3(*rotationIter, definition.localRotationDegrees);

    definition.useLocalSpace = Read_Bool(data, "useLocalSpace", definition.useLocalSpace);
}

void EffectAssetRuntimeLoader::Apply_SpawnModule(const json& data, ComputeSpriteEmitterDesc& desc)
{
    desc.spawn.instanceCount = max(1u, Read_UInt(data, "maxParticleCount", desc.spawn.instanceCount));
    desc.spawn.particleSpawn.processSpawnRate = Read_Bool(data, "processSpawnRate", desc.spawn.particleSpawn.processSpawnRate);
    desc.spawn.particleSpawn.processBurstList = Read_Bool(data, "processBurstList", desc.spawn.particleSpawn.processBurstList);
    desc.spawn.particleSpawn.maxActiveCount = desc.spawn.instanceCount;

    const auto spawnRateIter = data.find("spawnRate");
    if (spawnRateIter != data.end())
    {
        desc.spawn.particleSpawn.spawnRateRange = Vec2{
            max(0.f, Evaluate_FloatDistributionMin(*spawnRateIter, desc.spawn.particleSpawn.spawnRate)),
            max(0.f, Evaluate_FloatDistributionMax(*spawnRateIter, desc.spawn.particleSpawn.spawnRate))
        };
        desc.spawn.particleSpawn.spawnRate = desc.spawn.particleSpawn.spawnRateRange.y;
        desc.spawn.particleSpawn.spawnRateSeed = Internal::Read_DistributionRandomSeedDesc(*spawnRateIter, &data);
    }

    const auto spawnRateScaleIter = data.find("spawnRateScale");
    if (spawnRateScaleIter != data.end())
    {
        desc.spawn.particleSpawn.spawnRateScaleRange = Vec2{
            max(0.f, Evaluate_FloatDistributionMin(*spawnRateScaleIter, desc.spawn.particleSpawn.spawnRateScale)),
            max(0.f, Evaluate_FloatDistributionMax(*spawnRateScaleIter, desc.spawn.particleSpawn.spawnRateScale))
        };
        desc.spawn.particleSpawn.spawnRateScale = desc.spawn.particleSpawn.spawnRateScaleRange.y;
        desc.spawn.particleSpawn.spawnRateScaleSeed = Internal::Read_DistributionRandomSeedDesc(*spawnRateScaleIter, &data);
    }

    const auto burstScaleIter = data.find("burstScale");
    if (burstScaleIter != data.end())
    {
        desc.spawn.particleSpawn.burstScaleRange = Vec2{
            max(0.f, Evaluate_FloatDistributionMin(*burstScaleIter, desc.spawn.particleSpawn.burstScale)),
            max(0.f, Evaluate_FloatDistributionMax(*burstScaleIter, desc.spawn.particleSpawn.burstScale))
        };
        desc.spawn.particleSpawn.burstScale = desc.spawn.particleSpawn.burstScaleRange.y;
        desc.spawn.particleSpawn.burstScaleSeed = Internal::Read_DistributionRandomSeedDesc(*burstScaleIter, &data);
    }

    const auto burstListIter = data.find("burstList");
    if (burstListIter == data.end() || !burstListIter->is_array())
        return;

    desc.spawn.particleSpawn.bursts.clear();
    desc.spawn.particleSpawn.bursts.reserve(burstListIter->size());
    for (const json& burst : *burstListIter)
    {
        if (!burst.is_object())
            continue;

        desc.spawn.particleSpawn.bursts.push_back(
            PointParticleBurstDesc{
                .time = max(0.f, Read_Float(burst, "time")),
                .count = Read_UInt(burst, "count")
            }
        );
    }
}

void EffectAssetRuntimeLoader::Apply_SpawnModule(const json& data, MeshEmitterDesc& desc)
{
    desc.spawn.instanceCount = max(1u, Read_UInt(data, "maxParticleCount", desc.spawn.instanceCount));
    desc.spawn.particleSpawn.processSpawnRate = Read_Bool(data, "processSpawnRate", desc.spawn.particleSpawn.processSpawnRate);
    desc.spawn.particleSpawn.processBurstList = Read_Bool(data, "processBurstList", desc.spawn.particleSpawn.processBurstList);
    desc.spawn.particleSpawn.maxActiveCount = desc.spawn.instanceCount;

    const auto spawnRateIter = data.find("spawnRate");
    if (spawnRateIter != data.end())
    {
        desc.spawn.particleSpawn.spawnRateRange = Vec2{
            max(0.f, Evaluate_FloatDistributionMin(*spawnRateIter, desc.spawn.particleSpawn.spawnRate)),
            max(0.f, Evaluate_FloatDistributionMax(*spawnRateIter, desc.spawn.particleSpawn.spawnRate))
        };
        desc.spawn.particleSpawn.spawnRate = desc.spawn.particleSpawn.spawnRateRange.y;
        desc.spawn.particleSpawn.spawnRateSeed = Internal::Read_DistributionRandomSeedDesc(*spawnRateIter, &data);
    }

    const auto spawnRateScaleIter = data.find("spawnRateScale");
    if (spawnRateScaleIter != data.end())
    {
        desc.spawn.particleSpawn.spawnRateScaleRange = Vec2{
            max(0.f, Evaluate_FloatDistributionMin(*spawnRateScaleIter, desc.spawn.particleSpawn.spawnRateScale)),
            max(0.f, Evaluate_FloatDistributionMax(*spawnRateScaleIter, desc.spawn.particleSpawn.spawnRateScale))
        };
        desc.spawn.particleSpawn.spawnRateScale = desc.spawn.particleSpawn.spawnRateScaleRange.y;
        desc.spawn.particleSpawn.spawnRateScaleSeed = Internal::Read_DistributionRandomSeedDesc(*spawnRateScaleIter, &data);
    }

    const auto burstScaleIter = data.find("burstScale");
    if (burstScaleIter != data.end())
    {
        desc.spawn.particleSpawn.burstScaleRange = Vec2{
            max(0.f, Evaluate_FloatDistributionMin(*burstScaleIter, desc.spawn.particleSpawn.burstScale)),
            max(0.f, Evaluate_FloatDistributionMax(*burstScaleIter, desc.spawn.particleSpawn.burstScale))
        };
        desc.spawn.particleSpawn.burstScale = desc.spawn.particleSpawn.burstScaleRange.y;
        desc.spawn.particleSpawn.burstScaleSeed = Internal::Read_DistributionRandomSeedDesc(*burstScaleIter, &data);
    }

    const auto burstListIter = data.find("burstList");
    if (burstListIter == data.end() || !burstListIter->is_array())
        return;

    desc.spawn.particleSpawn.bursts.clear();
    desc.spawn.particleSpawn.bursts.reserve(burstListIter->size());
    for (const json& burst : *burstListIter)
    {
        if (!burst.is_object())
            continue;

        desc.spawn.particleSpawn.bursts.push_back(
            PointParticleBurstDesc{
                .time = max(0.f, Read_Float(burst, "time")),
                .count = Read_UInt(burst, "count")
            }
        );
    }
}

bool EffectAssetRuntimeLoader::Build_EmitterDefinition(const json& emitter, EffectEmitterDefinition& outDefinition)
{
    outDefinition = EffectEmitterDefinition{};
    outDefinition.id = Read_UInt(emitter, "id");
    outDefinition.name = Read_String(emitter, "name");
    outDefinition.kind = EffectEmitterKind::ComputeSprite;
    outDefinition.enabled = true;
    Read_Enum(emitter, "renderLayerOverride", outDefinition.renderLayerOverride);

    ComputeSpriteEmitterDesc desc{};
    desc.drawMode = PointParticleDrawMode::DrawIndexedInstancedIndirect;
    desc.renderLayerOverride = outDefinition.renderLayerOverride;

    if (const json* required = Find_ModuleData(emitter, "Required"))
        Apply_RequiredModule(*required, outDefinition, desc);

    if (const json* materialScalarModulation = Find_ModuleData(emitter, "MaterialScalarModulation"))
    {
        Fill_MaterialScalarModulationPayload(desc.required.material.scalarModulation, *materialScalarModulation);
        Fill_CoreColorRgbModulationPayload(desc.required.material.coreColorRgbModulation, *materialScalarModulation);
    }

    if (const json* spawn = Find_ModuleData(emitter, "Spawn"))
        Apply_SpawnModule(*spawn, desc);

    if (const json* initialSize = Find_ModuleData(emitter, "InitialSize"))
    {
        desc.initialSize.enabled = true;
        const auto sizeIter = initialSize->find("size");
        if (sizeIter != initialSize->end())
        {
            const Vec2 minSize = Evaluate_Vector2DistributionMin(*sizeIter, desc.initialSize.sizeMin);
            const Vec2 maxSize = Evaluate_Vector2DistributionMax(*sizeIter, desc.initialSize.sizeMax);
            desc.initialSize.sizeMin = Vec2{ min(minSize.x, maxSize.x), min(minSize.y, maxSize.y) };
            desc.initialSize.sizeMax = Vec2{ max(minSize.x, maxSize.x), max(minSize.y, maxSize.y) };
            desc.initialSize.sizeSeed = Internal::Read_DistributionRandomSeedDesc(*sizeIter, initialSize);
        }
    }

    if (const json* initialLocation = Find_ModuleData(emitter, "InitialLocation"))
    {
        const auto locationIter = initialLocation->find("location");
        if (locationIter != initialLocation->end())
        {
            const Vec3 minOffset = Evaluate_Vector3DistributionMin(*locationIter, desc.initialLocation.minOffset);
            const Vec3 maxOffset = Evaluate_Vector3DistributionMax(*locationIter, desc.initialLocation.maxOffset);
            desc.initialLocation.enabled = true;
            desc.initialLocation.minOffset = Vec3{
                min(minOffset.x, maxOffset.x),
                min(minOffset.y, maxOffset.y),
                min(minOffset.z, maxOffset.z)
            };
            desc.initialLocation.maxOffset = Vec3{
                max(minOffset.x, maxOffset.x),
                max(minOffset.y, maxOffset.y),
                max(minOffset.z, maxOffset.z)
            };
        }
        if (locationIter != initialLocation->end())
            desc.initialLocationSeed = Internal::Read_DistributionRandomSeedDesc(*locationIter, initialLocation);
    }

    if (const json* sphereLocation = Find_ModuleData(emitter, "SphereLocation"))
    {
        desc.sphereLocation.enabled = true;

        const auto offsetIter = sphereLocation->find("offset");
        if (offsetIter != sphereLocation->end())
            Read_Vec3(*offsetIter, desc.sphereLocation.offset);

        desc.sphereLocation.radius = max(0.f, Read_Float(*sphereLocation, "radius", desc.sphereLocation.radius));
        Read_Enum(*sphereLocation, "spawnMode", desc.sphereLocation.mode);
        Read_Enum(*sphereLocation, "placementMode", desc.sphereLocation.placementMode);
        desc.sphereLocationSeed = Internal::Read_RandomSeedDesc(*sphereLocation);
    }

    if (const json* planeRadialLocation = Find_ModuleData(emitter, "PlaneRadialLocation"))
        Fill_PlaneRadialLocationPayload(desc.planeRadialLocation, *planeRadialLocation);

    if (const json* lifetime = Find_ModuleData(emitter, "Lifetime"))
    {
        const auto lifeTimeIter = lifetime->find("lifeTime");
        if (lifeTimeIter != lifetime->end())
        {
            desc.lifetime.lifeTime = Vec2{
                Evaluate_FloatDistributionMin(*lifeTimeIter, desc.lifetime.lifeTime.x),
                Evaluate_FloatDistributionMax(*lifeTimeIter, desc.lifetime.lifeTime.y)
            };
            desc.lifetimeSeed = Internal::Read_DistributionRandomSeedDesc(*lifeTimeIter, lifetime);
        }
    }

    if (const json* initialColor = Find_ModuleData(emitter, "InitialColor"))
    {
        const auto colorIter = initialColor->find("color");
        if (colorIter != initialColor->end())
        {
            desc.initialColor.startColorMin = Evaluate_ColorDistributionMin(*colorIter, desc.initialColor.startColorMin);
            desc.initialColor.startColorMax = Evaluate_ColorDistributionMax(*colorIter, desc.initialColor.startColorMax);
            desc.initialColor.colorSeed = Internal::Read_DistributionRandomSeedDesc(*colorIter, initialColor);
        }

        const auto alphaIter = initialColor->find("alpha");
        if (alphaIter != initialColor->end())
        {
            desc.initialColor.startColorMin.w =
                clamp(Evaluate_FloatDistributionMin(*alphaIter, desc.initialColor.startColorMin.w), 0.f, 1.f);
            desc.initialColor.startColorMax.w =
                clamp(Evaluate_FloatDistributionMax(*alphaIter, desc.initialColor.startColorMax.w), 0.f, 1.f);
            desc.initialColor.alphaSeed = Internal::Read_DistributionRandomSeedDesc(*alphaIter, initialColor);
        }

        desc.colorOverLife.endColorMin = desc.initialColor.startColorMin;
        desc.colorOverLife.endColorMax = desc.initialColor.startColorMax;
    }

    if (const json* colorOverLife = Find_ModuleData(emitter, "ColorOverLife"))
    {
        const auto colorIter = colorOverLife->find("colorOverLife");
        if (colorIter != colorOverLife->end())
        {
            desc.colorOverLife.endColorMin = Evaluate_ColorOverLifeEndpointColorMin(*colorIter, desc.colorOverLife.endColorMin);
            desc.colorOverLife.endColorMax = Evaluate_ColorOverLifeEndpointColorMax(*colorIter, desc.colorOverLife.endColorMax);
            desc.colorOverLife.colorSeed = Internal::Read_DistributionRandomSeedDesc(*colorIter, colorOverLife);
        }

        const auto alphaIter = colorOverLife->find("alphaOverLife");
        if (alphaIter != colorOverLife->end())
        {
            desc.colorOverLife.endColorMin.w =
                clamp(Evaluate_ColorOverLifeEndpointAlphaMin(*alphaIter, desc.colorOverLife.endColorMin.w), 0.f, 1.f);
            desc.colorOverLife.endColorMax.w =
                clamp(Evaluate_ColorOverLifeEndpointAlphaMax(*alphaIter, desc.colorOverLife.endColorMax.w), 0.f, 1.f);
            desc.colorOverLife.alphaSeed = Internal::Read_DistributionRandomSeedDesc(*alphaIter, colorOverLife);
        }

        Fill_ColorOverLifeCurvePayload(desc.colorOverLife.curve, *colorOverLife);
    }

    if (const json* subUvFrameOverLife = Find_ModuleData(emitter, "SubUVFrameOverLife"))
    {
        desc.subUVFrameOverLife.enabled = true;
        desc.subUVFrameOverLife.startFrame = Read_UInt(*subUvFrameOverLife, "startFrame", desc.subUVFrameOverLife.startFrame);
        desc.subUVFrameOverLife.endFrame = Read_UInt(*subUvFrameOverLife, "endFrame", desc.subUVFrameOverLife.endFrame);
        desc.subUVFrameOverLife.loop = Read_Bool(*subUvFrameOverLife, "loop", desc.subUVFrameOverLife.loop);
        Read_Enum(*subUvFrameOverLife, "playbackMode", desc.subUVFrameOverLife.playbackMode);
        desc.subUVFrameOverLife.framesPerSecond =
            max(0.f, Read_Float(*subUvFrameOverLife, "framesPerSecond", desc.subUVFrameOverLife.framesPerSecond));

        const auto frameIndexIter = subUvFrameOverLife->find("frameIndex");
        if (frameIndexIter != subUvFrameOverLife->end())
            Fill_SubUVFrameCurvePayload(desc.subUVFrameOverLife, *frameIndexIter);
        if (desc.subUVFrameOverLife.playbackMode == SubUVFramePlaybackMode::RandomFrame)
            desc.subUVRandomFrameSeed = Internal::Read_RandomSeedDesc(*subUvFrameOverLife);

        const uint32 frameCount = max(1u, desc.required.material.subUVRows * desc.required.material.subUVCols);
        const uint32 lastFrame = frameCount - 1u;
        desc.subUVFrameOverLife.startFrame = min(desc.subUVFrameOverLife.startFrame, lastFrame);
        desc.subUVFrameOverLife.endFrame = min(desc.subUVFrameOverLife.endFrame, lastFrame);

        if (0u == Read_UInt(*subUvFrameOverLife, "startFrame") &&
            0u == Read_UInt(*subUvFrameOverLife, "endFrame") &&
            frameCount > 1u)
            desc.subUVFrameOverLife.endFrame = lastFrame;
    }

    if (const json* sizeByLife = Find_ModuleData(emitter, "SizeByLife"))
        Fill_SizeByLifePayload(desc.sizeByLife, *sizeByLife);

    if (const json* initialVelocity = Find_ModuleData(emitter, "InitialVelocity"))
    {
        desc.motion.enabled = true;
        desc.motion.initialVelocityEnabled = true;
        desc.motion.initialVelocityInWorldSpace = Read_Bool(*initialVelocity, "inWorldSpace", desc.motion.initialVelocityInWorldSpace);

        const auto velocityIter = initialVelocity->find("velocity");
        if (velocityIter != initialVelocity->end())
        {
            desc.motion.initialVelocityMin = Evaluate_Vector3DistributionMin(*velocityIter, desc.motion.initialVelocityMin);
            desc.motion.initialVelocityMax = Evaluate_Vector3DistributionMax(*velocityIter, desc.motion.initialVelocityMax);
        }
        if (velocityIter != initialVelocity->end())
            desc.motion.initialVelocitySeed = Internal::Read_DistributionRandomSeedDesc(*velocityIter, initialVelocity);
    }

    if (const json* initialRadialVelocity = Find_ModuleData(emitter, "InitialRadialVelocity"))
    {
        desc.motion.enabled = true;
        desc.motion.initialRadialVelocityEnabled = true;
        desc.motion.initialRadialVelocityInWorldSpace =
            Read_Bool(*initialRadialVelocity, "inWorldSpace", desc.motion.initialRadialVelocityInWorldSpace);

        const auto radialPivotIter = initialRadialVelocity->find("radialPivot");
        if (radialPivotIter != initialRadialVelocity->end())
            Read_Vec3(*radialPivotIter, desc.motion.radialPivot);
        Read_Enum(
            *initialRadialVelocity,
            "centerDirectionMode",
            desc.motion.initialRadialVelocityCenterDirectionMode
        );

        const auto speedIter = initialRadialVelocity->find("speed");
        if (speedIter != initialRadialVelocity->end())
        {
            desc.motion.radialSpeed = Vec2{
                Evaluate_FloatDistributionMin(*speedIter, desc.motion.radialSpeed.x),
                Evaluate_FloatDistributionMax(*speedIter, desc.motion.radialSpeed.y)
            };
        }
        if (speedIter != initialRadialVelocity->end())
            desc.motion.initialRadialVelocitySeed = Internal::Read_DistributionRandomSeedDesc(*speedIter, initialRadialVelocity);
    }

    if (const json* velocityCone = Find_ModuleData(emitter, "VelocityCone"))
    {
        desc.motion.enabled = true;
        Fill_VelocityConePayload(desc.motion, *velocityCone);
        if (const auto speedIter = velocityCone->find("speed"); speedIter != velocityCone->end())
            desc.motion.velocityConeSeed = Internal::Read_DistributionRandomSeedDesc(*speedIter, velocityCone);
    }

    if (const json* acceleration = Find_ModuleData(emitter, "Acceleration"))
    {
        desc.motion.enabled = true;
        const auto accelerationIter = acceleration->find("acceleration");
        if (accelerationIter != acceleration->end())
        {
            desc.motion.accelerationMin = Evaluate_Vector3DistributionMin(*accelerationIter, desc.motion.accelerationMin);
            desc.motion.accelerationMax = Evaluate_Vector3DistributionMax(*accelerationIter, desc.motion.accelerationMax);
            Fill_AccelerationCurvePayload(desc.motion, *accelerationIter);
        }
        if (accelerationIter != acceleration->end())
            desc.motion.accelerationSeed = Internal::Read_DistributionRandomSeedDesc(*accelerationIter, acceleration);
    }

    if (const json* drag = Find_ModuleData(emitter, "Drag"))
    {
        desc.motion.enabled = true;
        const auto dragIter = drag->find("drag");
        if (dragIter != drag->end())
        {
            desc.motion.drag = Vec2{
                max(0.f, Evaluate_FloatDistributionMin(*dragIter, desc.motion.drag.x)),
                max(0.f, Evaluate_FloatDistributionMax(*dragIter, desc.motion.drag.y))
            };
        }
        if (dragIter != drag->end())
            desc.motion.dragSeed = Internal::Read_DistributionRandomSeedDesc(*dragIter, drag);
    }

    if (const json* velocityOverLife = Find_ModuleData(emitter, "VelocityOverLife"))
    {
        desc.motion.enabled = true;
        Fill_VelocityScaleByLifePayload(desc.motion, *velocityOverLife);
    }

    if (const json* orbitOverLife = Find_ModuleData(emitter, "OrbitOverLife"))
        Fill_OrbitOverLifePayload(desc.orbitOverLife, *orbitOverLife);

    if (const json* initialRotation = Find_ModuleData(emitter, "InitialRotation"))
    {
        desc.rotation.enabled = true;
        const auto rotationIter = initialRotation->find("rotationDegrees");
        if (rotationIter != initialRotation->end())
        {
            desc.rotation.initialRotationDegrees = Vec2{
                Evaluate_FloatDistributionMin(*rotationIter, desc.rotation.initialRotationDegrees.x),
                Evaluate_FloatDistributionMax(*rotationIter, desc.rotation.initialRotationDegrees.y)
            };
        }
        if (rotationIter != initialRotation->end())
            desc.rotation.initialRotationSeed = Internal::Read_DistributionRandomSeedDesc(*rotationIter, initialRotation);
    }

    if (const json* planeRadialOrientation = Find_ModuleData(emitter, "PlaneRadialOrientation"))
        Fill_PlaneRadialOrientationPayload(desc.planeRadialOrientation, *planeRadialOrientation);

    if (const json* rotationOverLife = Find_ModuleData(emitter, "RotationOverLife"))
    {
        desc.rotation.enabled = true;
        Fill_RotationOverLifePayload(desc.rotation, *rotationOverLife);
    }

    if (const json* initialRotationRate = Find_ModuleData(emitter, "InitialRotationRate"))
    {
        desc.rotation.enabled = true;
        const auto rotationRateIter = initialRotationRate->find("rotationRateDegrees");
        if (rotationRateIter != initialRotationRate->end())
        {
            desc.rotation.initialRotationRateDegrees = Vec2{
                Evaluate_FloatDistributionMin(*rotationRateIter, desc.rotation.initialRotationRateDegrees.x),
                Evaluate_FloatDistributionMax(*rotationRateIter, desc.rotation.initialRotationRateDegrees.y)
            };
        }
        if (rotationRateIter != initialRotationRate->end())
            desc.rotation.initialRotationRateSeed = Internal::Read_DistributionRandomSeedDesc(*rotationRateIter, initialRotationRate);
    }

    if (const json* rotationRateScaleByLife = Find_ModuleData(emitter, "RotationRateScaleByLife"))
    {
        desc.rotation.enabled = true;
        Fill_RotationRateScaleByLifePayload(desc.rotation, *rotationRateScaleByLife);
    }

    outDefinition.concreteDesc = desc;
    return true;
}

bool EffectAssetRuntimeLoader::Build_TrailEmitterDefinition(const json& emitter, EffectEmitterDefinition& outDefinition)
{
    outDefinition = EffectEmitterDefinition{};
    outDefinition.id = Read_UInt(emitter, "id");
    outDefinition.name = Read_String(emitter, "name");
    outDefinition.kind = EffectEmitterKind::ComputeTrail;
    outDefinition.enabled = true;
    Read_Enum(emitter, "renderLayerOverride", outDefinition.renderLayerOverride);

    ComputeTrailEmitterDesc desc{};
    desc.renderLayerOverride = outDefinition.renderLayerOverride;

    const auto typeDataIter = emitter.find("typeData");
    if (typeDataIter != emitter.end() && typeDataIter->is_object())
    {
        const auto dataIter = typeDataIter->find("data");
        if (dataIter != typeDataIter->end() && dataIter->is_object())
        {
            const json& typeData = *dataIter;
            desc.width = max(0.001f, Read_Float(typeData, "width", desc.width));
            desc.segmentLifetime = max(0.001f, Read_Float(typeData, "segmentLifetime", desc.segmentLifetime));
            desc.historyCount = max(2u, Read_UInt(typeData, "historyCount", desc.historyCount));
            desc.uvTiling = max(0.001f, Read_Float(typeData, "uvTiling", desc.uvTiling));
            desc.maxTrailLength = max(0.f, Read_Float(typeData, "maxTrailLength", desc.maxTrailLength));
            desc.tailFadeLength = max(0.f, Read_Float(typeData, "tailFadeLength", desc.tailFadeLength));
            desc.sampleSpacing = max(0.001f, Read_Float(typeData, "sampleSpacing", desc.sampleSpacing));
            desc.curveSubdivision = max(0u, Read_UInt(typeData, "curveSubdivision", desc.curveSubdivision));
            desc.smoothTangent = Read_Bool(typeData, "smoothTangent", desc.smoothTangent);
            desc.sideFade = clamp(Read_Float(typeData, "sideFade", desc.sideFade), 0.001f, 0.49f);
        }
    }

    if (const json* required = Find_ModuleData(emitter, "Required"))
        Apply_RequiredModule(*required, outDefinition, desc);

    if (const json* materialScalarModulation = Find_ModuleData(emitter, "MaterialScalarModulation"))
    {
        Fill_MaterialScalarModulationPayload(desc.material.scalarModulation, *materialScalarModulation);
        Fill_CoreColorRgbModulationPayload(desc.material.coreColorRgbModulation, *materialScalarModulation);
    }

    if (const json* lifetime = Find_ModuleData(emitter, "Lifetime"))
    {
        const auto lifeTimeIter = lifetime->find("lifeTime");
        if (lifeTimeIter != lifetime->end())
        {
            desc.lifetime.lifeTime = Vec2{
                Evaluate_FloatDistributionMin(*lifeTimeIter, desc.lifetime.lifeTime.x),
                Evaluate_FloatDistributionMax(*lifeTimeIter, desc.lifetime.lifeTime.y)
            };
            desc.segmentLifetime = max(0.001f, desc.lifetime.lifeTime.y);
        }
    }

    if (const json* initialColor = Find_ModuleData(emitter, "InitialColor"))
    {
        const auto colorIter = initialColor->find("color");
        if (colorIter != initialColor->end())
        {
            desc.initialColor.startColorMin = Evaluate_ColorDistributionMin(*colorIter, desc.initialColor.startColorMin);
            desc.initialColor.startColorMax = Evaluate_ColorDistributionMax(*colorIter, desc.initialColor.startColorMax);
        }

        const auto alphaIter = initialColor->find("alpha");
        if (alphaIter != initialColor->end())
        {
            desc.initialColor.startColorMin.w =
                clamp(Evaluate_FloatDistributionMin(*alphaIter, desc.initialColor.startColorMin.w), 0.f, 1.f);
            desc.initialColor.startColorMax.w =
                clamp(Evaluate_FloatDistributionMax(*alphaIter, desc.initialColor.startColorMax.w), 0.f, 1.f);
        }

        desc.colorOverLife.endColorMin = desc.initialColor.startColorMin;
        desc.colorOverLife.endColorMax = desc.initialColor.startColorMax;
    }

    if (const json* colorOverLife = Find_ModuleData(emitter, "ColorOverLife"))
    {
        const auto colorIter = colorOverLife->find("colorOverLife");
        if (colorIter != colorOverLife->end())
        {
            desc.colorOverLife.endColorMin = Evaluate_ColorOverLifeEndpointColorMin(*colorIter, desc.colorOverLife.endColorMin);
            desc.colorOverLife.endColorMax = Evaluate_ColorOverLifeEndpointColorMax(*colorIter, desc.colorOverLife.endColorMax);
        }

        const auto alphaIter = colorOverLife->find("alphaOverLife");
        if (alphaIter != colorOverLife->end())
        {
            desc.colorOverLife.endColorMin.w =
                clamp(Evaluate_ColorOverLifeEndpointAlphaMin(*alphaIter, desc.colorOverLife.endColorMin.w), 0.f, 1.f);
            desc.colorOverLife.endColorMax.w =
                clamp(Evaluate_ColorOverLifeEndpointAlphaMax(*alphaIter, desc.colorOverLife.endColorMax.w), 0.f, 1.f);
        }

        Fill_ColorOverLifeCurvePayload(desc.colorOverLife.curve, *colorOverLife);
    }

    if (const json* sizeByLife = Find_ModuleData(emitter, "SizeByLife"))
        Fill_SizeByLifePayload(desc.sizeByLife, *sizeByLife);

    if (const json* spawnPerUnit = Find_ModuleData(emitter, "SpawnPerUnit"))
    {
        desc.spawnPerUnit.enabled = Read_Bool(*spawnPerUnit, "enabled", true);

        const auto spawnPerUnitIter = spawnPerUnit->find("spawnPerUnit");
        if (spawnPerUnitIter != spawnPerUnit->end())
        {
            desc.spawnPerUnit.spawnPerUnit =
                max(0.f, Evaluate_FloatDistributionMax(*spawnPerUnitIter, desc.spawnPerUnit.spawnPerUnit));
        }

        desc.spawnPerUnit.unitScalar = max(0.0001f, Read_Float(*spawnPerUnit, "unitScalar", desc.spawnPerUnit.unitScalar));
        desc.spawnPerUnit.movementTolerance =
            max(0.f, Read_Float(*spawnPerUnit, "movementTolerance", desc.spawnPerUnit.movementTolerance));
        desc.spawnPerUnit.maxFrameDistance =
            max(0.f, Read_Float(*spawnPerUnit, "maxFrameDistance", desc.spawnPerUnit.maxFrameDistance));
    }

    outDefinition.concreteDesc = desc;
    return true;
}

bool EffectAssetRuntimeLoader::Build_SourceHistoryRibbonEmitterDefinition(const json& emitter, EffectEmitterDefinition& outDefinition)
{
    outDefinition = EffectEmitterDefinition{};
    outDefinition.id = Read_UInt(emitter, "id");
    outDefinition.name = Read_String(emitter, "name");
    outDefinition.kind = EffectEmitterKind::SourceHistoryRibbon;
    outDefinition.enabled = true;
    Read_Enum(emitter, "renderLayerOverride", outDefinition.renderLayerOverride);

    ComputeSourceHistoryRibbonEmitterDesc desc{};
    desc.renderLayerOverride = outDefinition.renderLayerOverride;

    const auto typeDataIter = emitter.find("typeData");
    if (typeDataIter != emitter.end() && typeDataIter->is_object())
    {
        const auto dataIter = typeDataIter->find("data");
        if (dataIter != typeDataIter->end() && dataIter->is_object())
        {
            const json& typeData = *dataIter;
            desc.maxSampleCount = max(2u, Read_UInt(typeData, "maxSampleCount", desc.maxSampleCount));
            desc.sampleSpacing = max(0.001f, Read_Float(typeData, "sampleSpacing", desc.sampleSpacing));
            desc.sampleInterval = max(0.f, Read_Float(typeData, "sampleInterval", desc.sampleInterval));
            desc.maxLength = max(0.f, Read_Float(typeData, "maxLength", desc.maxLength));
            desc.tailFadeLength = max(0.f, Read_Float(typeData, "tailFadeLength", desc.tailFadeLength));
            desc.baseWidth = max(0.001f, Read_Float(typeData, "baseWidth", desc.baseWidth));
            desc.tilingDistance = max(0.f, Read_Float(typeData, "tilingDistance", desc.tilingDistance));
        }
    }

    if (const json* required = Find_ModuleData(emitter, "Required"))
        Apply_RequiredModule(*required, outDefinition, desc);

    if (const json* lifetime = Find_ModuleData(emitter, "Lifetime"))
    {
        const auto lifeTimeIter = lifetime->find("lifeTime");
        if (lifeTimeIter != lifetime->end())
        {
            desc.lifetime.lifeTime = Vec2{
                Evaluate_FloatDistributionMin(*lifeTimeIter, desc.lifetime.lifeTime.x),
                Evaluate_FloatDistributionMax(*lifeTimeIter, desc.lifetime.lifeTime.y)
            };
            desc.sampleLifetime = max(0.0001f, desc.lifetime.lifeTime.y);
        }
    }

    if (const json* initialColor = Find_ModuleData(emitter, "InitialColor"))
    {
        const auto colorIter = initialColor->find("color");
        if (colorIter != initialColor->end())
        {
            desc.initialColor.startColorMin = Evaluate_ColorDistributionMin(*colorIter, desc.initialColor.startColorMin);
            desc.initialColor.startColorMax = Evaluate_ColorDistributionMax(*colorIter, desc.initialColor.startColorMax);
            desc.initialColor.colorSeed = Internal::Read_DistributionRandomSeedDesc(*colorIter, initialColor);
        }

        const auto alphaIter = initialColor->find("alpha");
        if (alphaIter != initialColor->end())
        {
            desc.initialColor.startColorMin.w =
                clamp(Evaluate_FloatDistributionMin(*alphaIter, desc.initialColor.startColorMin.w), 0.f, 1.f);
            desc.initialColor.startColorMax.w =
                clamp(Evaluate_FloatDistributionMax(*alphaIter, desc.initialColor.startColorMax.w), 0.f, 1.f);
            desc.initialColor.alphaSeed = Internal::Read_DistributionRandomSeedDesc(*alphaIter, initialColor);
        }

        desc.colorOverLife.endColorMin = desc.initialColor.startColorMin;
        desc.colorOverLife.endColorMax = desc.initialColor.startColorMax;
    }

    if (const json* colorOverLife = Find_ModuleData(emitter, "ColorOverLife"))
    {
        const auto colorIter = colorOverLife->find("colorOverLife");
        if (colorIter != colorOverLife->end())
        {
            desc.colorOverLife.endColorMin = Evaluate_ColorOverLifeEndpointColorMin(*colorIter, desc.colorOverLife.endColorMin);
            desc.colorOverLife.endColorMax = Evaluate_ColorOverLifeEndpointColorMax(*colorIter, desc.colorOverLife.endColorMax);
            desc.colorOverLife.colorSeed = Internal::Read_DistributionRandomSeedDesc(*colorIter, colorOverLife);
        }

        const auto alphaIter = colorOverLife->find("alphaOverLife");
        if (alphaIter != colorOverLife->end())
        {
            desc.colorOverLife.endColorMin.w =
                clamp(Evaluate_ColorOverLifeEndpointAlphaMin(*alphaIter, desc.colorOverLife.endColorMin.w), 0.f, 1.f);
            desc.colorOverLife.endColorMax.w =
                clamp(Evaluate_ColorOverLifeEndpointAlphaMax(*alphaIter, desc.colorOverLife.endColorMax.w), 0.f, 1.f);
            desc.colorOverLife.alphaSeed = Internal::Read_DistributionRandomSeedDesc(*alphaIter, colorOverLife);
        }

        Fill_ColorOverLifeCurvePayload(desc.colorOverLife.curve, *colorOverLife);
    }

    if (const json* sizeByLife = Find_ModuleData(emitter, "SizeByLife"))
        Fill_SizeByLifePayload(desc.sizeByLife, *sizeByLife);

    outDefinition.concreteDesc = desc;
    return true;
}

bool EffectAssetRuntimeLoader::Build_MeshEmitterDefinition(const json& emitter, EffectEmitterDefinition& outDefinition)
{
    outDefinition = EffectEmitterDefinition{};
    outDefinition.id = Read_UInt(emitter, "id");
    outDefinition.name = Read_String(emitter, "name");
    outDefinition.kind = EffectEmitterKind::Mesh;
    outDefinition.enabled = true;
    Read_Enum(emitter, "renderLayerOverride", outDefinition.renderLayerOverride);

    MeshEmitterDesc desc{};
    desc.renderLayerOverride = outDefinition.renderLayerOverride;
    string assignedEffectMaterialGuid{};
    string assignedEffectMaterialPath{};
    bool hasAssignedMaterialInstance = false;
    json assignedMaterialPayload{};

    const auto typeDataIter = emitter.find("typeData");
    if (typeDataIter != emitter.end() && typeDataIter->is_object())
    {
        const auto dataIter = typeDataIter->find("data");
        if (dataIter != typeDataIter->end() && dataIter->is_object())
        {
            const json& typeData = *dataIter;
            desc.modelGuid = Read_String(typeData, "modelGuid", desc.modelGuid);
            desc.modelPath = Read_String(typeData, "modelPath", desc.modelPath);
            desc.useModelMaterials = Read_Bool(typeData, "useModelMaterials", desc.useModelMaterials);
            assignedEffectMaterialGuid = Read_String(typeData, "assignedEffectMaterialGuid", assignedEffectMaterialGuid);
            assignedEffectMaterialPath = Read_String(typeData, "assignedEffectMaterialPath", assignedEffectMaterialPath);
            hasAssignedMaterialInstance = Read_Bool(typeData, "hasAssignedMaterialInstance", hasAssignedMaterialInstance);
            if (const auto assignedMaterialIter = typeData.find("assignedMaterial");
                assignedMaterialIter != typeData.end() && assignedMaterialIter->is_object())
            {
                assignedMaterialPayload = *assignedMaterialIter;
                hasAssignedMaterialInstance = true;
            }

            const auto previewScaleIter = typeData.find("previewScale");
            if (previewScaleIter != typeData.end())
                Read_Vec3(*previewScaleIter, desc.previewScale);
        }
    }

    if (const json* required = Find_ModuleData(emitter, "Required"))
        Apply_RequiredModule(*required, outDefinition, desc);

    if (const json* materialScalarModulation = Find_ModuleData(emitter, "MaterialScalarModulation"))
    {
        Fill_MaterialScalarModulationPayload(desc.material.scalarModulation, *materialScalarModulation);
        Fill_CoreColorRgbModulationPayload(desc.material.coreColorRgbModulation, *materialScalarModulation);
    }

    if (hasAssignedMaterialInstance && assignedMaterialPayload.is_object())
        Apply_MaterialPayload(assignedMaterialPayload, desc.material);
    else
    {
        fs::path assignedMaterialResolvedPath{};
        if (!assignedEffectMaterialGuid.empty() && GAME != nullptr)
        {
            const AssetMeta* assetMeta = GAME->Find_AssetByGUID(assignedEffectMaterialGuid);
            if (assetMeta != nullptr && assetMeta->type == "EffectMaterial")
            {
                const wstring resolvedPath = GAME->Resolve_AssetPath(assignedEffectMaterialGuid);
                if (!resolvedPath.empty())
                    assignedMaterialResolvedPath = fs::path(resolvedPath).lexically_normal();
            }
        }

        if (assignedMaterialResolvedPath.empty() && !assignedEffectMaterialPath.empty())
        {
            assignedMaterialResolvedPath = fs::path(String::ToWString(assignedEffectMaterialPath));
            if (assignedMaterialResolvedPath.is_relative() && GAME != nullptr)
                assignedMaterialResolvedPath = fs::path(GAME->Get_AssetRoot()) / assignedMaterialResolvedPath;
            assignedMaterialResolvedPath = assignedMaterialResolvedPath.lexically_normal();
        }

        if (!assignedMaterialResolvedPath.empty() && fs::exists(assignedMaterialResolvedPath))
        {
            ifstream materialFile{ assignedMaterialResolvedPath };
            json materialRoot{};
            try
            {
                materialFile >> materialRoot;
                if (materialRoot.is_object())
                    Apply_MaterialPayload(materialRoot, desc.material);
            }
            catch (...)
            {
                LOG_WARN(
                    "EffectAssetRuntimeLoader failed to read assigned mesh effect material. emitter='{}', materialPath='{}'",
                    outDefinition.name,
                    Internal::To_LogPath(assignedMaterialResolvedPath)
                );
            }
        }
    }

    if (const json* required = Find_ModuleData(emitter, "Required");
        required != nullptr && required->is_object())
    {
        if (const auto materialIter = required->find("material");
            materialIter != required->end() && materialIter->is_object())
        {
            Read_Enum(*materialIter, "blendMode", desc.material.blendMode);
            desc.material.alphaCutoff = Read_Float(*materialIter, "alphaCutoff", desc.material.alphaCutoff);
        }
    }

    if (const json* spawn = Find_ModuleData(emitter, "Spawn"))
        Apply_SpawnModule(*spawn, desc);

    if (const json* initialMeshSize = Find_ModuleData(emitter, "InitialMeshSize"))
    {
        const auto sizeIter = initialMeshSize->find("size");
        if (sizeIter != initialMeshSize->end())
        {
            desc.meshTransform.initialScaleEnabled = true;
            const Vec3 minSize = Evaluate_Vector3DistributionMin(*sizeIter, desc.meshTransform.initialScaleMin);
            const Vec3 maxSize = Evaluate_Vector3DistributionMax(*sizeIter, desc.meshTransform.initialScaleMax);
            desc.meshTransform.initialScaleMin = Vec3{ min(minSize.x, maxSize.x), min(minSize.y, maxSize.y), min(minSize.z, maxSize.z) };
            desc.meshTransform.initialScaleMax = Vec3{ max(minSize.x, maxSize.x), max(minSize.y, maxSize.y), max(minSize.z, maxSize.z) };
        }
    }

    if (const json* initialLocation = Find_ModuleData(emitter, "InitialLocation"))
    {
        const auto locationIter = initialLocation->find("location");
        if (locationIter != initialLocation->end())
        {
            const Vec3 minOffset = Evaluate_Vector3DistributionMin(*locationIter, desc.initialLocation.minOffset);
            const Vec3 maxOffset = Evaluate_Vector3DistributionMax(*locationIter, desc.initialLocation.maxOffset);
            desc.initialLocation.enabled = true;
            desc.initialLocation.minOffset = Vec3{
                min(minOffset.x, maxOffset.x),
                min(minOffset.y, maxOffset.y),
                min(minOffset.z, maxOffset.z)
            };
            desc.initialLocation.maxOffset = Vec3{
                max(minOffset.x, maxOffset.x),
                max(minOffset.y, maxOffset.y),
                max(minOffset.z, maxOffset.z)
            };
        }
    }

    if (const json* sphereLocation = Find_ModuleData(emitter, "SphereLocation"))
    {
        desc.sphereLocation.enabled = true;

        const auto offsetIter = sphereLocation->find("offset");
        if (offsetIter != sphereLocation->end())
            Read_Vec3(*offsetIter, desc.sphereLocation.offset);

        desc.sphereLocation.radius = max(0.f, Read_Float(*sphereLocation, "radius", desc.sphereLocation.radius));
        Read_Enum(*sphereLocation, "spawnMode", desc.sphereLocation.mode);
        Read_Enum(*sphereLocation, "placementMode", desc.sphereLocation.placementMode);
        desc.sphereLocationSeed = Internal::Read_RandomSeedDesc(*sphereLocation);
    }

    if (const json* planeRadialLocation = Find_ModuleData(emitter, "PlaneRadialLocation"))
        Fill_PlaneRadialLocationPayload(desc.planeRadialLocation, *planeRadialLocation);

    if (const json* lifetime = Find_ModuleData(emitter, "Lifetime"))
    {
        const auto lifeTimeIter = lifetime->find("lifeTime");
        if (lifeTimeIter != lifetime->end())
        {
            desc.lifetime.lifeTime = Vec2{
                Evaluate_FloatDistributionMin(*lifeTimeIter, desc.lifetime.lifeTime.x),
                Evaluate_FloatDistributionMax(*lifeTimeIter, desc.lifetime.lifeTime.y)
            };
        }
    }

    if (const json* initialColor = Find_ModuleData(emitter, "InitialColor"))
    {
        const auto colorIter = initialColor->find("color");
        if (colorIter != initialColor->end())
        {
            desc.initialColor.startColorMin = Evaluate_ColorDistributionMin(*colorIter, desc.initialColor.startColorMin);
            desc.initialColor.startColorMax = Evaluate_ColorDistributionMax(*colorIter, desc.initialColor.startColorMax);
        }

        const auto alphaIter = initialColor->find("alpha");
        if (alphaIter != initialColor->end())
        {
            desc.initialColor.startColorMin.w =
                clamp(Evaluate_FloatDistributionMin(*alphaIter, desc.initialColor.startColorMin.w), 0.f, 1.f);
            desc.initialColor.startColorMax.w =
                clamp(Evaluate_FloatDistributionMax(*alphaIter, desc.initialColor.startColorMax.w), 0.f, 1.f);
        }

        desc.colorOverLife.endColorMin = desc.initialColor.startColorMin;
        desc.colorOverLife.endColorMax = desc.initialColor.startColorMax;
    }

    if (const json* colorOverLife = Find_ModuleData(emitter, "ColorOverLife"))
    {
        const auto colorIter = colorOverLife->find("colorOverLife");
        if (colorIter != colorOverLife->end())
        {
            desc.colorOverLife.endColorMin = Evaluate_ColorOverLifeEndpointColorMin(*colorIter, desc.colorOverLife.endColorMin);
            desc.colorOverLife.endColorMax = Evaluate_ColorOverLifeEndpointColorMax(*colorIter, desc.colorOverLife.endColorMax);
        }

        const auto alphaIter = colorOverLife->find("alphaOverLife");
        if (alphaIter != colorOverLife->end())
        {
            desc.colorOverLife.endColorMin.w =
                clamp(Evaluate_ColorOverLifeEndpointAlphaMin(*alphaIter, desc.colorOverLife.endColorMin.w), 0.f, 1.f);
            desc.colorOverLife.endColorMax.w =
                clamp(Evaluate_ColorOverLifeEndpointAlphaMax(*alphaIter, desc.colorOverLife.endColorMax.w), 0.f, 1.f);
        }

        Fill_ColorOverLifeCurvePayload(desc.colorOverLife.curve, *colorOverLife);
    }

    if (const json* meshSizeByLife = Find_ModuleData(emitter, "MeshSizeByLife"))
    {
        const auto scaleIter = meshSizeByLife->find("scaleOverLife");
        if (scaleIter != meshSizeByLife->end())
        {
            Fill_MeshVector3CurvePayload(desc.meshTransform.scaleByLife, *scaleIter, Vec3{ 1.f, 1.f, 1.f });
            if (!Read_Bool(*meshSizeByLife, "multiplyX", true))
            {
                desc.meshTransform.scaleByLife.curveKeyValuesX = Vec4{ 1.f, 1.f, 1.f, 1.f };
                desc.meshTransform.scaleByLife.curveKeyValuesXBlock1 = Vec4{ 1.f, 1.f, 1.f, 1.f };
            }
            if (!Read_Bool(*meshSizeByLife, "multiplyY", true))
            {
                desc.meshTransform.scaleByLife.curveKeyValuesY = Vec4{ 1.f, 1.f, 1.f, 1.f };
                desc.meshTransform.scaleByLife.curveKeyValuesYBlock1 = Vec4{ 1.f, 1.f, 1.f, 1.f };
            }
            if (!Read_Bool(*meshSizeByLife, "multiplyZ", true))
            {
                desc.meshTransform.scaleByLife.curveKeyValuesZ = Vec4{ 1.f, 1.f, 1.f, 1.f };
                desc.meshTransform.scaleByLife.curveKeyValuesZBlock1 = Vec4{ 1.f, 1.f, 1.f, 1.f };
            }
        }
    }

    if (const json* initialVelocity = Find_ModuleData(emitter, "InitialVelocity"))
    {
        desc.motion.enabled = true;
        desc.motion.initialVelocityEnabled = true;
        desc.motion.initialVelocityInWorldSpace = Read_Bool(*initialVelocity, "inWorldSpace", desc.motion.initialVelocityInWorldSpace);

        const auto velocityIter = initialVelocity->find("velocity");
        if (velocityIter != initialVelocity->end())
        {
            desc.motion.initialVelocityMin = Evaluate_Vector3DistributionMin(*velocityIter, desc.motion.initialVelocityMin);
            desc.motion.initialVelocityMax = Evaluate_Vector3DistributionMax(*velocityIter, desc.motion.initialVelocityMax);
        }
    }

    if (const json* initialRadialVelocity = Find_ModuleData(emitter, "InitialRadialVelocity"))
    {
        desc.motion.enabled = true;
        desc.motion.initialRadialVelocityEnabled = true;
        desc.motion.initialRadialVelocityInWorldSpace =
            Read_Bool(*initialRadialVelocity, "inWorldSpace", desc.motion.initialRadialVelocityInWorldSpace);

        const auto radialPivotIter = initialRadialVelocity->find("radialPivot");
        if (radialPivotIter != initialRadialVelocity->end())
            Read_Vec3(*radialPivotIter, desc.motion.radialPivot);
        Read_Enum(
            *initialRadialVelocity,
            "centerDirectionMode",
            desc.motion.initialRadialVelocityCenterDirectionMode
        );

        const auto speedIter = initialRadialVelocity->find("speed");
        if (speedIter != initialRadialVelocity->end())
        {
            desc.motion.radialSpeed = Vec2{
                Evaluate_FloatDistributionMin(*speedIter, desc.motion.radialSpeed.x),
                Evaluate_FloatDistributionMax(*speedIter, desc.motion.radialSpeed.y)
            };
        }
    }

    if (const json* velocityCone = Find_ModuleData(emitter, "VelocityCone"))
    {
        desc.motion.enabled = true;
        Fill_VelocityConePayload(desc.motion, *velocityCone);
    }

    if (const json* acceleration = Find_ModuleData(emitter, "Acceleration"))
    {
        desc.motion.enabled = true;
        const auto accelerationIter = acceleration->find("acceleration");
        if (accelerationIter != acceleration->end())
        {
            desc.motion.accelerationMin = Evaluate_Vector3DistributionMin(*accelerationIter, desc.motion.accelerationMin);
            desc.motion.accelerationMax = Evaluate_Vector3DistributionMax(*accelerationIter, desc.motion.accelerationMax);
            Fill_AccelerationCurvePayload(desc.motion, *accelerationIter);
        }
    }

    if (const json* drag = Find_ModuleData(emitter, "Drag"))
    {
        desc.motion.enabled = true;
        const auto dragIter = drag->find("drag");
        if (dragIter != drag->end())
        {
            desc.motion.drag = Vec2{
                max(0.f, Evaluate_FloatDistributionMin(*dragIter, desc.motion.drag.x)),
                max(0.f, Evaluate_FloatDistributionMax(*dragIter, desc.motion.drag.y))
            };
        }
    }

    if (const json* velocityOverLife = Find_ModuleData(emitter, "VelocityOverLife"))
    {
        desc.motion.enabled = true;
        Fill_VelocityScaleByLifePayload(desc.motion, *velocityOverLife);
    }

    if (const json* orbitOverLife = Find_ModuleData(emitter, "OrbitOverLife"))
        Fill_OrbitOverLifePayload(desc.orbitOverLife, *orbitOverLife);

    if (const json* initialMeshRotation = Find_ModuleData(emitter, "InitialMeshRotation"))
    {
        const auto rotationIter = initialMeshRotation->find("rotationDegrees");
        if (rotationIter != initialMeshRotation->end())
        {
            const Vec3 minRotation = Evaluate_Vector3DistributionMin(*rotationIter, Vec3{});
            const Vec3 maxRotation = Evaluate_Vector3DistributionMax(*rotationIter, Vec3{});
            desc.meshTransform.rotationEnabled = true;
            desc.meshTransform.initialRotationDegreesMin = Vec3{
                min(minRotation.x, maxRotation.x), min(minRotation.y, maxRotation.y), min(minRotation.z, maxRotation.z)
            };
            desc.meshTransform.initialRotationDegreesMax = Vec3{
                max(minRotation.x, maxRotation.x), max(minRotation.y, maxRotation.y), max(minRotation.z, maxRotation.z)
            };
        }
    }

    if (const json* sphereRadialOrientation = Find_ModuleData(emitter, "SphereRadialOrientation"))
        Fill_SphereRadialOrientationPayload(desc.sphereRadialOrientation, *sphereRadialOrientation);
    if (const json* planeRadialOrientation = Find_ModuleData(emitter, "PlaneRadialOrientation"))
        Fill_PlaneRadialOrientationPayload(desc.planeRadialOrientation, *planeRadialOrientation);

    if (const json* meshRotationOverLife = Find_ModuleData(emitter, "MeshRotationOverLife"))
    {
        const auto rotationIter = meshRotationOverLife->find("rotationOverLife");
        if (rotationIter != meshRotationOverLife->end())
        {
            desc.meshTransform.rotationEnabled = true;
            Fill_MeshVector3CurvePayload(desc.meshTransform.rotationByLife, *rotationIter, Vec3{});
        }
    }

    if (const json* initialMeshRotationRate = Find_ModuleData(emitter, "InitialMeshRotationRate"))
    {
        const auto rotationRateIter = initialMeshRotationRate->find("rotationRateDegrees");
        if (rotationRateIter != initialMeshRotationRate->end())
        {
            const Vec3 minRate = Evaluate_Vector3DistributionMin(*rotationRateIter, Vec3{});
            const Vec3 maxRate = Evaluate_Vector3DistributionMax(*rotationRateIter, Vec3{});
            desc.meshTransform.rotationEnabled = true;
            desc.meshTransform.initialAngularVelocityDegreesMin = Vec3{ min(minRate.x, maxRate.x), min(minRate.y, maxRate.y), min(minRate.z, maxRate.z) };
            desc.meshTransform.initialAngularVelocityDegreesMax = Vec3{ max(minRate.x, maxRate.x), max(minRate.y, maxRate.y), max(minRate.z, maxRate.z) };
        }
    }

    if (const json* meshRotationRateScaleByLife = Find_ModuleData(emitter, "MeshRotationRateScaleByLife"))
    {
        const auto scaleIter = meshRotationRateScaleByLife->find("scaleOverLife");
        if (scaleIter != meshRotationRateScaleByLife->end())
        {
            desc.meshTransform.rotationEnabled = true;
            Fill_MeshVector3CurvePayload(desc.meshTransform.angularVelocityScaleByLife, *scaleIter, Vec3{ 1.f, 1.f, 1.f });
        }
    }

    if (desc.modelGuid.empty() && desc.modelPath.empty())
    {
        LOG_WARN(
            "EffectAssetRuntimeLoader skipped mesh emitter with empty model. emitter='{}'",
            outDefinition.name
        );
        return false;
    }

    bool hasResolvableModel = false;
    if (!desc.modelGuid.empty() && GAME != nullptr)
    {
        const AssetMeta* assetMeta = GAME->Find_AssetByGUID(desc.modelGuid);
        const wstring resolvedPath = assetMeta != nullptr && assetMeta->type == "Model"
                                     ? GAME->Resolve_AssetPath(desc.modelGuid)
                                     : wstring{};
        hasResolvableModel = !resolvedPath.empty() && fs::exists(resolvedPath);
    }

    if (!hasResolvableModel && !desc.modelPath.empty())
    {
        fs::path modelPath = String::ToWString(desc.modelPath);
        if (modelPath.is_relative() && GAME != nullptr)
            modelPath = fs::path(GAME->Get_AssetRoot()) / modelPath;

        hasResolvableModel = fs::exists(modelPath.lexically_normal());
    }

    if (!hasResolvableModel)
    {
        LOG_WARN(
            "EffectAssetRuntimeLoader skipped mesh emitter with missing model. emitter='{}', modelGuid='{}', modelPath='{}'",
            outDefinition.name,
            desc.modelGuid,
            desc.modelPath
        );
        return false;
    }

    outDefinition.concreteDesc = desc;
    return true;
}

bool EffectAssetRuntimeLoader::Try_MakeResourceRelativePath(
    const fs::path& fullPath,
    const char* logContext,
    const string& assetKey,
    wstring& outRelativePath)
{
    outRelativePath.clear();

    const wstring assetRoot = GAME->Get_AssetRoot();
    if (assetRoot.empty())
    {
        LOG_ERROR(
            "EffectAssetRuntimeLoader {} failed: asset root is empty. key='{}'",
            logContext,
            assetKey
        );
        return false;
    }

    const fs::path assetRootPath = fs::path(assetRoot).lexically_normal();
    const fs::path normalizedFullPath = fullPath.lexically_normal();

    error_code errorCode{};
    const fs::path relativePath = fs::relative(normalizedFullPath, assetRootPath, errorCode).lexically_normal();
    if (errorCode || relativePath.empty() || relativePath.native().starts_with(L".."))
    {
        LOG_ERROR(
            "EffectAssetRuntimeLoader {} failed: resolved path is outside asset root. key='{}', path='{}', root='{}'",
            logContext,
            assetKey,
            To_LogPath(normalizedFullPath),
            To_LogPath(assetRootPath)
        );
        return false;
    }

    outRelativePath = relativePath.wstring();
    return true;
}

bool EffectAssetRuntimeLoader::Resolve_EffectAssetPathByGuid(const string& effectAssetGuid, wstring& outEffectAssetPath)
{
    outEffectAssetPath.clear();

    if (effectAssetGuid.empty())
    {
        LOG_ERROR("Empty guid");
        return false;
    }

    const AssetMeta* assetMeta = GAME->Find_AssetByGUID(effectAssetGuid);
    if (nullptr == assetMeta)
    {
        LOG_WARN("Asset not found. guid='{}'", effectAssetGuid);
        return false;
    }

    if (assetMeta->type != "Effect")
    {
        LOG_ERROR("Asset type mismatch. guid='{}', type='{}'", effectAssetGuid, assetMeta->type);
        return false;
    }

    const wstring resolvedPath = GAME->Resolve_AssetPath(effectAssetGuid);
    if (resolvedPath.empty())
    {
        LOG_WARN("Resolved path is empty. guid='{}'", effectAssetGuid);
        return false;
    }

    return Try_MakeResourceRelativePath(fs::path(resolvedPath), "guid resolve", effectAssetGuid, outEffectAssetPath);
}

string EffectAssetRuntimeLoader::Normalize_EffectNameKey(const wstring& effectName)
{
    const string effectJsonSuffix = ".effect.json";
    string key = String::ToLowerCopy(String::ToString(fs::path(effectName).filename().wstring()));
    if (key.ends_with(effectJsonSuffix))
        key.erase(key.size() - effectJsonSuffix.size());
    else
        key = fs::path(key).stem().generic_string();

    return key;
}

bool EffectAssetRuntimeLoader::Is_EffectDefinitionAssetPath(const wstring& relativePath)
{
    const string lowerPath = String::ToLowerCopy(fs::path(relativePath).generic_string());
    return lowerPath.starts_with("effects/assets/") && lowerPath.ends_with(".effect.json");
}

bool EffectAssetRuntimeLoader::Find_EffectAssetGuidByName(const wstring& effectName, string& outEffectAssetGuid)
{
    outEffectAssetGuid.clear();

    const string effectNameKey = Normalize_EffectNameKey(effectName);
    if (effectNameKey.empty())
    {
        LOG_ERROR("Empty effect name");
        return false;
    }

    AssetMeta matchedAsset{};
    bool hasMatch = false;
    const vector<AssetMeta> effectAssets = GAME->Get_AssetsByType("Effect");
    for (const AssetMeta& assetMeta : effectAssets)
    {
        if (!Is_EffectDefinitionAssetPath(assetMeta.relativePath))
            continue;

        const string candidateKey = Normalize_EffectNameKey(fs::path(assetMeta.relativePath).filename().wstring());
        if (candidateKey != effectNameKey)
            continue;

        if (hasMatch)
        {
            LOG_ERROR(
                "Duplicate effect asset name. name='{}', first='{}', duplicate='{}'",
                String::ToString(effectName),
                To_LogPath(matchedAsset.relativePath),
                To_LogPath(assetMeta.relativePath)
            );
            return false;
        }

        matchedAsset = assetMeta;
        hasMatch = true;
    }

    if (!hasMatch)
    {
        LOG_WARN("Effect asset name not found. name='{}'", String::ToString(effectName));
        return false;
    }

    if (matchedAsset.guid.empty())
    {
        LOG_ERROR(
            "Effect asset guid is empty. name='{}', path='{}'",
            String::ToString(effectName),
            To_LogPath(matchedAsset.relativePath)
        );
        return false;
    }

    outEffectAssetGuid = matchedAsset.guid;
    return true;
}

bool EffectAssetRuntimeLoader::Load_JsonRoot(const wstring& effectAssetPath, json& outRoot)
{
    if (effectAssetPath.empty())
    {
        LOG_ERROR("Empty asset path");
        return false;
    }

    const wstring assetRoot = GAME->Get_AssetRoot();
    if (assetRoot.empty())
    {
        LOG_ERROR("Asset root is empty");
        return false;
    }

    const fs::path fullPath = (fs::path(assetRoot) / effectAssetPath).lexically_normal();
    if (!fs::exists(fullPath))
    {
        LOG_WARN("File not found. path='{}'", To_LogPath(fullPath));
        return false;
    }

    try
    {
        ifstream file(fullPath);
        if (!file.is_open())
        {
            LOG_ERROR("Open failed. path='{}'", To_LogPath(fullPath));
            return false;
        }

        file >> outRoot;
        return true;
    }
    catch (const exception& exception)
    {
        LOG_ERROR(
            "Json parse failed. path='{}', error='{}'",
            To_LogPath(fullPath),
            exception.what()
        );
        return false;
    }
}

NS_END
