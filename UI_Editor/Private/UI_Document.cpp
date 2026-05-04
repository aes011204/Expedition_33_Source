#include "pch.h"
#include "UI_Document.h"

namespace UIEditor
{
    UI_Document::UI_Document()
    {
        Reset();
    }

    void UI_Document::Reset()
    {
        _canvasSize = ImVec2(1280.f, 720.f);
    }

    const ImVec2& UI_Document::Get_CanvasSize() const
    {
        return _canvasSize;
    }

    void UI_Document::Set_CanvasSize(const ImVec2& canvasSize)
    {
        _canvasSize.x = max(1.f, canvasSize.x);
        _canvasSize.y = max(1.f, canvasSize.y);
    }
}
