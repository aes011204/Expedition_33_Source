#pragma once

#include "Client_Defines.h"

NS_BEGIN(Client)

// 무기 프리펩을 읽어서 Player_BattleBase에 세팅할 수 있게
// 일단은, 아직 월드애들 말고, Battle 에게만 먼저 세팅될 수 있도록.
class WeaponPrefabLoader final
{
public:
    struct LoadedWeaponPrefab
    {
        json        rootJson  {};
        wstring     modelTag  {};
        string      modelGuid {};
        ModelType   modelType { ModelType::NonAnim };
    };

public:
    static fs::path Resolve_WeaponPrefabPath(const string& weaponPrefabName);
    static bool     Load(
                        const string& weaponPrefabName,
                        LoadedWeaponPrefab& outPrefab,
                        const string& expectedStaticClass = "Weapon");

private:
    static bool Try_ReadModelComponent(
                    const json& rootJson,
                    wstring& outModelTag,
                    string& outModelGuid,
                    ModelType& outModelType);
};

NS_END
