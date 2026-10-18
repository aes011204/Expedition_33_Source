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

    if (state != BTNodeState::RUN)
    {
        OnFinish();
        _started = false;
    }

    return Commit_State(state);
}

void Task::Abort()
{
    if (_started)
    {
        OnFinish();
        _started = false;
    }
}

