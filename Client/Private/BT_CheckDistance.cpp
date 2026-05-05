#include "BT_CheckDistance.h"
#include "AIController.h"
#include "BlackBoard.h"
#include "TransformCom.h"
#include "GameObject.h"

BT_CheckDistance::BT_CheckDistance()
{
}

BT_CheckDistance::~BT_CheckDistance()
{
}

HRESULT BT_CheckDistance::Initialize(float distance, bool isLower)
{
    _distance = distance;
    _isLower = isLower;
    return S_OK;
}

bool BT_CheckDistance::Check_Condition()
{

    auto owner = Get_Owner();
    auto bb = owner->Get_BlackBoard();

    Vec3 playerPos = bb->Get_Value<Vec3>("playerPos");
    Vec3 myPos = Get_Owner()->Get_Owner()->Get_Transform()->Get_Position();

    float dist = (playerPos - myPos).Length();


    if (_isLower == true)
    {
    return dist < _distance;

    }
    else
    {
        return dist > _distance;

    }

}

BNode::NODE_STATE BT_CheckDistance::Update(float deltaTime)
{
    return __super::Update(deltaTime);
}


Shared<BT_CheckDistance> BT_CheckDistance::Create(float distance, bool isLower)
{
    auto instance = make_shared<BT_CheckDistance>();

    
   if (FAILED(instance->Initialize(distance, isLower)))
    {
        //MSG_BOX("Failed to Created : Monster");
        return nullptr;
    }

    return instance;
}

void BT_CheckDistance::Free()
{
}
