#include "Profile_View.h"

#include <cctype>
#include <cstring>
#include <psapi.h>

#pragma comment(lib, "psapi.lib")

NS_BEGIN(EffectEditor)

Profile_View* Profile_View::s_active = nullptr;

struct Profile_View::Profiler_Runtime
{
    struct Active_Scope
    {
        const char* name = nullptr;
        clock::time_point beginTime = {};
    };

    struct Scope_Stats_Internal
    {
        Scope_Stats ui = {};
        deque<double> window = {};
        double windowSum = 0.0;
        uint64 lastTouchedFrame = 0;
    };

    vector<Active_Scope> scopeStack = {};
    unordered_map<string, Scope_Stats_Internal> scopes = {};

    uint64 frameIndex = 0;
    int avgWindow = 120;
    bool capture = true;

    bool externalFrameTime = false;
    double externalFrameMs = 0.0;

    deque<double> frameWindow = {};
    double frameWindowSum = 0.0;
};

namespace Internal
{
    static bool Str_IContains(const string& haystack, const char* needle)
    {
        if (nullptr == needle || '\0' == needle[0])
            return true;

        auto toLower = [](unsigned char ch) { return static_cast<char>(tolower(ch)); };

        string lowerHaystack = {};
        lowerHaystack.reserve(haystack.size());
        for (const char ch : haystack)
            lowerHaystack.push_back(toLower(static_cast<unsigned char>(ch)));

        string lowerNeedle = {};
        lowerNeedle.reserve(strlen(needle));
        for (const char* cursor = needle; '\0' != *cursor; ++cursor)
            lowerNeedle.push_back(toLower(static_cast<unsigned char>(*cursor)));

        return string::npos != lowerHaystack.find(lowerNeedle);
    }
}

Profile_View::Scope::Scope(const char* scopeName)
{
    if (nullptr == scopeName || '\0' == scopeName[0])
        return;

    Begin_Scope(scopeName);
    _active = true;
}

Profile_View::Scope::~Scope()
{
    if (_active)
        End_Scope();
}

Profile_View::Profile_View()
    : Editor_Window{ L"Profile", ICON_FA_GAUGE }
{
    _runtime = make_unique<Profiler_Runtime>();
    _prevFrame = clock::now();
    s_active = this;
}

Profile_View::~Profile_View()
{
    if (s_active == this)
        s_active = nullptr;
}

void Profile_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string& windowName = Get_ImGuiWindowName();

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

        Draw_Toolbar();
        ImGui::Separator();
        Draw_Frame_Info();
        ImGui::Separator();
        Draw_Scopes();
        ImGui::Separator();
        Draw_Memory();
    }

    Set_Open(isOpen);
    ImGui::End();
}

void Profile_View::Frame_Begin()
{
    if (nullptr == s_active || nullptr == s_active->_runtime)
        return;

    auto& runtime = *s_active->_runtime;
    runtime.frameIndex++;
    runtime.avgWindow = max(1, s_active->_avgWindow);
    runtime.capture = s_active->_capture;
}

void Profile_View::Frame_End()
{
    if (nullptr == s_active || nullptr == s_active->_runtime)
        return;

    auto& runtime = *s_active->_runtime;
    const clock::time_point now = clock::now();

    double frameMs = 0.0;
    if (runtime.externalFrameTime)
    {
        frameMs = runtime.externalFrameMs;
        runtime.externalFrameTime = false;
        s_active->_prevFrame = now;
    }
    else
    {
        frameMs = chrono::duration<double, milli>(now - s_active->_prevFrame).count();
        s_active->_prevFrame = now;
    }

    s_active->_frameMsLast = frameMs;
    s_active->_fpsLast = frameMs > 0.0001 ? 1000.0 / frameMs : 0.0;

    runtime.frameWindow.push_back(frameMs);
    runtime.frameWindowSum += frameMs;

    while (static_cast<int>(runtime.frameWindow.size()) > runtime.avgWindow)
    {
        runtime.frameWindowSum -= runtime.frameWindow.front();
        runtime.frameWindow.pop_front();
    }

    s_active->_frameMsAvg = runtime.frameWindow.empty()
                            ? 0.0
                            : runtime.frameWindowSum / static_cast<double>(runtime.frameWindow.size());
    s_active->_fpsAvg = s_active->_frameMsAvg > 0.0001 ? 1000.0 / s_active->_frameMsAvg : 0.0;

    s_active->Prune_Dead_Scopes();
}

