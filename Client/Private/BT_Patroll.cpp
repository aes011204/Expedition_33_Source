#include "BT_Patroll.h"
#include "AIController.h"
#include "MW_Lancelier.h"

IMPLEMENT_REFLECTION(BT_Patroll)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

   // PROPERTY_BBKEY_VEC3("Target Key", _targetKey);

    return true;
}

BT_Patroll::BT_Patroll()
{
}

BT_Patroll::~BT_Patroll()
{
}

void BT_Patroll::OnStart()
{

    Shared<Monster_WorldBase> monster = dynamic_pointer_cast<Monster_WorldBase>(Get_Owner()->Get_Owner());
    monster->Set_CurrentAction(MW_Lancelier::MonsterWorldAnim::Patrol);
}

BNode::NODE_STATE BT_Patroll::Logic_Update(float deltaTime)
{
    return BNode::NODE_STATE::RUN;
}

void BT_Patroll::OnFinish()
{
}

Shared<BT_Patroll> BT_Patroll::Create()
{
    auto instance = make_shared<BT_Patroll>();

    /*if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : MW_Lancelier");
        return nullptr;
    }*/

    return instance;
}

void BT_Patroll::Free()
{
    __super::Free();
}
