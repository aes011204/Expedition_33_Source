#include "Spawn_Prefab.h"

#include "GameInstance.h"
#include "GameObject.h"
#include "Helper_String.h"
#include "Player_WorldBase.h"
#include "Player_BattleBase.h"
#include "Body_Player.h"
#include "ModelCom.h"
#include "Monster_Part.h"
#include "Monster_WorldBase.h"
#include "Monster_BattleBase.h"
#include "TransformCom.h"
#include "Client_Constants.h"

namespace fs = std::filesystem;

namespace
{
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
    }
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
    desc.overrides["position"] = { position.x, position.y, position.z };

    return Spawn_Implementation(desc);
}

Shared<GameObject> Spawn_Prefab::SpawnForEditorPlacement(
    const string& prefabName,
    uint32 levelIndex,
    const Vec3& position)
{
    const fs::path prefabPath = Resolve_PrefabPath(prefabName);

    Shared<GameObject> gameObject = GAME->Instantiate_GameObjectJson(prefabPath);
    CHECK_NULL(gameObject, nullptr);

    gameObject->Set_SourcePrefabName(prefabName);
    gameObject->Set_RuntimeAuthoringLoaded(true); // 에디터 드래그 배치 프리펩은 authored 관리 대상으로 표시한다.
    Refresh_PlayerBodyAnimMapping(gameObject);
    Refresh_MonsterBodyAnimMapping(gameObject);

    Shared<TransformCom> transform = gameObject->Get_Transform();
    if (transform != nullptr)
        transform->Set_WorldPosition(position);

    wstring layerTag = Resolve_DefaultLayerTagFromPrefabFile(prefabPath);
    if (layerTag.empty())
        layerTag = Resolve_DefaultLayerTag(gameObject); // 프리펩 메타가 없으면 기존 타입 기반 추론으로 안전하게 fallback 한다.

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

    if (prefabName.starts_with("PW_") || prefabName.starts_with("PB_"))
        return assetRoot / Constants::Prefab::Player_Dir / fileName; // 플레이어 프리팹은 Player 폴더로 정규화한다.

    if (prefabName.starts_with("MW_") || prefabName.starts_with("MB_"))
        return assetRoot / Constants::Prefab::Monster_Dir / fileName; // 몬스터 프리팹은 Monster 폴더로 정규화한다.

    if (prefabName.starts_with("WP_"))
        return assetRoot / Constants::Prefab::Weapon_Dir / fileName; // 무기 프리팹은 기존 Weapons 폴더를 계속 쓴다.

    if (prefabName.starts_with("BSS_"))
        return assetRoot / Constants::Prefab::BattleStartSpawner_Dir / fileName; // BattleStartSpawner는 전용 폴더를 쓴다.

    return assetRoot / Constants::Prefab::Root_Dir / fileName; // 분류 규칙이 없는 기존 프리팹만 루트 fallback을 쓴다.
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
    const json root = GAME->Load_JsonFile(prefabPath);
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

    return TEXT("Layer_GameObject");
}

Shared<GameObject> Spawn_Prefab::Spawn_Implementation(const FSpawnDesc& desc)
{
    if (desc.prefabName.empty())
        return nullptr;

    const fs::path prefabPath = Resolve_PrefabPath(desc.prefabName);

    Shared<GameObject> gameObject = GAME->Instantiate_GameObjectJson(prefabPath);
    CHECK_NULL(gameObject, nullptr);

    gameObject->Set_SourcePrefabName(desc.prefabName);
    Refresh_PlayerBodyAnimMapping(gameObject);
    Refresh_MonsterBodyAnimMapping(gameObject);

    Apply_Overrides(gameObject, desc.overrides);

    CHECK_FAILED(GAME->Add_GameObject(desc.levelIndex, desc.layerTag, gameObject), nullptr);

    return gameObject;
}
