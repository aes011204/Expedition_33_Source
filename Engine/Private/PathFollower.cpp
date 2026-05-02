#include "PathFollower.h"

PathFollower::PathFollower()
{
}

PathFollower::~PathFollower()
{
}

void PathFollower::Update(float deltaTime, Vec3& position, float speed)
{
    Vec3 dir = _target - position;
    dir.Normalize();
    float dist = dir.Length();


    if (dist < 0.1f)
    {

        _fin = true;
    }

    _fin = false;

    position += dir * speed * deltaTime;
}


bool PathFollower::Is_Finished()
{
    return _fin;
}

void PathFollower::Request_Move(Vec3& start, Vec3& target)
{
    _fin = false;
    _target = target;
}
