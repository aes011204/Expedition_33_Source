#pragma once

NS_BEGIN(Engine)

// 모델 충돌 타입
enum class ModelCollisionProxyType : uint8
{
    TriangleMesh,
    Box,
    Sphere,
    Capsule,
    Convex
};

struct ModelCollisionProxy
{
    ModelCollisionProxyType type = ModelCollisionProxyType::TriangleMesh;

    string name{};

    // primitive local pos
    Vec3 center = Vec3::Zero;
    Quat rotation = Quat::Identity;

    // box
    Vec3 extents = Vec3::Zero;

    // sphere / capsule
    float radius = 0.f;
    float halfHeight = 0.f;

    // convex
    vector<Vec3> positions;
    vector<uint32> indices;
};

NS_END
