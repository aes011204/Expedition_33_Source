#include "BT_RotateToTarget.h"
#include "AIController.h"
#include "BlackBoard.h"
#include "GameObject.h"


IMPLEMENT_REFLECTION(BT_RotateToTarget)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_BBKEY_VEC3("Target Key", _targetKey);

    return true;
}


BT_RotateToTarget::BT_RotateToTarget()
{
}

BT_RotateToTarget::~BT_RotateToTarget()
{
}

void BT_RotateToTarget::OnStart()
{
    auto bb = Get_Owner()->Get_BlackBoard();

    Vec3 target = bb->Get_Value<Vec3>(_targetKey);
    Get_Owner()->Request_Move(target);
    _prevPos = target;
    // _started = true;
}


void BT_RotateToTarget::OnFinish()
{
    Get_Owner()->Stop_Move();
    //_started = false;
}

BNode::NODE_STATE BT_RotateToTarget::Logic_Update(float deltaTime)
{

    auto bb = Get_Owner()->Get_BlackBoard();


    Vec3 target = bb->Get_Value<Vec3>(_targetKey);
    auto owner = Get_Owner()->Get_Owner()->Get_Transform();
    Vec3 faceTarget = target;
    faceTarget.y = owner->Get_Position().y;



    if ((faceTarget - owner->Get_Position()).LengthSquared() > 0.0001f)
    {
        Get_Owner()->Get_Owner()->Get_Transform()->LookAt(faceTarget);
    }



    if (Get_Owner()->Is_Finished())
    {
        return BNode::NODE_STATE::SUCCESS;
    }


    return BNode::NODE_STATE::RUN;

}

Shared<BT_RotateToTarget> BT_RotateToTarget::Create()
{
    auto instance = make_shared<BT_RotateToTarget>();

    /*if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : Monster");
        return nullptr;
    }*/

    return instance;
}

void BT_RotateToTarget::Free()
{
    __super::Free();
}
