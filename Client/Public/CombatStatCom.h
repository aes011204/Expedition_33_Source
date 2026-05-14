#pragma once

#include "Battle_Enum.h"
#include "Client_Defines.h"
#include "Component.h"

NS_BEGIN(Client)

class Buff;

    class CombatStatCom : public Component
{
    GENERATED_COMPONENT(CombatStatCom)

public:
    // 초기 세팅 값
    struct CombatStatDesc
    {
        string rowName;
        string ProtoName;

        uint32 level = 1;
        uint32 exp = 0;
        uint32 chroma = 0.f;

        float maxHp = 0.f;
        uint32 maxAp = 0.f;
        float physicalAttack = 0.f;
        float physicalDefense = 0.f;

        float accuracy = 0.f;
        float speed = 0.f;
        float criticalRate = 0.f;
        float mastery = 0.f;
        float criticalChance = 0.f;

        string portrait;

        FCommonCharacterStats commonCharacterStats = {};

    };

  
    struct RuntimeCombatStat
    {
        float CurHp = {};
        uint32 CurShield = {};
        uint32 CurAp = {};

        float physicalAttack = 0.f;
        float physicalDefense = 0.f;

        float CurSpeed = 0.f;

        EAliveStatus CurAliveState = EAliveStatus::Invalid;
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
    const CombatStatDesc& Get_StatInfo() const { return _statDesc; }

    RuntimeCombatStat& Get_RuntimeStat() { return _runtimeStat; }

    //void ApplyDamage(float damage);
    //void Heal(float amount);

    bool IsDead() const;

    int32 Get_CurrentShield() const{return _runtimeStat.CurShield;}
    void Add_Shield(int32 value);
    void Remove_Shield(int32 value);

    //void Add_Ailment(const Shared<Buff>& ailment);
    //void Remove_Ailment(const Shared<Buff>& ailment);
    //void Clear_Ailments();
    vector<Shared<Buff>>& Get_Buff() { return _buff; };

    bool Is_Player() { return _isPlayer; }

    EAliveStatus Get_AliveStatus() { return _runtimeStat.CurAliveState; };
    void Set_AliveStatus(EAliveStatus Alive) { _runtimeStat.CurAliveState = Alive; }

    float Get_Speed() { return _runtimeStat.CurSpeed; };
    void Set_Speed(float speed) { _runtimeStat.CurSpeed = speed; }
    PlayerCharactorType Get_PlayerType() { return _playerType; }
protected:
    CombatStatDesc _statDesc;

    RuntimeCombatStat _runtimeStat;

    vector<Shared<Buff>> _buff;// 상태이상


    bool _isPlayer = { false };
    PlayerCharactorType _playerType = PlayerCharactorType::None;
public:
    static Shared<CombatStatCom> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<Component> Clone(void* arg) override;
    void Free() override;
    
};

NS_END
