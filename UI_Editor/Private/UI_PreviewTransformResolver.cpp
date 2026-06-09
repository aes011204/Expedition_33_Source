#include "pch.h"
#include "UI_PreviewTransformResolver.h"
#include "UI_AnimationPreviewService.h"

namespace UIEditor
{
    void UI_PreviewTransformResolver::Reset(
        const UI_Document& document,
        const ImVec2& canvasSize,
        SampleFunc sampleFunc)
    {
        _document = &document;
        _canvasSize = canvasSize;
        _sampleFunc = sampleFunc;
        _previewObjectCache.clear();
        _resolvedLocalSizeCache.clear();
        _resolvedLocalPositionCache.clear();
        _worldScaleCache.clear();
        _worldRotationCache.clear();
        _effectiveAlphaMultiplierCache.clear();
        _worldFrameCache.clear();
    }

    const UI_Object* UI_PreviewTransformResolver::Find_Object(ObjectId objectId) const
    {
        return nullptr == _document ? nullptr : _document->Find_Object(objectId);
    }

    UI_Object UI_PreviewTransformResolver::Make_PreviewObject(const UI_Object& object) const
    {
        const auto cachedIter = _previewObjectCache.find(object.id);
        if (cachedIter != _previewObjectCache.end())
            return cachedIter->second;

        UI_Object previewObject = object;
        UI_AnimationPreviewSample sample = {};
        if (_sampleFunc && _sampleFunc(object, sample))
        {
            previewObject.position = sample.position;
            previewObject.size = sample.size;
            previewObject.scale = sample.scale;
            previewObject.rotationRadians = sample.rotationRadians;
            previewObject.color = sample.color;
            if (sample.hasAlphaMultiplier)
                previewObject.alphaMultiplier = sample.alphaMultiplier;
            if (sample.hasFontSize)
                previewObject.fontSize = sample.fontSize;
        }

        _previewObjectCache[object.id] = previewObject;
        return previewObject;
    }

    ImVec2 UI_PreviewTransformResolver::Get_ResolvedLocalSize(ObjectId objectId) const
    {
        const auto cachedIter = _resolvedLocalSizeCache.find(objectId);
        if (cachedIter != _resolvedLocalSizeCache.end())
            return cachedIter->second;

        unordered_set<ObjectId> resolvingIds;
        return Resolve_ResolvedLocalSize(objectId, resolvingIds);
    }

    ImVec2 UI_PreviewTransformResolver::Get_ResolvedLocalPosition(ObjectId objectId) const
    {
        const auto cachedIter = _resolvedLocalPositionCache.find(objectId);
        if (cachedIter != _resolvedLocalPositionCache.end())
            return cachedIter->second;

        unordered_set<ObjectId> resolvingIds;
        return Resolve_ResolvedLocalPosition(objectId, resolvingIds);
    }

    ImVec2 UI_PreviewTransformResolver::Get_ParentReferenceSize(const UI_Object& object) const
    {
        if (object.parentId == 0)
            return _canvasSize;

        return Get_ResolvedLocalSize(object.parentId);
    }

    ImVec2 UI_PreviewTransformResolver::Transform_LocalPointToWorld(ObjectId objectId, const ImVec2& localPoint) const
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

            const UI_Object previewParent = Make_PreviewObject(*parentObject);
            unordered_set<ObjectId> positionResolvingIds;
            unordered_set<ObjectId> sizeResolvingIds;
            const ImVec2 parentPivotPosition = Resolve_ResolvedLocalPosition(previewParent.id, positionResolvingIds);
            const ImVec2 parentSize = Resolve_ResolvedLocalSize(previewParent.id, sizeResolvingIds);
            const ImVec2 pivotOffset = Get_PivotOffset(previewParent, parentSize);
            const ImVec2 scaledOffset = ImVec2(
                (worldPoint.x - pivotOffset.x) * previewParent.scale.x,
                (worldPoint.y - pivotOffset.y) * previewParent.scale.y);
            const ImVec2 rotatedOffset = ImVec2(
                scaledOffset.x * cosf(previewParent.rotationRadians) - scaledOffset.y * sinf(previewParent.rotationRadians),
                scaledOffset.x * sinf(previewParent.rotationRadians) + scaledOffset.y * cosf(previewParent.rotationRadians));

