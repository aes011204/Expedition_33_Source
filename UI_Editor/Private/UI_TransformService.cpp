#include "pch.h"
#include "UI_TransformService.h"

namespace UIEditor
{
    namespace
    {
        float Clamp01Value(float value)
        {
            return min(max(value, 0.f), 1.f);
        }

        bool NearlyEqualValue(float lhs, float rhs)
        {
            return fabsf(lhs - rhs) <= 0.0001f;
        }
    }

    bool UI_TransformService::Set_WorldPosition(
        UI_Document& document,
        ObjectId objectId,
        const ImVec2& worldPosition) const
    {
        return document.Set_WorldPosition(objectId, worldPosition);
    }

    bool UI_TransformService::Can_SetWorldRect(const UI_Document& document, ObjectId objectId) const
    {
        return nullptr != document.Find_Object(objectId);
    }

    bool UI_TransformService::Set_WorldRect(
        UI_Document& document,
        ObjectId objectId,
        const ImVec2& worldPosition,
        const ImVec2& worldSize) const
    {
        return document.Set_WorldRect(objectId, worldPosition, worldSize);
    }

    bool UI_TransformService::Can_SetPivotKeepWorldRect(
        const UI_Document& document,
        ObjectId objectId,
        const ImVec2& pivot) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        if (nullptr == object)
            return false;

        return !NearlyEqual(object->pivot, Clamp01(pivot));
    }

    bool UI_TransformService::Set_PivotKeepWorldRect(
        UI_Document& document,
        ObjectId objectId,
        const ImVec2& pivot) const
    {
        return document.Set_PivotKeepWorldRect(objectId, Clamp01(pivot));
    }

    bool UI_TransformService::Can_SetAnchorsKeepWorldRect(
        const UI_Document& document,
        ObjectId objectId,
        const ImVec2& anchorMin,
        const ImVec2& anchorMax) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        if (nullptr == object)
            return false;

        const ImVec2 clampedAnchorMin = Clamp01(anchorMin);
        const ImVec2 clampedAnchorMax = Clamp01(anchorMax);
        return !NearlyEqual(object->anchorMin, clampedAnchorMin) ||
            !NearlyEqual(object->anchorMax, clampedAnchorMax);
    }

    bool UI_TransformService::Set_AnchorsKeepWorldRect(
        UI_Document& document,
        ObjectId objectId,
        const ImVec2& anchorMin,
        const ImVec2& anchorMax) const
    {
        return document.Set_AnchorsKeepWorldRect(objectId, Clamp01(anchorMin), Clamp01(anchorMax));
    }

    ImVec2 UI_TransformService::Get_WorldTopLeft(const UI_Document& document, ObjectId objectId) const
    {
        const ImVec2 worldCenter = document.Get_WorldCenter(objectId);
        const ImVec2 worldSize = document.Get_WorldSize(objectId);
        return ImVec2(
            worldCenter.x - worldSize.x * 0.5f,
            worldCenter.y - worldSize.y * 0.5f);
    }

    ImVec2 UI_TransformService::Clamp01(const ImVec2& value) const
    {
        return ImVec2(Clamp01Value(value.x), Clamp01Value(value.y));
    }

    bool UI_TransformService::NearlyEqual(const ImVec2& lhs, const ImVec2& rhs) const
    {
        return NearlyEqualValue(lhs.x, rhs.x) && NearlyEqualValue(lhs.y, rhs.y);
    }
}
