#pragma once
#include "Engine_Define.h"

struct alignas(16) CullParams
{
    Vec4 planes[6];
    uint32 totalCount;
    float pad[3];
};
