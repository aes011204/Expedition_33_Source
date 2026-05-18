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
