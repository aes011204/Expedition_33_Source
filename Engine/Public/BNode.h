#pragma once
#include"Base.h"
NS_BEGIN(Engine)
class BNode :
    public Base
{
public:
    enum NODE_STATE{ RUN, SUCCESS, FAILED };
public:
    BNode();
    virtual ~BNode();


public:
    virtual NODE_STATE Update(float deltaTime)=0;

    void Set_Owner(weak_ptr<class AIController> aiCtrl) { _AICtrl = aiCtrl; }
    shared_ptr<class AIController> Get_Owner() { return _AICtrl.lock(); }

protected:
    weak_ptr<class AIController> _AICtrl = {};
};

NS_END
