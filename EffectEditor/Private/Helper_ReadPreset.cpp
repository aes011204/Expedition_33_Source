#include "Helper_ReadPreset.h"

namespace EffectEditor::ReadPreset
{
bool Read(const fs::path& filePath, EffectMaterialInstanceData& outMaterial)
{
    outMaterial = EffectMaterialInstanceData{};

    if (!fs::exists(filePath))
        return false;

    ifstream file{ filePath };
    if (!file.is_open())
        return false;

    json root{};
    try
    {
        file >> root;
    }
    catch (...)
    {
        return false;
    }

    const auto read_string = [&](const char* key, const string& defaultValue) -> string
    {
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_string())
            return defaultValue;

        return iter->get<string>();
    };

    const auto read_float = [&](const char* key, float defaultValue) -> float
    {
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_number())
            return defaultValue;

        return iter->get<float>();
    };

    const auto read_uint = [&](const char* key, uint32 defaultValue) -> uint32
    {
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_number_unsigned())
            return defaultValue;

        return iter->get<uint32>();
    };

    const auto read_color = [&](const char* key, const Color& defaultValue) -> Color
    {
        const auto iter = root.find(key);
        if (iter == root.end())
            return defaultValue;

        if (iter->is_object())
        {
            return Color(
                iter->value("r", defaultValue.x),
                iter->value("g", defaultValue.y),
                iter->value("b", defaultValue.z),
                iter->value("a", defaultValue.w)
            );
        }

        if (iter->is_array() && iter->size() >= 4)
        {
            return Color(
                iter->at(0).is_number() ? iter->at(0).get<float>() : defaultValue.x,
                iter->at(1).is_number() ? iter->at(1).get<float>() : defaultValue.y,
                iter->at(2).is_number() ? iter->at(2).get<float>() : defaultValue.z,
                iter->at(3).is_number() ? iter->at(3).get<float>() : defaultValue.w
            );
        }

        return defaultValue;
    };

    const EffectMaterialInstanceData defaultMaterial{};
    EffectMaterialInstanceData material = defaultMaterial;

    material.sourcePresetName = ResolveDisplayName(filePath);
    material.mainTextureId = read_string("mainTextureId", defaultMaterial.mainTextureId);
    material.noiseTextureId = read_string("noiseTextureId", defaultMaterial.noiseTextureId);
    material.tint = read_color("tint", defaultMaterial.tint);
    material.intensity = read_float("intensity", defaultMaterial.intensity);
    material.opacityPower = read_float("opacityPower", defaultMaterial.opacityPower);
    material.noiseStrength = read_float("noiseStrength", defaultMaterial.noiseStrength);
    material.blendMode = read_string("blendMode", defaultMaterial.blendMode);
    material.opacitySource = read_string("opacitySource", defaultMaterial.opacitySource);
    material.subUVRows = max(1u, read_uint("subUVRows", defaultMaterial.subUVRows));
    material.subUVCols = max(1u, read_uint("subUVCols", defaultMaterial.subUVCols));

    outMaterial = material;
    return true;
}

string ResolveDisplayName(const fs::path& filePath)
{
    string fileName = String::ToString(filePath.filename().wstring());
    constexpr const char* suffix = ".effectmaterial.json";
    if (fileName.ends_with(suffix))
        fileName.erase(fileName.size() - strlen(suffix));

    return fileName;
}
}
