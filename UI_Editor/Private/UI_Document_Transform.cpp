#include "pch.h"
#include "UI_Document.h"

#include <cmath>

namespace UIEditor
{
    namespace
    {
        ImVec2 Rotate_Point(const ImVec2& point, float rotationRadians)
        {
            const float cosAngle = cosf(rotationRadians);
            const float sinAngle = sinf(rotationRadians);

            return ImVec2(
                point.x * cosAngle - point.y * sinAngle,
                point.x * sinAngle + point.y * cosAngle);
        }

        float Get_ObjectTextScale(const UI_Object& object)
        {
            const float fontScale = max(4.f, object.fontSize) / 32.f;
            return max(0.01f, object.scale.x * fontScale);
        }

        ImVec2 Get_ObjectLocalPivotOffset(const UI_Object& object, const ImVec2& resolvedLocalSize)
        {
            return ImVec2(
                resolvedLocalSize.x * object.pivot.x,
                resolvedLocalSize.y * object.pivot.y);
        }

        ImVec2 Get_ObjectLocalRuntimePointFromPivot(
            const UI_Object& object,
            const ImVec2& localOffsetFromPivot)
        {
            const float scaleX = object.type == UI_ObjectType::Text ? Get_ObjectTextScale(object) : object.scale.x;
            const float scaleY = object.type == UI_ObjectType::Text ? Get_ObjectTextScale(object) : object.scale.y;
            const ImVec2 scaledOffset = ImVec2(
                localOffsetFromPivot.x * scaleX,
                localOffsetFromPivot.y * scaleY);

            return Rotate_Point(scaledOffset, object.rotationRadians);
        }

        float Clamp01(float value)
        {
            return min(max(value, 0.f), 1.f);
        }

        ImVec2 Clamp01(const ImVec2& value)
        {
            return ImVec2(Clamp01(value.x), Clamp01(value.y));
        }

        bool NearlyEqual(const ImVec2& lhs, const ImVec2& rhs)
        {
            return fabsf(lhs.x - rhs.x) <= 0.001f && fabsf(lhs.y - rhs.y) <= 0.001f;
        }
    }

    ImVec2 UI_Document::Get_ParentAnchorReferenceSize(ObjectId objectId) const
    {
        const UI_Object* object = Find_Object(objectId);
        if (nullptr == object || object->parentId == 0)
            return _canvasSize;

        return Get_ResolvedLocalSize(object->parentId);
    }

    ImVec2 UI_Document::Get_ResolvedLocalPosition(ObjectId objectId) const
    {
        const UI_Object* object = Find_Object(objectId);
        if (nullptr == object)
            return ImVec2(0.f, 0.f);

        const ImVec2 parentReferenceSize = Get_ParentAnchorReferenceSize(objectId);
        return ImVec2(
            object->position.x + parentReferenceSize.x * object->anchorMin.x,
            object->position.y + parentReferenceSize.y * object->anchorMin.y);
    }

    ImVec2 UI_Document::Get_ResolvedLocalTopLeft(ObjectId objectId) const
    {
        const UI_Object* object = Find_Object(objectId);
        if (nullptr == object)
            return ImVec2(0.f, 0.f);

        const ImVec2 pivotPosition = Get_ResolvedLocalPosition(objectId);
        const ImVec2 resolvedSize = Get_ResolvedLocalSize(objectId);
        const ImVec2 pivotOffset = Get_ObjectLocalPivotOffset(*object, resolvedSize);

        return ImVec2(
            pivotPosition.x - pivotOffset.x,
            pivotPosition.y - pivotOffset.y);
    }

    ImVec2 UI_Document::Get_ResolvedLocalSize(ObjectId objectId) const
    {
        const UI_Object* object = Find_Object(objectId);
        if (nullptr == object)
            return ImVec2(0.f, 0.f);

        const ImVec2 parentReferenceSize = Get_ParentAnchorReferenceSize(objectId);
        return ImVec2(
            max(1.f, object->size.x + parentReferenceSize.x * max(0.f, object->anchorMax.x - object->anchorMin.x)),
            max(1.f, object->size.y + parentReferenceSize.y * max(0.f, object->anchorMax.y - object->anchorMin.y)));
    }

