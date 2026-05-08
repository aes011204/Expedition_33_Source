#include "BT_MoveTo.h"
#include "AIController.h"
#include "BlackBoard.h"
#include "GameObject.h"


IMPLEMENT_REFLECTION(BT_MoveTo)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_BBKEY_VEC3("Target Key", _targetKey);

    return true;
}


BT_MoveTo::BT_MoveTo()
{
}

BT_MoveTo::~BT_MoveTo()
{
}

void BT_MoveTo::OnStart()
{
    auto bb = Get_Owner()->Get_BlackBoard();

    Vec3 target = bb->Get_Value<Vec3>(_targetKey);
    Get_Owner()->Request_Move(target);
    _prevPos = target;
   // _started = true;
}


void BT_MoveTo::OnFinish()
{
    Get_Owner()->Stop_Move();
    //_started = false;
}

BNode::NODE_STATE BT_MoveTo::Logic_Update(float deltaTime)
{

    auto bb = Get_Owner()->Get_BlackBoard();


    Vec3 target = bb->Get_Value<Vec3>(_targetKey);
    auto ownerPos = Get_Owner()->Get_Owner()->Get_Transform()->Get_Position();

    float distToTarget = (target - ownerPos).Length();

    if (distToTarget <= 1.5f)
    {
        //_started = false;
       
        return BNode::NODE_STATE::SUCCESS;
    }

    //_started
    /*if (!_started)
    {
        Get_Owner()->Request_Move(target);
        _prevPos = target;
        _started = true;
    }*/
    if ((_prevPos - target).Length() >= 1.f)
    {
        Get_Owner()->Request_Move(target);
        _prevPos = target;

    }

    //Get_Owner()->Get_Owner()->Get_Transform()->LookAt(target); 클라에서 



    if (Get_Owner()->Is_Finished())
    {

       
        return BNode::NODE_STATE::SUCCESS;
    }


    return BNode::NODE_STATE::RUN;

}

Shared<BT_MoveTo> BT_MoveTo::Create()
{
    auto instance = make_shared<BT_MoveTo>();

    /*if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : Monster");
        return nullptr;
    }*/

    return instance;
}

void BT_MoveTo::Free()
{
    __super::Free();
}
