#include "Profile_View.h"

#include "ClientInstance.h"
#include "DebugMemoryTracker.h"
#include "Debug_Profile.h"
#include "EffectInstancePool.h"
#include "Helper_String.h"
#include "Renderer_Define.h"

#include <cctype>
#include <cstring>
#include <psapi.h>

#pragma comment(lib, "psapi.lib")

NS_BEGIN(MainEditor)

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
    bool timingSampleFrame = false;

    deque<double> frameWindow = {};
    double frameWindowSum = 0.0;
};

namespace
{
constexpr const char* kProfileSnapshotTag = "MainEditorProfileSnapshot";
constexpr const char* kViewHotspotTag = "MainEditorViewHotspot";
constexpr const char* kMemoryTraceTag = "MemoryTrace";
constexpr size_t kViewHotspotTopLimit = 20;

const char* Get_Profile_Level_Name()
{
#if ENGINE_PROFILE_LEVEL == ENGINE_PROFILE_LEVEL_OFF
    return "Off";
#elif ENGINE_PROFILE_LEVEL == ENGINE_PROFILE_LEVEL_BASIC
    return "Basic";
#elif ENGINE_PROFILE_LEVEL == ENGINE_PROFILE_LEVEL_DETAIL
    return "Detail";
#else
    return "Custom";
#endif
}

bool Str_IContains(const string& haystack, const char* needle)
{
    if (nullptr == needle || '\0' == needle[0])
        return true;

    auto toLower = [](unsigned char ch) { return static_cast<char>(tolower(ch)); };

    string lowerHaystack = {};
    lowerHaystack.reserve(haystack.size());
    for (char ch : haystack)
        lowerHaystack.push_back(toLower(static_cast<unsigned char>(ch)));

    string lowerNeedle = {};
    lowerNeedle.reserve(strlen(needle));
    for (const char* cursor = needle; '\0' != *cursor; ++cursor)
        lowerNeedle.push_back(toLower(static_cast<unsigned char>(*cursor)));

    return string::npos != lowerHaystack.find(lowerNeedle);
}

bool Should_Refresh_Sample(
    const Profile_View::clock::time_point now,
    const Profile_View::clock::time_point lastSampleTime,
    const bool hasSample,
    const float intervalSec)
{
    if (!hasSample)
        return true;

    if (intervalSec <= 0.f)
        return true;

    const float elapsedSec = static_cast<float>(
        chrono::duration<double>(now - lastSampleTime).count());
    return elapsedSec >= intervalSec;
}

double Bytes_To_Mb(const size_t bytes)
{
    return static_cast<double>(bytes) / (1024.0 * 1024.0);
}

void Log_Tagged_Line(const spdlog::source_loc& sourceLoc, const char* tag, const string& message)
{
    GAME->Log_FileOnly(
        sourceLoc,
        spdlog::level::info,
        fmt::format("[{}] {}", tag, message));
}

void Log_Profile_Snapshot_Line(const spdlog::source_loc& sourceLoc, const string& message)
{
    Log_Tagged_Line(sourceLoc, kProfileSnapshotTag, message);
}

void Log_ViewHotspot_Line(const spdlog::source_loc& sourceLoc, const string& message)
{
    Log_Tagged_Line(sourceLoc, kViewHotspotTag, message);
}

const char* To_RenderGroupName(const Engine::RenderGroup renderGroup)
{
    switch (renderGroup)
    {
    case Engine::RenderGroup::Priority:
        return "Priority";
    case Engine::RenderGroup::Shadow:
        return "Shadow";
    case Engine::RenderGroup::NonBlend:
        return "NonBlend";
    case Engine::RenderGroup::NonLight:
        return "NonLight";
    case Engine::RenderGroup::EffectMasked:
        return "EffectMasked";
    case Engine::RenderGroup::Distortion:
        return "Distortion";
    case Engine::RenderGroup::Blend:
        return "Blend";
    case Engine::RenderGroup::Glow:
        return "Glow";
    case Engine::RenderGroup::UI:
        return "UI";
    case Engine::RenderGroup::UIEffect:
        return "UIEffect";
    default:
        return "END";
    }
}

vector<pair<string, uint32>> Build_TopCounts(const unordered_map<string, uint32>& counts, size_t limit)
{
    vector<pair<string, uint32>> rows{};
    rows.reserve(counts.size());

    for (const auto& [key, count] : counts)
        rows.emplace_back(key, count);

    sort(rows.begin(), rows.end(), [](const auto& lhs, const auto& rhs)
    {
        if (lhs.second != rhs.second)
            return lhs.second > rhs.second;

        return lhs.first < rhs.first;
    });

    if (rows.size() > limit)
        rows.resize(limit);

    return rows;
}

float Get_MaxExtent(const Vec3& extents)
{
    return max(extents.x, max(extents.y, extents.z));
}

double Get_BoundsVolume(const Vec3& extents)
{
    return static_cast<double>(extents.x) * 2.0 *
        static_cast<double>(extents.y) * 2.0 *
        static_cast<double>(extents.z) * 2.0;
}

}