    ImVec2 UI_Document::Get_WorldPosition(ObjectId objectId) const
    {
        const UI_Object* object = Find_Object(objectId);
        if (nullptr == object)
            return ImVec2(0.f, 0.f);

        const ImVec2 pivotPosition = Get_ResolvedLocalPosition(objectId);
        const ImVec2 resolvedSize = Get_ResolvedLocalSize(objectId);
        const ImVec2 pivotOffset = Get_ObjectLocalPivotOffset(*object, resolvedSize);
        const ImVec2 topLeftOffset = Get_ObjectLocalRuntimePointFromPivot(
            *object,
            ImVec2(-pivotOffset.x, -pivotOffset.y));
        const ImVec2 localTopLeft = ImVec2(
            pivotPosition.x + topLeftOffset.x,
            pivotPosition.y + topLeftOffset.y);

        return Transform_LocalPointToWorld(objectId, localTopLeft);
    }

    ImVec2 UI_Document::Transform_LocalPointToWorld(ObjectId objectId, const ImVec2& localPoint) const
    {
        ImVec2 worldPoint = localPoint;
        unordered_set<ObjectId> visitedIds;

        const UI_Object* currentObject = Find_Object(objectId);
        while (nullptr != currentObject && currentObject->parentId != 0)
        {
            const UI_Object* parentObject = Find_Object(currentObject->parentId);
            if (nullptr == parentObject)
                break;

            if (!visitedIds.insert(parentObject->id).second)
                break;

            const ImVec2 parentSize = Get_ResolvedLocalSize(parentObject->id);
            const ImVec2 parentPivotPosition = Get_ResolvedLocalPosition(parentObject->id);
            const ImVec2 pivotOffset = Get_ObjectLocalPivotOffset(*parentObject, parentSize);
            const ImVec2 scaledOffset = ImVec2(
                (worldPoint.x - pivotOffset.x) * parentObject->scale.x,
                (worldPoint.y - pivotOffset.y) * parentObject->scale.y);
            const ImVec2 rotatedOffset = Rotate_Point(scaledOffset, parentObject->rotationRadians);

            worldPoint = ImVec2(
                parentPivotPosition.x + rotatedOffset.x,
                parentPivotPosition.y + rotatedOffset.y);

            currentObject = parentObject;
        }

        return worldPoint;
    }

    ImVec2 UI_Document::Get_WorldCenter(ObjectId objectId) const
    {
        const UI_Object* object = Find_Object(objectId);
        if (nullptr == object)
            return ImVec2(0.f, 0.f);

        const ImVec2 resolvedLocalPosition = Get_ResolvedLocalPosition(objectId);
        const ImVec2 resolvedLocalSize = Get_ResolvedLocalSize(objectId);
        const ImVec2 pivotOffset = Get_ObjectLocalPivotOffset(*object, resolvedLocalSize);
        const ImVec2 baseCenterOffset = ImVec2(
            resolvedLocalSize.x * 0.5f,
            resolvedLocalSize.y * 0.5f);
        const ImVec2 centerOffset = Get_ObjectLocalRuntimePointFromPivot(
            *object,
            ImVec2(
                baseCenterOffset.x - pivotOffset.x,
                baseCenterOffset.y - pivotOffset.y));
        const ImVec2 localCenter = ImVec2(
            resolvedLocalPosition.x + centerOffset.x,
            resolvedLocalPosition.y + centerOffset.y);

        return Transform_LocalPointToWorld(objectId, localCenter);
    }

