#include "Debug_Profile.h"

NS_BEGIN(Engine)

namespace
{
mutex g_debugProfileMutex;
vector<DebugProfileSample> g_debugProfileSamples;
constexpr size_t MaxDebugProfileSamples = 8192;
}

Debug_Profile::Scope::Scope(const char* name)
{
#ifdef _DEBUG
    if (name == nullptr || name[0] == '\0')
        return;

    _name = name;
    _beginTime = chrono::steady_clock::now();
    _active = true;
#else
    UNREFERENCED_PARAMETER(name);
#endif
}

Debug_Profile::Scope::~Scope()
{
#ifdef _DEBUG
    if (!_active)
        return;

    const chrono::steady_clock::time_point endTime = chrono::steady_clock::now();
    const double elapsedMs = chrono::duration<double, milli>(endTime - _beginTime).count();
    Debug_Profile::Push_Sample(_name, elapsedMs);
#endif
}

void Debug_Profile::Push_Sample(const char* name, double elapsedMs)
{
#ifdef _DEBUG
    if (name == nullptr || name[0] == '\0')
        return;

    lock_guard<mutex> lock(g_debugProfileMutex);
    if (g_debugProfileSamples.size() >= MaxDebugProfileSamples)
        g_debugProfileSamples.clear();

    g_debugProfileSamples.push_back(DebugProfileSample{ name, elapsedMs });
#else
    UNREFERENCED_PARAMETER(name);
    UNREFERENCED_PARAMETER(elapsedMs);
#endif
}

void Debug_Profile::Consume_Samples(vector<DebugProfileSample>& outSamples)
{
#ifdef _DEBUG
    lock_guard<mutex> lock(g_debugProfileMutex);
    outSamples.insert(outSamples.end(), g_debugProfileSamples.begin(), g_debugProfileSamples.end());
    g_debugProfileSamples.clear();
#else
    UNREFERENCED_PARAMETER(outSamples);
#endif
}

NS_END
