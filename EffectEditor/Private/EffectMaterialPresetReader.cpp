#include "EffectMaterialPresetReader.h"

namespace EffectEditor::EffectMaterialPresetReader
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

    const auto read_bool = [&](const char* key, bool defaultValue) -> bool
    {
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_boolean())
            return defaultValue;

        return iter->get<bool>();
    };

    const auto read_vec2 = [&](const char* key, const Vec2& defaultValue) -> Vec2
    {
        const auto iter = root.find(key);
        if (iter == root.end() || !iter->is_array() || iter->size() < 2)
            return defaultValue;

        return Vec2{
            iter->at(0).is_number() ? iter->at(0).get<float>() : defaultValue.x,
            iter->at(1).is_number() ? iter->at(1).get<float>() : defaultValue.y
        };
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

    const auto migrate_legacy_texture_id = [](const string& textureId, string& outGuid, string& outPath) -> bool
    {
        if (textureId == "Texture_Effect_DefaultTexture")
        {
            outGuid = "3c90794a-31bb-4e97-969a-3fd4497acc0f";
            outPath = "Effects/Textures/Shared/DefaultTexture.dds";
            return true;
        }

        if (textureId == "Texture_Effect_Clouds4")
        {
            outGuid = "321bf66c-e6f9-4495-9855-275274727e83";
            outPath = "Effects/Textures/Shared/clouds4_T.dds";
            return true;
        }

        return false;
    };

    const EffectMaterialInstanceData defaultMaterial{};
    EffectMaterialInstanceData material = defaultMaterial;

    material.sourcePresetName = ResolveDisplayName(filePath);
    material.mainTextureGuid = read_string("mainTextureGuid", defaultMaterial.mainTextureGuid);
    material.mainTexturePath = read_string("mainTexturePath", defaultMaterial.mainTexturePath);
    material.noiseTextureGuid = read_string("noiseTextureGuid", defaultMaterial.noiseTextureGuid);
    material.noiseTexturePath = read_string("noiseTexturePath", defaultMaterial.noiseTexturePath);
    material.maskTextureGuid = read_string("maskTextureGuid", defaultMaterial.maskTextureGuid);
    material.maskTexturePath = read_string("maskTexturePath", defaultMaterial.maskTexturePath);
    if (material.mainTextureGuid.empty() && material.mainTexturePath.empty())
    {
        const string legacyMainTextureId = read_string("mainTextureId", {});
        migrate_legacy_texture_id(legacyMainTextureId, material.mainTextureGuid, material.mainTexturePath);
    }
    if (material.noiseTextureGuid.empty() && material.noiseTexturePath.empty())
    {
        const string legacyNoiseTextureId = read_string("noiseTextureId", {});
        migrate_legacy_texture_id(legacyNoiseTextureId, material.noiseTextureGuid, material.noiseTexturePath);
    }
    material.tint = read_color("tint", defaultMaterial.tint);
    material.intensity = read_float("intensity", defaultMaterial.intensity);
    material.opacityPower = read_float("opacityPower", defaultMaterial.opacityPower);
    material.noiseStrength = read_float("noiseStrength", defaultMaterial.noiseStrength);
    material.alphaCutoff = read_float("alphaCutoff", defaultMaterial.alphaCutoff);
    material.alphaErosion = read_float("alphaErosion", defaultMaterial.alphaErosion);
    material.mainUvScale = read_vec2("mainUvScale", defaultMaterial.mainUvScale);
    material.mainUvScrollSpeed = read_vec2("mainUvScrollSpeed", defaultMaterial.mainUvScrollSpeed);
    material.noiseUvScale = read_vec2("noiseUvScale", defaultMaterial.noiseUvScale);
    material.noiseUvScrollSpeed = read_vec2("noiseUvScrollSpeed", defaultMaterial.noiseUvScrollSpeed);
    material.maskUvScale = read_vec2("maskUvScale", defaultMaterial.maskUvScale);
    material.maskUvScrollSpeed = read_vec2("maskUvScrollSpeed", defaultMaterial.maskUvScrollSpeed);
    material.blendMode = read_string("blendMode", defaultMaterial.blendMode);
    material.opacitySource = read_string("opacitySource", defaultMaterial.opacitySource);
    material.twoSided = read_bool("twoSided", defaultMaterial.twoSided);
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

