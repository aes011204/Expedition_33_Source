#pragma once
#include "Component.h"
NS_BEGIN(Engine)

class ENGINE_DLL AIController :
    public Component
{

    
public:

    AIController(const ComPtr<Device>& device, const ComPtr<Context>& context);
    AIController(const AIController& prototype);
    virtual ~AIController();
public:
    void Update(float timeDelta);
public:
    shared_ptr<class BlackBoard> Get_BlackBoard() { return _BB; }
    void Set_BehaviorTree(shared_ptr<class BehaviorTree> tree);

    bool Is_Finished();
    void Request_Move(Vec3& target);
    void Stop_Move();

protected:
    shared_ptr<class BlackBoard> _BB = {};
    shared_ptr<class BehaviorTree> _BT = {};
    shared_ptr<class PathFollower> _PF = {};

    vector<shared_ptr<class Service>> _services;

    Vec3 _testPos = {};


    virtual Shared<Component> Clone(void* arg) = 0;
    virtual void Free() override;

};

NS_END
