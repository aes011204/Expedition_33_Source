#include "BT_RotateToTarget.h"
#include "AIController.h"
#include "BlackBoard.h"
#include "GameObject.h"


IMPLEMENT_REFLECTION(BT_RotateToTarget)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_BBKEY_VEC3("Target Key", _targetKey);
    PROPERTY_FLOAT("speed", _speed);

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
   // Get_Owner()->Request_Move(target);
    _prevPos = target;
    // _started = true;
}


void BT_RotateToTarget::OnFinish()
{
    //Get_Owner()->Stop_Move();
    //_started = false;
}

BNode::NODE_STATE BT_RotateToTarget::Logic_Update(float deltaTime)
{

    auto bb = Get_Owner()->Get_BlackBoard();


    Vec3 target = bb->Get_Value<Vec3>(_targetKey);
    auto ownerTransform = Get_Owner()->Get_Owner()->Get_Transform();
    Vec3 faceTarget = target;
    faceTarget.y = ownerTransform->Get_Position().y;


    Vec3 dir = faceTarget - ownerTransform->Get_Position();
    dir.y = 0.f;

    if(dir.LengthSquared() > 0.0001f)
    {
        dir.Normalize();

        const float targetYawRad = atan2f(dir.x, dir.z);
        const Quat currentRot = ownerTransform->Get_RotationQuaternion();

        const Quat targetRot = Quat::CreateFromYawPitchRoll(targetYawRad, 0.f, 0.f);

        const float t = std::clamp(deltaTime * _speed, 0.f, 1.f);
        const Quat smoothRot = Quaternion::Slerp(currentRot, targetRot, t);


        ownerTransform->Set_RotationQuaternion(smoothRot);
    }

//    if ((faceTarget - ownerTransform->Get_Position()).LengthSquared() > 0.0001f)
//    {
//        Get_Owner()->Get_Owner()->Get_Transform()->LookAt(faceTarget);
//    }



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
