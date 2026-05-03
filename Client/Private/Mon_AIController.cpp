#include "Mon_AIController.h"
#include "BlackBoard.h"
#include "BehaviorTree.h"
#include "BT_MoveTo.h"


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
    // behavior tree 연결

    //임시 밖에서 넣어 줘야 함
    auto moveTo = make_shared<BT_MoveTo>();

    auto tree = make_shared<BehaviorTree>();
    tree->Set_Root(moveTo);

    Set_BehaviorTree(tree);


    // blackboard 에 값넣기 
    _BB = BlackBoard::Create();
    _BB->Set_Value("PlayerPos", Vec3(0.f, 0.f, 0.f));
    return S_OK;
}

void Mon_AIController::Update(float timeDelta)
{



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
