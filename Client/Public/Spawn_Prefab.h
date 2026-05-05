#pragma once

#include "Base.h"

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

private:
    static fs::path Resolve_PrefabPath(const string& prefabName);
    static void Apply_Overrides(const Shared<GameObject>& gameObject, const json& overrides);
    static Shared<GameObject> Spawn_Implementation(const FSpawnDesc& desc);
};

NS_END