void Profile_View::Begin_Scope(const char* scopeName)
{
    if (nullptr == s_active || nullptr == s_active->_runtime)
        return;

    auto& runtime = *s_active->_runtime;

    if (!runtime.capture)
        return;

    Profiler_Runtime::Active_Scope activeScope = {};
    activeScope.name = scopeName;
    activeScope.beginTime = clock::now();
    runtime.scopeStack.push_back(activeScope);
}

void Profile_View::End_Scope()
{
    if (nullptr == s_active || nullptr == s_active->_runtime)
        return;

    auto& runtime = *s_active->_runtime;

    if (!runtime.capture || runtime.scopeStack.empty())
        return;

    const clock::time_point endTime = clock::now();
    const Profiler_Runtime::Active_Scope activeScope = runtime.scopeStack.back();
    runtime.scopeStack.pop_back();

    auto& entry = runtime.scopes[activeScope.name];
    auto& stats = entry.ui;

    const double elapsedMs = chrono::duration<double, milli>(endTime - activeScope.beginTime).count();

    if (0 == stats.samples)
    {
        stats.name = activeScope.name;
        stats.minMs = elapsedMs;
        stats.maxMs = elapsedMs;
        stats.isEnabled = true;
    }

    entry.lastTouchedFrame = runtime.frameIndex;

    if (!stats.isEnabled)
        return;

    stats.lastMs = elapsedMs;
    stats.minMs = min(stats.minMs, elapsedMs);
    stats.maxMs = max(stats.maxMs, elapsedMs);
    stats.samples++;

    entry.window.push_back(elapsedMs);
    entry.windowSum += elapsedMs;

    while (static_cast<int>(entry.window.size()) > runtime.avgWindow)
    {
        entry.windowSum -= entry.window.front();
        entry.window.pop_front();
    }

    stats.avgMs = entry.window.empty()
                  ? 0.0
                  : entry.windowSum / static_cast<double>(entry.window.size());
}

void Profile_View::Set_Frame_Time_External(double frameMs)
{
    if (nullptr == s_active || nullptr == s_active->_runtime)
        return;

    auto& runtime = *s_active->_runtime;
    runtime.externalFrameTime = true;
    runtime.externalFrameMs = frameMs;
}

void Profile_View::Set_RenderCallCount_External(uint32 renderCallCount)
{
    if (nullptr == s_active)
        return;

    s_active->_renderCallCount = renderCallCount;
}

void Profile_View::Draw_Toolbar()
{
    if (ImGui::Checkbox("Capture", &_capture))
    {
        if (_runtime)
            _runtime->capture = _capture;
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset"))
        Reset_Stats();

    ImGui::SameLine();
    ImGui::Checkbox("Sort by cost", &_sortByCost);

    ImGui::SameLine();
    ImGui::Checkbox("Show disabled", &_showDisabled);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.f);
    ImGui::InputInt("AvgWindow", &_avgWindow);
    _avgWindow = max(1, min(_avgWindow, 1000));

    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.f);
    ImGui::InputText("Filter", _filter, IM_ARRAYSIZE(_filter));
}

void Profile_View::Draw_Frame_Info()
{
    ImGui::Text("FPS (Last / Avg): %d / %d", static_cast<int>(_fpsLast), static_cast<int>(_fpsAvg));
    ImGui::Text("Frame ms (Last / Avg): %.2f / %.2f", static_cast<float>(_frameMsLast), static_cast<float>(_frameMsAvg));
    ImGui::Text("Render Calls: %u", _renderCallCount);
}