    ImVec2 UI_Document::Get_WorldSize(ObjectId objectId) const
    {
        const UI_Object* object = Find_Object(objectId);
        if (nullptr == object)
            return ImVec2(0.f, 0.f);

        const ImVec2 resolvedLocalSize = Get_ResolvedLocalSize(objectId);
        if (object->type == UI_ObjectType::Text)
        {
            float textScale = Get_ObjectTextScale(*object);
            unordered_set<ObjectId> visitedIds;
            for (const UI_Object* parentObject = Find_Object(object->parentId);
                nullptr != parentObject;
                parentObject = parentObject->parentId == 0 ? nullptr : Find_Object(parentObject->parentId))
            {
                if (!visitedIds.insert(parentObject->id).second)
                    break;

                textScale *= parentObject->scale.x;
            }

            textScale = max(0.01f, textScale);
            return ImVec2(
                max(1.f, resolvedLocalSize.x * textScale),
                max(1.f, resolvedLocalSize.y * textScale));
        }

        const ImVec2 worldScale = Get_WorldScale(objectId);
        return ImVec2(
            max(1.f, resolvedLocalSize.x * worldScale.x),
            max(1.f, resolvedLocalSize.y * worldScale.y));
    }

    ImVec2 UI_Document::Get_WorldScale(ObjectId objectId) const
    {
        const UI_Object* currentObject = Find_Object(objectId);
        if (nullptr == currentObject)
            return ImVec2(1.f, 1.f);

        ImVec2 worldScale = currentObject->scale;
        unordered_set<ObjectId> visitedIds;

        while (nullptr != currentObject && currentObject->parentId != 0)
        {
            const UI_Object* parentObject = Find_Object(currentObject->parentId);
            if (nullptr == parentObject)
                break;

            if (!visitedIds.insert(parentObject->id).second)
                break;

            worldScale.x *= parentObject->scale.x;
            worldScale.y *= parentObject->scale.y;
            currentObject = parentObject;
        }

        return worldScale;
    }

    float UI_Document::Get_WorldRotationRadians(ObjectId objectId) const
    {
        const UI_Object* currentObject = Find_Object(objectId);
        if (nullptr == currentObject)
            return 0.f;

        float worldRotationRadians = currentObject->rotationRadians;
        unordered_set<ObjectId> visitedIds;

        while (nullptr != currentObject && currentObject->parentId != 0)
        {
            const UI_Object* parentObject = Find_Object(currentObject->parentId);
            if (nullptr == parentObject)
                break;

            if (!visitedIds.insert(parentObject->id).second)
                break;

            worldRotationRadians += parentObject->rotationRadians;
            currentObject = parentObject;
        }

        return worldRotationRadians;
    }

    bool UI_Document::Set_WorldPosition(ObjectId objectId, const ImVec2& worldPosition)
    {
        UI_Object* object = Find_Object(objectId);
        if (nullptr == object)
            return false;

        if (NearlyEqual(Get_WorldPosition(objectId), worldPosition))
            return false;

        vector<const UI_Object*> parentChain;
        unordered_set<ObjectId> visitedIds;
        for (const UI_Object* parentObject = Find_Object(object->parentId);
            nullptr != parentObject;
            parentObject = parentObject->parentId == 0 ? nullptr : Find_Object(parentObject->parentId))
        {
            if (!visitedIds.insert(parentObject->id).second)
                break;

            parentChain.push_back(parentObject);
        }

        ImVec2 localPoint = worldPosition;
        for (auto iter = parentChain.rbegin(); iter != parentChain.rend(); ++iter)
        {
            const UI_Object* parentObject = *iter;
            const ImVec2 parentSize = Get_ResolvedLocalSize(parentObject->id);
            const ImVec2 parentPivotPosition = Get_ResolvedLocalPosition(parentObject->id);
            const ImVec2 pivotOffset = Get_ObjectLocalPivotOffset(*parentObject, parentSize);
            const ImVec2 translated = ImVec2(
                localPoint.x - parentPivotPosition.x,
                localPoint.y - parentPivotPosition.y);
            const ImVec2 unrotated = Rotate_Point(translated, -parentObject->rotationRadians);
            const float safeScaleX = fabsf(parentObject->scale.x) > 0.0001f ? parentObject->scale.x : 0.0001f;
            const float safeScaleY = fabsf(parentObject->scale.y) > 0.0001f ? parentObject->scale.y : 0.0001f;

            localPoint = ImVec2(
                unrotated.x / safeScaleX + pivotOffset.x,
                unrotated.y / safeScaleY + pivotOffset.y);
        }

        const ImVec2 parentReferenceSize = Get_ParentAnchorReferenceSize(objectId);
        const ImVec2 resolvedSize = Get_ResolvedLocalSize(objectId);
        const ImVec2 pivotOffset = Get_ObjectLocalPivotOffset(*object, resolvedSize);
        const ImVec2 topLeftOffset = Get_ObjectLocalRuntimePointFromPivot(
            *object,
            ImVec2(-pivotOffset.x, -pivotOffset.y));
        const ImVec2 localPivot = ImVec2(
            localPoint.x - topLeftOffset.x,
            localPoint.y - topLeftOffset.y);

        object->position = ImVec2(
            localPivot.x - parentReferenceSize.x * object->anchorMin.x,
            localPivot.y - parentReferenceSize.y * object->anchorMin.y);
        return true;
    }

