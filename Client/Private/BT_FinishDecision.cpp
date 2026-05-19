#include "BT_FinishDecision.h"

#include "AIController.h"
#include "BlackBoard.h"

IMPLEMENT_REFLECTION(BT_FinishDecision)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    return true;
}

BT_FinishDecision::BT_FinishDecision()
{
}

BT_FinishDecision::~BT_FinishDecision()
{
}

void BT_FinishDecision::OnStart()
{
}

BNode::NODE_STATE BT_FinishDecision::Logic_Update(float deltaTime)
{
    const Shared<BlackBoard> bb = _AICtrl.lock()->Get_BlackBoard();
    CHECK_NULL(bb, BNode::NODE_STATE::FAILED);

    bb->Set_Value("DecisionComplete", true);
    return BNode::NODE_STATE::SUCCESS;
}

void BT_FinishDecision::OnFinish()
{
}

Shared<BT_FinishDecision> BT_FinishDecision::Create()
{
    return make_shared<BT_FinishDecision>();
}

void BT_FinishDecision::Free()
{
    Task::Free();
}