void Profile_View::Draw_Scopes()
{
    if (nullptr == _runtime)
        return;

    struct Row
    {
        Scope_Stats* stats = nullptr;
        double sortKey = 0.0;
    };

    vector<Row> rows = {};
    rows.reserve(_runtime->scopes.size());

    for (auto& [_, entry] : _runtime->scopes)
    {
        Scope_Stats& stats = entry.ui;

        if (!_showDisabled && !stats.isEnabled)
            continue;

        if (!Internal::Str_IContains(stats.name, _filter))
            continue;

        Row row = {};
        row.stats = &stats;
        row.sortKey = _sortByCost ? stats.avgMs : 0.0;
        rows.push_back(row);
    }

    if (_sortByCost)
    {
        sort(
            rows.begin(),
            rows.end(),
            [](const Row& lhs, const Row& rhs)
            {
                return lhs.sortKey > rhs.sortKey;
            }
        );
    }
    else
    {
        sort(
            rows.begin(),
            rows.end(),
            [](const Row& lhs, const Row& rhs)
            {
                return lhs.stats->name < rhs.stats->name;
            }
        );
    }

    if (static_cast<int>(rows.size()) > _maxScopeRows)
        rows.resize(static_cast<size_t>(_maxScopeRows));

    if (ImGui::BeginTable("##ProfileScopes", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, 36.f);
        ImGui::TableSetupColumn("Scope");
        ImGui::TableSetupColumn("Last (ms)");
        ImGui::TableSetupColumn("Avg (ms)");
        ImGui::TableSetupColumn("Min (ms)");
        ImGui::TableSetupColumn("Max (ms)");
        ImGui::TableHeadersRow();

        for (const Row& row : rows)
        {
            Scope_Stats& stats = *row.stats;

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(stats.name.c_str());
            ImGui::Checkbox("##enabled", &stats.isEnabled);
            ImGui::PopID();

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(stats.name.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.3f", static_cast<float>(stats.lastMs));

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.3f", static_cast<float>(stats.avgMs));

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.3f", static_cast<float>(stats.minMs));

            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%.3f", static_cast<float>(stats.maxMs));
        }

        ImGui::EndTable();
    }
}

void Profile_View::Draw_Memory()
{
    const Process_Memory memory = Get_Process_Memory();

    ImGui::Text("Memory");
    ImGui::BulletText("Working Set : %zu MB", memory.workingSetMb);
    ImGui::BulletText("Private     : %zu MB", memory.privateBytesMb);
}

void Profile_View::Reset_Stats()
{
    if (nullptr == _runtime)
        return;

    for (auto& [_, entry] : _runtime->scopes)
    {
        Scope_Stats& stats = entry.ui;
        stats.lastMs = 0.0;
        stats.avgMs = 0.0;
        stats.minMs = 0.0;
        stats.maxMs = 0.0;
        stats.samples = 0;

        entry.window.clear();
        entry.windowSum = 0.0;
        entry.lastTouchedFrame = _runtime->frameIndex;
    }

    _runtime->frameWindow.clear();
    _runtime->frameWindowSum = 0.0;
    _frameMsLast = 0.0;
    _frameMsAvg = 0.0;
    _fpsLast = 0.0;
    _fpsAvg = 0.0;
}

void Profile_View::Prune_Dead_Scopes()
{
    if (nullptr == _runtime)
        return;

    const uint64 keepFrames = static_cast<uint64>(max(60, _runtime->avgWindow * 10));

    for (auto iter = _runtime->scopes.begin(); iter != _runtime->scopes.end();)
    {
        const uint64 age = _runtime->frameIndex - iter->second.lastTouchedFrame;
        if (age > keepFrames)
            iter = _runtime->scopes.erase(iter);
        else
            ++iter;
    }
}

Profile_View::Process_Memory Profile_View::Get_Process_Memory() const
{
    PROCESS_MEMORY_COUNTERS_EX processMemoryCounters = {};
    GetProcessMemoryInfo(
        GetCurrentProcess(),
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&processMemoryCounters),
        sizeof(processMemoryCounters)
    );

    Process_Memory result = {};
    result.workingSetMb = processMemoryCounters.WorkingSetSize / (1024 * 1024);
    result.privateBytesMb = processMemoryCounters.PrivateUsage / (1024 * 1024);
    return result;
}

Shared<Profile_View> Profile_View::Create()
{
    return make_shared<Profile_View>();
}

NS_END
