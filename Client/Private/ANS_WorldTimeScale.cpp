#include "ANS_WorldTimeScale.h"
#include "ClientInstance.h"
#include "GameInstance.h"

#include <algorithm>

namespace
{
    struct WorldTimeScaleSection
    {
        uint64 id = 0;
        float worldTimeScale = 1.f;
    };

    uint64 g_nextWorldTimeScaleSectionId = 1;
    float g_worldTimeScaleRestoreBase = 1.f;
    vector<WorldTimeScaleSection> g_activeWorldTimeScaleSections;
}

IMPLEMENT_REFLECTION(ANS_WorldTimeScale)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    PROPERTY_FLOAT("월드 시간", _worldTimeScale);
    PROPERTY_BOOL("프리뷰 이펙트 시간 적용", _scalePreviewEffects);

    return true;
}

void ANS_WorldTimeScale::On_Begin(const AnimNotifyContext& context)
{
    if (context.isCinematic)
        return;

    if (_isApplied)
        return;

    if (g_activeWorldTimeScaleSections.empty())
        g_worldTimeScaleRestoreBase = GAME->Get_WorldTimeScaleRestoreBase();

    _activeSectionId = g_nextWorldTimeScaleSectionId++;
    _isApplied = true;

    g_activeWorldTimeScaleSections.push_back({ _activeSectionId, _worldTimeScale });
    GAME->Set_WorldTimeScale(_worldTimeScale);
}

void ANS_WorldTimeScale::On_Tick(const AnimNotifyContext& context)
{
    UNREFERENCED_PARAMETER(context);
}

void ANS_WorldTimeScale::On_End(const AnimNotifyContext& context)
{
    if (context.isCinematic)
        return;

    if (!_isApplied)
        return;

    auto it = std::remove_if(g_activeWorldTimeScaleSections.begin(), g_activeWorldTimeScaleSections.end(),
        [this](const WorldTimeScaleSection& section)
        {
            return section.id == _activeSectionId;
        });

    g_activeWorldTimeScaleSections.erase(it, g_activeWorldTimeScaleSections.end());

    if (g_activeWorldTimeScaleSections.empty())
        GAME->Set_WorldTimeScale(g_worldTimeScaleRestoreBase);
    else
        GAME->Set_WorldTimeScale(g_activeWorldTimeScaleSections.back().worldTimeScale);

    _activeSectionId = 0;
    _isApplied = false;
}
