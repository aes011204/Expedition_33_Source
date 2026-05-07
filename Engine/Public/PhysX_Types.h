#pragma once

namespace Engine
{
    class ColliderCom;

    // 이번 프레임에 무슨 충돌이 있었는지
    struct PhysXPairEvent
    {
        Weak<ColliderCom> self;
        Weak<ColliderCom> other;

        CollisionResponse response = CollisionResponse::Ignore;

        bool began = false;
        bool staying = false;
        bool ended = false;
    };

    struct PhysXQueryHit
    {
        bool    hit = false;            // Hit가 됐는지?
        float   distance = 0.f;         // 얼마나 가다가 Hit됐는지
        Vec3    position = Vec3::Zero;  // 충돌 지점
        Vec3    normal = Vec3::Up;      // 충돌면 법선벡터

    };
}

