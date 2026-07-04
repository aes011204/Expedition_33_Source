#include "BattleFollowupManager.h"
#include "GameObject.h"
#include "GameInstance.h"
#include "MapInstancedMesh.h"
#include "MapStaticMesh.h"

HRESULT BattleFollowupManager::Initialize()
{
    Clear_PendingFollowup();

    return S_OK;
}

void BattleFollowupManager::Handle_BattleVictory(const vector<string>& defeatedEnemyKeys, const vector<HeroId>& heroIds,
    uint32 battleFieldId, const Shared<GameObject>& sourceWorldObject)
{
    Clear_PendingFollowup();

    if (Should_StartSimonPhase2(defeatedEnemyKeys, sourceWorldObject))
    {
        Queue_SimonPhase2(heroIds, battleFieldId, sourceWorldObject);
    }
}

bool BattleFollowupManager::Should_StartSimonPhase2(const vector<string>& defeatedEnemyKeys,
    const Shared<GameObject>& sourceWorldObject) const
{
    if (!sourceWorldObject)
        return false;

    // 페이즈 전환은 시몬한테만
    if (sourceWorldObject->Get_SourcePrefabName() != Constants::Prefab::MW_Simon)
        return false;

    // 시몽 1페가 잡혔는지 확인
    const bool defeatedSimonPhase1 =
        find(defeatedEnemyKeys.begin(), defeatedEnemyKeys.end(), Constants::Prefab::MB_Simon) !=
        defeatedEnemyKeys.end();

    const bool defeatedSimonPhase2 =
        find(defeatedEnemyKeys.begin(), defeatedEnemyKeys.end(), Constants::Prefab::MB_Simon_V2) !=
        defeatedEnemyKeys.end();

    return defeatedSimonPhase1 && !defeatedSimonPhase2;
}

void BattleFollowupManager::Queue_SimonPhase2(const vector<HeroId>& heroIds, uint32 battleFieldId,
    const Shared<GameObject>& sourceWorldObject)
{
    BattleMonsterSpawnDesc simonPhase2{};
    simonPhase2.prefabName = Constants::Prefab::MB_Simon_V2;

    _hasPendingFollowup = true;
    _pendingFollowup = {};
    _pendingFollowup.battleFieldId = battleFieldId;
    _pendingFollowup.isPlayerAdvantage = false;
    _pendingFollowup.heroIds = heroIds;
    _pendingFollowup.monsterSpawnDescs = { simonPhase2 };
    _pendingFollowup.sourceWorldObject = sourceWorldObject;
}

bool BattleFollowupManager::Consume_PendingFollowup(BattleFollowupDesc& outDesc)
{
    if (!_hasPendingFollowup)
        return false;

    if (_pendingFollowup.sourceWorldObject == nullptr)
    {
        Clear_PendingFollowup();
        return false;
    }

    outDesc = _pendingFollowup;
    Clear_PendingFollowup();
    return true;
}

void BattleFollowupManager::Clear_PendingFollowup()
{
    _hasPendingFollowup = false;
    _pendingFollowup = {};
}

void BattleFollowupManager::Apply_SimonPhase2WorldVisualChange()
{
    GAME->Visit_LevelObjects(
        GAME->Current_LevelIndex(),
        [](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            if (const Shared<MapInstancedMesh> instancedMesh = dynamic_pointer_cast<MapInstancedMesh>(gameObject))
            {
                instancedMesh->Set_MapVisible(false);
                return;
            }

            if (const Shared<MapStaticMesh> staticMesh = dynamic_pointer_cast<MapStaticMesh>(gameObject))
            {
                if (staticMesh->Get_Name() == TEXT("EditorSphere_11"))
                {
                    staticMesh->Set_MapVisible(true);
                }
            }
        });
}

Unique<BattleFollowupManager> BattleFollowupManager::Create()
{
    auto instance = make_unique<BattleFollowupManager>();

    if (FAILED(instance->Initialize()))
    {
        LOG_ERROR("Failed to Created : BattleFollowupManager");
        return nullptr;
    }

    return instance;
}

void BattleFollowupManager::Free()
{
    Base::Free();
}
