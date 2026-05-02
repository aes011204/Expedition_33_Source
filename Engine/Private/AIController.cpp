#include "AIController.h"
#include "Service.h"
#include "PathFollower.h"
#include "BehaviorTree.h"

AIController::AIController(const ComPtr<Device>& device, const ComPtr<Context>& context)
    :Component(device, context)
{
}

AIController::~AIController()
{
}

void AIController::Update(float timeDelta)
{
    _testPos = Vec3{ 10.f, 0, 6.f };
    _PF->Update(timeDelta, _testPos, 3.f);

    for (auto& service : _services)
    {
        service->Update(timeDelta);
    }

    _BT->Update(timeDelta);
}
void AIController::SetBehaviorTree(shared_ptr<BehaviorTree> tree)
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
    _PF->Request_Move(_testPos,target);

}

