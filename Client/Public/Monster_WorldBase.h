#pragma once

#include "BattleMonsterSpawnDesc.h"
#include "Character_WorldBase.h"
#include "Delegate.h"

NS_BEGIN(Engine)
class AIController;
NS_END

NS_BEGIN(Client)

class MonsterStatCom;

class Monster_WorldBase abstract : public Character_WorldBase
{
public:
    enum class MonsterWorldAnim
    {
        Idle,
        Patrol,
        Chase
    };

public:
    Monster_WorldBase(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Monster_WorldBase(const Monster_WorldBase& prototype);
    virtual ~Monster_WorldBase() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    BeginPlay() override;
    void    Update(float timeDelta) override;
    void    Late_Update(float timeDelta) override;
    HRESULT Render() override;
    HRESULT Render_Shadow() override;

    json    To_Json() const override;
    void    From_Json(const json& data) override;

public:
    void            OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void            On_EditorStopRuntime();

    // 이 월드 몬스터와 충돌했을 때 생성할 배틀 몬스터 목록을 넘긴다.
    virtual void    Set_BattleMonsterSpawnDescs(vector<BattleMonsterSpawnDesc>& outSpawnDescs) const;

    // Prefab_View가 배틀 소환 목록을 직접 편집할 때 사용한다.
    vector<BattleMonsterSpawnDesc>& Get_BattleMonsterSpawnDescs_ForEdit() { return _battleMonsterSpawnDescs; }
    const vector<BattleMonsterSpawnDesc>& Get_BattleMonsterSpawnDescs() const { return _battleMonsterSpawnDescs; }

    void Set_CurrentAction(MonsterWorldAnim action) { _currentAction = action; }
    MonsterWorldAnim Get_CurrentAction() const { return _currentAction; }
protected:
    virtual HRESULT Ready_Components();

private:
    MonsterWorldAnim _currentAction = MonsterWorldAnim::Idle;
    MonsterWorldAnim _previousAction = MonsterWorldAnim::Idle;

protected:
    Shared<AIController>         _aiCtrl;
    Shared<MonsterStatCom>       _combatStat;

    vector<BattleMonsterSpawnDesc> _battleMonsterSpawnDescs = {};

    bool                         _isBattleStart = false;
    FDelegateHandle              _editorStopHandle = {};

public:
    void Free() override;

};

NS_END
