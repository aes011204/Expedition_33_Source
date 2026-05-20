#pragma once

#include <iosfwd>
#include <vector>

#include "Battle_Enum.h"
#include "Client_Defines.h"
#include "Character.h"
#include "Mon_Lancelier_Body.h"

NS_BEGIN(Engine)
class ShaderCom;
class ModelCom;
class AIController;
NS_END

NS_BEGIN(Client)

class MonsterStatCom;
struct Action;
class Mon_Lancelier_Weapon;
class Mon_Lancelier_Body;

class Monster final : public Character
{
    GENERATED_GAMEOBJECT(Monster, ObjectType::Monster)
public:
    enum class LancelierAnim
    {
        Skill_1,
        Skill_2,
        Skill_2_Hollow,
        Walk,
        Idle,
    };

    enum class MonsterAction
    {
        Idle,
        Move,
        Attack
    };
    struct BattleDecision
    {
        int skillIndex = -1;
        int targetIndex = -1;
        EBattleContext actionType = EBattleContext::Invalid;
        float score = -FLT_MAX;
    };
    struct BattleAIPolicy
    {
        bool allowBaseAttack = true;
        bool allowSkill = true;
        bool allowDefend = true;

        float baseAttackBias = 0.f;
        float skillBias = 0.f;
        float defendBias = 0.f;

        vector<string> forbiddenSkillIds = {};
        vector<string> preferredSkillIds = {};
    };
public:
    Monster(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Monster(const Monster& prototype);
    ~Monster() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Priority_Update(float timeDelta) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;
    HRESULT Render_Shadow() override;

public:
protected:
    Shared<Mon_Lancelier_Body> _body = {};
    Shared<Mon_Lancelier_Weapon> _weapon = {};

    Shared<AIController> _aiCtrl = { nullptr };
    Shared<MonsterStatCom> _combatStat;
    Shared<ColliderCom> _collider;
    BoundingBox _cullingAABB{};

    int32 _rootindex = {};

protected:
    HRESULT Ready_Components();
    HRESULT Ready_PartObjects();
    HRESULT Ready_Collider();
    void Decide_Action_BTOnly(Action& curturnAction, const vector<BattleChar>& heroes);

    void ActionFromDecision(Action& curturnAction, const BattleDecision decision,const vector<BattleChar>& heroes);
public:
    void Set_CurrentAction(MonsterAction action) { _currentAction = action; }
    MonsterAction Get_CurrentAction() const { return _currentAction; }
    void Decide_Action(Action& curturnAction, const vector<BattleChar>& heroes);

private:
    MonsterAction _currentAction = MonsterAction::Idle;
    MonsterAction _previousAction = MonsterAction::Idle;

public:
    static Shared<Monster> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
