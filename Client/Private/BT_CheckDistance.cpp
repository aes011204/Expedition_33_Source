#include "BT_CheckDistance.h"
#include "AIController.h"
#include "BlackBoard.h"
#include "TransformCom.h"
#include "GameObject.h"
IMPLEMENT_REFLECTION(BT_CheckDistance)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("Distance", _distance, 0.f, 10000.f, 0.1f);
    PROPERTY_FLOAT("Distance 2", _distance2, 0.f, 10000.f, 0.1f);

    info.properties.push_back(Engine::PropertyInfo{
        "_condition",
        "Condition",
        "_condition",
        Engine::PropertyType::Enum,
        offsetof(SelfType, _condition),
        0.f,
        0.f,
        0.1f,
        { "LessThan", "GreaterThan", "Between" },
        {
            static_cast<int>(BT_CheckDistance::EDistanceCondition::LessThan),
            static_cast<int>(BT_CheckDistance::EDistanceCondition::GreaterThan),
            static_cast<int>(BT_CheckDistance::EDistanceCondition::Between)
        }
        });

    return true;
}
BT_CheckDistance::BT_CheckDistance()
{
}

BT_CheckDistance::~BT_CheckDistance()
{
}

HRESULT BT_CheckDistance::Initialize(float distance, enum EDistanceCondition condition, float distance2 = 0)
{
    _condition = condition;

    _distance = distance;
    _distance2 = distance2;

    return S_OK;
}

bool BT_CheckDistance::Check_Condition()
{

    auto owner = Get_Owner();
    auto bb = owner->Get_BlackBoard();

    Vec3 playerPos = bb->Get_Value<Vec3>("playerPos");
    Vec3 myPos = Get_Owner()->Get_Owner()->Get_Transform()->Get_Position();

    float dist = (playerPos - myPos).Length();


    if (_condition == EDistanceCondition::LessThan)
    {
        return dist < _distance;

    }
    else if (_condition == EDistanceCondition::GreaterThan)
    {
        return dist > _distance;

    }
    else if (_condition == EDistanceCondition::Between)
    {
        return _distance < dist && dist < _distance2;

    }
}

BNode::NODE_STATE BT_CheckDistance::Update(float deltaTime)
{
    return __super::Update(deltaTime);
}


Shared<BT_CheckDistance> BT_CheckDistance::Create(float distance, enum EDistanceCondition condition, float distance2)
{
    auto instance = make_shared<BT_CheckDistance>();


    if (FAILED(instance->Initialize(distance, condition, distance2)))
    {
        //MSG_BOX("Failed to Created : Monster");
        return nullptr;
    }

    return instance;
}

void BT_CheckDistance::Free()
{
}
