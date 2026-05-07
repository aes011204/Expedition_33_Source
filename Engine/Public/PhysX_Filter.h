#pragma once

#include "Collision_Define.h"

NS_BEGIN(Engine)

// ColliderCom이 넣어둔 channel / overlap / block 정보를 피직스 Filter Data 구조로 바꾸기
PxFilterData Build_FilterData(Collision_Channel channel, uint32 overlapMask, uint32 blockMask);

// 피직스 Filter Data 두 개를 비교해서 Ignore / Overlap / Block 판정 만들기
CollisionResponse Calculate_PhysXResponse(const PxFilterData& src, const PxFilterData& dst);

// 피직스 scene이 pair를 만들 때 호출하는 필터
PxFilterFlags CustomFilterShader(
    PxFilterObjectAttributes attributes0, PxFilterData filterData0,
    PxFilterObjectAttributes attributes1, PxFilterData filterData1,
    PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize);


NS_END
