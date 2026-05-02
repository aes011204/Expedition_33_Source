#include "Decorator.h"

Decorator::Decorator()
{
}

Decorator::~Decorator()
{
}

bool Decorator::Check_Condition()
{
    return false;
}

BNode::NODE_STATE Decorator::Update(float deltaTime)
{

    if (Check_Condition() == false)
        return BNode::NODE_STATE::FAILED;

    return _child->Update(deltaTime);
}
