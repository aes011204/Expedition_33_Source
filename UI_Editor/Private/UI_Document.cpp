#include "pch.h"
#include "UI_Document.h"
#include "UI_DocumentSerializer.h"

#include <cmath>

namespace UIEditor
{
    namespace
    {
        constexpr const char* kDefaultCanvasName = "MainCanvas";
        constexpr const char* kDefaultFontPath = "../Bin/Resources/Fonts/33_MainKR_Font.spritefont";

        string Make_FontTagFromPath(const string& fontPath)
        {
            if (fontPath.empty())
                return {};

            return String::ToString(fs::path(String::ToWString(fontPath)).stem().wstring());
        }

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
    }

    UI_Document::UI_Document()
    {
        Reset();
    }

    void UI_Document::Reset()
    {
        _canvasName = kDefaultCanvasName;
        _canvasSize = ImVec2(1600.f, 900.f);
        _nextObjectId = 1;

        _objects.clear();
    }

    const string& UI_Document::Get_CanvasName() const
    {
        return _canvasName;
    }

    const ImVec2& UI_Document::Get_CanvasSize() const
    {
        return _canvasSize;
    }

    const vector<UI_Object>& UI_Document::Get_Objects() const
    {
        return _objects;
    }

    vector<UI_Object>& UI_Document::Get_MutableObjects()
    {
        return _objects;
    }

    ObjectId UI_Document::Create_Object(UI_ObjectType objectType, ObjectId parentId)
    {
        if (parentId != 0 && !Can_HaveChildren(parentId))
            parentId = 0;

        UI_Object object = {};
        object.id = _nextObjectId++;
        object.parentId = parentId;
        object.type = objectType;
        object.position = ImVec2(100.f, 100.f);
        object.scale = ImVec2(1.f, 1.f);
        object.rotationRadians = 0.f;
        object.sortOrder = static_cast<int32>(_objects.size());

        switch (objectType)
        {
        case UI_ObjectType::Image:
            object.name = Generate_UniqueName("Image");
            object.size = ImVec2(100.f, 100.f);
            object.color = ImVec4(1.f, 1.f, 1.f, 1.f);
            break;

        case UI_ObjectType::Text:
            object.name = Generate_UniqueName("Text");
            object.size = ImVec2(300.f, 50.f);
            object.color = ImVec4(1.f, 1.f, 1.f, 1.f);
            object.text = "New Text";
            object.fontSize = 32.f;
            object.textAlign = UI_TextAlign::Left;
            object.textWrap = false;
            object.fontPath = kDefaultFontPath;
            object.fontTag = Make_FontTagFromPath(object.fontPath);
            break;

        case UI_ObjectType::Panel:
            object.name = Generate_UniqueName("Panel");
            object.size = ImVec2(400.f, 300.f);
            object.color = ImVec4(0.35f, 0.35f, 0.35f, 0.6f);
            break;

        case UI_ObjectType::Button:
            object.name = Generate_UniqueName("Button");
            object.size = ImVec2(200.f, 60.f);
            object.color = ImVec4(0.25f, 0.44f, 0.72f, 0.9f);
            break;

        default:
            object.name = Generate_UniqueName("Object");
            object.size = ImVec2(100.f, 100.f);
            object.color = ImVec4(1.f, 1.f, 1.f, 1.f);
            break;
        }

        _objects.push_back(object);
        return object.id;
    }

    ObjectId UI_Document::Duplicate_Object(ObjectId objectId, const ImVec2& offset)
    {
        const UI_Object* sourceObject = Find_Object(objectId);
        if (nullptr == sourceObject)
            return 0;

        return Duplicate_ObjectRecursive(*sourceObject, sourceObject->parentId, offset, true);
    }

