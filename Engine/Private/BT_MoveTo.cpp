#include "BT_MoveTo.h"
#include "AIController.h"
#include "BlackBoard.h"

BT_MoveTo::BT_MoveTo()
{
}

BT_MoveTo::~BT_MoveTo()
{
}

BNode::NODE_STATE BT_MoveTo::Update(float deltaTime)
{

    auto bb = Get_Owner()->Get_BlackBoard();
        Vec3 target = bb->Get_Value<Vec3>("playerPos");


        //_started
        if (!_started)
        {
            Get_Owner()->Request_Move(target);
            _started = true;
        }

  


    if (!Get_Owner()->Is_Finished())
    {

    return BNode::NODE_STATE::RUN;
    }


        _started = false;
        return BNode::NODE_STATE::SUCCESS;

}
