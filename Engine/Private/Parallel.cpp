
#include "Parallel.h"

Parallel::Parallel()
{
}

Parallel::~Parallel()
{
}

BNode::NODE_STATE Parallel::Update(float deltaTime)
{
    if (_children.empty())
        return Commit_State(BNode::NODE_STATE::FAILED);

    NODE_STATE mainState = BNode::NODE_STATE::FAILED;

    for (size_t i = 0; i < _children.size(); ++i)
    {
        auto& child = _children[i];
        if (child == nullptr)
            continue;

        NODE_STATE state = child->Update(deltaTime);

        if (i == 0)
            mainState = state;
    }

    if (mainState == BNode::NODE_STATE::SUCCESS ||
        mainState == BNode::NODE_STATE::FAILED)
    {
        for (size_t i = 1; i < _children.size(); ++i)
        {
            if (_children[i])
                _children[i]->Abort();
        }
    }

    return Commit_State(mainState);
}

void Parallel::Abort()
{
    for (auto& child : _children)
    {
        if (child)
            child->Abort();
    }
}

Shared<Parallel> Parallel::Create()
{
    return make_shared<Parallel>();
}

void Parallel::Free()
{
    __super::Free();
}
