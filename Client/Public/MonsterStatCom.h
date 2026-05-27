#pragma once
#include "Battle_Enum.h"
#include "CombatStatCom.h"

NS_BEGIN(Client)

class Buff;

class MonsterStatCom : public CombatStatCom
{
    GENERATED_COMPONENT(MonsterStatCom)
public:
    // 초기 세팅 값
    struct MonsterCombatStatDesc : CombatStatCom::CombatStatDesc
    {
        bool isBoss = false;
        bool isAir = false;

        EEnemyClass enemyClass = EEnemyClass::Normal;

        float breakBarHPPercent = 120.f;
        float breakBarStunDuration = 1.f;

        vector<FElementalAffinity> initialElementalAffinities = {};
        vector<FLootData> possibleLoot = {};

        vector<string> _skillKeys;
    };

    // UI, 전투 데미지 계산, 디버그 출력이 읽는 값
    // 변경된 실제 Stat 정보
    struct MRuntimeCombatStat : CombatStatCom::RuntimeCombatStat
    {
        bool isBreak = { false };
    };

public:
    MonsterStatCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MonsterStatCom(const MonsterStatCom& Prototype);
    ~MonsterStatCom() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void From_Json(const json& data) override;

    void Update(float timeDelta);

public:
    CombatStatDesc& Get_StatInfo() override { return _monsterDesc; };
    RuntimeCombatStat& Get_RuntimeStat() override { return _MRuntimeStat; };
    const MonsterCombatStatDesc& Get_MonsterDesc() const { return _monsterDesc; }
    const MRuntimeCombatStat& Get_MonsterRuntimeStat() const { return _MRuntimeStat; }
    MRuntimeCombatStat& Get_MonsterRuntimeStat() { return _MRuntimeStat; }
    const vector<Shared<Buff>>& Get_Buffs() const { return _buff; }

    bool IsBreak() const { return _MRuntimeStat.isBreak; }
    void SetBreak(bool value) { _MRuntimeStat.isBreak = value; }



    void Set_RuntimeCombatProfile(
        float maxHp,
        uint32 maxAp,
        float physicalAttack,
        float physicalDefense,
        float speed);

private:
    MonsterCombatStatDesc _monsterDesc;
    MRuntimeCombatStat _MRuntimeStat;
public:
    static Shared<MonsterStatCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
};

NS_END
