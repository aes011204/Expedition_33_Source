#pragma once
#include "Component.h"
NS_BEGIN(Engine)

class AIController :
    public Component
{
public:

    AIController(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual ~AIController();
public:
    void Update(float timeDelta);
public:
    shared_ptr<class BlackBoard> Get_BlackBoard() { return _BB; }
    void SetBehaviorTree(shared_ptr<class BehaviorTree> tree);

    bool Is_Finished();
    void Request_Move(Vec3& target);

private:
    shared_ptr<class BlackBoard> _BB = {};
    shared_ptr<class BehaviorTree> _BT = {};
    shared_ptr<class PathFollower> _PF = {};

    vector<shared_ptr<class Service>> _services;

    Vec3 _testPos = {};
};

NS_END