    vector<ObjectId> UI_Document::Import_PrefabObjects(
        const vector<UI_Object>& sourceObjects,
        const vector<ObjectId>& rootObjectIds,
        ObjectId parentId,
        const ImVec2& rootOffset)
    {
        if (sourceObjects.empty())
            return {};

        if (parentId != 0 && !Can_HaveChildren(parentId))
            parentId = 0;

        unordered_map<ObjectId, const UI_Object*> sourceObjectById;
        sourceObjectById.reserve(sourceObjects.size());
        for (const UI_Object& sourceObject : sourceObjects)
        {
            if (sourceObject.id == 0)
                continue;

            sourceObjectById.emplace(sourceObject.id, &sourceObject);
        }

        if (sourceObjectById.empty())
            return {};

        vector<ObjectId> effectiveRootObjectIds;
        unordered_set<ObjectId> rootObjectIdSet;
        for (ObjectId rootObjectId : rootObjectIds)
        {
            if (sourceObjectById.find(rootObjectId) == sourceObjectById.end())
                continue;

            if (rootObjectIdSet.insert(rootObjectId).second)
                effectiveRootObjectIds.push_back(rootObjectId);
        }

        if (effectiveRootObjectIds.empty())
        {
            for (const UI_Object& sourceObject : sourceObjects)
            {
                if (sourceObject.id != 0 && sourceObject.parentId == 0 && rootObjectIdSet.insert(sourceObject.id).second)
                    effectiveRootObjectIds.push_back(sourceObject.id);
            }
        }

        if (effectiveRootObjectIds.empty())
            return {};

        unordered_map<ObjectId, ObjectId> importedObjectIds;
        unordered_set<ObjectId> importingObjectIds;

        const auto belongsToRoot = [&](ObjectId objectId)
        {
            unordered_set<ObjectId> visitedObjectIds;
            ObjectId currentObjectId = objectId;
            while (currentObjectId != 0)
            {
                if (!visitedObjectIds.insert(currentObjectId).second)
                    return false;

                if (rootObjectIdSet.find(currentObjectId) != rootObjectIdSet.end())
                    return true;

                const auto sourceIter = sourceObjectById.find(currentObjectId);
                if (sourceIter == sourceObjectById.end())
                    return false;

                currentObjectId = sourceIter->second->parentId;
            }

            return false;
        };

        int32 maxExistingSortOrder = -1;
        for (const UI_Object& object : _objects)
            maxExistingSortOrder = max(maxExistingSortOrder, object.sortOrder);

        int32 minPrefabSortOrder = numeric_limits<int32>::max();
        for (const UI_Object& sourceObject : sourceObjects)
        {
            if (sourceObject.id != 0 && belongsToRoot(sourceObject.id))
                minPrefabSortOrder = min(minPrefabSortOrder, sourceObject.sortOrder);
        }

        if (minPrefabSortOrder == numeric_limits<int32>::max())
            minPrefabSortOrder = 0;

        const int64 sortOrderBase = static_cast<int64>(maxExistingSortOrder) + 1;
        const auto resolveImportedSortOrder = [&](const UI_Object& sourceObject)
        {
            const int64 relativeSortOrder = static_cast<int64>(sourceObject.sortOrder) - static_cast<int64>(minPrefabSortOrder);
            const int64 resolvedSortOrder = sortOrderBase + max<int64>(0, relativeSortOrder);
            return static_cast<int32>(min<int64>(resolvedSortOrder, numeric_limits<int32>::max()));
        };

        const auto importObject = [&](auto&& self, ObjectId sourceObjectId) -> ObjectId
        {
            const auto importedIter = importedObjectIds.find(sourceObjectId);
            if (importedIter != importedObjectIds.end())
                return importedIter->second;

            const auto sourceIter = sourceObjectById.find(sourceObjectId);
            if (sourceIter == sourceObjectById.end())
                return 0;

            if (!importingObjectIds.insert(sourceObjectId).second)
            {
                LOG_WARN("UI prefab import skipped cyclic object id: {}", sourceObjectId);
                return 0;
            }

            const UI_Object& sourceObject = *sourceIter->second;
            const bool isRootObject = rootObjectIdSet.find(sourceObject.id) != rootObjectIdSet.end();

            ObjectId importedParentId = parentId;
            if (!isRootObject)
            {
                importedParentId = self(self, sourceObject.parentId);
                if (importedParentId == 0)
                    importedParentId = parentId;
            }

            if (importedParentId != 0 && !Can_HaveChildren(importedParentId))
                importedParentId = parentId;
            if (importedParentId != 0 && !Can_HaveChildren(importedParentId))
                importedParentId = 0;

            UI_Object importedObject = sourceObject;
            importedObject.id = _nextObjectId++;
            importedObject.parentId = importedParentId;
            importedObject.name = Generate_UniqueName(sourceObject.name.empty() ? "Object" : sourceObject.name);
            if (isRootObject)
            {
                importedObject.position.x += rootOffset.x;
                importedObject.position.y += rootOffset.y;
            }
            importedObject.sortOrder = resolveImportedSortOrder(sourceObject);

            const ObjectId importedObjectId = importedObject.id;
            _objects.push_back(importedObject);
            importedObjectIds[sourceObject.id] = importedObjectId;
            importingObjectIds.erase(sourceObject.id);
            return importedObjectId;
        };

        vector<ObjectId> importedRootObjectIds;
        importedRootObjectIds.reserve(effectiveRootObjectIds.size());
        for (ObjectId rootObjectId : effectiveRootObjectIds)
        {
            const ObjectId importedRootObjectId = importObject(importObject, rootObjectId);
            if (importedRootObjectId != 0)
                importedRootObjectIds.push_back(importedRootObjectId);
        }

        for (const UI_Object& sourceObject : sourceObjects)
        {
            if (sourceObject.id != 0 && belongsToRoot(sourceObject.id))
                importObject(importObject, sourceObject.id);
        }

        return importedRootObjectIds;
    }

