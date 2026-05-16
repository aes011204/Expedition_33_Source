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
public:
	Monster(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Monster(const Monster& prototype);
	virtual ~Monster() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* arg) override;
	virtual void    Priority_Update(float timeDelta) override;
	virtual void    Update(float timeDelta) override;
	virtual void    Late_Update(float timeDelta) override;
	virtual HRESULT Render() override;
    virtual HRESULT Render_Shadow() override;

    public:

protected:
    Shared<Mon_Lancelier_Body> _body = {};
    Shared<Mon_Lancelier_Weapon> _weapon = {};

    Shared<AIController> _aiCtrl = { nullptr };
    Shared<MonsterStatCom> _combatStat;
    Shared<ColliderCom>     _collider;
    int32 _rootindex = {};

    BoundingBox _cullingAABB{};

protected:
	HRESULT Ready_Components();
    HRESULT Ready_PartObjects();
    HRESULT Bind_ShaderResources();

    HRESULT Ready_Collider();

private:
    MonsterAction _currentAction = MonsterAction::Idle;
    MonsterAction _previousAction = MonsterAction::Idle;


public:
    void Set_CurrentAction(MonsterAction action) { _currentAction = action; }
    MonsterAction Get_CurrentAction() const { return _currentAction; }



    public:
        void Decide_Action(Action& curturnAction, const vector<BattleChar>& heroes);
public:
	static Shared<Monster> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* arg) override;
	virtual void Free() override;
};

NS_END
