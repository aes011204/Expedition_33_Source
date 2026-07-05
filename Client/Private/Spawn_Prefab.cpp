#include "Spawn_Prefab.h"

#include "GameInstance.h"
#include "GameObject.h"
#include "Helper_String.h"
#include "Player_WorldBase.h"
#include "Player_BattleBase.h"
#include "SL_Base.h"
#include "SL_Maelle.h"
#include "Body_Player.h"
#include "ModelCom.h"
#include "Monster_Part.h"
#include "Monster_WorldBase.h"
#include "Monster_BattleBase.h"
#include "TransformCom.h"
#include "Client_Constants.h"
#include <mutex>
#include <system_error>
#include <unordered_map>

namespace fs = std::filesystem;

namespace
{
mutex g_prefabJsonCacheMutex{};

struct PrefabJsonCacheEntry
{
    json root{};
    fs::file_time_type lastWriteTime{};
};

unordered_map<wstring, PrefabJsonCacheEntry> g_prefabJsonCache{};

wstring Make_PrefabJsonCacheKey(const fs::path& prefabPath)
{
    return prefabPath.lexically_normal().wstring();
}

void Refresh_PlayerBodyAnimMapping(const Shared<GameObject>& gameObject)
{
    if (auto worldPlayer = dynamic_pointer_cast<Player_WorldBase>(gameObject))
    {
        if (auto* body = dynamic_cast<Body_Player*>(worldPlayer->Get_PartObject(L"Part_Body")))
        {
            if (Shared<ModelCom> model = body->Get_Model())
                model->Refresh_AnimStateMappingFromOwnerContext();
        }
        return;
    }

    if (auto battlePlayer = dynamic_pointer_cast<Player_BattleBase>(gameObject))
    {
        if (auto* body = dynamic_cast<Body_Player*>(battlePlayer->Get_PartObject(L"Part_Body")))
        {
            if (Shared<ModelCom> model = body->Get_Model())
                model->Refresh_AnimStateMappingFromOwnerContext();
        }
        return;
    }

    if (auto soulLikePlayer = dynamic_pointer_cast<SL_Maelle>(gameObject))
    {
        if (auto* body = dynamic_cast<Body_Player*>(soulLikePlayer->Get_PartObject(L"Part_Body")))
        {
            if (Shared<ModelCom> model = body->Get_Model())
                model->Refresh_AnimStateMappingFromOwnerContext(); // Spawn_Prefab에서 SL_Maelle source_prefab 세팅 후 SL 전용 animmap을 다시 잡는다.
        }
    }
}

void Refresh_MonsterBodyAnimMapping(const Shared<GameObject>& gameObject)
{
    if (auto worldMonster = dynamic_pointer_cast<Monster_WorldBase>(gameObject))
    {
        if (auto* body = dynamic_cast<Monster_Part*>(worldMonster->Get_PartObject(L"Part_Body")))
        {
            if (Shared<ModelCom> model = body->Get_Model())
                model->Refresh_AnimStateMappingFromOwnerContext();
        }
        return;
    }

    if (auto battleMonster = dynamic_pointer_cast<Monster_BattleBase>(gameObject))
    {
        if (auto* body = dynamic_cast<Monster_Part*>(battleMonster->Get_PartObject(L"Part_Body")))
        {
            if (Shared<ModelCom> model = body->Get_Model())
                model->Refresh_AnimStateMappingFromOwnerContext();
        }
        return;
    }

    if (auto soulLikeActor = dynamic_pointer_cast<SL_Base>(gameObject))
    {
        if (auto* body = dynamic_cast<Monster_Part*>(soulLikeActor->Get_PartObject(L"Part_Body")))
        {
            if (Shared<ModelCom> model = body->Get_Model())
                model->Refresh_AnimStateMappingFromOwnerContext();
        }
    }
}

string Resolve_CanonicalPrefabName(const string& requestedPrefabName, const fs::path& prefabPath)
{
    if (requestedPrefabName.find('/') != string::npos || requestedPrefabName.find('\\') != string::npos)
        return requestedPrefabName; // ETC/EyeTarget처럼 하위 폴더 prefab은 저장/로드 이름에도 폴더를 보존한다.

    string stem = String::ToString(prefabPath.stem().wstring());
    constexpr string_view prefabSuffix = ".prefab";
    if (stem.ends_with(prefabSuffix))
        stem.erase(stem.size() - prefabSuffix.size());

    return stem.empty() ? requestedPrefabName : stem;
}

}

