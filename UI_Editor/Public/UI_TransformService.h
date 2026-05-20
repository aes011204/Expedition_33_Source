#pragma once

#include "UI_Document.h"

namespace UIEditor
{
    class UI_TransformService final
    {
    public:
        UI_TransformService() = default;
        ~UI_TransformService() = default;

    public:
        bool Set_WorldPosition(UI_Document& document, ObjectId objectId, const ImVec2& worldPosition) const;

        bool Can_SetWorldRect(const UI_Document& document, ObjectId objectId) const;
        bool Set_WorldRect(UI_Document& document, ObjectId objectId, const ImVec2& worldPosition, const ImVec2& worldSize) const;

        bool Can_SetPivotKeepWorldRect(const UI_Document& document, ObjectId objectId, const ImVec2& pivot) const;
        bool Set_PivotKeepWorldRect(UI_Document& document, ObjectId objectId, const ImVec2& pivot) const;

        bool Can_SetAnchorsKeepWorldRect(const UI_Document& document, ObjectId objectId, const ImVec2& anchorMin, const ImVec2& anchorMax) const;
        bool Set_AnchorsKeepWorldRect(UI_Document& document, ObjectId objectId, const ImVec2& anchorMin, const ImVec2& anchorMax) const;

        ImVec2 Get_WorldTopLeft(const UI_Document& document, ObjectId objectId) const;

    private:
        ImVec2 Clamp01(const ImVec2& value) const;
        bool NearlyEqual(const ImVec2& lhs, const ImVec2& rhs) const;
    };
}
