#include "Mon_AIController.h"
#include "BlackBoard.h"
#include "BehaviorTree.h"
#include "BT_MoveTo.h"
#include "BT_Attack.h"
#include "BT_Idle.h"
#include "BT_CheckDistance.h"
#include "Selector.h"
#include "Decorator.h"
#include "PathFollower.h"
#include "MW_Lancelier.h"
#include "ClientInstance.h"


Mon_AIController::Mon_AIController(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : AIController(device, context)
{
}

Mon_AIController::Mon_AIController(const Mon_AIController& prototype)
    :AIController(prototype)
{
}

HRESULT Mon_AIController::Initialize_Prototype()
{
    return S_OK;
}

HRESULT Mon_AIController::Initialize(void* arg)
{
    auto moveTo = BT_MoveTo::Create();
    auto idle = BT_Idle::Create();
    auto attack = BT_Attack::Create();

    auto select = Selector::Create();

    auto deco = BT_CheckDistance::Create(5.f, BT_CheckDistance::EDistanceCondition::LessThan);
    deco->Set_Child(attack);

    auto deco2 = BT_CheckDistance::Create(5.f, BT_CheckDistance::EDistanceCondition::Between, 20.f);
    deco2->Set_Child(moveTo);

    select->Add_Child(deco);
    select->Add_Child(deco2);
    select->Add_Child(idle);
    auto tree = make_shared<BehaviorTree>();
    tree->Set_Root(select);

    Set_BehaviorTree(tree);

    _BB = BlackBoard::Create();
    _PF = PathFollower::Create();

    return S_OK;
}

void Mon_AIController::Update(float timeDelta)
{
   auto pos =  CLIENT->Find_Player(GAME->Current_LevelIndex())->Get_Transform()->Get_Position();
    _BB->Set_Value("playerPos", pos);

    __super::Update(timeDelta);
}

Shared<Mon_AIController> Mon_AIController::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Mon_AIController>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : Monster");
        return nullptr;
    }

    return instance;
}

Shared<Component> Mon_AIController::Clone(void* arg)
{
    auto clone = make_shared<Mon_AIController>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : Monster");
        return nullptr;
    }

    return clone;
}

void Mon_AIController::Free()
{
    __super::Free();
}