    bool UI_Document::Delete_Object(ObjectId objectId)
    {
        if (objectId == 0 || nullptr == Find_Object(objectId))
            return false;

        vector<ObjectId> deleteObjectIds;
        Collect_ObjectAndChildren(objectId, deleteObjectIds);

        _objects.erase(
            remove_if(
                _objects.begin(),
                _objects.end(),
                [&deleteObjectIds](const UI_Object& object)
                {
                    return find(deleteObjectIds.begin(), deleteObjectIds.end(), object.id) != deleteObjectIds.end();
                }),
            _objects.end());

        return true;
    }

    bool UI_Document::Change_Parent(ObjectId objectId, ObjectId newParentId, bool keepWorldPosition)
    {
        UI_Object* object = Find_Object(objectId);
        if (nullptr == object || !Can_Parent(objectId, newParentId))
            return false;

        if (object->parentId == newParentId)
            return true;

        const ImVec2 oldWorldPosition = Get_WorldPosition(objectId);
        object->parentId = newParentId;

        if (keepWorldPosition)
            Set_WorldPosition(objectId, oldWorldPosition);

        return true;
    }

    bool UI_Document::Can_Parent(ObjectId objectId, ObjectId newParentId) const
    {
        if (newParentId == 0)
            return true;

        const UI_Object* parentObject = Find_Object(newParentId);
        if (nullptr == parentObject)
            return false;

        if (parentObject->type != UI_ObjectType::Panel && parentObject->type != UI_ObjectType::Button)
            return false;

        if (objectId == 0)
            return true;

        if (objectId == newParentId)
            return false;

        if (nullptr == Find_Object(objectId))
            return false;

        return !Is_Descendant(objectId, newParentId);
    }

    bool UI_Document::Can_HaveChildren(ObjectId objectId) const
    {
        if (objectId == 0)
            return true;

        const UI_Object* object = Find_Object(objectId);
        if (nullptr == object)
            return false;

        return object->type == UI_ObjectType::Panel || object->type == UI_ObjectType::Button;
    }

