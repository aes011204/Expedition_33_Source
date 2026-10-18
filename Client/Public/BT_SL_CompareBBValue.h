#pragma once

#include "Decorator.h"

NS_BEGIN(Client)

class BT_SL_CompareBBValue final : public Decorator
{
    GENERATED_BTNODE(BT_SL_CompareBBValue, "Decorator")

public:
    enum class BBValueType
    {
        Bool,
        Int,
        Float,
    };

    enum class CompareOp
    {
        Equal,
        NotEqual,
        Less,
        LessEqual,
        Greater,
        GreaterEqual,
    };

public:
    BT_SL_CompareBBValue();
    ~BT_SL_CompareBBValue() override = default;

public:
    bool Check_Condition() override;

private:
    bool CompareBool(bool value) const;
    bool CompareInt(int value) const;
    bool CompareFloat(float value) const;

private:
    string _bbKey = "";
    BBValueType _valueType = BBValueType::Bool;
    CompareOp _compareOp = CompareOp::Equal;
    bool _boolValue = true;
    int _intValue = 0;
    float _floatValue = 0.f;

public:
    static Shared<BT_SL_CompareBBValue> Create();
    void Free() override;
};

NS_END
