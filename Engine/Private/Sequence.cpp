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
        {
            if (_runningNode && _runningNode != child)
            {
                _runningNode->Abort();
            }
            _runningNode = nullptr;
            return Commit_State(BNode::NODE_STATE::FAILED);
        }

        if (state == BNode::NODE_STATE::RUN)
        {
            if (_runningNode && _runningNode != child)
            {
                _runningNode->Abort();
            }
            _runningNode = child;
            return Commit_State(BNode::NODE_STATE::RUN);
        }
    }

    if (_runningNode)
    {
        _runningNode->Abort();
        _runningNode = nullptr;
    }

    return Commit_State(BNode::NODE_STATE::SUCCESS);
}

void Sequence::Abort()
{
    if (_runningNode)
    {
        _runningNode->Abort();
        _runningNode = nullptr;
    }
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
