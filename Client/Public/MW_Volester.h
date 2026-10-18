#pragma once
#include "Monster_WorldBase.h"

NS_BEGIN(Client)

class Mon_Volester_Body;
class Mon_WeakPoint;

class MW_Volester :
    public Monster_WorldBase
{
    GENERATED_GAMEOBJECT(MW_Volester, ObjectType::Monster)

public:
    MW_Volester(const ComPtr<Device>& device, const ComPtr<Context>& context);
    MW_Volester(const MW_Volester& prototype);
    virtual ~MW_Volester() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Priority_Update(float timeDelta) override;
    void    Update(float timeDelta) override;
    HRESULT Render() override;
    HRESULT Render_Shadow() override;

public:
    void    OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other) override;

protected:
    HRESULT Ready_Components() override;
    HRESULT Ready_PartObjects();

private:
    Shared<Mon_Volester_Body>      _body = {};
    Shared<Mon_WeakPoint>          _weak = {};

    //MonsterWorldAnim _currentAction = MonsterWorldAnim::Idle;
    //MonsterWorldAnim _previousAction = MonsterWorldAnim::Idle;

public:
    //void Set_CurrentAction(MonsterWorldAnim action) { _currentAction = action; }
    //MonsterWorldAnim Get_CurrentAction() const { return _currentAction; }

public:
    static Shared<MW_Volester> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
