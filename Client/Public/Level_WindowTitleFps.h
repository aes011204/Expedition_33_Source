#pragma once

#include "Client_Defines.h"

NS_BEGIN(Client)

struct WindowTitleFpsState
{
    chrono::steady_clock::time_point sampleBegin = {};
    uint32 frameCount = 0;
    int32 displayFps = 0;
};

inline void Update_WindowTitleFps(WindowTitleFpsState& state, const wchar_t* levelName)
{
    const auto now = chrono::steady_clock::now();
    if (state.sampleBegin.time_since_epoch().count() == 0)
    {
        state.sampleBegin = now;
        state.frameCount = 0;
    }

    ++state.frameCount;

    const auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - state.sampleBegin);
    if (elapsed >= chrono::milliseconds(500))
    {
        state.displayFps = static_cast<int32>(
            (state.frameCount * 1000ull) / static_cast<uint64>(elapsed.count()));

        state.sampleBegin = now;
        state.frameCount = 0;
    }

    wchar_t title[128] = {};
    swprintf_s(title, L"%s | FPS : %d", levelName, state.displayFps);
    SetWindowText(g_hWnd, title);
}

NS_END
