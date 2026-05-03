#pragma once
#include "Task.h"
class ENGINE_DLL BT_MoveTo :
    public Task
{
public:

    BT_MoveTo();
    virtual ~BT_MoveTo();

public:
    virtual NODE_STATE Update(float deltaTime);

    void Set_Target(Vec3 target) { _target = target; };
private:
    bool _started = false;


    Vec3 _target = {};
public:

};

