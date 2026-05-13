#pragma once

#include "Engine_Define.h"

NS_BEGIN(Engine)

struct RenderPerfSummary
{
    bool valid{};
    uint32 renderCallCount{};

    float renderAvgMs{};
    float renderMaxMs{};

    float priorityAvgMs{};
    float priorityMaxMs{};

    float shadowAvgMs{};
    float shadowMaxMs{};

    float nonBlendAvgMs{};
    float nonBlendMaxMs{};

    float lightsAvgMs{};
    float lightsMaxMs{};

    float combinedAvgMs{};
    float combinedMaxMs{};

    float nonLightAvgMs{};
    float nonLightMaxMs{};

    float blendAvgMs{};
    float blendMaxMs{};

    float uiAvgMs{};
    float uiMaxMs{};

    float debugAvgMs{};
    float debugMaxMs{};
};

struct UpdatePerfSummary
{
    bool valid{};
    uint32 updateFrameCount{};

    float updateAvgMs{};
    float updateMaxMs{};

    float debugMgrAvgMs{};
    float debugMgrMaxMs{};

    float inputMgrAvgMs{};
    float inputMgrMaxMs{};

    float printMgrAvgMs{};
    float printMgrMaxMs{};

    float clientAvgMs{};
    float clientMaxMs{};

    float levelAvgMs{};
    float levelMaxMs{};

    float priorityObjAvgMs{};
    float priorityObjMaxMs{};

    float pipelineAvgMs{};
    float pipelineMaxMs{};

    float objAvgMs{};
    float objMaxMs{};

    float physxBeginAvgMs{};
    float physxBeginMaxMs{};

    float collClearAvgMs{};
    float collClearMaxMs{};

    float lateObjAvgMs{};
    float lateObjMaxMs{};

    float physxUpdateAvgMs{};
    float physxUpdateMaxMs{};

    float physxGatherAvgMs{};
    float physxGatherMaxMs{};

    float collFromPhysxAvgMs{};
    float collFromPhysxMaxMs{};

    float cameraAvgMs{};
    float cameraMaxMs{};

    float uiAvgMs{};
    float uiMaxMs{};
};

struct CollisionPerfSummary
{
    bool valid{};
    uint32 updateFrameCount{};
    float activeCollidersAvg{};
    float pairsPerSec{};
    float intersectsPerSec{};
    float hitsPerSec{};
    float updateAvgMs{};
    float updateMaxMs{};
};

struct RuntimePerfSummary
{
    float windowSec{};
    float approxFps{};
    RenderPerfSummary render{};
    UpdatePerfSummary update{};
    CollisionPerfSummary collision{};
};

struct PerfCounter
{
    size_t sampleCount{};
    double accumulatedMs{};
    double maxMs{};
};

struct UpdatePerfWindow
{
    PerfCounter update{};
    PerfCounter debugMgr{};
    PerfCounter inputMgr{};
    PerfCounter printMgr{};
    PerfCounter client{};
    PerfCounter level{};
    PerfCounter priorityObj{};
    PerfCounter pipeline{};
    PerfCounter obj{};
    PerfCounter physxBegin{};
    PerfCounter collClear{};
    PerfCounter lateObj{};
    PerfCounter physxUpdate{};
    PerfCounter physxGather{};
    PerfCounter collFromPhysx{};
    PerfCounter camera{};
    PerfCounter ui{};
};

    NS_BEGIN(LogUtils)
    inline void Accumulate_PassPerf(PerfCounter& counter, const double passMs)
    {
        ++counter.sampleCount;
        counter.accumulatedMs += passMs;
        counter.maxMs = max(counter.maxMs, passMs);
    }

    inline void Reset_UpdatePerfWindow(UpdatePerfWindow& window)
    {
        window = {};
    }

    inline double Safe_Divide(const double numerator, const double denominator)
    {
        if (denominator <= 0.0)
            return 0.0;

        return numerator / denominator;
    }
    NS_END


NS_END