Spawn_Prefab::Builder::Builder(const string& prefabName)
{
    _spawnDesc.prefabName = prefabName;
}

Spawn_Prefab::Builder& Spawn_Prefab::Builder::AtLevel(uint32 levelIndex)
{
    _spawnDesc.levelIndex = levelIndex;
    return *this;
}

Spawn_Prefab::Builder& Spawn_Prefab::Builder::InLayer(const wstring& layerTag)
{
    _spawnDesc.layerTag = layerTag;
    _spawnDesc.hasExplicitLayerTag = true;
    return *this;
}

Spawn_Prefab::Builder& Spawn_Prefab::Builder::Position(const Vec3& position)
{
    _spawnDesc.overrides["position"] = { position.x, position.y, position.z };
    return *this;
}

Spawn_Prefab::Builder& Spawn_Prefab::Builder::Rotation(const Vec3& eulerDegree)
{
    _spawnDesc.overrides["rotation"] = { eulerDegree.x, eulerDegree.y, eulerDegree.z };
    return *this;
}

Spawn_Prefab::Builder& Spawn_Prefab::Builder::Scale(const Vec3& scale)
{
    _spawnDesc.overrides["scale"] = { scale.x, scale.y, scale.z };
    return *this;
}

Shared<GameObject> Spawn_Prefab::Builder::Spawn() const
{
    return Spawn_Prefab::Spawn_Implementation(_spawnDesc);
}

Shared<GameObject> Spawn_Prefab::Spawn(
    const string& prefabName,
    uint32 levelIndex,
    const wstring& layerTag)
{
    FSpawnDesc desc{};
    desc.prefabName = prefabName;
    desc.levelIndex = levelIndex;
    desc.layerTag = layerTag;
    desc.hasExplicitLayerTag = true;

    return Spawn_Implementation(desc);
}

Shared<GameObject> Spawn_Prefab::SpawnAt(
    const string& prefabName,
    uint32 levelIndex,
    const wstring& layerTag,
    const Vec3& position)
{
    FSpawnDesc desc{};
    desc.prefabName = prefabName;
    desc.levelIndex = levelIndex;
    desc.layerTag = layerTag;
    desc.hasExplicitLayerTag = true;
    desc.overrides["position"] = { position.x, position.y, position.z };

    return Spawn_Implementation(desc);
}

Shared<GameObject> Spawn_Prefab::SpawnForEditorPlacement(
    const string& prefabName,
    uint32 levelIndex,
    const Vec3& position)
{
    const fs::path prefabPath = Resolve_PrefabPath(prefabName);

    json root{};
    if (!Load_PrefabJson(prefabPath, root))
        return nullptr;

    Shared<GameObject> gameObject = GAME->Instantiate_GameObjectJson(root);
    CHECK_NULL(gameObject, nullptr);

    gameObject->Set_SourcePrefabName(Resolve_CanonicalPrefabName(prefabName, prefabPath));
    gameObject->Set_RuntimeAuthoringLoaded(true); // 에디터 드래그 배치 프리펩은 authored 관리 대상으로 표시한다.
    Refresh_PlayerBodyAnimMapping(gameObject);
    Refresh_MonsterBodyAnimMapping(gameObject);

    Shared<TransformCom> transform = gameObject->Get_Transform();
    if (transform != nullptr)
        transform->Set_WorldPosition(position);

    const wstring typeLayerTag = Resolve_DefaultLayerTag(gameObject);
    wstring layerTag = Resolve_DefaultLayerTagFromPrefabFile(prefabPath);
    if (layerTag.empty() || (layerTag == TEXT("Layer_GameObject") && typeLayerTag != TEXT("Layer_GameObject")))
        layerTag = typeLayerTag; // Content Browser 배치 몬스터는 generic layer 대신 runtime 몬스터 레이어로 정규화한다.

    CHECK_FAILED(GAME->Add_GameObject(levelIndex, layerTag, gameObject), nullptr);

    return gameObject;
}

