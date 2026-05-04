#include "Selector.h"

Selector::Selector()
{
}

Selector::~Selector()
{
}

BNode::NODE_STATE Selector::Update(float deltaTime)
{
    for (auto& child : _children)
    {
        BNode::NODE_STATE state = child->Update(deltaTime);

        if (state == BNode::NODE_STATE::SUCCESS)
        {
            if (_runningNode && _runningNode != child)
            {
                _runningNode->Abort();
            }
                _runningNode = nullptr;
            return BNode::NODE_STATE::SUCCESS;
        }

        if (state == BNode::NODE_STATE::RUN)
        {
            if (_runningNode && _runningNode != child)
            {
                _runningNode->Abort();
               
            }
            _runningNode = child;
            return BNode::NODE_STATE::RUN;
        }
    }


    if (_runningNode)
    {
        _runningNode->Abort();
        _runningNode = nullptr;
    }
    
    return BNode::NODE_STATE::FAILED;
}

void Selector::Abort()
{
    if (_runningNode)
    {
    _runningNode->Abort();
    _runningNode = nullptr;
    }
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
