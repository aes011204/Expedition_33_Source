#pragma once

#include "Base.h"

NS_BEGIN(Engine)
class GameObject;
NS_END

NS_BEGIN(Client)

// 배틀 시작 시 스폰되는 객체들을 관리할 클래스
class BattleSpawnRegistry final : public Base
{
public:
    struct BattleEntry
    {
        uint32 levelIndex = 0;
        Shared<GameObject> object = nullptr;
        Vec3 spawnPosition = {};
        Vec3 spawnRotation = {};
    };

public:
    BattleSpawnRegistry() = default;
    ~BattleSpawnRegistry() override = default;

public:
    void Register_SpawnObject(const Shared<GameObject>& object, uint32 levelIndex);
    void Register_BattleMonsterObject(const Shared<GameObject>& object, uint32 levelIndex);
    void Set_SourceWorldObject(const Shared<GameObject>& object, uint32 levelIndex);
    void Update_SpawnPose(const Shared<GameObject>& object);

    void Visit_SpawnedObjects(const function<void(const BattleEntry&)>& visitor) const;

    const BattleEntry* Find_SpawnEntry(const Shared<GameObject>& object) const;

    void Destroy_SpawnedObjects();
    void Destroy_SourceWorldObject();

    uint32 Get_BattleRewardExp() const { return _battleRewardExp; }
    void Clear_BattleReward();

    void Clear_SourceWorldObject();
    void Clear_All();

    bool Is_Empty() const { return _spawnedObjects.empty(); }

private:
    vector<BattleEntry> _spawnedObjects;

    uint32 _battleRewardExp = 0;

    // 전투를 진입할 때, 충돌된 월드베이스 몬스터
    Weak<GameObject> _sourceWorldObject;
    uint32 _sourceWorldObjectLevelIndex = 0;

public:
    static Shared<BattleSpawnRegistry> Create();
    void Free() override;
};

NS_END