Profile_View::Scope::Scope(const char* scopeName)
{
#if ENGINE_PROFILE_LEVEL > ENGINE_PROFILE_LEVEL_OFF
    if (nullptr == scopeName || '\0' == scopeName[0])
        return;

    Profile_View::Begin_Scope(scopeName);
    _active = true;
#else
    UNREFERENCED_PARAMETER(scopeName);
#endif
}

Profile_View::Scope::~Scope()
{
#if ENGINE_PROFILE_LEVEL > ENGINE_PROFILE_LEVEL_OFF
    if (_active)
        Profile_View::End_Scope();
#endif
}

Profile_View::Profile_View()
    : Editor_Window(L"Profile")
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
    const string windowName = String::ToString(Get_WindowName());

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

        Draw_Toolbar();
        ImGui::Separator();
        Draw_Frame_Info();
        ImGui::Separator();
        Draw_Render_Isolation();
        ImGui::Separator();
        Draw_Scopes();
#if ENGINE_PROFILE_LEVEL >= ENGINE_PROFILE_LEVEL_BASIC
        Refresh_Memory_Samples();
        ImGui::Separator();
        Draw_Memory();
#endif
    }

    Set_Open(isOpen);
    ImGui::End();
}

void Profile_View::Frame_Begin()
{
    if (nullptr == s_active || nullptr == s_active->_runtime)
        return;

    auto& runtime = *s_active->_runtime;
    const clock::time_point now = clock::now();
    runtime.frameIndex++;
    runtime.avgWindow = max(1, s_active->_avgWindow);
    runtime.timingSampleFrame = Should_Refresh_Sample(
        now,
        s_active->_lastTimingSampleTime,
        s_active->_hasTimingSample,
        ENGINE_PROFILE_TIMING_SAMPLE_INTERVAL_SEC);
    if (runtime.timingSampleFrame)
    {
        s_active->_lastTimingSampleTime = now;
        s_active->_hasTimingSample = true;
    }
    runtime.capture = s_active->_capture && runtime.timingSampleFrame;
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

    if (runtime.timingSampleFrame)
    {
        s_active->_frameMsLast = frameMs;
        s_active->_fpsLast = (frameMs > 0.0001) ? (1000.0 / frameMs) : 0.0;

        runtime.frameWindow.push_back(frameMs);
        runtime.frameWindowSum += frameMs;

        while (static_cast<int>(runtime.frameWindow.size()) > runtime.avgWindow)
        {
            runtime.frameWindowSum -= runtime.frameWindow.front();
            runtime.frameWindow.pop_front();
        }

        s_active->_frameMsAvg = runtime.frameWindow.empty()
            ? 0.0
            : (runtime.frameWindowSum / static_cast<double>(runtime.frameWindow.size()));
        s_active->_fpsAvg = (s_active->_frameMsAvg > 0.0001) ? (1000.0 / s_active->_frameMsAvg) : 0.0;
    }

    vector<Engine::DebugProfileSample> debugSamples = {};
    Engine::Debug_Profile::Consume_Samples(debugSamples);

    if (runtime.capture)
    {
        unordered_map<string, double> debugSampleSums = {};
        for (const Engine::DebugProfileSample& sample : debugSamples)
            debugSampleSums[sample.name] += sample.elapsedMs;

        for (const auto& [scopeName, elapsedMs] : debugSampleSums)
            s_active->Record_Scope_Sample(scopeName, elapsedMs);
    }

    s_active->Log_AutoHitch(frameMs);
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
        : (entry.windowSum / static_cast<double>(entry.window.size()));
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
    if (ImGui::Button("Log Snapshot"))
        Log_Snapshot();

    ImGui::SameLine();
    if (ImGui::Button("Log View Hotspot"))
        Log_ViewHotspot();

    ImGui::SameLine();
    ImGui::Checkbox("Sort by cost", &_sortByCost);

    ImGui::SameLine();
    ImGui::Checkbox("Show disabled", &_showDisabled);

    ImGui::SameLine();
    ImGui::Checkbox("Auto Hitch Log", &_autoHitchLog);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.f);
    ImGui::DragFloat("Hitch ms", &_autoHitchThresholdMs, 1.f, 16.f, 1000.f, "%.0f");

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
    ImGui::Text("Profile Level: %s (%d)", Get_Profile_Level_Name(), ENGINE_PROFILE_LEVEL);
    ImGui::Text("FPS (Last / Avg): %d / %d", static_cast<int>(_fpsLast), static_cast<int>(_fpsAvg));
    ImGui::Text("Frame ms (Last / Avg): %.2f / %.2f", static_cast<float>(_frameMsLast), static_cast<float>(_frameMsAvg));
    ImGui::Text("Render Calls: %u", _renderCallCount);
}

