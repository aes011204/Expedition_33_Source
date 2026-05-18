#pragma once
#include "CombatStatCom.h"

NS_BEGIN(Client)

class PlayerStatCom :
    public CombatStatCom
{
    GENERATED_COMPONENT(PlayerStatCom)

public:
    // 초기 세팅 값
    struct PlayerCombatStatDesc : CombatStatCom::CombatStatDesc
    {
      
        uint32 statPoint = 10;
        uint32 skillPoint = 10;
        uint32 respecCount = 1; // 스탯 초기화 횟수

        int32 vitality = 1;     // 활력
        int32 might = 1;        // 무력
        int32 agility = 1;      // 민첩
        int32 defense = 1;      // 방어
        int32 luck = 1;         // 행운
    };

    // UI, 전투 데미지 계산, 디버그 출력이 읽는 값
    // 변경된 실제 Stat 정보
    struct PRuntimeCombatStat : CombatStatCom::RuntimeCombatStat
    {
 
        uint32 nextExp = 0;
        uint32 CurStatPoint = 0;
        uint32 CurSkillPoint = 0;
        uint32 CurRespecCount = 0;

        int32 CurVitality = 0;
        int32 CurMight = 0;
        int32 CurAgility = 0;
        int32 CurDefense = 0; // 표시용
        int32 CurLuck = 0;

        uint32 CurLevel = 1;
        uint32 CurExp = 0;
        uint32 CurChroma = 0;

        float CurMaxHp = 0.f;
        //추가해서 설정해줘야 함
        uint32 CurMaxExp = 0;
        uint32 CurMaxAp = 0;
        float CurPhysicalAttack = 0.f;
        float CurPhysicalDefense = 0.f;

        float CurAccuracy = 0.f;
        float CurSpeed = 0.f;
        float CurCriticalRate = 0.f;
        float CurMastery = 0.f;
        float CurCriticalChance = 0.f;

    };

public:
    PlayerStatCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    PlayerStatCom(const PlayerStatCom& Prototype);
    virtual ~PlayerStatCom() = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    From_Json(const json& data) override;

public:
    virtual  CombatStatDesc& Get_StatInfo() { return _playerDesc; };
    virtual  RuntimeCombatStat& Get_RuntimeStat() { return _PRuntimeStat; };

    const PlayerCombatStatDesc& Get_PlayerDesc() const{return _playerDesc;}
    PRuntimeCombatStat& Get_PlayerRuntimeStat(){return _PRuntimeStat;}
    bool    Add_Stat(StatType type, int32 value = 1);
    bool    Reset_Stat();

    uint32  Get_NextExp(uint32 level) const;
    void Add_Exp(uint32 value);


/// <test>
    Skill_Data Get_Basic_Skill()
    {
        Skill_Data skill = {}; skill.scriptClassName = "Base_Gustave_Attack";
        skill.apCost = 0; skill.targetingType = ECharacterTeamSelector::Invalid;


        return skill;
    };
/// </test>
private:
    int32* Find_StatValue(StatType type);
    array<int32*, static_cast<size_t>(StatType::END)> Get_StatValues();

    void    Recalculate_Stats();    // 실제 전투 스탯 계산
    void    Update_StatInfo();      // 계산된 내부 값들을 업데이트

private:

    PlayerCombatStatDesc _playerDesc;


    PRuntimeCombatStat _PRuntimeStat;

    //
   
public:
    static Shared<PlayerStatCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;

};

NS_END