    bool UI_Document::Set_WorldRect(ObjectId objectId, const ImVec2& worldPosition, const ImVec2& worldSize)
    {
        UI_Object* object = Find_Object(objectId);
        if (nullptr == object)
            return false;

        const ImVec2 previousWorldPosition = Get_WorldPosition(objectId);
        const ImVec2 previousWorldSize = Get_WorldSize(objectId);
        if (NearlyEqual(previousWorldPosition, worldPosition) && NearlyEqual(previousWorldSize, worldSize))
            return false;

        const ImVec2 worldScale = Get_WorldScale(objectId);
        const ImVec2 parentReferenceSize = Get_ParentAnchorReferenceSize(objectId);
        const float anchorWidth = parentReferenceSize.x * max(0.f, object->anchorMax.x - object->anchorMin.x);
        const float anchorHeight = parentReferenceSize.y * max(0.f, object->anchorMax.y - object->anchorMin.y);

        if (object->type == UI_ObjectType::Text)
        {
            const float fontScale = max(4.f, object->fontSize) / 32.f;
            const float safeTextScale = max(0.01f, worldScale.x * fontScale);
            object->size = ImVec2(
                max(1.f, worldSize.x) / safeTextScale - anchorWidth,
                max(1.f, worldSize.y) / safeTextScale - anchorHeight);
        }
        else
        {
            const float safeScaleX = fabsf(worldScale.x) > 0.0001f ? worldScale.x : 0.0001f;
            const float safeScaleY = fabsf(worldScale.y) > 0.0001f ? worldScale.y : 0.0001f;
            object->size = ImVec2(
                max(1.f, worldSize.x) / safeScaleX - anchorWidth,
                max(1.f, worldSize.y) / safeScaleY - anchorHeight);
        }

        const bool positionChanged = Set_WorldPosition(objectId, worldPosition);
        return positionChanged || !NearlyEqual(previousWorldSize, Get_WorldSize(objectId));
    }

    bool UI_Document::Set_PivotKeepWorldRect(ObjectId objectId, const ImVec2& pivot)
    {
        UI_Object* object = Find_Object(objectId);
        if (nullptr == object)
            return false;

        const ImVec2 worldPosition = Get_WorldPosition(objectId);
        const ImVec2 worldSize = Get_WorldSize(objectId);

        const ImVec2 previousPivot = object->pivot;
        object->pivot = Clamp01(pivot);

        if (!Set_WorldRect(objectId, worldPosition, worldSize))
        {
            object->pivot = previousPivot;
            return false;
        }

        return true;
    }

    bool UI_Document::Set_AnchorsKeepWorldRect(ObjectId objectId, const ImVec2& anchorMin, const ImVec2& anchorMax)
    {
        UI_Object* object = Find_Object(objectId);
        if (nullptr == object)
            return false;

        const ImVec2 worldPosition = Get_WorldPosition(objectId);
        const ImVec2 worldSize = Get_WorldSize(objectId);

        const ImVec2 previousAnchorMin = object->anchorMin;
        const ImVec2 previousAnchorMax = object->anchorMax;
        object->anchorMin = Clamp01(anchorMin);
        object->anchorMax = Clamp01(anchorMax);

        if (!Set_WorldRect(objectId, worldPosition, worldSize))
        {
            object->anchorMin = previousAnchorMin;
            object->anchorMax = previousAnchorMax;
            return false;
        }

        return true;
    }
}
