#pragma once

#include "UI_Editor_Defines.h"

namespace UIEditor
{
    class UI_Document final
    {
    public:
        UI_Document();

        void Reset();
        const ImVec2& Get_CanvasSize() const;
        void Set_CanvasSize(const ImVec2& canvasSize);

    private:
        ImVec2 _canvasSize = ImVec2(1280.f, 720.f);
    };
}
