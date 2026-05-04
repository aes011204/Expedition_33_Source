#include "AIController.h"
#include "Service.h"
#include "PathFollower.h"
#include "BehaviorTree.h"
#include "GameObject.h"

AIController::AIController(const ComPtr<Device>& device, const ComPtr<Context>& context)
    :Component(device, context)
{
}

AIController::AIController(const AIController& prototype)
    : Component(prototype)
{
}

AIController::~AIController()
{
}

void AIController::Update(float timeDelta)
{

    for (auto& service : _services)
    {
        service->Update(timeDelta);
    }

    _BT->Update(timeDelta);


   Vec3 pos =  Get_Owner()->Get_Transform()->Get_Position();
    _PF->Update(timeDelta, pos, 1.5f);
    Get_Owner()->Get_Transform()->Set_Position(pos);

}

void AIController::Set_BehaviorTree(shared_ptr<BehaviorTree> tree)
{
    _BT = tree;

    if (_BT)
        _BT->Bind_Owner(static_pointer_cast<AIController>(shared_from_this()));
}

bool AIController::Is_Finished()
{
    return _PF->Is_Finished();
}

void AIController::Request_Move(Vec3& target)
{
    Vec3 pos = Get_Owner()->Get_Transform()->Get_Position();
    _PF->Request_Move(pos,target);

}

void AIController::Stop_Move()
{
    Vec3 pos = Get_Owner()->Get_Transform()->Get_Position();
    _PF->Stop_Move(pos);
}

void AIController::Free()
{
    __super::Free();
}


