#include "WeaponPrefabLoader.h"
#include "GameInstance.h"
#include "Spawn_Prefab.h"

fs::path WeaponPrefabLoader::Resolve_WeaponPrefabPath(const string& weaponPrefabName)
{
    return fs::path(GAME->Get_AssetRoot()) /
        L"Data" / L"json" / L"Prefabs" / L"Weapons" /
        (String::ToWString(weaponPrefabName) + L".prefab.json");
}

bool WeaponPrefabLoader::Load(const string& weaponPrefabName, LoadedWeaponPrefab& outPrefab, const string& expectedStaticClass)
{
    outPrefab = {};

    if (weaponPrefabName.empty())
        return false;

    const fs::path prefabPath = Resolve_WeaponPrefabPath(weaponPrefabName);
    if (!fs::exists(prefabPath))
        return false;

    json rootJson{};
    if (!Spawn_Prefab::Load_PrefabJson(prefabPath, rootJson))
        return false;

    if (!rootJson.is_object())
        return false;

    const string staticClass = rootJson.value("static_class", string{});
    if (staticClass != expectedStaticClass)
        return false;

    wstring modelTag{};
    string modelGuid{};
    ModelType modelType = ModelType::NonAnim;

    if (!Try_ReadModelComponent(rootJson, modelTag, modelGuid, modelType))
        return false;

    outPrefab.rootJson = std::move(rootJson);
    outPrefab.modelTag = std::move(modelTag);
    outPrefab.modelGuid = std::move(modelGuid);
    outPrefab.modelType = modelType;
    return true;
}

bool WeaponPrefabLoader::Try_ReadModelComponent(const json& rootJson, wstring& outModelTag, string& outModelGuid,
    ModelType& outModelType)
{
    if (!rootJson.contains("components") || !rootJson["components"].is_array())
        return false;

    for (const json& componentJson : rootJson["components"])
    {
        if (!componentJson.is_object())
            continue;

        const string prototypeTag = componentJson.value("prototype_tag", string{});
        if (prototypeTag.empty())
            continue;

        if (!componentJson.contains("model_type"))
            continue;

        outModelTag = String::ToWString(prototypeTag);
        outModelGuid = componentJson.value("model_guid", string{});

        const string modelTypeString = componentJson.value("model_type", string{});
        outModelType = (modelTypeString == "Anim") ? ModelType::Anim : ModelType::NonAnim;
        return true;
    }

    return false;
}
