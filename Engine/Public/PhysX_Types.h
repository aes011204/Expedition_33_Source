#pragma once

#include "Base.h"
#include "Collision_Define.h"
#include "PhysX_Handle.h"

NS_BEGIN(Engine)

class ColliderCom;
//class GameObject;

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

    ColliderCom* collider = nullptr; // 맞은 충돌체를 바로 Battle 쪽에서 actor로 역추적하기 위해 추가
    GameObject*  owner = nullptr;    // collider owner를 바로 쓰기 위한 캐시
};

struct CharacterControllerDesc
{
    float radius = 0.32f;
    float height = 1.50f;              // 전체 캡슐 높이 기준
    float stepOffset = 0.35f;
    float contactOffset = 0.03f;
    float slopeLimitDeg = 49.0f;       // walkableNormalY=0.65와 맞춘 기본값
    float groundLerpSpeed = 15.0f;     // transform 반영 시 Y 보간 속도
    Collision_Preset preset = Collision_Preset::Player_Body;
};

struct CharacterControllerMoveResult
{
    Vec3 footPosition = Vec3::Zero;    // CCT는 발 위치 기준으로 읽고 쓴다
    bool collisionDown = false;
    bool collisionSide = false;
    bool collisionUp = false;
};

NS_END
