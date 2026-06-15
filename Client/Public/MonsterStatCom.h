#pragma once
#include "Battle_Enum.h"
#include "CombatStatCom.h"
#include <unordered_map>

NS_BEGIN(Client)

class Buff;
struct BattleMonsterStatSnapshot;

class MonsterStatCom : public CombatStatCom
{
    GENERATED_COMPONENT(MonsterStatCom)
public:
    // 초기 세팅 값
    struct MonsterCombatStatDesc : CombatStatCom::CombatStatDesc
    {
        MonsterCombatStatDesc()
        {
            chroma = 100; // prefab/json에 세팅안돼있으면 기본값 세팅
        }

        wstring battleAnnounceName = L"";


        bool isAir = false;

        EEnemyClass enemyClass = EEnemyClass::Normal;

        float breakBarHPPercent = 120.f;
        float breakBarStunDuration = 1.f;

        vector<FElementalAffinity> initialElementalAffinities = {};
        vector<FLootData> possibleLoot = {};

        vector<string> _skillKeys;

        float _criticalRatio = {};
        float _midRatio = {};
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
    MonsterCombatStatDesc& Get_MonsterDesc() { return _monsterDesc; }
    const MonsterCombatStatDesc& Get_MonsterDesc() const { return _monsterDesc; }
    const MRuntimeCombatStat& Get_MonsterRuntimeStat() const { return _MRuntimeStat; }
    MRuntimeCombatStat& Get_MonsterRuntimeStat() { return _MRuntimeStat; }
    const vector<Shared<Buff>>& Get_Buffs() const { return _buff; }

    float Get_CriticalRatio() const { return _monsterDesc._criticalRatio; }
    float Get_MidRatio() const { return _monsterDesc._midRatio; }

    bool IsBreak() const { return _MRuntimeStat.isBreak; }
    void SetBreak(bool value) { _MRuntimeStat.isBreak = value; }
    bool Is_Boss() const { return _monsterDesc.enemyClass == EEnemyClass::Boss; }

    void Set_RuntimeCombatProfile(
        float maxHp,
        uint32 maxAp,
        float physicalAttack,
        float physicalDefense,
        float speed);

    const wstring& Get_BattleAnnounceSubjectName() const override
    {
        return _monsterDesc.battleAnnounceName;
    }

    BattleMonsterStatSnapshot Create_BattleSyncSnapshot() const;
    void Apply_BattleSyncSnapshot(const BattleMonsterStatSnapshot& snapshot);
    void Rebuild_SkillRuntimeState();

    Skill_Data_Inst* Find_SkillRuntimeState(const string& skillKey);
    const Skill_Data_Inst* Find_SkillRuntimeState(const string& skillKey) const;

    bool Can_UseSkill(const Skill_Data& skill) const;
    void Notify_SkillUsed(const Skill_Data& skill);
    void Advance_SkillCooldownTurn();

    int32 Get_BattleTurnCount() const { return _battleTurnCount; }

private:
    MonsterCombatStatDesc _monsterDesc;
    MRuntimeCombatStat _MRuntimeStat;
    unordered_map<string, Skill_Data_Inst> _skillRuntimeByKey;
    int32 _battleTurnCount = 0;
public:
    static Shared<MonsterStatCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
};

NS_END