fs::path Spawn_Prefab::Resolve_PrefabPath(const string& prefabName)
{
    const wstring resolvedPath = GAME->Resolve_AssetPath(prefabName);
    if (!resolvedPath.empty())
        return fs::path(resolvedPath); // 에디터 payload가 guid를 싣는 경우에는 AssetRegistry/meta 기준으로 실제 프리펩 경로를 바로 푼다.

    const wstring fileName = String::ToWString(prefabName) + L".prefab.json";
    const fs::path assetRoot = fs::path(GAME->Get_AssetRoot());

    if (prefabName.starts_with("SL_"))
    {
        const fs::path playerPath = assetRoot / Constants::Prefab::Player_Dir / fileName;
        const fs::path monsterPath = assetRoot / Constants::Prefab::Monster_Dir / fileName;

        if (fs::exists(monsterPath) && !fs::exists(playerPath))
            return monsterPath;

        return playerPath;
    }

    if (prefabName.starts_with("PW_") ||
        prefabName.starts_with("PB_") ||
        prefabName.starts_with("PP_"))
        return assetRoot / Constants::Prefab::Player_Dir / fileName; // 플레이어 프리팹은 Player 폴더로 정규화한다.

    if (prefabName.starts_with("NPC_"))
        return assetRoot / Constants::Prefab::NPC_Dir / fileName; // NPC 프리팹은 전용 NPC 폴더에서 찾는다.

    if (prefabName.starts_with("Item_"))
        return assetRoot / Constants::Prefab::Item_Dir / fileName; // 아이템 프리팹은 Item 하위 폴더에서 찾는다.

    if (prefabName.starts_with("MW_") || prefabName.starts_with("MB_") || prefabName.starts_with("TW_"))
        return assetRoot / Constants::Prefab::Monster_Dir / fileName; // 튜토리얼 월드 몬스터도 Monster 폴더로 정규화한다.

    if (prefabName.starts_with("WP_"))
        return assetRoot / Constants::Prefab::Weapon_Dir / fileName; // 무기 프리팹은 기존 Weapons 폴더를 계속 쓴다.

    if (prefabName.starts_with("BSS_"))
        return assetRoot / Constants::Prefab::BattleStartSpawner_Dir / fileName; // BattleStartSpawner는 전용 폴더를 쓴다.

    const fs::path rootFallback = assetRoot / Constants::Prefab::Root_Dir / fileName;
    if (fs::exists(rootFallback))
        return rootFallback; // 분류 규칙이 없는 기존 프리팹만 루트 fallback을 쓴다.

    const fs::path recursiveRoot = assetRoot / Constants::Prefab::Root_Dir;
    for (const auto& entry : fs::recursive_directory_iterator(recursiveRoot))
    {
        if (entry.is_regular_file() && entry.path().filename() == fileName)
            return entry.path(); // 기존 저장본의 "EyeTarget"처럼 폴더가 빠진 이름도 복원한다.
    }

    return rootFallback;
}

bool Spawn_Prefab::Load_PrefabJson(const fs::path& prefabPath, json& outRoot)
{
    outRoot = json{};

    if (prefabPath.empty())
        return false;

    const wstring cacheKey = Make_PrefabJsonCacheKey(prefabPath);
    std::error_code timeError{};
    const fs::file_time_type lastWriteTime = fs::last_write_time(prefabPath, timeError);

    {
        lock_guard<mutex> lock(g_prefabJsonCacheMutex);
        const auto iter = g_prefabJsonCache.find(cacheKey);
        if (iter != g_prefabJsonCache.end() && !timeError && iter->second.lastWriteTime == lastWriteTime)
        {
            outRoot = iter->second.root;
            return outRoot.is_object();
        }
    }

    json root = GAME->Load_JsonFile(prefabPath);
    if (!root.is_object())
        return false;

    outRoot = root;

    {
        lock_guard<mutex> lock(g_prefabJsonCacheMutex);
        g_prefabJsonCache[cacheKey] = { std::move(root), lastWriteTime };
    }

    return true;
}

void Spawn_Prefab::Cache_PrefabJson(const fs::path& prefabPath, const json& root)
{
    if (prefabPath.empty() || !root.is_object())
        return;

    std::error_code timeError{};
    const fs::file_time_type lastWriteTime = fs::last_write_time(prefabPath, timeError);
    if (timeError)
        return;

    lock_guard<mutex> lock(g_prefabJsonCacheMutex);
    g_prefabJsonCache[Make_PrefabJsonCacheKey(prefabPath)] = { root, lastWriteTime };
}

