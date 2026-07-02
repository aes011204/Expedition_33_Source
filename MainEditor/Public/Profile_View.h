#pragma once

#include "Debug_Profile.h"
#include "Editor_Window.h"
#include "GameInstance.h"

NS_BEGIN(MainEditor)

class Profile_View final : public Editor_Window
{
public:
    using clock = chrono::steady_clock;

    struct Scope_Stats
    {
        string name = {};
        double lastMs = 0.0;
        double avgMs = 0.0;
        double minMs = 0.0;
        double maxMs = 0.0;
        uint64 samples = 0;
        bool isEnabled = true;
    };

    class Scope final
    {
    public:
        explicit Scope(const char* scopeName);
        ~Scope();

    private:
        bool _active = false;
    };

public:
    Profile_View();
    virtual ~Profile_View() override;

public:
    virtual void Render() override;

public:
    static void Frame_Begin();
    static void Frame_End();
    static void Begin_Scope(const char* scopeName);
    static void End_Scope();
    static void Set_Frame_Time_External(double frameMs);
    static void Set_RenderCallCount_External(uint32 renderCallCount);

private:
    struct Scope_Row
    {
        const Scope_Stats* stats = nullptr;
        double sortKey = 0.0;
    };

    struct Profiler_Runtime;
    struct Process_Memory
    {
        size_t workingSetMb = 0;
        size_t privateBytesMb = 0;
    };

    struct Resource_Memory
    {
        Engine::DebugResourceMemorySummary engine{};
        uint32 effectPoolEntryCount = 0;
        uint32 effectPoolActiveCount = 0;
        uint32 effectPoolIdleCount = 0;
        size_t editorPlayBindings = 0;
        size_t editorPauseBindings = 0;
        size_t editorResumeBindings = 0;
        size_t editorStopBindings = 0;
        bool hasData = false;
    };

private:
    void Draw_Camera_Debug();
    void Draw_Toolbar();
    void Draw_Frame_Info();
    void Draw_Render_Isolation();
    void Draw_Scopes();
    void Draw_Memory();
    void Draw_Resource_Memory();
    void Draw_Memory_Trace();
    void Reset_Stats();
    void Prune_Dead_Scopes();
    vector<Scope_Row> Build_Scope_Rows() const;
    void Log_Snapshot() const;
    void Log_ViewHotspot() const;
    void Log_AutoHitch(double frameMs);
    void Record_Scope_Sample(const string& scopeName, double elapsedMs);
    void Refresh_Memory_Samples();
    Process_Memory Get_Process_Memory() const;
    Resource_Memory Get_Resource_Memory() const;
    void Log_Memory_Trace_Snapshot() const;

private:
    static Profile_View* s_active;

    Unique<Profiler_Runtime> _runtime = nullptr;
    clock::time_point _prevFrame = clock::now();

    bool _capture = true;
    bool _sortByCost = true;
    bool _showDisabled = false;
    bool _autoHitchLog = true;
    int _avgWindow = 120;
    int _maxScopeRows = 64;
    int _autoHitchLogRemaining = 8;
    int _pendingViewHotspotVisibleCountLogFrames = 0;
    float _autoHitchThresholdMs = 80.f;
    char _filter[128] = {};

    double _frameMsLast = 0.0;
    double _frameMsAvg = 0.0;
    double _fpsLast = 0.0;
    double _fpsAvg = 0.0;
    uint32 _renderCallCount = 0;
    Process_Memory _processMemory = {};
    Resource_Memory _resourceMemory = {};
    clock::time_point _lastTimingSampleTime = {};
    clock::time_point _lastProcessMemorySampleTime = {};
    clock::time_point _lastResourceMemorySampleTime = {};
    clock::time_point _lastAutoHitchLogTime = {};
    bool _hasTimingSample = false;
    bool _hasProcessMemorySample = false;
    bool _hasResourceMemorySample = false;
    size_t _peakWorkingSetMb = 0;
    size_t _peakPrivateBytesMb = 0;

    float _cameraFarOverride = 500.f;
    bool _cameraFarOverrideEnabled = false;

public:
    static Shared<Profile_View> Create();
};

NS_END
