#include "BT_Chase.h"
#include "AIController.h"
#include "MW_Lancelier.h"
#include "BlackBoard.h"

IMPLEMENT_REFLECTION(BT_Chase)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_BBKEY_VEC3("Target Key", _targetKey);

    return true;
}

BT_Chase::BT_Chase()
{
}

BT_Chase::~BT_Chase()
{
}

void BT_Chase::OnStart()
{

    const Shared<Monster_WorldBase> monster = dynamic_pointer_cast<Monster_WorldBase>(Get_Owner()->Get_Owner());
    if (monster)
        monster->Set_DesiredAction(Monster_WorldBase::MonsterWorldAnim::Chase);

    const auto bb = Get_Owner()->Get_BlackBoard();
    if (!bb || _targetKey.empty() || !bb->Has_Value(_targetKey))
    {
        LOG_WARN("BT_Chase::OnStart missing target key='{}'", _targetKey);
        return;
    }

    Vec3 target = bb->Get_Value<Vec3>(_targetKey);
    Get_Owner()->Request_Move(target);
    _prevPos = target;

    LOG_DEBUG("BT_Chase RequestMove target=({}, {}, {})", target.x, target.y, target.z);

    // _started = true;
}

BNode::NODE_STATE BT_Chase::Logic_Update(float deltaTime)
{
    const auto bb = Get_Owner()->Get_BlackBoard();
    if (!bb || _targetKey.empty() || !bb->Has_Value(_targetKey))
    {
        LOG_WARN("BT_Chase::Logic_Update missing target key='{}'", _targetKey);
        return BNode::NODE_STATE::FAILED;
    }

    Vec3 target = bb->Get_Value<Vec3>(_targetKey);
    const auto ownerPos = Get_Owner()->Get_Owner()->Get_Transform()->Get_Position();
    Vec3 faceTarget = target;
    faceTarget.y = ownerPos.y;

    float distToTarget = (target - ownerPos).Length();

    if ((_prevPos - target).Length() >= 1.f)
    {
        Get_Owner()->Request_Move(target);
        _prevPos = target;
    }

    //if (Get_Owner()->Is_Finished())
    //{
    //    return BNode::NODE_STATE::SUCCESS;
    //}

    return BNode::NODE_STATE::RUN;
}

void BT_Chase::OnFinish()
{
    Get_Owner()->Stop_Move();
}

Shared<BT_Chase> BT_Chase::Create()
{
    auto instance = make_shared<BT_Chase>();

    /*if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : MW_Lancelier");
        return nullptr;
    }*/

    return instance;
}

void BT_Chase::Free()
{
    __super::Free();
}