bool Spawn_Prefab::Cache_PrefabJsonIfUnmodified(
    const fs::path& prefabPath,
    const json& root,
    const fs::file_time_type& expectedLastWriteTime)
{
    if (prefabPath.empty() || !root.is_object())
        return false;

    std::error_code timeError{};
    const fs::file_time_type lastWriteTime = fs::last_write_time(prefabPath, timeError);
    if (timeError || lastWriteTime != expectedLastWriteTime)
        return false;

    lock_guard<mutex> lock(g_prefabJsonCacheMutex);
    g_prefabJsonCache[Make_PrefabJsonCacheKey(prefabPath)] = { root, lastWriteTime };
    return true;
}

void Spawn_Prefab::Clear_PrefabJsonCache()
{
    lock_guard<mutex> lock(g_prefabJsonCacheMutex);
    g_prefabJsonCache.clear();
}

void Spawn_Prefab::Apply_Overrides(const Shared<GameObject>& gameObject, const json& overrides)
{
    if (gameObject == nullptr || overrides.is_null() || !overrides.is_object())
        return;

    Shared<TransformCom> transform = gameObject->Get_Transform();
    if (transform == nullptr)
        return;

    if (overrides.contains("position"))
    {
        const json& arr = overrides["position"];
        if (arr.is_array() && arr.size() >= 3)
        {
            transform->Set_WorldPosition(Vec3(
                arr[0].get<float>(),
                arr[1].get<float>(),
                arr[2].get<float>()));
        }
    }

    if (overrides.contains("rotation"))
    {
        const json& arr = overrides["rotation"];
        if (arr.is_array() && arr.size() >= 3)
        {
            transform->Set_WorldRotationEuler(Vec3(
                arr[0].get<float>(),
                arr[1].get<float>(),
                arr[2].get<float>()));
        }
    }

    if (overrides.contains("scale"))
    {
        const json& arr = overrides["scale"];
        if (arr.is_array() && arr.size() >= 3)
        {
            transform->Set_Scale(Vec3(
                arr[0].get<float>(),
                arr[1].get<float>(),
                arr[2].get<float>()));
        }
    }
}

wstring Spawn_Prefab::Resolve_DefaultLayerTagFromPrefabFile(const fs::path& prefabPath)
{
    json root{};
    if (!Load_PrefabJson(prefabPath, root))
        return {};

    if (!root.is_object() || !root.contains("default_layer_tag") || !root["default_layer_tag"].is_string())
        return {};

    return String::ToWString(root["default_layer_tag"].get<string>());
}

wstring Spawn_Prefab::Resolve_DefaultLayerTag(const Shared<GameObject>& gameObject)
{
    if (dynamic_pointer_cast<Monster_WorldBase>(gameObject) != nullptr)
        return TEXT("Layer_Monster");

    if (dynamic_pointer_cast<Player_WorldBase>(gameObject) != nullptr)
        return TEXT("Layer_Player");

    if (dynamic_pointer_cast<SL_Maelle>(gameObject) != nullptr)
        return TEXT("Layer_Player");

    return TEXT("Layer_GameObject");
}

Shared<GameObject> Spawn_Prefab::Spawn_Implementation(const FSpawnDesc& desc)
{
    if (desc.prefabName.empty())
        return nullptr;

    const fs::path prefabPath = Resolve_PrefabPath(desc.prefabName);

    json root{};
    if (!Load_PrefabJson(prefabPath, root))
        return nullptr;

    Shared<GameObject> gameObject = GAME->Instantiate_GameObjectJson(root);
    CHECK_NULL(gameObject, nullptr);

    gameObject->Set_SourcePrefabName(Resolve_CanonicalPrefabName(desc.prefabName, prefabPath));
    Refresh_PlayerBodyAnimMapping(gameObject);
    Refresh_MonsterBodyAnimMapping(gameObject);

    Apply_Overrides(gameObject, desc.overrides);

    wstring layerTag = desc.layerTag;
    const wstring typeLayerTag = Resolve_DefaultLayerTag(gameObject);
    if (!desc.hasExplicitLayerTag)
    {
        layerTag = Resolve_DefaultLayerTagFromPrefabFile(prefabPath);
    }
    if (layerTag.empty() || (layerTag == TEXT("Layer_GameObject") && typeLayerTag != TEXT("Layer_GameObject")))
        layerTag = typeLayerTag; // 저장된 GUID authored prefab도 코드 spawn과 같은 몬스터 레이어로 복원한다.

    CHECK_FAILED(GAME->Add_GameObject(desc.levelIndex, layerTag, gameObject), nullptr);

    return gameObject;
}
