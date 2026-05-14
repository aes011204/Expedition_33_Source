#include "PhysX_Filter.h"

PxFilterData Engine::Build_FilterData(Collision_Channel channel, uint32 overlapMask, uint32 blockMask)
{
    PxFilterData data{};
    data.word0 = ETOI(channel);
    data.word1 = overlapMask;
    data.word2 = blockMask;
    data.word3 = 0;
    return data;
}

CollisionResponse Engine::Calculate_PhysXResponse(const PxFilterData& src, const PxFilterData& dst)
{
    const Collision_Channel channelSrc = static_cast<Collision_Channel>(src.word0);
    const Collision_Channel channelDst = static_cast<Collision_Channel>(dst.word0);

    return Calculate_ResponseResult(
        channelSrc, src.word1, src.word2,
        channelDst, dst.word1, dst.word2);

}

PxFilterFlags Engine::CustomFilterShader(PxFilterObjectAttributes attributes0, PxFilterData filterData0,
    PxFilterObjectAttributes attributes1, PxFilterData filterData1, PxPairFlags& pairFlags, const void* constantBlock,
    PxU32 constantBlockSize)
{
    UNREFERENCED_PARAMETER(attributes0);
    UNREFERENCED_PARAMETER(attributes1);
    UNREFERENCED_PARAMETER(constantBlock);
    UNREFERENCED_PARAMETER(constantBlockSize);

    pairFlags = PxPairFlags();

    const CollisionResponse response = Calculate_PhysXResponse(filterData0, filterData1);

    if (response == CollisionResponse::Ignore)
    {
        // 이 둘은 아예 충돌 쌍으로 만들지 마라.
        pairFlags = PxPairFlags();
        return PxFilterFlag::eSUPPRESS;
    }

    if (response == CollisionResponse::Overlap)
    {
        pairFlags = PxPairFlag::eDETECT_DISCRETE_CONTACT |
                    PxPairFlag::eNOTIFY_TOUCH_FOUND |
                    PxPairFlag::eNOTIFY_TOUCH_PERSISTS |
                    PxPairFlag::eNOTIFY_TOUCH_LOST;

        return PxFilterFlag::eDEFAULT;
    }

    pairFlags =
        PxPairFlag::eSOLVE_CONTACT |
        PxPairFlag::eDETECT_DISCRETE_CONTACT |
        PxPairFlag::eNOTIFY_TOUCH_FOUND |
        PxPairFlag::eNOTIFY_TOUCH_PERSISTS |
        PxPairFlag::eNOTIFY_TOUCH_LOST;

    return PxFilterFlag::eDEFAULT;
}
