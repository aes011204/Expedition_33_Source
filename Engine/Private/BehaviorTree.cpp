#include "BehaviorTree.h"
#include "AIController.h"
#include "BNode.h"
#include "Composite.h"
#include "Decorator.h"

BehaviorTree::BehaviorTree()
{
}

BehaviorTree::~BehaviorTree()
{
}

void BehaviorTree::Update(float deltaTime)
{
    if (_root == nullptr)
        return;

    ++_debugTick;
    SetDebugTickRecursive(_root, _debugTick);
    _root->Update(deltaTime);
}

void BehaviorTree::Bind_Owner(shared_ptr<AIController> owner)
{
    SetOwnerRecursive(_root, owner);
}

void BehaviorTree::SetOwnerRecursive(shared_ptr<BNode> node, shared_ptr<class AIController> owner)
{
    if (!_root)
        return;

    node->Set_Owner(owner);

    if (auto com = dynamic_pointer_cast<Composite>(node))
    {
        for (auto& children : com->Get_Children())
        {
            SetOwnerRecursive(children, owner);
        }
    }

    if (auto De = dynamic_pointer_cast<Decorator>(node))
    {
        SetOwnerRecursive(De->Get_Child(), owner);

    }
}

void BehaviorTree::SetDebugTickRecursive(shared_ptr<BNode> node, uint64 tick)
{
    if (node == nullptr)
        return;

    node->Set_CurrentDebugTick(tick);

    if (auto com = dynamic_pointer_cast<Composite>(node))
    {
        for (auto& child : com->Get_Children())
            SetDebugTickRecursive(child, tick);

        return;
    }

    if (auto de = dynamic_pointer_cast<Decorator>(node))
        SetDebugTickRecursive(de->Get_Child(), tick);
}
