#include "Selector.h"

Selector::Selector()
{
}

Selector::~Selector()
{
}

BNode::NODE_STATE Selector::Update(float deltaTime)
{
    for (auto child : _children)
    {
        BNode::NODE_STATE state = child->Update(deltaTime);

        if (state == BNode::NODE_STATE::SUCCESS)
            return BNode::NODE_STATE::SUCCESS;

        if (state == BNode::NODE_STATE::RUN)
            return BNode::NODE_STATE::RUN;
    }


    return BNode::NODE_STATE::FAILED;
}



Shared<Selector> Selector::Create()
{
    auto instance = make_shared<Selector>();

    /*if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : Monster");
        return nullptr;
    }*/

    return instance;
}

void Selector::Free()
{
}
