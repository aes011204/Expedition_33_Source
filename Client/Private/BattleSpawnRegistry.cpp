#include "BattleSpawnRegistry.h"

#include "GameInstance.h"
#include "GameObject.h"
#include "MonsterStatCom.h"
#include "TransformCom.h"

void BattleSpawnRegistry::Register_SpawnObject(const Shared<GameObject>& object, uint32 levelIndex)
{
    if (object == nullptr)
        return;

    BattleEntry entry{};
    entry.levelIndex = levelIndex;
    entry.object = object;

    if (const Shared<TransformCom> transform = object->Get_Transform())
    {
        entry.spawnPosition = transform->Get_Position();
        entry.spawnRotation = transform->Get_RotationEuler();
    }

    _spawnedObjects.push_back(entry);
}

void BattleSpawnRegistry::Register_BattleHeroObject(const Shared<GameObject>& object, uint32 levelIndex)
{
    Register_SpawnObject(object, levelIndex);

    if (!object)
        return;

    ++_battleHeroSpawnCount;
}

void BattleSpawnRegistry::Register_BattleMonsterObject(const Shared<GameObject>& object, uint32 levelIndex)
{
    Register_SpawnObject(object, levelIndex);

    if (object == nullptr)
        return;

    const Shared<MonsterStatCom> monsterStat = object->Get_Component<MonsterStatCom>();
    if (monsterStat == nullptr)
        return;

	const CombatStatCom::CombatStatDesc& statInfo = monsterStat->Get_StatInfo();

    _battleRewardExp += monsterStat->Get_StatInfo().exp;
	_battleRewardChroma += statInfo.chroma;

    if (!statInfo.portraitGuid.empty())
    {
        _battleMonsterPortraitGuids.push_back(statInfo.portraitGuid);
    }
}

void BattleSpawnRegistry::Set_SourceWorldObject(const Shared<GameObject>& object, uint32 levelIndex)
{
    _sourceWorldObject = object;
    _sourceWorldObjectLevelIndex = object ? levelIndex : UINT32_MAX;
}

void BattleSpawnRegistry::Update_SpawnPose(const Shared<GameObject>& object)
{
    if (object == nullptr)
        return;

    const Shared<TransformCom> transform = object->Get_Transform();
    if (transform == nullptr)
        return;

    for (BattleEntry& entry : _spawnedObjects)
    {
        if (entry.object != object)
            continue;

        entry.spawnPosition = transform->Get_Position();
        entry.spawnRotation = transform->Get_RotationEuler();
        return;
    }
}

void BattleSpawnRegistry::Visit_SpawnedObjects(const function<void(const BattleEntry&)>& visitor) const
{
    for (auto& entry : _spawnedObjects)
    {
        visitor(entry);
    }
}

const BattleSpawnRegistry::BattleEntry* BattleSpawnRegistry::Find_SpawnEntry(const Shared<GameObject>& object) const
{
    if (object == nullptr)
        return nullptr;

    for (const BattleEntry& entry : _spawnedObjects)
    {
        if (entry.object == object)
            return &entry;
    }

    return nullptr;
}

void BattleSpawnRegistry::Destroy_SpawnedObjects()
{
    for (const BattleEntry& entry : _spawnedObjects)
    {
        if (entry.object)
            GAME->Remove_GameObject(entry.levelIndex, entry.object);
    }

    _spawnedObjects.clear();
    _battleHeroSpawnCount = 0;
}

void BattleSpawnRegistry::Destroy_SourceWorldObject()
{
    const Shared<GameObject> sourceObject = _sourceWorldObject.lock();

    if (sourceObject)
        GAME->Remove_GameObject(_sourceWorldObjectLevelIndex, sourceObject);

    Clear_SourceWorldObject();
}

void BattleSpawnRegistry::Clear_BattleReward()
{
    _battleRewardExp = 0;
    _battleRewardChroma = 0;
    _battleMonsterPortraitGuids.clear();
}

void BattleSpawnRegistry::Clear_SourceWorldObject()
{
    _sourceWorldObject.reset();
    _sourceWorldObjectLevelIndex = UINT32_MAX;
}

void BattleSpawnRegistry::Clear_All()
{
    Destroy_SpawnedObjects();
    Clear_SourceWorldObject();
    Clear_BattleReward();

    _battleHeroSpawnCount = 0;
}

Shared<BattleSpawnRegistry> BattleSpawnRegistry::Create()
{
    return make_shared<BattleSpawnRegistry>();
}

void BattleSpawnRegistry::Free()
{
    Clear_All();

    Base::Free();
}