    bool UI_Document::Is_Descendant(ObjectId ancestorId, ObjectId candidateDescendantId) const
    {
        if (ancestorId == 0 || candidateDescendantId == 0)
            return false;

        unordered_set<ObjectId> visitedIds;
        const UI_Object* currentObject = Find_Object(candidateDescendantId);
        while (nullptr != currentObject && currentObject->parentId != 0)
        {
            if (currentObject->parentId == ancestorId)
                return true;

            if (visitedIds.find(currentObject->parentId) != visitedIds.end())
                return true;

            visitedIds.insert(currentObject->parentId);
            currentObject = Find_Object(currentObject->parentId);
        }

        return false;
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

        return Set_WorldPosition(objectId, worldPosition);
    }

    bool UI_Document::Set_PivotKeepWorldRect(ObjectId objectId, const ImVec2& pivot)
    {
        UI_Object* object = Find_Object(objectId);
        if (nullptr == object)
            return false;

        const ImVec2 worldSize = Get_WorldSize(objectId);
        const ImVec2 worldCenter = Get_WorldCenter(objectId);
        const ImVec2 worldPosition = ImVec2(
            worldCenter.x - worldSize.x * 0.5f,
            worldCenter.y - worldSize.y * 0.5f);

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

        const ImVec2 worldSize = Get_WorldSize(objectId);
        const ImVec2 worldCenter = Get_WorldCenter(objectId);
        const ImVec2 worldPosition = ImVec2(
            worldCenter.x - worldSize.x * 0.5f,
            worldCenter.y - worldSize.y * 0.5f);

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

    string UI_Document::Generate_UniqueName(const string& baseName, ObjectId ignoredObjectId) const
    {
        const string normalizedBaseName = baseName.empty() ? "Object" : baseName;

        auto nameExists = [this, ignoredObjectId](const string& name)
        {
            for (const UI_Object& object : _objects)
            {
                if (object.id != ignoredObjectId && object.name == name)
                    return true;
            }

            return false;
        };

        if (!nameExists(normalizedBaseName))
            return normalizedBaseName;

        for (uint32 index = 1; index < 10000; ++index)
        {
            char suffix[16] = {};
            sprintf_s(suffix, "_%03u", index);

            const string candidateName = normalizedBaseName + suffix;
            if (!nameExists(candidateName))
                return candidateName;
        }

        return normalizedBaseName + "_" + to_string(_nextObjectId);
    }

    string UI_Document::Generate_DuplicateName(const string& sourceName) const
    {
        const string baseName = (sourceName.empty() ? "Object" : sourceName) + "_Copy";

        for (uint32 index = 1; index < 10000; ++index)
        {
            char suffix[16] = {};
            sprintf_s(suffix, "%03u", index);

            const string candidateName = baseName + suffix;
            bool exists = false;
            for (const UI_Object& object : _objects)
            {
                if (object.name == candidateName)
                {
                    exists = true;
                    break;
                }
            }

            if (!exists)
                return candidateName;
        }

        return baseName + to_string(_nextObjectId);
    }

    UI_Object* UI_Document::Find_Object(ObjectId objectId)
    {
        for (UI_Object& object : _objects)
        {
            if (object.id == objectId)
                return &object;
        }

        return nullptr;
    }

    const UI_Object* UI_Document::Find_Object(ObjectId objectId) const
    {
        for (const UI_Object& object : _objects)
        {
            if (object.id == objectId)
                return &object;
        }

        return nullptr;
    }

    bool UI_Document::Save_ToJsonFile(const wstring& filePath) const
    {
        return UI_DocumentSerializer::Save_ToJsonFile(*this, filePath);
    }

    bool UI_Document::Load_FromJsonFile(const wstring& filePath)
    {
        return UI_DocumentSerializer::Load_FromJsonFile(*this, filePath);
    }

    json UI_Document::To_Json() const
    {
        return UI_DocumentSerializer::To_Json(*this);
    }

    bool UI_Document::From_Json(const json& root)
    {
        return UI_DocumentSerializer::From_Json(*this, root);
    }

    void UI_Document::Set_CanvasSize(const ImVec2& canvasSize)
    {
        _canvasSize.x = max(1.f, canvasSize.x);
        _canvasSize.y = max(1.f, canvasSize.y);
    }

    void UI_Document::Set_CanvasName(const string& canvasName)
    {
        _canvasName = canvasName.empty() ? kDefaultCanvasName : canvasName;
    }

    void UI_Document::Resolve_NextObjectId()
    {
        ObjectId maxObjectId = 0;
        for (const UI_Object& object : _objects)
            maxObjectId = max(maxObjectId, object.id);

        if (maxObjectId < numeric_limits<ObjectId>::max())
        {
            _nextObjectId = maxObjectId + 1;
            if (_nextObjectId == 0)
                _nextObjectId = 1;
            return;
        }

        unordered_set<ObjectId> usedIds;
        for (const UI_Object& object : _objects)
            usedIds.insert(object.id);

        for (ObjectId candidateId = 1; candidateId < numeric_limits<ObjectId>::max(); ++candidateId)
        {
            if (usedIds.find(candidateId) == usedIds.end())
            {
                _nextObjectId = candidateId;
                return;
            }
        }

        _nextObjectId = 1;
    }

    void UI_Document::Validate_ParentLinks()
    {
        unordered_set<ObjectId> validObjectIds;
        for (const UI_Object& object : _objects)
        {
            if (object.id != 0)
                validObjectIds.insert(object.id);
        }

        for (UI_Object& object : _objects)
        {
            if (object.parentId == 0)
                continue;

            if (object.parentId == object.id || validObjectIds.find(object.parentId) == validObjectIds.end())
            {
                LOG_WARN("UI Document load: object {} has invalid parent {}. Reparenting to Canvas.", object.id, object.parentId);
                object.parentId = 0;
                continue;
            }

            unordered_set<ObjectId> visitedParentIds;
            ObjectId currentParentId = object.parentId;
            bool hasInvalidChain = false;

            while (currentParentId != 0)
            {
                if (currentParentId == object.id || visitedParentIds.find(currentParentId) != visitedParentIds.end())
                {
                    hasInvalidChain = true;
                    break;
                }

                visitedParentIds.insert(currentParentId);

                const UI_Object* parentObject = Find_Object(currentParentId);
                if (nullptr == parentObject)
                {
                    hasInvalidChain = true;
                    break;
                }

                currentParentId = parentObject->parentId;
            }

            if (hasInvalidChain)
            {
                LOG_WARN("UI Document load: object {} has cyclic parent chain. Reparenting to Canvas.", object.id);
                object.parentId = 0;
            }
        }
    }

    void UI_Document::Collect_ObjectAndChildren(ObjectId objectId, vector<ObjectId>& objectIds) const
    {
        objectIds.push_back(objectId);

        for (const UI_Object& object : _objects)
        {
            if (object.parentId == objectId)
                Collect_ObjectAndChildren(object.id, objectIds);
        }
    }

    ObjectId UI_Document::Duplicate_ObjectRecursive(UI_Object sourceObject, ObjectId parentId, const ImVec2& offset, bool applyOffset)
    {
        const ObjectId sourceObjectId = sourceObject.id;

        UI_Object duplicateObject = sourceObject;
        duplicateObject.id = _nextObjectId++;
        duplicateObject.parentId = parentId;
        duplicateObject.name = Generate_DuplicateName(sourceObject.name);
        if (applyOffset)
        {
            duplicateObject.position.x += offset.x;
            duplicateObject.position.y += offset.y;
        }
        duplicateObject.sortOrder = static_cast<int32>(_objects.size());

        _objects.push_back(duplicateObject);

        for (size_t i = 0; i < _objects.size(); ++i)
        {
            if (_objects[i].parentId == sourceObjectId)
            {
                UI_Object childSourceObject = _objects[i];
                Duplicate_ObjectRecursive(childSourceObject, duplicateObject.id, offset, false);
            }
        }

        return duplicateObject.id;
    }
}
