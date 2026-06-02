#pragma once

#include "EffectInstancePool.h"

namespace Client
{
    class Player_WorldBase;

    struct HeroPartyEntry
    {
        HeroId  heroId = HeroId::Gustave;
        Shared<Player_WorldBase> hero;
    };

    struct HeroSpawnDesc
    {
        HeroId heroId = HeroId::Gustave;
        const char* worldPrefabName = "";
        const char* battlePrefabName = "";
    };

    // Player_WorldBase Hair Physics
    struct HairChainPhysicsDesc
    {
        HairPhysicsProfile profile = HairPhysicsProfile::Lune;

        string bonePrefix = "Strand_";

        // ABP_PostProcess_Hair_LUNE_Rebelious 에서 추출한 값
        float stiffness = 0.05f;
        float damping = 0.1f;
        float radius = 3.f;

        float windScale = 0.35f;

        Vec3 gravity = Vec3(0.f, -2.5f, 0.f);

        // 캐릭터 이동방향 반대로 주는 힘
        float movementInertiaScale = 0.08f;
        float maxMovementInertia = 1.5f;

        // 한 프레임에 move inertia가 node를 너무 많이 밀지못하게 제한
        float maxMovementOffset = 0.035f;

        // 멈췄을 때 원래 parent -> child bind 방향으로 돌아오게 하는 힘.
        float poseRestoreStrength = 0.18f;

        // bind 방향에서 너무 많이 꺾이지 않게 제한한다.
        float maxBendAngleDegree = 28.f;

        // 큰 이동/순간이동 시 이전 위치를 버리고 현재 pose로 리셋하기 위한 기준.
        float teleportDistance = 300.f;

        // Verlet constraint 반복 횟수
        uint32 solverIterations = 3;
    };

    struct PlayEffectDesc
    {
        wstring effectName{};
        wstring layerTag = L"Layer_Effect";

        // attach가 아니라, spawn 순간 local transform 계산에만 쓰는 기준 오브젝트다.
        Shared<GameObject> spawnBasisTarget{};

        Vec3 worldPosition  = Vec3::Zero;
        Vec3 localOffset    = Vec3::Zero;
        Vec3 localRotation  = Vec3::Zero;
        Vec3 scale          = Vec3::One;
    };

}
