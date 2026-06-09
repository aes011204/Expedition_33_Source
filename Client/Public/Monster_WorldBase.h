#pragma once

#include "BattleMonsterSpawnDesc.h"
#include "Character_WorldBase.h"
#include "Delegate.h"

#include <utility>

NS_BEGIN(Engine)
class AIController;
class MovementCom;
NS_END

NS_BEGIN(Client)

class MonsterStatCom;
class EffectCom;

class Monster_WorldBase abstract : public Character_WorldBase
{
public:
    enum class MonsterWorldAnim
    {
        Idle,
        Patrol,
        Chase,
        Stun,
        Hit
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
    void Set_DesiredAction(MonsterWorldAnim action) { _desiredAction = action; }
    void Apply_Stun(float duration);
    void OnHitAnimationFinished();
    MonsterWorldAnim Get_CurrentAction() const { return _currentAction; }
    MonsterWorldAnim Get_DesiredAction() const { return _desiredAction; }
    bool Is_Stunned() const { return _isStun; }

    void Set_PatrolPoints(vector<Vec3> patrolPoints) { _PatrolPoints = std::move(patrolPoints); }
    const vector<Vec3>& Get_PatrolPoints() const { return _PatrolPoints; }
    const vector<Vec3>& Get_EffectivePatrolPoints();

    int32 Get_BattleStartSpawnerId() const { return _battleStartSpawnerId; }
    void  Set_BattleStartSpawnerId(int32 spawnerId) { _battleStartSpawnerId = spawnerId; }
    Vec3  Get_BattleStartOffset() const { return _battleStartOffset; }
    void  Set_BattleStartOffset(const Vec3& offset) { _battleStartOffset = offset; }

    uint32 Get_BattleFieldId() const { return _battleEncounter; }
    void   Set_BattleFieldId(uint32 fieldId) { _battleEncounter = fieldId; }

protected:
    virtual HRESULT Ready_Components();

private:
    void Build_DefaultPatrolPoints();

    MonsterWorldAnim _desiredAction = MonsterWorldAnim::Idle;
    MonsterWorldAnim _currentAction = MonsterWorldAnim::Idle;
    MonsterWorldAnim _previousAction = MonsterWorldAnim::Idle;
    bool _hasAppliedAction = false;

protected:
    Shared<AIController>         _aiCtrl;
    Shared<MovementCom>          _movementCom;
    Shared<MonsterStatCom>       _combatStat;
    Shared<EffectCom>            _effectCom;

    vector<BattleMonsterSpawnDesc> _battleMonsterSpawnDescs = {};

    bool                         _isBattleStart = false;
    FDelegateHandle              _editorStopHandle = {};

    vector<Vec3>                 _PatrolPoints;
    vector<Vec3>                 _defaultPatrolPoints;

    bool _isStun = {false};
    float _maxStunTime = {3.f};
    float _stunTime = {};

    // 기본 월드 몬스터 root motion -> 이동 속도 환산값
    float _worldMoveSpeedScale = 0.4f;

    int32 _battleStartSpawnerId = 0; // 배틀 시작 스포너 ID
    Vec3  _battleStartOffset = Vec3::Zero; // MW 프리팹에서 플레이어 기준 몬스터 시작 위치를 조절한다.

    uint32 _battleEncounter = 1;

public:
    void Free() override;

};

NS_END
