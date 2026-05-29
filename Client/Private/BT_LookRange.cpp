#include "BT_LookRange.h"
#include "AIController.h"
#include "BlackBoard.h"
#include "TransformCom.h"
#include "GameObject.h"
IMPLEMENT_REFLECTION(BT_LookRange)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("Range", _range, 0.f, 360.f, 0.1f);
    PROPERTY_BBKEY_VEC3("Target Pos Key", _targetPosKey);
    return true;
}

BT_LookRange::BT_LookRange()
{
}

BT_LookRange::~BT_LookRange()
{
}


bool BT_LookRange::Check_Condition()
{

    auto owner = Get_Owner();
    auto bb = owner->Get_BlackBoard();

    Vec3 playerPos = bb->Get_Value<Vec3>(_targetPosKey);
    Vec3 myPos = Get_Owner()->Get_Owner()->Get_Transform()->Get_Position();

    Vec3 forward = Get_Owner()->Get_Owner()->Get_Transform()->Get_LocalForward();
    forward.y = 0.f;
    forward.Normalize();

    Vec3 dir = playerPos - myPos;
    dir.y = 0.f;
    dir.Normalize();

    const float dot = forward.Dot(dir);
    const float limit = cosf(XMConvertToRadians(90.f * 0.5f)); // cos(45도)


    if(dot >= limit)
    {
        return true;
    }
    else
    {
        return false;

    }

}

BNode::NODE_STATE BT_LookRange::Update(float deltaTime)
{
    return __super::Update(deltaTime);
}


Shared<BT_LookRange> BT_LookRange::Create()
{
    auto instance = make_shared<BT_LookRange>();

    return instance;
}

void BT_LookRange::Free()
{
}
