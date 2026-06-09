#pragma once

#include "Base.h"
#include "Client_Defines.h"

NS_BEGIN(Engine)
class GameObject;
NS_END

NS_BEGIN(Client)

class Spawn_Prefab final
{
public:
    struct FSpawnDesc
    {
        string  prefabName;
        uint32  levelIndex = ETOI(LevelType::GamePlay);
        wstring layerTag = TEXT("Layer_GameObject");
        bool    hasExplicitLayerTag = false;

        json    overrides{};
    };

    class Builder
    {
    public:
        explicit Builder(const string& prefabName);

    public:
        Builder& AtLevel(uint32 levelIndex);
        Builder& InLayer(const wstring& layerTag);

        Builder& Position(const Vec3& position);
        Builder& Rotation(const Vec3& eulerDegree);
        Builder& Scale(const Vec3& scale);

        Shared<GameObject> Spawn() const;

    private:
        FSpawnDesc _spawnDesc{};
    };

public:
    static Builder Prefab(const string& prefabName) { return Builder(prefabName); }

    static Shared<GameObject> Spawn(
        const string& prefabName,
        uint32 levelIndex,
        const wstring& layerTag);

    static Shared<GameObject> SpawnAt(
        const string& prefabName,
        uint32 levelIndex,
        const wstring& layerTag,
        const Vec3& position);

    static Shared<GameObject> SpawnForEditorPlacement(
        const string& prefabName,
        uint32 levelIndex,
        const Vec3& position);

    static fs::path Resolve_PrefabPath(const string& prefabName);

private:
    static void Apply_Overrides(const Shared<GameObject>& gameObject, const json& overrides);
    static wstring Resolve_DefaultLayerTagFromPrefabFile(const fs::path& prefabPath);
    static wstring Resolve_DefaultLayerTag(const Shared<GameObject>& gameObject);
    static Shared<GameObject> Spawn_Implementation(const FSpawnDesc& desc);
};

NS_END
