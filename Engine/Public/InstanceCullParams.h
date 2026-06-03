#pragma once
#include "Engine_Define.h"

struct alignas(16) CullParams
{
    Vec4 planes[6];
    uint32 totalCount;
    float maxDist;
    float pad[2];
    Vec3 camPos;
    float pad2;
};