            worldPoint = ImVec2(
                parentPivotPosition.x + rotatedOffset.x,
                parentPivotPosition.y + rotatedOffset.y);
            currentObject = parentObject;
        }

        return worldPoint;
    }

    ImVec2 UI_PreviewTransformResolver::Get_WorldScale(ObjectId objectId) const
    {
        const auto cachedIter = _worldScaleCache.find(objectId);
        if (cachedIter != _worldScaleCache.end())
            return cachedIter->second;

        unordered_set<ObjectId> resolvingIds;
        return Resolve_WorldScale(objectId, resolvingIds);
    }

    float UI_PreviewTransformResolver::Get_WorldRotation(ObjectId objectId) const
    {
        const auto cachedIter = _worldRotationCache.find(objectId);
        if (cachedIter != _worldRotationCache.end())
            return cachedIter->second;

        unordered_set<ObjectId> resolvingIds;
        return Resolve_WorldRotation(objectId, resolvingIds);
    }

    ImVec2 UI_PreviewTransformResolver::Get_WorldSize(ObjectId objectId) const
    {
        const UI_Object* object = Find_Object(objectId);
        if (nullptr == object)
            return ImVec2(0.f, 0.f);

        const UI_Object previewObject = Make_PreviewObject(*object);
        const ImVec2 resolvedLocalSize = Get_ResolvedLocalSize(objectId);
        if (previewObject.type == UI_ObjectType::Text)
        {
            const ImVec2 parentWorldScale = previewObject.parentId == 0 ? ImVec2(1.f, 1.f) : Get_WorldScale(previewObject.parentId);
            const float textScale = max(0.01f, Get_UITextRuntimeScale(previewObject) * parentWorldScale.x);
            return ImVec2(
                max(1.f, resolvedLocalSize.x * textScale),
                max(1.f, resolvedLocalSize.y * textScale));
        }

        const ImVec2 worldScale = Get_WorldScale(objectId);
        return ImVec2(
            max(1.f, resolvedLocalSize.x * worldScale.x),
            max(1.f, resolvedLocalSize.y * worldScale.y));
    }

    ImVec2 UI_PreviewTransformResolver::Get_WorldCenter(ObjectId objectId) const
    {
        const UI_Object* object = Find_Object(objectId);
        if (nullptr == object)
            return ImVec2(0.f, 0.f);

        const UI_Object previewObject = Make_PreviewObject(*object);
        const ImVec2 resolvedLocalPosition = Get_ResolvedLocalPosition(objectId);
        const ImVec2 resolvedLocalSize = Get_ResolvedLocalSize(objectId);
        const ImVec2 pivotOffset = Get_PivotOffset(previewObject, resolvedLocalSize);
        const ImVec2 baseCenterOffset = ImVec2(
            resolvedLocalSize.x * 0.5f,
            resolvedLocalSize.y * 0.5f);
        const ImVec2 centerOffset = Get_LocalRuntimePointFromPivot(
            previewObject,
            ImVec2(
                baseCenterOffset.x - pivotOffset.x,
                baseCenterOffset.y - pivotOffset.y));
        const ImVec2 localCenter = ImVec2(
            resolvedLocalPosition.x + centerOffset.x,
            resolvedLocalPosition.y + centerOffset.y);

        return Transform_LocalPointToWorld(objectId, localCenter);
    }

    ImVec2 UI_PreviewTransformResolver::Get_WorldPosition(ObjectId objectId) const
    {
        const UI_Object* object = Find_Object(objectId);
        if (nullptr == object)
            return ImVec2(0.f, 0.f);

        const UI_Object previewObject = Make_PreviewObject(*object);
        const ImVec2 resolvedLocalPosition = Get_ResolvedLocalPosition(objectId);
        const ImVec2 resolvedLocalSize = Get_ResolvedLocalSize(objectId);
        const ImVec2 pivotOffset = Get_PivotOffset(previewObject, resolvedLocalSize);
        const ImVec2 topLeftOffset = Get_LocalRuntimePointFromPivot(
            previewObject,
            ImVec2(-pivotOffset.x, -pivotOffset.y));
        const ImVec2 localTopLeft = ImVec2(
            resolvedLocalPosition.x + topLeftOffset.x,
            resolvedLocalPosition.y + topLeftOffset.y);

        return Transform_LocalPointToWorld(objectId, localTopLeft);
    }

    ImVec2 UI_PreviewTransformResolver::Get_WorldPivot(ObjectId objectId) const
    {
        return Transform_LocalPointToWorld(objectId, Get_ResolvedLocalPosition(objectId));
    }

    float UI_PreviewTransformResolver::Get_EffectiveAlphaMultiplier(ObjectId objectId) const
    {
        const auto cachedIter = _effectiveAlphaMultiplierCache.find(objectId);
        if (cachedIter != _effectiveAlphaMultiplierCache.end())
            return cachedIter->second;

        unordered_set<ObjectId> resolvingIds;
        const float alphaMultiplier = min(max(Resolve_EffectiveAlphaMultiplier(objectId, resolvingIds), 0.f), 1.f);
        _effectiveAlphaMultiplierCache[objectId] = alphaMultiplier;
        return alphaMultiplier;
    }

    ImVec4 UI_PreviewTransformResolver::Get_EffectiveRenderColor(ObjectId objectId, const ImVec4& color) const
    {
        ImVec4 renderColor = color;
        renderColor.w = min(max(renderColor.w, 0.f), 1.f) * Get_EffectiveAlphaMultiplier(objectId);
        return renderColor;
    }

    UI_WorldObjectFrame UI_PreviewTransformResolver::Get_WorldFrame(ObjectId objectId) const
    {
        const auto cachedIter = _worldFrameCache.find(objectId);
        if (cachedIter != _worldFrameCache.end())
            return cachedIter->second;

        UI_WorldObjectFrame frame = Build_UIWorldObjectFrame(
            objectId,
            Get_WorldPosition(objectId),
            Get_WorldCenter(objectId),
            Get_WorldSize(objectId),
            Get_WorldRotation(objectId));
        _worldFrameCache[objectId] = frame;
        return frame;
    }

    ImVec2 UI_PreviewTransformResolver::Resolve_ResolvedLocalSize(ObjectId objectId, unordered_set<ObjectId>& resolvingIds) const
    {
        const auto cachedIter = _resolvedLocalSizeCache.find(objectId);
        if (cachedIter != _resolvedLocalSizeCache.end())
            return cachedIter->second;

        const UI_Object* object = Find_Object(objectId);
        if (nullptr == object)
            return ImVec2(0.f, 0.f);

        if (!resolvingIds.insert(objectId).second)
            return object->size;

        const UI_Object previewObject = Make_PreviewObject(*object);
        const ImVec2 parentReferenceSize = previewObject.parentId == 0
            ? _canvasSize
            : Resolve_ResolvedLocalSize(previewObject.parentId, resolvingIds);
        const ImVec2 resolvedSize = ImVec2(
            max(1.f, previewObject.size.x + parentReferenceSize.x * max(0.f, previewObject.anchorMax.x - previewObject.anchorMin.x)),
            max(1.f, previewObject.size.y + parentReferenceSize.y * max(0.f, previewObject.anchorMax.y - previewObject.anchorMin.y)));

        resolvingIds.erase(objectId);
        _resolvedLocalSizeCache[objectId] = resolvedSize;
        return resolvedSize;
    }

    ImVec2 UI_PreviewTransformResolver::Resolve_ResolvedLocalPosition(ObjectId objectId, unordered_set<ObjectId>& resolvingIds) const
    {
        const auto cachedIter = _resolvedLocalPositionCache.find(objectId);
        if (cachedIter != _resolvedLocalPositionCache.end())
            return cachedIter->second;

        const UI_Object* object = Find_Object(objectId);
        if (nullptr == object)
            return ImVec2(0.f, 0.f);

        if (!resolvingIds.insert(objectId).second)
            return object->position;

        const UI_Object previewObject = Make_PreviewObject(*object);
        unordered_set<ObjectId> sizeResolvingIds;
        const ImVec2 parentReferenceSize = previewObject.parentId == 0
            ? _canvasSize
            : Resolve_ResolvedLocalSize(previewObject.parentId, sizeResolvingIds);
        const ImVec2 resolvedPosition = ImVec2(
            previewObject.position.x + parentReferenceSize.x * previewObject.anchorMin.x,
            previewObject.position.y + parentReferenceSize.y * previewObject.anchorMin.y);

        resolvingIds.erase(objectId);
        _resolvedLocalPositionCache[objectId] = resolvedPosition;
        return resolvedPosition;
    }

    ImVec2 UI_PreviewTransformResolver::Resolve_WorldScale(ObjectId objectId, unordered_set<ObjectId>& resolvingIds) const
    {
        const auto cachedIter = _worldScaleCache.find(objectId);
        if (cachedIter != _worldScaleCache.end())
            return cachedIter->second;

        const UI_Object* object = Find_Object(objectId);
        if (nullptr == object)
            return ImVec2(1.f, 1.f);

        if (!resolvingIds.insert(objectId).second)
            return object->scale;

        const UI_Object previewObject = Make_PreviewObject(*object);
        ImVec2 worldScale = previewObject.scale;
        if (previewObject.parentId != 0)
        {
            const ImVec2 parentScale = Resolve_WorldScale(previewObject.parentId, resolvingIds);
            worldScale.x *= parentScale.x;
            worldScale.y *= parentScale.y;
        }

        resolvingIds.erase(objectId);
        _worldScaleCache[objectId] = worldScale;
        return worldScale;
    }

    float UI_PreviewTransformResolver::Resolve_WorldRotation(ObjectId objectId, unordered_set<ObjectId>& resolvingIds) const
    {
        const auto cachedIter = _worldRotationCache.find(objectId);
        if (cachedIter != _worldRotationCache.end())
            return cachedIter->second;

        const UI_Object* object = Find_Object(objectId);
        if (nullptr == object)
            return 0.f;

        if (!resolvingIds.insert(objectId).second)
            return object->rotationRadians;

        const UI_Object previewObject = Make_PreviewObject(*object);
        float worldRotation = previewObject.rotationRadians;
        if (previewObject.parentId != 0)
            worldRotation += Resolve_WorldRotation(previewObject.parentId, resolvingIds);

        resolvingIds.erase(objectId);
        _worldRotationCache[objectId] = worldRotation;
        return worldRotation;
    }

    float UI_PreviewTransformResolver::Resolve_EffectiveAlphaMultiplier(ObjectId objectId, unordered_set<ObjectId>& resolvingIds) const
    {
        const auto cachedIter = _effectiveAlphaMultiplierCache.find(objectId);
        if (cachedIter != _effectiveAlphaMultiplierCache.end())
            return cachedIter->second;

        const UI_Object* object = Find_Object(objectId);
        if (nullptr == object)
            return 1.f;

        if (!resolvingIds.insert(objectId).second)
            return 1.f;

        const UI_Object previewObject = Make_PreviewObject(*object);
        float alphaMultiplier = min(max(previewObject.alphaMultiplier, 0.f), 1.f);
        if (previewObject.parentId != 0)
            alphaMultiplier *= Resolve_EffectiveAlphaMultiplier(previewObject.parentId, resolvingIds);

        resolvingIds.erase(objectId);
        _effectiveAlphaMultiplierCache[objectId] = alphaMultiplier;
        return alphaMultiplier;
    }

    ImVec2 UI_PreviewTransformResolver::Get_PivotOffset(const UI_Object& object, const ImVec2& resolvedSize) const
    {
        return ImVec2(
            resolvedSize.x * object.pivot.x,
            resolvedSize.y * object.pivot.y);
    }

    ImVec2 UI_PreviewTransformResolver::Get_LocalRuntimePointFromPivot(const UI_Object& object, const ImVec2& offsetFromPivot) const
    {
        const float scaleX = object.type == UI_ObjectType::Text ? Get_UITextRuntimeScale(object) : object.scale.x;
        const float scaleY = object.type == UI_ObjectType::Text ? Get_UITextRuntimeScale(object) : object.scale.y;
        const ImVec2 scaledOffset = ImVec2(
            offsetFromPivot.x * scaleX,
            offsetFromPivot.y * scaleY);
        const float cosAngle = cosf(object.rotationRadians);
        const float sinAngle = sinf(object.rotationRadians);

        return ImVec2(
            scaledOffset.x * cosAngle - scaledOffset.y * sinAngle,
            scaledOffset.x * sinAngle + scaledOffset.y * cosAngle);
    }
}
