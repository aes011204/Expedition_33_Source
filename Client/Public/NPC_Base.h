#pragma once
#include "Character_WorldBase.h"

NS_BEGIN(Client)

class NPC_Base abstract : public Character_WorldBase
{
public:
    NPC_Base(const ComPtr<Device>& device, const ComPtr<Context>& context);
    NPC_Base(const NPC_Base& prototype);
    virtual ~NPC_Base() override = default;

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
    void            OnStayOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;
    void            On_EditorStopRuntime();

    bool Is_CinematicDriven() const { return _isCinematicDriven; }

    void Set_PatrolPoints(vector<Vec3> patrolPoints) { _PatrolPoints = std::move(patrolPoints); }
    const vector<Vec3>& Get_PatrolPoints() const { return _PatrolPoints; }
    const vector<Vec3>& Get_EffectivePatrolPoints();

    // 전투 도주 시, 몬스터 복구
    void   Restore_AfterBattleExit();

protected:
    virtual HRESULT Ready_Components();

private:
    void Build_DefaultPatrolPoints();

protected:
    FDelegateHandle              _editorStopHandle = {};

    vector<Vec3>                 _PatrolPoints;
    vector<Vec3>                 _defaultPatrolPoints;

    bool _isCinematicDriven = false;


    // 기본 월드 몬스터 root motion -> 이동 속도 환산값
    float _worldMoveSpeedScale = 0.4f;

public:
    void Free() override;

};

NS_END
