#include "PathFollower.h"

PathFollower::PathFollower()
{
}

PathFollower::~PathFollower()
{
}

void PathFollower::Update(float deltaTime, Vec3& position, float speed)
{
    if (_fin)
        return;

    Vec3 dir = _target - position;
    float dist = dir.Length();


    if (dist <= 0.5f)
    {
        //position = _target;
        _fin = true;
        return;
    }

    float moveDist = speed * deltaTime;

    if (dist <= moveDist)
    {
        //position = _target;
        _fin = true;
        return;
    }


   ;

    dir.Normalize();
    position += dir * speed * deltaTime;
}


bool PathFollower::Is_Finished()
{
    return _fin;
}

void PathFollower::Request_Move(Vec3& start, Vec3& target)
{
    //float distToTarget = (target - ownerPos).Length();

    //if (distToTarget <= 1.5f)
    //{
    //    _started = false;
    //    return BNode::NODE_STATE::SUCCESS;
    //}

    _fin = false;
    _target = target;
}

void PathFollower::Stop_Move(const Vec3& currentPos)
{
    _target = currentPos;
    _fin = true;
}

Shared<PathFollower> PathFollower::Create()
{
    auto instance = make_shared<PathFollower>();

    //if (FAILED(instance->Initialize_Prototype()))
    //{
    //    MSG_BOX("Failed to Created : Monster");
    //    return nullptr;
    //}

    return instance;
}

void PathFollower::Free()
{
    __super::Free();
}
