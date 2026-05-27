#pragma once

#include "Debug_Profile.h"
#include "Editor_Window.h"

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
    struct Profiler_Runtime;
    struct Process_Memory
    {
        size_t workingSetMb = 0;
        size_t privateBytesMb = 0;
    };

private:
    void Draw_Toolbar();
    void Draw_Frame_Info();
    void Draw_Scopes();
    void Draw_Memory();
    void Reset_Stats();
    void Prune_Dead_Scopes();
    void Record_Scope_Sample(const string& scopeName, double elapsedMs);
    Process_Memory Get_Process_Memory() const;

private:
    static Profile_View* s_active;

    Unique<Profiler_Runtime> _runtime = nullptr;
    clock::time_point _prevFrame = clock::now();

    bool _capture = true;
    bool _sortByCost = true;
    bool _showDisabled = false;
    int _avgWindow = 120;
    int _maxScopeRows = 64;
    char _filter[128] = {};

    double _frameMsLast = 0.0;
    double _frameMsAvg = 0.0;
    double _fpsLast = 0.0;
    double _fpsAvg = 0.0;
    uint32 _renderCallCount = 0;

public:
    static Shared<Profile_View> Create();
};

NS_END
