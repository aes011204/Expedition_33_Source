#include "Task.h"

Task::Task()
{
}

Task::~Task()
{
}


BNode::NODE_STATE Task::Update(float deltaTime)
{
    if (!_started)
    {
        OnStart();
        _started = true;
    }

    NODE_STATE state = Logic_Update(deltaTime);

    if (state != RUN)
    {
        OnFinish();
        _started = false;
    }

    return state;
}

