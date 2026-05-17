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
    virtual ~MonsterStatCom() = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;

    void Update(float timeDelta);


public:
    virtual  CombatStatDesc& Get_StatInfo() { return _monsterDesc; };
    virtual  RuntimeCombatStat& Get_RuntimeStat() { return _MRuntimeStat; };
    const MonsterCombatStatDesc& Get_MonsterDesc() const { return _monsterDesc; }
    const MRuntimeCombatStat& Get_MonsterRuntimeStat() const { return _MRuntimeStat; }
    MRuntimeCombatStat& Get_MonsterRuntimeStat() { return _MRuntimeStat; }
    const vector<Shared<Buff>>& Get_Buffs() const { return _buff; }


    bool IsBreak() const { return _MRuntimeStat.isBreak; }
    void SetBreak(bool value) { _MRuntimeStat.isBreak = value; }

private:

    MonsterCombatStatDesc _monsterDesc;

    MRuntimeCombatStat _MRuntimeStat;


public:

    /// <test>
    Skill_Data Get_Basic_Skill()
    {
        Skill_Data skill = {}; skill.scriptClassName = "Base_Gustave_Attack";
        skill.apCost = 0;
        skill.targetingType = ECharacterTeamSelector::Invalid;

        skill.animationKey.push_back("Lancelier_Lancelier_Walk");
        skill.animationKey.push_back("Lancelier_Lancelier_Dash_Out");
        skill.animationKey.push_back("Lancelier_Lancelier_Skill_1");
        skill.IdleAnimationKey = "Lancelier_Idle"; // action 종료 후 복귀
      

        return skill;
    };
    /// </test>

public:
    static Shared<MonsterStatCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;

};

NS_END
