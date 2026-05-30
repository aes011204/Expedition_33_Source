#include "BT_IsStun.h"
#include "AIController.h"
#include "BlackBoard.h"
#include "TransformCom.h"
#include "GameObject.h"
IMPLEMENT_REFLECTION(BT_IsStun)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();


    PROPERTY_BBKEY_BOOL("isStun", _isStun);

    return true;
}
BT_IsStun::BT_IsStun()
{
}

BT_IsStun::~BT_IsStun()
{
}


bool BT_IsStun::Check_Condition()
{

    const auto bb = Get_Owner()->Get_BlackBoard();
    if (!bb || !bb->Has_Value(_isStun))
        return false;

    const bool isStun = bb->Get_Value<bool>(_isStun);
    if (!isStun)
        return false;

    Get_Owner()->Stop_Move();
    return true;
}

BNode::NODE_STATE BT_IsStun::Update(float deltaTime)
{
    return __super::Update(deltaTime);
}


Shared<BT_IsStun> BT_IsStun::Create()
{
    auto instance = make_shared<BT_IsStun>();

    return instance;
}

void BT_IsStun::Free()
{
}
