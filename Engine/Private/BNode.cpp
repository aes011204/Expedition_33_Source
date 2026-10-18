#include "BNode.h"
#include "AIController.h"
#include "GameObject.h"

BNode::BNode()
{
}

BNode::~BNode()
{
}

BNode::NODE_STATE Update(float deltaTime)
{
    return BNode::NODE_STATE();
}
void BNode::Set_Owner(weak_ptr<class AIController> aiCtrl)
{
    auto test = aiCtrl.lock();   // 여기 되는지
    _AICtrl = aiCtrl;
    
    _owner = dynamic_pointer_cast<GameObject>(aiCtrl.lock()->Get_Owner());
}
