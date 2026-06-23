#include "BT_SL_CompareBBValue.h"

#include <any>
#include <cmath>

#include "AIController.h"
#include "BlackBoard.h"

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(BT_SL_CompareBBValue)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_STRING("BB Key", _bbKey);
    PROPERTY_ENUM("Value Type", _valueType, BBValueType);
    PROPERTY_ENUM("Compare Op", _compareOp, CompareOp);
    PROPERTY_BOOL("Bool Value", _boolValue);
    PROPERTY_INT("Int Value", _intValue);
    PROPERTY_FLOAT("Float Value", _floatValue);

    return true;
}

BT_SL_CompareBBValue::BT_SL_CompareBBValue()
{
}

bool BT_SL_CompareBBValue::Check_Condition()
{
    const Shared<AIController> owner = Get_Owner();
    if (owner == nullptr)
        return false;

    const Shared<BlackBoard> bb = owner->Get_BlackBoard();
    if (bb == nullptr || _bbKey.empty())
        return false;

    const any* value = bb->Find_Value(_bbKey);
    if (value == nullptr)
        return false;

    try
    {
        switch (_valueType)
        {
        case BBValueType::Bool:
            return CompareBool(any_cast<bool>(*value));

        case BBValueType::Int:
            return CompareInt(any_cast<int>(*value));

        case BBValueType::Float:
            return CompareFloat(any_cast<float>(*value));

        default:
            return false;
        }
    }
    catch (const bad_any_cast&)
    {
        return false;
    }
}

bool BT_SL_CompareBBValue::CompareBool(bool value) const
{
    switch (_compareOp)
    {
    case CompareOp::Equal:
        return value == _boolValue;

    case CompareOp::NotEqual:
        return value != _boolValue;

    case CompareOp::Less:
    case CompareOp::LessEqual:
    case CompareOp::Greater:
    case CompareOp::GreaterEqual:
    default:
        return false;
    }
}

bool BT_SL_CompareBBValue::CompareInt(int value) const
{
    switch (_compareOp)
    {
    case CompareOp::Equal:
        return value == _intValue;

    case CompareOp::NotEqual:
        return value != _intValue;

    case CompareOp::Less:
        return value < _intValue;

    case CompareOp::LessEqual:
        return value <= _intValue;

    case CompareOp::Greater:
        return value > _intValue;

    case CompareOp::GreaterEqual:
        return value >= _intValue;

    default:
        return false;
    }
}

bool BT_SL_CompareBBValue::CompareFloat(float value) const
{
    switch (_compareOp)
    {
    case CompareOp::Equal:
        return fabsf(value - _floatValue) <= 0.0001f;

    case CompareOp::NotEqual:
        return fabsf(value - _floatValue) > 0.0001f;

    case CompareOp::Less:
        return value < _floatValue;

    case CompareOp::LessEqual:
        return value <= _floatValue;

    case CompareOp::Greater:
        return value > _floatValue;

    case CompareOp::GreaterEqual:
        return value >= _floatValue;

    default:
        return false;
    }
}

Shared<BT_SL_CompareBBValue> BT_SL_CompareBBValue::Create()
{
    return make_shared<BT_SL_CompareBBValue>();
}

void BT_SL_CompareBBValue::Free()
{
    Decorator::Free();
}

NS_END
