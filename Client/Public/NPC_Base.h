#pragma once
#include "Character_WorldBase.h"

NS_BEGIN(Client)

class Trigger_Interaction;

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
    void    On_EditorStopRuntime();
    void    BuildDialogueRequest(const string& npcPrefabName);
    static void Register_CommonReflectionProperties(Engine::ClassReflectionInfo& info);

    bool Is_CinematicDriven() const { return _isCinematicDriven; }

    void Set_PatrolPoints(vector<Vec3> patrolPoints) { _PatrolPoints = std::move(patrolPoints); }
    const vector<Vec3>& Get_PatrolPoints() const { return _PatrolPoints; }
    const vector<Vec3>& Get_EffectivePatrolPoints();
    void Set_DialogueAnimation(const string& animationKey);
    string Get_CurrentAnimationKey() const;
    void Reset_ToDefaultAnimation();
    void Set_DialogueVisualOffsetY(float offsetY);
    void Reset_DialogueVisualOffset();

    // Restore monster state after battle exit.
    void   Restore_AfterBattleExit();

protected:
    virtual HRESULT Ready_Components();
    virtual HRESULT Ready_PartObjects();
    void Apply_DefaultAnimation();

private:
    void Build_DefaultPatrolPoints();

protected:
    FDelegateHandle              _editorStopHandle = {};

    vector<Vec3>                 _PatrolPoints;
    vector<Vec3>                 _defaultPatrolPoints;

    bool _isCinematicDriven = false;


    // Converts world-monster root motion into movement speed.
    float _worldMoveSpeedScale = 0.4f;
    string _defaultAnimationKey = {};
    Vec3 _dialogueVisualBaseLocalPosition = Vec3::Zero;
    bool _hasDialogueVisualBaseLocalPosition = false;
    Shared<Trigger_Interaction> _sharedTrigger = {};
public:
    void Free() override;

};

NS_END

