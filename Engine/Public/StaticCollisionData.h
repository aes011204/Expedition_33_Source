#pragma once

#include "Base.h"
#include "Engine_Model_Struct.h"
#include "CollisionProxy_Types.h"

NS_BEGIN(Engine)

// MapStatic 전용 정적 충돌 데이터
struct ENGINE_DLL StaticCollisionData
{
    wstring cacheKey{}; // triangle cook cache key
    Matrix preLocalTransform = Matrix::Identity;

    vector<ModelCollisionMesh> triangleMeshes;
    vector<ModelCollisionProxy> primitiveProxies;
};


NS_END
