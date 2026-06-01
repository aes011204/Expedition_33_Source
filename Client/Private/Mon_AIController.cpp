#include "Mon_AIController.h"
#include "BlackBoard.h"
#include "BehaviorTree.h"
#include "BT_MoveTo.h"
#include "BT_Chase.h"
#include "BT_Idle.h"
#include "BT_Patrol.h"
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
    CHECK_FAILED(__super::Initialize(arg), E_FAIL);

    auto moveTo = BT_MoveTo::Create();
    auto attack = BT_Chase::Create();
    auto patrol = BT_Patrol::Create();
    auto idle = BT_Idle::Create();

    CHECK_NULL(moveTo, E_FAIL);
    CHECK_NULL(attack, E_FAIL);
    CHECK_NULL(patrol, E_FAIL);
    CHECK_NULL(idle, E_FAIL);

    moveTo->Set_TargetKey("playerPos");

    auto select = Selector::Create();
    CHECK_NULL(select, E_FAIL);

    auto deco = BT_CheckDistance::Create(5.f, BT_CheckDistance::EDistanceCondition::LessThan);
    CHECK_NULL(deco, E_FAIL);
    deco->Set_Child(attack);

    auto deco2 = BT_CheckDistance::Create(5.f, BT_CheckDistance::EDistanceCondition::Between, 20.f);
    CHECK_NULL(deco2, E_FAIL);
    deco2->Set_Child(moveTo);

    select->Add_Child(deco);
    select->Add_Child(deco2);
    select->Add_Child(patrol);
    select->Add_Child(idle);
    auto tree = make_shared<BehaviorTree>();
    tree->Set_Root(select);

    Set_BehaviorTree(tree);

    return S_OK;
}

void Mon_AIController::Update(float timeDelta)
{
    const Shared<GameObject> player = CLIENT->Find_Player(GAME->Current_LevelIndex());
    if (player != nullptr)
    {
        const Vec3 pos = player->Get_Transform()->Get_Position();
        _BB->Set_Value("playerPos", pos);
    }

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
