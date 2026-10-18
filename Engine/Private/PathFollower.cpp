#include "PathFollower.h"

PathFollower::PathFollower()
{
}

PathFollower::~PathFollower()
{
}

bool PathFollower::Update(const Vec3& position, float reachDistance, Vec3& outWorldDir)
{
    outWorldDir = Vec3::Zero;

    if (_fin)
        return false;

    Vec3 dir = _target - position;
    dir.y = 0.f;

    const float dist = dir.Length();
    if (dist <= reachDistance)
    {
        _fin = true;
        return false;
    }

    dir.Normalize();
    outWorldDir = dir;
    return true;
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