void Profile_View::Draw_Render_Isolation()
{
    ImGui::TextUnformatted("Render Isolation");

    bool disableDynamicShadowPass = Client::ClientInstance::Is_DebugDisableDynamicShadowRenderPass();
    if (ImGui::Checkbox("Disable Dynamic Shadow Render Pass", &disableDynamicShadowPass))
        Client::ClientInstance::Set_DebugDisableDynamicShadowRenderPass(disableDynamicShadowPass);

    bool disableMapInstanced = Client::ClientInstance::Is_DebugDisableMapInstancedMeshRender();
    if (ImGui::Checkbox("Disable MapInstancedMesh", &disableMapInstanced))
        Client::ClientInstance::Set_DebugDisableMapInstancedMeshRender(disableMapInstanced);

    bool disableGrassInstanced = Client::ClientInstance::Is_DebugDisableGrassInstancedRender();
    if (ImGui::Checkbox("Disable Grass Instanced", &disableGrassInstanced))
        Client::ClientInstance::Set_DebugDisableGrassInstancedRender(disableGrassInstanced);

    bool disableMapStatic = Client::ClientInstance::Is_DebugDisableMapStaticMeshRender();
    if (ImGui::Checkbox("Disable MapStaticMesh", &disableMapStatic))
        Client::ClientInstance::Set_DebugDisableMapStaticMeshRender(disableMapStatic);

    bool disableMapInstancedShadow = Client::ClientInstance::Is_DebugDisableMapInstancedMeshShadow();
    if (ImGui::Checkbox("Disable MapInstancedMesh Shadow", &disableMapInstancedShadow))
        Client::ClientInstance::Set_DebugDisableMapInstancedMeshShadow(disableMapInstancedShadow);

    bool limitMapInstancedShadowCascade = Client::ClientInstance::Is_DebugLimitMapInstancedMeshShadowCascade();
    if (ImGui::Checkbox("Limit MapInstancedMesh Shadow Cascade <= 2", &limitMapInstancedShadowCascade))
        Client::ClientInstance::Set_DebugLimitMapInstancedMeshShadowCascade(limitMapInstancedShadowCascade);

    bool disableGrassInstancedShadow = Client::ClientInstance::Is_DebugDisableGrassInstancedShadow();
    if (ImGui::Checkbox("Disable Grass Instanced Shadow", &disableGrassInstancedShadow))
        Client::ClientInstance::Set_DebugDisableGrassInstancedShadow(disableGrassInstancedShadow);

    bool disableMapStaticShadow = Client::ClientInstance::Is_DebugDisableMapStaticMeshShadow();
    if (ImGui::Checkbox("Disable MapStaticMesh Shadow", &disableMapStaticShadow))
        Client::ClientInstance::Set_DebugDisableMapStaticMeshShadow(disableMapStaticShadow);
}

