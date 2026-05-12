#pragma once

#include "Component.h"

NS_BEGIN(Client)

class CombatStatCom : public Component
{
    GENERATED_COMPONENT(CombatStatCom)

public:
    // 초기 세팅 값
    struct CombatStatDesc
    {
        uint32 level = 1;
        uint32 exp = 0;
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
    struct CombatStatInfo
    {
        uint32 level = 1;
        uint32 exp  = 0;
        uint32 nextExp = 0;
        uint32 statPoint = 10;
        uint32 skillPoint = 10;
        uint32 respecCount = 1;

        int32 vitality = 1;
        int32 might = 1;
        int32 agility = 1;
        int32 defense = 1;
        int32 luck = 1;

        float maxHp = 0.f;
        float hp = 0.f;
        float attack = 0.f;
        float defenseValue = 0.f;
        float speed = 0.f;
        float criticalRate = 0.f;
    };

public:
    CombatStatCom(const ComPtr<Device>& device, const ComPtr<Context>& context);
    CombatStatCom(const CombatStatCom& Prototype);
    virtual ~CombatStatCom() = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;

    json To_Json() const override;
    void From_Json(const json& data) override;

public:
    const CombatStatInfo& Get_StatInfo() const { return _statInfo; }

    bool    Add_Stat(StatType type, int32 value = 1);
    bool    Reset_Stat();

    void    Add_Exp(uint32 value);
    uint32  Get_NextExp(uint32 level) const;

    uint32  Get_Level() const       { return _level; }
    uint32  Get_Exp() const         { return _exp; }
    uint32  Get_StatPoint() const   { return _statPoint; }
    uint32  Get_SkillPoint() const  { return _skillPoint; }
    uint32  Get_RespecCount() const { return _respecCount; }

private:
    int32*  Find_StatValue(StatType type);
    array<int32*, static_cast<size_t>(StatType::END)> Get_StatValues();

    void    Recalculate_Stats();    // 실제 전투 스탯 계산
    void    Update_StatInfo();      // 계산된 내부 값들을 업데이트

private:
    uint32 _level = 1;
    uint32 _exp = 0;
    uint32 _statPoint = 0;
    uint32 _skillPoint = 0;
    uint32 _respecCount = 1;

    int32 _vitality = 2;            
    int32 _might = 3;               
    int32 _agility = 4;             
    int32 _defenseStat = 5;         
    int32 _luck = 6;                

    float _maxHp = 100.f;
    float _hp = 100.f;
    float _attack = 10.f;
    float _defense = 0.f;
    float _speed = 100.f;
    float _criticalRate = 0.f;

    CombatStatInfo _statInfo = {};

public:
    static Shared<CombatStatCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
    
};

NS_END
