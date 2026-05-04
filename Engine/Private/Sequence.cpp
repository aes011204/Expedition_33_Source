#include "Sequence.h"

Sequence::Sequence()
{
}

Sequence::~Sequence()
{
}

BNode::NODE_STATE Sequence::Update(float deltaTime)
{
    for (auto child : _children)
    {
        BNode::NODE_STATE state = child->Update(deltaTime);

        if (state == BNode::NODE_STATE::FAILED)
            return BNode::NODE_STATE::FAILED;

        if (state == BNode::NODE_STATE::RUN)
            return BNode::NODE_STATE::RUN;
    }


    return BNode::NODE_STATE::SUCCESS;
}


Shared<Sequence> Sequence::Create()
{
    auto instance = make_shared<Sequence>();

    //if (FAILED(instance->Initialize_Prototype()))
    //{
    //    MSG_BOX("Failed to Created : Monster");
    //    return nullptr;
    //}

    return instance;
}

void Sequence::Free()
{
    __super::Free();
}