void Profile_View::Draw_Scopes()
{
    if (nullptr == _runtime)
        return;

    const vector<Scope_Row> rows = Build_Scope_Rows();

    if (ImGui::BeginTable("##ProfileScopes", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, 36.f);
        ImGui::TableSetupColumn("Scope");
        ImGui::TableSetupColumn("Last (ms)");
        ImGui::TableSetupColumn("Avg (ms)");
        ImGui::TableSetupColumn("Min (ms)");
        ImGui::TableSetupColumn("Max (ms)");
        ImGui::TableHeadersRow();

        for (const Scope_Row& row : rows)
        {
            const Scope_Stats& stats = *row.stats;

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(stats.name.c_str());
            bool isEnabled = stats.isEnabled;
            if (ImGui::Checkbox("##enabled", &isEnabled))
            {
                auto iter = _runtime->scopes.find(stats.name);
                if (iter != _runtime->scopes.end())
                    iter->second.ui.isEnabled = isEnabled;
            }
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

vector<Profile_View::Scope_Row> Profile_View::Build_Scope_Rows() const
{
    vector<Scope_Row> rows = {};
    if (nullptr == _runtime)
        return rows;

    rows.reserve(_runtime->scopes.size());

    for (const auto& [_, entry] : _runtime->scopes)
    {
        const Scope_Stats& stats = entry.ui;

        if (!_showDisabled && !stats.isEnabled)
            continue;

        if (!Str_IContains(stats.name, _filter))
            continue;

        Scope_Row row = {};
        row.stats = &stats;
        row.sortKey = _sortByCost ? stats.avgMs : 0.0;
        rows.push_back(row);
    }

    if (_sortByCost)
    {
        sort(rows.begin(), rows.end(), [](const Scope_Row& lhs, const Scope_Row& rhs)
        {
            return lhs.sortKey > rhs.sortKey;
        });
    }
    else
    {
        sort(rows.begin(), rows.end(), [](const Scope_Row& lhs, const Scope_Row& rhs)
        {
            return lhs.stats->name < rhs.stats->name;
        });
    }

    if (static_cast<int>(rows.size()) > _maxScopeRows)
        rows.resize(static_cast<size_t>(_maxScopeRows));

    return rows;
}

void Profile_View::Log_Snapshot() const
{
    const spdlog::source_loc sourceLoc{ __FILE__, __LINE__, SPDLOG_FUNCTION };
    const vector<Scope_Row> rows = Build_Scope_Rows();
    const string filterText = ('\0' == _filter[0]) ? string("<none>") : string(_filter);

    Log_Profile_Snapshot_Line(sourceLoc, "----- begin -----");
    Log_Profile_Snapshot_Line(sourceLoc, fmt::format(
        "frame profile_level={}({}) fps_last={} fps_avg={} frame_ms_last={:.2f} frame_ms_avg={:.2f} render_calls={}",
        Get_Profile_Level_Name(),
        ENGINE_PROFILE_LEVEL,
        static_cast<int>(_fpsLast),
        static_cast<int>(_fpsAvg),
        _frameMsLast,
        _frameMsAvg,
        _renderCallCount));
    Log_Profile_Snapshot_Line(sourceLoc, fmt::format(
        "toolbar capture={} sort_by_cost={} show_disabled={} avg_window={} filter=\"{}\" row_limit={}",
        _capture,
        _sortByCost,
        _showDisabled,
        _avgWindow,
        filterText,
        _maxScopeRows));

    if (rows.empty())
    {
        Log_Profile_Snapshot_Line(sourceLoc, "scopes empty_under_current_filters=true");
    }
    else
    {
        Log_Profile_Snapshot_Line(sourceLoc, fmt::format("scopes visible_count={}", rows.size()));
        for (size_t index = 0; index < rows.size(); ++index)
        {
            const Scope_Stats& stats = *rows[index].stats;
            Log_Profile_Snapshot_Line(sourceLoc, fmt::format(
                "scope[{}] name=\"{}\" enabled={} last_ms={:.3f} avg_ms={:.3f} min_ms={:.3f} max_ms={:.3f} samples={}",
                index,
                stats.name,
                stats.isEnabled,
                stats.lastMs,
                stats.avgMs,
                stats.minMs,
                stats.maxMs,
                stats.samples));
        }
    }

    if (_hasProcessMemorySample)
    {
        Log_Profile_Snapshot_Line(sourceLoc, fmt::format(
            "memory working_set_mb={} peak_working_set_mb={} private_mb={} peak_private_mb={}",
            _processMemory.workingSetMb,
            _peakWorkingSetMb,
            _processMemory.privateBytesMb,
            _peakPrivateBytesMb));
    }
    else
    {
        Log_Profile_Snapshot_Line(sourceLoc, "memory pending=true");
    }

    if (_hasResourceMemorySample && _resourceMemory.hasData)
    {
        const Engine::DebugResourceMemorySummary& summary = _resourceMemory.engine;
        Log_Profile_Snapshot_Line(sourceLoc, fmt::format(
            "resource textures={} textures_mb={:.1f} texture_srvs={} "
            "models={} models_mb={:.1f} model_meshes={} model_materials={} model_bones={} model_anims={} "
            "standalone_buffers={} standalone_buffers_mb={:.1f} objects={} components={} "
            "effect_pool_entries={} effect_pool_active={} effect_pool_idle={} "
            "editor_delegate_bindings(play={}, pause={}, resume={}, stop={})",
            summary.texturePrototypeCount,
            Bytes_To_Mb(summary.textureEstimatedBytes),
            summary.textureSrvCount,
            summary.modelPrototypeCount,
            Bytes_To_Mb(summary.modelEstimatedBytes),
            summary.modelMeshCount,
            summary.modelMaterialCount,
            summary.modelBoneCount,
            summary.modelAnimationCount,
            summary.standaloneBufferPrototypeCount,
            Bytes_To_Mb(summary.standaloneBufferEstimatedBytes),
            summary.currentLevelObjectCount,
            summary.currentLevelComponentCount,
            _resourceMemory.effectPoolEntryCount,
            _resourceMemory.effectPoolActiveCount,
            _resourceMemory.effectPoolIdleCount,
            _resourceMemory.editorPlayBindings,
            _resourceMemory.editorPauseBindings,
            _resourceMemory.editorResumeBindings,
            _resourceMemory.editorStopBindings));
    }
    else if (_hasResourceMemorySample)
    {
        Log_Profile_Snapshot_Line(sourceLoc, "resource pending=false has_data=false");
    }
    else
    {
        Log_Profile_Snapshot_Line(sourceLoc, "resource pending=true");
    }

    Log_Profile_Snapshot_Line(sourceLoc, "----- end -----");
}

void Profile_View::Log_Memory_Trace_Snapshot() const
{
    const spdlog::source_loc sourceLoc{ __FILE__, __LINE__, SPDLOG_FUNCTION };
    const DebugMemoryTraceSnapshot snapshot = DebugMemoryTracker::Build_Snapshot(32, 24);

    Log_Tagged_Line(sourceLoc, kMemoryTraceTag, "----- begin -----");
    Log_Tagged_Line(sourceLoc, kMemoryTraceTag, fmt::format(
        "enabled={} events={}",
        snapshot.enabled,
        snapshot.sequence));

    for (const DebugMemoryTraceCategory& row : snapshot.categories)
    {
        Log_Tagged_Line(sourceLoc, kMemoryTraceTag, fmt::format(
            "category name=\"{}\" events={} counted_mb={:.2f} estimated_mb={:.2f}",
            row.category,
            row.eventCount,
            Bytes_To_Mb(row.countedBytes),
            Bytes_To_Mb(row.estimatedBytes)));
    }

    for (const DebugMemoryTracePath& row : snapshot.topPaths)
    {
        Log_Tagged_Line(sourceLoc, kMemoryTraceTag, fmt::format(
            "top_path category=\"{}\" owner=\"{}\" events={} counted_mb={:.2f} estimated_mb={:.2f} path=\"{}\"",
            row.category,
            row.owner,
            row.eventCount,
            Bytes_To_Mb(row.countedBytes),
            Bytes_To_Mb(row.estimatedBytes),
            row.path));
    }

    Log_Tagged_Line(sourceLoc, kMemoryTraceTag, "----- end -----");
}

void Profile_View::Log_ViewHotspot() const
{
    const spdlog::source_loc sourceLoc{ __FILE__, __LINE__, SPDLOG_FUNCTION };

    Engine::RenderViewHotspotSnapshot snapshot{};

    Log_ViewHotspot_Line(sourceLoc, "----- begin -----");

    if (!GAME->Try_GetRenderViewHotspotSnapshot(snapshot))
    {
        Log_ViewHotspot_Line(sourceLoc, "snapshot unavailable=true");
        Log_ViewHotspot_Line(sourceLoc, "----- end -----");
        return;
    }

    if (snapshot.hasCamera)
    {
        Log_ViewHotspot_Line(sourceLoc, fmt::format(
            "camera position=({:.3f}, {:.3f}, {:.3f}) forward=({:.3f}, {:.3f}, {:.3f})",
            snapshot.cameraPosition.x,
            snapshot.cameraPosition.y,
            snapshot.cameraPosition.z,
            snapshot.cameraForward.x,
            snapshot.cameraForward.y,
            snapshot.cameraForward.z));
    }
    else
    {
        Log_ViewHotspot_Line(sourceLoc, "camera unavailable=true");
    }

    const uint32 shadowCandidateCount = snapshot.renderGroupCounts[ETOI(Engine::RenderGroup::Shadow)];
    const uint32 alphaEffectCount =
        snapshot.renderGroupCounts[ETOI(Engine::RenderGroup::Blend)] +
        snapshot.renderGroupCounts[ETOI(Engine::RenderGroup::EffectMasked)] +
        snapshot.renderGroupCounts[ETOI(Engine::RenderGroup::Distortion)];

    unordered_set<uintptr_t> dynamicShadowObjects{};
    unordered_map<string, uint32> classCounts{};
    unordered_map<string, uint32> sourcePrefabCounts{};
    unordered_map<string, uint32> sourceModelPathCounts{};
    vector<const Engine::RenderViewHotspotEntry*> instancedNonBlendRows{};
    vector<const Engine::RenderViewHotspotEntry*> nonBlendBoundsRows{};
    uint32 nonBlendBoundsCount = 0;
    uint32 instancedNonBlendGroupCount = 0;
    uint64 instancedNonBlendSubmittedInstances = 0;

    for (const Engine::RenderViewHotspotEntry& entry : snapshot.entries)
    {
        if (entry.shadowCasterType == Engine::ShadowCasterType::Dynamic)
            dynamicShadowObjects.insert(entry.objectKey);

        const string className = entry.className.empty() ? string("<none>") : entry.className;
        const string sourcePrefab = entry.sourcePrefab.empty() ? string("<none>") : entry.sourcePrefab;
        const string sourceModelPath = entry.sourceModelPath.empty() ? string("<none>") : String::ToString(entry.sourceModelPath);

        ++classCounts[className];
        ++sourcePrefabCounts[sourcePrefab];
        ++sourceModelPathCounts[sourceModelPath];

        if (entry.renderGroup == Engine::RenderGroup::NonBlend)
        {
            if (entry.hasWorldBounds)
            {
                ++nonBlendBoundsCount;
                nonBlendBoundsRows.push_back(&entry);
            }

            if (entry.renderHotspotInstanceCount > 0)
            {
                ++instancedNonBlendGroupCount;
                instancedNonBlendSubmittedInstances += entry.renderHotspotInstanceCount;
                instancedNonBlendRows.push_back(&entry);
            }
        }
    }

    Log_ViewHotspot_Line(sourceLoc, fmt::format(
        "summary total_submissions={} unique_objects={} shadow_candidates={} dynamic_shadow_unique={} alpha_effect_submissions={}",
        snapshot.totalSubmissions,
        snapshot.uniqueObjectCount,
        shadowCandidateCount,
        dynamicShadowObjects.size(),
        alphaEffectCount));

    Log_ViewHotspot_Line(sourceLoc, fmt::format(
        "nonblend_diagnostics bounds_entries={} instanced_groups={} submitted_instances={}",
        nonBlendBoundsCount,
        instancedNonBlendGroupCount,
        instancedNonBlendSubmittedInstances));

    Log_ViewHotspot_Line(sourceLoc, fmt::format(
        "rendered_counts nonblend={} shadow_total={} shadow_cascade0={} shadow_cascade1={} shadow_cascade2={} shadow_cascade3={}",
        snapshot.nonBlendRenderedCount,
        snapshot.shadowRenderedCount,
        snapshot.shadowCascadeRenderedCounts[0],
        snapshot.shadowCascadeRenderedCounts[1],
        snapshot.shadowCascadeRenderedCounts[2],
        snapshot.shadowCascadeRenderedCounts[3]));

    for (size_t groupIndex = 0; groupIndex < ETOI(Engine::RenderGroup::END); ++groupIndex)
    {
        const Engine::RenderGroup renderGroup = static_cast<Engine::RenderGroup>(groupIndex);
        Log_ViewHotspot_Line(sourceLoc, fmt::format(
            "render_group[{}]={}",
            To_RenderGroupName(renderGroup),
            snapshot.renderGroupCounts[groupIndex]));
    }

    const auto logTopCounts = [&](const char* label, const unordered_map<string, uint32>& counts)
    {
        const vector<pair<string, uint32>> rows = Build_TopCounts(counts, kViewHotspotTopLimit);
        if (rows.empty())
        {
            Log_ViewHotspot_Line(sourceLoc, fmt::format("{} empty=true", label));
            return;
        }

        for (size_t index = 0; index < rows.size(); ++index)
        {
            Log_ViewHotspot_Line(sourceLoc, fmt::format(
                "{}[{}] count={} key=\"{}\"",
                label,
                index,
                rows[index].second,
                rows[index].first));
        }
    };

    logTopCounts("top_class", classCounts);
    logTopCounts("top_source_prefab", sourcePrefabCounts);
    logTopCounts("top_source_model", sourceModelPathCounts);

    sort(instancedNonBlendRows.begin(), instancedNonBlendRows.end(),
        [](const Engine::RenderViewHotspotEntry* lhs, const Engine::RenderViewHotspotEntry* rhs)
    {
        if (lhs->renderHotspotInstanceCount != rhs->renderHotspotInstanceCount)
            return lhs->renderHotspotInstanceCount > rhs->renderHotspotInstanceCount;

        return Get_MaxExtent(lhs->worldBoundsExtents) > Get_MaxExtent(rhs->worldBoundsExtents);
    });

    if (instancedNonBlendRows.size() > kViewHotspotTopLimit)
        instancedNonBlendRows.resize(kViewHotspotTopLimit);

    for (size_t index = 0; index < instancedNonBlendRows.size(); ++index)
    {
        const Engine::RenderViewHotspotEntry& entry = *instancedNonBlendRows[index];
        Log_ViewHotspot_Line(sourceLoc, fmt::format(
            "top_instanced_nonblend[{}] instances={} class=\"{}\" source_model=\"{}\" object=0x{:X} bounds_center=({:.3f}, {:.3f}, {:.3f}) bounds_extents=({:.3f}, {:.3f}, {:.3f}) max_extent={:.3f}",
            index,
            entry.renderHotspotInstanceCount,
            entry.className.empty() ? string("<none>") : entry.className,
            entry.sourceModelPath.empty() ? string("<none>") : String::ToString(entry.sourceModelPath),
            entry.objectKey,
            entry.worldBoundsCenter.x,
            entry.worldBoundsCenter.y,
            entry.worldBoundsCenter.z,
            entry.worldBoundsExtents.x,
            entry.worldBoundsExtents.y,
            entry.worldBoundsExtents.z,
            Get_MaxExtent(entry.worldBoundsExtents)));
    }

    sort(nonBlendBoundsRows.begin(), nonBlendBoundsRows.end(),
        [](const Engine::RenderViewHotspotEntry* lhs, const Engine::RenderViewHotspotEntry* rhs)
    {
        const double lhsVolume = Get_BoundsVolume(lhs->worldBoundsExtents);
        const double rhsVolume = Get_BoundsVolume(rhs->worldBoundsExtents);
        if (lhsVolume != rhsVolume)
            return lhsVolume > rhsVolume;

        return Get_MaxExtent(lhs->worldBoundsExtents) > Get_MaxExtent(rhs->worldBoundsExtents);
    });

    if (nonBlendBoundsRows.size() > kViewHotspotTopLimit)
        nonBlendBoundsRows.resize(kViewHotspotTopLimit);

    for (size_t index = 0; index < nonBlendBoundsRows.size(); ++index)
    {
        const Engine::RenderViewHotspotEntry& entry = *nonBlendBoundsRows[index];
        Log_ViewHotspot_Line(sourceLoc, fmt::format(
            "top_nonblend_bounds[{}] volume={:.3f} max_extent={:.3f} instances={} class=\"{}\" source_model=\"{}\" object=0x{:X} bounds_center=({:.3f}, {:.3f}, {:.3f}) bounds_extents=({:.3f}, {:.3f}, {:.3f})",
            index,
            Get_BoundsVolume(entry.worldBoundsExtents),
            Get_MaxExtent(entry.worldBoundsExtents),
            entry.renderHotspotInstanceCount,
            entry.className.empty() ? string("<none>") : entry.className,
            entry.sourceModelPath.empty() ? string("<none>") : String::ToString(entry.sourceModelPath),
            entry.objectKey,
            entry.worldBoundsCenter.x,
            entry.worldBoundsCenter.y,
            entry.worldBoundsCenter.z,
            entry.worldBoundsExtents.x,
            entry.worldBoundsExtents.y,
            entry.worldBoundsExtents.z));
    }

    Log_ViewHotspot_Line(sourceLoc, "----- end -----");
}

void Profile_View::Log_AutoHitch(double frameMs)
{
    if (!_autoHitchLog || _autoHitchLogRemaining <= 0)
        return;

    if (frameMs < static_cast<double>(_autoHitchThresholdMs))
        return;

    const clock::time_point now = clock::now();
    if (_lastAutoHitchLogTime != clock::time_point{})
    {
        const double elapsedSec = chrono::duration<double>(now - _lastAutoHitchLogTime).count();
        if (elapsedSec < 1.0)
            return;
    }

    _lastAutoHitchLogTime = now;
    --_autoHitchLogRemaining;

    const spdlog::source_loc sourceLoc{ __FILE__, __LINE__, SPDLOG_FUNCTION };
    Log_Profile_Snapshot_Line(sourceLoc, fmt::format(
        "AutoHitch frame_ms={:.2f} threshold_ms={:.2f} remaining={}",
        frameMs,
        _autoHitchThresholdMs,
        _autoHitchLogRemaining));
    Log_Snapshot();
    Log_ViewHotspot();
}

void Profile_View::Draw_Memory()
{
    ImGui::Text("Memory");

    if (ImGui::BeginTable("MemoryTable", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Metric");
        ImGui::TableSetupColumn("Current");
        ImGui::TableSetupColumn("Peak");
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Working Set");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%zu MB", _processMemory.workingSetMb);
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%zu MB", _peakWorkingSetMb);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Private");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%zu MB", _processMemory.privateBytesMb);
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%zu MB", _peakPrivateBytesMb);

        ImGui::EndTable();
    }

    ImGui::Separator();
    Draw_Resource_Memory();
    ImGui::Separator();
    Draw_Memory_Trace();
}

void Profile_View::Draw_Resource_Memory()
{
    ImGui::Text("Resource Summary");

    if (!_resourceMemory.hasData)
    {
        ImGui::TextDisabled("Pending");
        return;
    }

    const Engine::DebugResourceMemorySummary& summary = _resourceMemory.engine;

    if (ImGui::BeginTable("ResourceMemoryTable", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Category");
        ImGui::TableSetupColumn("Count");
        ImGui::TableSetupColumn("Estimated");
        ImGui::TableSetupColumn("Detail");
        ImGui::TableHeadersRow();

        auto drawRow = [](const char* category, const string& count, const string& estimated, const string& detail)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(category);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(count.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(estimated.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(detail.c_str());
        };

        drawRow(
            "Textures",
            fmt::format("{} prototypes", summary.texturePrototypeCount),
            fmt::format("{:.1f} MB", Bytes_To_Mb(summary.textureEstimatedBytes)),
            fmt::format("{} SRVs incl. materials", summary.textureSrvCount));

        drawRow(
            "Models",
            fmt::format("{} prototypes", summary.modelPrototypeCount),
            fmt::format("{:.1f} MB", Bytes_To_Mb(summary.modelEstimatedBytes)),
            fmt::format(
                "{} meshes / {} materials / {} bones / {} anims",
                summary.modelMeshCount,
                summary.modelMaterialCount,
                summary.modelBoneCount,
                summary.modelAnimationCount));

        drawRow(
            "Standalone Buffers",
            fmt::format("{} prototypes", summary.standaloneBufferPrototypeCount),
            fmt::format("{:.1f} MB", Bytes_To_Mb(summary.standaloneBufferEstimatedBytes)),
            "VIBuffer prototypes");

        drawRow(
            "Objects",
            fmt::format("{} objects", summary.currentLevelObjectCount),
            "N/A",
            fmt::format("{} components", summary.currentLevelComponentCount));

        drawRow(
            "Effect Pool",
            fmt::format("{} entries", _resourceMemory.effectPoolEntryCount),
            "N/A",
            fmt::format(
                "{} active / {} idle",
                _resourceMemory.effectPoolActiveCount,
                _resourceMemory.effectPoolIdleCount));

        drawRow(
            "Editor Delegates",
            fmt::format(
                "{} bindings",
                _resourceMemory.editorPlayBindings +
                _resourceMemory.editorPauseBindings +
                _resourceMemory.editorResumeBindings +
                _resourceMemory.editorStopBindings),
            "N/A",
            fmt::format(
                "play {} / pause {} / resume {} / stop {}",
                _resourceMemory.editorPlayBindings,
                _resourceMemory.editorPauseBindings,
                _resourceMemory.editorResumeBindings,
                _resourceMemory.editorStopBindings));

        ImGui::EndTable();
    }
}

void Profile_View::Draw_Memory_Trace()
{
    bool enabled = DebugMemoryTracker::Is_Enabled();
    if (ImGui::Checkbox("Enable memory trace", &enabled))
        DebugMemoryTracker::Set_Enabled(enabled);

    ImGui::SameLine();
    if (ImGui::Button("Clear trace"))
        DebugMemoryTracker::Clear();

    ImGui::SameLine();
    if (ImGui::Button("Log snapshot"))
        Log_Memory_Trace_Snapshot();

    if (!enabled)
    {
        ImGui::TextDisabled("Memory trace is off by default. Enable it before loading/reloading assets.");
        return;
    }

    const DebugMemoryTraceSnapshot snapshot = DebugMemoryTracker::Build_Snapshot(32, 24);
    ImGui::Text("Memory Trace Events: %llu", static_cast<unsigned long long>(snapshot.sequence));

    if (ImGui::BeginTable("MemoryTraceCategories", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Category");
        ImGui::TableSetupColumn("Events");
        ImGui::TableSetupColumn("Counted");
        ImGui::TableSetupColumn("Estimated");
        ImGui::TableHeadersRow();

        for (const DebugMemoryTraceCategory& row : snapshot.categories)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row.category.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u", row.eventCount);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.1f MB", Bytes_To_Mb(row.countedBytes));
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.1f MB", Bytes_To_Mb(row.estimatedBytes));
        }

        ImGui::EndTable();
    }

    if (ImGui::CollapsingHeader("Top Resource Paths", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::BeginTable("MemoryTraceTopPaths", 5, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("Category");
            ImGui::TableSetupColumn("Owner");
            ImGui::TableSetupColumn("Counted");
            ImGui::TableSetupColumn("Events");
            ImGui::TableSetupColumn("Path");
            ImGui::TableHeadersRow();

            for (const DebugMemoryTracePath& row : snapshot.topPaths)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(row.category.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(row.owner.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.1f MB", Bytes_To_Mb(row.countedBytes));
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%u", row.eventCount);
                ImGui::TableSetColumnIndex(4);
                ImGui::TextUnformatted(row.path.c_str());
            }

            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Recent Load Events"))
    {
        if (ImGui::BeginTable("MemoryTraceRecentEvents", 5, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("#");
            ImGui::TableSetupColumn("Category");
            ImGui::TableSetupColumn("Owner");
            ImGui::TableSetupColumn("Counted");
            ImGui::TableSetupColumn("Path");
            ImGui::TableHeadersRow();

            for (const DebugMemoryTraceEvent& row : snapshot.recentEvents)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%llu", static_cast<unsigned long long>(row.sequence));
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(row.category.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(row.owner.c_str());
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.1f MB", Bytes_To_Mb(row.countedBytes));
                ImGui::TableSetColumnIndex(4);
                ImGui::TextUnformatted(row.path.c_str());
            }

            ImGui::EndTable();
        }
    }
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
    _processMemory = {};
    _resourceMemory = {};
    _hasTimingSample = false;
    _hasProcessMemorySample = false;
    _hasResourceMemorySample = false;
    _peakWorkingSetMb = 0;
    _peakPrivateBytesMb = 0;
    _autoHitchLogRemaining = 8;
    _lastAutoHitchLogTime = {};
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

void Profile_View::Record_Scope_Sample(const string& scopeName, double elapsedMs)
{
    if (nullptr == _runtime || scopeName.empty())
        return;

    auto& runtime = *_runtime;

    if (!runtime.capture)
        return;

    auto& entry = runtime.scopes[scopeName];
    auto& stats = entry.ui;

    if (0 == stats.samples)
    {
        stats.name = scopeName;
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
        : (entry.windowSum / static_cast<double>(entry.window.size()));
}

void Profile_View::Refresh_Memory_Samples()
{
    const clock::time_point now = clock::now();

    if (Should_Refresh_Sample(
        now,
        _lastProcessMemorySampleTime,
        _hasProcessMemorySample,
        ENGINE_PROFILE_PROCESS_MEMORY_SAMPLE_INTERVAL_SEC))
    {
        _processMemory = Get_Process_Memory();
        _peakWorkingSetMb = max(_peakWorkingSetMb, _processMemory.workingSetMb);
        _peakPrivateBytesMb = max(_peakPrivateBytesMb, _processMemory.privateBytesMb);
        _lastProcessMemorySampleTime = now;
        _hasProcessMemorySample = true;
    }

    if (Should_Refresh_Sample(
        now,
        _lastResourceMemorySampleTime,
        _hasResourceMemorySample,
        ENGINE_PROFILE_RESOURCE_MEMORY_SAMPLE_INTERVAL_SEC))
    {
        _resourceMemory = Get_Resource_Memory();
        _lastResourceMemorySampleTime = now;
        _hasResourceMemorySample = true;
    }
}

Profile_View::Process_Memory Profile_View::Get_Process_Memory() const
{
    PROCESS_MEMORY_COUNTERS_EX processMemoryCounters = {};
    GetProcessMemoryInfo(
        GetCurrentProcess(),
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&processMemoryCounters),
        sizeof(processMemoryCounters));

    Process_Memory result = {};
    result.workingSetMb = processMemoryCounters.WorkingSetSize / (1024 * 1024);
    result.privateBytesMb = processMemoryCounters.PrivateUsage / (1024 * 1024);
    return result;
}

Profile_View::Resource_Memory Profile_View::Get_Resource_Memory() const
{
    Resource_Memory result{};
    result.hasData = GAME->Build_DebugResourceMemorySummary(result.engine);

    Engine::DelegateHub& delegateHub = GAME->Get_DelegateHub();
    result.editorPlayBindings = delegateHub.OnEditorPlayRuntime.GetBindingCount();
    result.editorPauseBindings = delegateHub.OnEditorPauseRuntime.GetBindingCount();
    result.editorResumeBindings = delegateHub.OnEditorResumeRuntime.GetBindingCount();
    result.editorStopBindings = delegateHub.OnEditorStopRuntime.GetBindingCount();

    const Shared<Client::ClientInstance>& client = Client::ClientInstance::GetInstance();
    if (client && client->Get_EffectInstancePool())
    {
        const Client::EffectInstancePool::DebugSummary poolSummary =
            client->Get_EffectInstancePool()->Get_DebugSummary();
        result.effectPoolEntryCount = poolSummary.entryCount;
        result.effectPoolActiveCount = poolSummary.activeCount;
        result.effectPoolIdleCount = poolSummary.idleCount;
    }

    return result;
}

Shared<Profile_View> Profile_View::Create()
{
    return make_shared<Profile_View>();
}

NS_END
