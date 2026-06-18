#pragma once

#include "Battle_Enum.h"
#include "Client_Defines.h"
#include "Component.h"

NS_BEGIN(Client)

class Buff;

class CombatStatCom : public Component
{
public:
   
public:
    // 초기 세팅 값
    struct CombatStatDesc
    {
        string rowName;
        string ProtoName;

        uint32 level = 1;
        uint32 exp = 0;
        uint32 chroma = 0;

        float maxHp = 100.f;
        uint32 maxAp = 0;
        float physicalAttack = 0.f;
        float physicalDefense = 0.f;

        float accuracy = 0.f;
        float speed = 0.f;
        float criticalRate = 0.f;
        float mastery = 0.f;
        float criticalChance = 0.f;

        string portraitGuid;

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

        EAliveStatus CurAliveState = EAliveStatus::Alive;
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
    virtual  CombatStatDesc& Get_StatInfo() = 0;;

    virtual  RuntimeCombatStat& Get_RuntimeStat() = 0;
    virtual const wstring& Get_BattleAnnounceSubjectName() const
    {
        static const wstring empty{};
        return empty;
    }

    EAliveStatus Get_CurAliveState();

    uint32 Get_CurrentShield() { return Get_RuntimeStat().CurShield; }
    void Add_Shield(int32 value);
    void Remove_Shield(int32 value);

    vector<Shared<Buff>>& Get_Buff() { return _buff; };

    bool Is_Player() { return _isPlayer; }

    EAliveStatus Get_AliveStatus() { return Get_RuntimeStat().CurAliveState; };
    void Set_AliveStatus(EAliveStatus Alive) { Get_RuntimeStat().CurAliveState = Alive; }

    float Get_Speed() { return Get_RuntimeStat().CurSpeed; };
    void Set_Speed(float speed) { Get_RuntimeStat().CurSpeed = speed; }

    float Get_CurHp() { return Get_RuntimeStat().CurHp; };
    //void Set_CurHp(float hp) { Get_RuntimeStat().CurHp = hp; } //이건 일단  데미지 시스템을 통해서
    PlayerCharacterType Get_PlayerType() { return _playerType; }
    void Set_PlayerType(PlayerCharacterType playerType) { _playerType = playerType; }

    vector<string> Get_SkillKey() { return _skillKeys; }
    const vector<string>& Get_SkillKeys() const { return _skillKeys; }

    void Set_SkillKeys(const vector<string>& skillKeys)
    {
        _skillKeys = skillKeys;
    }

    // TEMP : 전투 진입 시 world 해금 상태 기준으로 스킬 슬롯 채우기
    void Clear_Skills() { _skillKeys.clear(); }

    void Set_Skill(string skillName)
    {
        _skillKeys.push_back(skillName);
    }

    bool Try_GetSkillKeyBySlot(uint32 slotIndex, string& outSkillKey) const;

    //test
    string Get_FirstSkill();

public: /* AP */
    uint32 Get_CurAP() { return Get_RuntimeStat().CurAp; }
    uint32 Get_MaxAP() { return Get_StatInfo().maxAp; }

    void Add_AP(uint32 value);          // AP 즉시 회복
    bool Try_ConsumeAP(uint32 value);   // AP 즉시 소모 부족하면 false

protected:
    vector<Shared<Buff>> _buff;// 상태이상
    vector<string> _skillKeys;

    bool _isPlayer = { false };

    PlayerCharacterType _playerType = PlayerCharacterType::None;
public:
    virtual  Shared<Component> Clone(void* arg) = 0;
    void Free() override;

};

NS_END
