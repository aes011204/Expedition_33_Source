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
    {
        Abort();
        return BNode::NODE_STATE::FAILED;
    }

    return _child->Update(deltaTime);
}

void Decorator::Abort()
{
    if (_child)
    {
        _child->Abort();
    }
}

//Shared<Decorator> Decorator::Create()
//{
//    auto instance = make_shared<Decorator>();
//
//    /*if (FAILED(instance->Initialize_Prototype()))
//    {
//        MSG_BOX("Failed to Created : Monster");
//        return nullptr;
//    }*/
//
//    return instance;
//}

void Decorator::Free()
{
}
