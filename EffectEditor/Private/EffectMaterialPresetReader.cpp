#include "EffectMaterialPresetReader.h"

#include "GameInstance.h"

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

    const auto resolve_texture_path_by_path = [](const string& texturePath) -> wstring
    {
        if (texturePath.empty())
            return {};

        fs::path candidatePath = String::ToWString(texturePath);
        if (candidatePath.is_relative() && GAME != nullptr)
            candidatePath = fs::path(GAME->Get_AssetRoot()) / candidatePath;

        candidatePath = candidatePath.lexically_normal();
        return fs::exists(candidatePath) ? candidatePath.wstring() : wstring{};
    };

    const auto migrate_legacy_texture_id = [](const string& textureId, string& outGuid, string& outPath) -> bool
    {
        if (textureId == "Texture_Effect_DefaultTexture")
        {
            outGuid.clear();
            outPath = "Effects/Textures/Shared/DefaultTexture.dds";
            return true;
        }

        if (textureId == "Texture_Effect_Clouds4")
        {
            outGuid.clear();
            outPath = "Effects/Textures/Shared/clouds4_T.dds";
            return true;
        }

        return false;
    };

    const auto fill_texture_guid_from_path = [&](string& textureGuid, const string& texturePath)
    {
        if (!textureGuid.empty() || GAME == nullptr)
            return;

        const wstring resolvedPath = resolve_texture_path_by_path(texturePath);
        if (resolvedPath.empty())
            return;

        textureGuid = GAME->Ensure_AssetGUID(resolvedPath, "Texture");
    };

    const auto fill_texture_path_hint_from_guid = [](const string& textureGuid, string& texturePath)
    {
        if (!texturePath.empty() || GAME == nullptr || textureGuid.empty())
            return;

        const AssetMeta* assetMeta = GAME->Find_AssetByGUID(textureGuid);
        if (assetMeta == nullptr || assetMeta->type != "Texture" || assetMeta->relativePath.empty())
            return;

        texturePath = String::ToString(assetMeta->relativePath);
        ranges::replace(texturePath, '\\', '/');
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
    fill_texture_guid_from_path(material.mainTextureGuid, material.mainTexturePath);
    fill_texture_guid_from_path(material.noiseTextureGuid, material.noiseTexturePath);
    fill_texture_guid_from_path(material.maskTextureGuid, material.maskTexturePath);
    fill_texture_path_hint_from_guid(material.mainTextureGuid, material.mainTexturePath);
    fill_texture_path_hint_from_guid(material.noiseTextureGuid, material.noiseTexturePath);
    fill_texture_path_hint_from_guid(material.maskTextureGuid, material.maskTexturePath);
    material.tint = read_color("tint", defaultMaterial.tint);
    material.intensity = read_float("intensity", defaultMaterial.intensity);
    material.opacityPower = read_float("opacityPower", defaultMaterial.opacityPower);
    material.noiseStrength = read_float("noiseStrength", defaultMaterial.noiseStrength);
    material.alphaCutoff = read_float("alphaCutoff", defaultMaterial.alphaCutoff);
    material.alphaErosion = read_float("alphaErosion", defaultMaterial.alphaErosion);
    material.mainUVScale = read_vec2("mainUVScale", defaultMaterial.mainUVScale);
    material.mainUVScrollSpeed = read_vec2("mainUVScrollSpeed", defaultMaterial.mainUVScrollSpeed);
    material.noiseUVScale = read_vec2("noiseUVScale", defaultMaterial.noiseUVScale);
    material.noiseUVScrollSpeed = read_vec2("noiseUVScrollSpeed", defaultMaterial.noiseUVScrollSpeed);
    material.maskUVScale = read_vec2("maskUVScale", defaultMaterial.maskUVScale);
    material.maskUVScrollSpeed = read_vec2("maskUVScrollSpeed", defaultMaterial.maskUVScrollSpeed);
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

