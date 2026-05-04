#include "Renderer.h"

#include "GameInstance.h"
#include "GameObject.h"

Renderer::Renderer(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : _device{ device }
    , _context{ context }
{
}

HRESULT Renderer::Initialize()
{
    return S_OK;
}

void Renderer::Add_RenderGroup(RenderGroup renderGroup, const Shared<GameObject>& gameObject)
{
    CHECK_NULL(gameObject);

    if (renderGroup < RenderGroup::Priority || renderGroup >= RenderGroup::END)
        return;

    _renderGroups[ETOI(renderGroup)].push_back(gameObject);
}

void Renderer::Draw()
{
#ifdef _DEBUG
    const auto renderBegin = chrono::steady_clock::now();
    double priorityMs = 0.0;
    double nonBlendMs = 0.0;
    double blendMs = 0.0;
    double uiMs = 0.0;

    const auto priorityBegin = chrono::steady_clock::now();
#endif

    Render_Priority();

#ifdef _DEBUG
    priorityMs = chrono::duration<double, milli>(chrono::steady_clock::now() - priorityBegin).count();
    const auto nonBlendBegin = chrono::steady_clock::now();
#endif

    Render_NonBlend();

#ifdef _DEBUG
    nonBlendMs = chrono::duration<double, milli>(chrono::steady_clock::now() - nonBlendBegin).count();
    const auto blendBegin = chrono::steady_clock::now();
#endif

    Render_Blend();

#ifdef _DEBUG
    blendMs = chrono::duration<double, milli>(chrono::steady_clock::now() - blendBegin).count();
    const auto uiBegin = chrono::steady_clock::now();
#endif

    Render_UI();

#ifdef _DEBUG
    uiMs = chrono::duration<double, milli>(chrono::steady_clock::now() - uiBegin).count();
    const double renderMs = chrono::duration<double, milli>(chrono::steady_clock::now() - renderBegin).count();

    Accumulate_RenderPerf(renderMs, priorityMs, nonBlendMs, blendMs, uiMs);
#endif
}

void Renderer::Render_Priority()
{
    for (auto& renderObject : _renderGroups[ETOI(RenderGroup::Priority)])
    {
        Shared<GameObject> gameObject = renderObject.lock();
        if (nullptr != gameObject)
            gameObject->Render();
    }

    _renderGroups[ETOI(RenderGroup::Priority)].clear();
}

void Renderer::Render_NonBlend()
{
    for (auto& renderObject : _renderGroups[ETOI(RenderGroup::NonBlend)])
    {
        Shared<GameObject> gameObject = renderObject.lock();
        if (nullptr != gameObject)
            gameObject->Render();
    }

    _renderGroups[ETOI(RenderGroup::NonBlend)].clear();
}

void Renderer::Render_Blend()
{
    for (auto& renderObject : _renderGroups[ETOI(RenderGroup::Blend)])
    {
        Shared<GameObject> gameObject = renderObject.lock();
        if (nullptr != gameObject)
            gameObject->Render();
    }

    _renderGroups[ETOI(RenderGroup::Blend)].clear();
}

void Renderer::Render_UI()
{
    for (auto& renderObject : _renderGroups[ETOI(RenderGroup::UI)])
    {
        Shared<GameObject> gameObject = renderObject.lock();
        if (nullptr != gameObject)
            gameObject->Render();
    }

    _renderGroups[ETOI(RenderGroup::UI)].clear();
}

#ifdef _DEBUG
bool Renderer::Try_ConsumePerfSummary(const float windowSec, RenderPerfSummary& outSummary)
{
    outSummary = {};

    if (windowSec <= 0.f || 0 == _renderPerfWindow.render.sampleCount)
        return false;

    outSummary.valid = true;
    outSummary.renderCallCount = static_cast<uint32>(_renderPerfWindow.render.sampleCount);
    outSummary.renderAvgMs = static_cast<float>(
        Safe_Divide(_renderPerfWindow.render.accumulatedMs, static_cast<double>(_renderPerfWindow.render.sampleCount)));
    outSummary.renderMaxMs = static_cast<float>(_renderPerfWindow.render.maxMs);
    outSummary.priorityAvgMs = static_cast<float>(
        Safe_Divide(_renderPerfWindow.priority.accumulatedMs, static_cast<double>(_renderPerfWindow.priority.sampleCount)));
    outSummary.nonBlendAvgMs = static_cast<float>(
        Safe_Divide(_renderPerfWindow.nonBlend.accumulatedMs, static_cast<double>(_renderPerfWindow.nonBlend.sampleCount)));
    outSummary.blendAvgMs = static_cast<float>(
        Safe_Divide(_renderPerfWindow.blend.accumulatedMs, static_cast<double>(_renderPerfWindow.blend.sampleCount)));
    outSummary.uiAvgMs = static_cast<float>(
        Safe_Divide(_renderPerfWindow.ui.accumulatedMs, static_cast<double>(_renderPerfWindow.ui.sampleCount)));

    // TODO(RuntimePerf): 아래 future fields는 counter 슬롯만 예약되어 있다.
    // 실제 preview/deferred/shadow/light/combine path가 생기면 accumulate 지점을 먼저 연결한다.
    outSummary.shadowAvgMs = static_cast<float>(
        Safe_Divide(_renderPerfWindow.shadow.accumulatedMs, static_cast<double>(_renderPerfWindow.shadow.sampleCount)));
    outSummary.shadowMaxMs = static_cast<float>(_renderPerfWindow.shadow.maxMs);
    outSummary.lightsAvgMs = static_cast<float>(
        Safe_Divide(_renderPerfWindow.lights.accumulatedMs, static_cast<double>(_renderPerfWindow.lights.sampleCount)));
    outSummary.combinedAvgMs = static_cast<float>(
        Safe_Divide(_renderPerfWindow.combined.accumulatedMs, static_cast<double>(_renderPerfWindow.combined.sampleCount)));

    Reset_RenderPerfWindow(_renderPerfWindow);
    return true;
}

void Renderer::Accumulate_RenderPerf(
    const double renderMs,
    const double priorityMs,
    const double nonBlendMs,
    const double blendMs,
    const double uiMs)
{
    Accumulate_PassPerf(_renderPerfWindow.render, renderMs);
    Accumulate_PassPerf(_renderPerfWindow.priority, priorityMs);
    Accumulate_PassPerf(_renderPerfWindow.nonBlend, nonBlendMs);
    Accumulate_PassPerf(_renderPerfWindow.blend, blendMs);
    Accumulate_PassPerf(_renderPerfWindow.ui, uiMs);
}

void Renderer::Accumulate_PassPerf(PerfCounter& counter, const double passMs)
{
    ++counter.sampleCount;
    counter.accumulatedMs += passMs;
    counter.maxMs = max(counter.maxMs, passMs);
}

void Renderer::Reset_RenderPerfWindow(RenderPerfWindow& window)
{
    window = {};
}

double Renderer::Safe_Divide(const double numerator, const double denominator)
{
    if (denominator <= 0.0)
        return 0.0;

    return numerator / denominator;
}
#endif

Unique<Renderer> Renderer::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    Unique<Renderer> instance{ new Renderer(device, context) };

    if (FAILED(instance->Initialize()))
    {
        MSG_BOX("Failed to Created : Renderer");
        instance->Free();
        return nullptr;
    }
    return instance;
}

void Renderer::Free()
{
    __super::Free();

    for (size_t i = 0; i < ETOI(RenderGroup::END); i++)
        _renderGroups[i].clear();
}
