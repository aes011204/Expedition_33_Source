#pragma once
#include "BNode.h"

//블랙보드 업데이트 해주는 역할 노드 업데이트는 안씀
NS_BEGIN(Engine)
class Service :
    public BNode
{
public:

    Service();
    virtual ~Service();
public:
    virtual void Service_Update(float deltaTime) = 0;;
    
    virtual NODE_STATE Update(float deltaTime) { return BNode::NODE_STATE::SUCCESS; };

};

NS_END
