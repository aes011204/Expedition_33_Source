#pragma once

#include "Engine_Define.h"

NS_BEGIN(Engine)

struct DebugProfileSample
{
    string name = {};
    double elapsedMs = 0.0;
};

class ENGINE_DLL Debug_Profile final
{
public:
    class ENGINE_DLL Scope final
    {
    public:
        explicit Scope(const char* name);
        ~Scope();

    private:
        const char* _name = nullptr;
        chrono::steady_clock::time_point _beginTime = {};
        bool _active = false;
    };

public:
    static void Push_Sample(const char* name, double elapsedMs);
    static void Consume_Samples(vector<DebugProfileSample>& outSamples);
};

NS_END

#if ENGINE_PROFILE_LEVEL >= ENGINE_PROFILE_LEVEL_BASIC
#define ENGINE_PROFILE_SCOPE_BASIC(_name) Engine::Debug_Profile::Scope ENGINE_LOG_UNIQUE_NAME(engineProfileScope, __LINE__)(_name)
#define ENGINE_PROFILE_PUSH_BASIC(_name, _elapsedMs) Engine::Debug_Profile::Push_Sample((_name), (_elapsedMs))
#else
#define ENGINE_PROFILE_SCOPE_BASIC(_name) ((void)0)
#define ENGINE_PROFILE_PUSH_BASIC(_name, _elapsedMs) ((void)0)
#endif

#if ENGINE_PROFILE_LEVEL >= ENGINE_PROFILE_LEVEL_DETAIL
#define ENGINE_PROFILE_SCOPE_DETAIL(_name) Engine::Debug_Profile::Scope ENGINE_LOG_UNIQUE_NAME(engineProfileScope, __LINE__)(_name)
#define ENGINE_PROFILE_PUSH_DETAIL(_name, _elapsedMs) Engine::Debug_Profile::Push_Sample((_name), (_elapsedMs))
#else
#define ENGINE_PROFILE_SCOPE_DETAIL(_name) ((void)0)
#define ENGINE_PROFILE_PUSH_DETAIL(_name, _elapsedMs) ((void)0)
#endif
