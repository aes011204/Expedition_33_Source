#include "pch.h"
#include "UI_ObjectCommandService.h"
#include "UI_Editor_Utils.h"

namespace UIEditor
{
    namespace
    {
        struct ObjectRect
        {
            ObjectId id = 0;
            ImVec2 worldPosition = ImVec2(0.f, 0.f);
            ImVec2 size = ImVec2(0.f, 0.f);
            ImVec2 center = ImVec2(0.f, 0.f);
        };
    }

    ObjectId UI_ObjectCommandService::Resolve_CreateParentId(const UI_Document& document, ObjectId selectedObjectId) const
    {
        if (selectedObjectId == 0)
            return 0;

        if (Is_ObjectEffectivelyLocked(document, selectedObjectId))
            return 0;

        const UI_Object* selectedObject = document.Find_Object(selectedObjectId);
        if (nullptr == selectedObject)
            return 0;

        if (Is_ContainerObjectType(selectedObject->type))
            return selectedObject->id;

        return Is_ObjectEffectivelyLocked(document, selectedObject->parentId) ? 0 : selectedObject->parentId;
    }

    ObjectId UI_ObjectCommandService::Resolve_PasteParentId(const UI_Document& document, ObjectId selectedObjectId) const
    {
        const UI_Object* selectedObject = document.Find_Object(selectedObjectId);
        if (nullptr == selectedObject)
            return 0;

        const ObjectId parentId = selectedObject->parentId;
        if (parentId != 0 && Is_ObjectEffectivelyLocked(document, parentId))
            return 0;

        return document.Can_HaveChildren(parentId) ? parentId : 0;
    }

    ObjectId UI_ObjectCommandService::Create_Object(UI_Document& document, UI_ObjectType objectType, ObjectId parentId) const
    {
        const ObjectId createdObjectId = document.Create_Object(objectType, parentId);
        if (createdObjectId == 0)
            return 0;

        if (objectType == UI_ObjectType::Button)
            Configure_ButtonLabel(document, createdObjectId);

        return createdObjectId;
    }

    vector<ObjectId> UI_ObjectCommandService::Create_ObjectsFromPrefab(
        UI_Document& document,
        const UI_PrefabAsset& prefab,
        ObjectId parentId,
        const ImVec2& rootOffset) const
    {
        if (prefab.objects.empty() || prefab.rootObjectIds.empty())
            return {};

        return document.Import_PrefabObjects(prefab.objects, prefab.rootObjectIds, parentId, rootOffset);
    }

    bool UI_ObjectCommandService::Can_DuplicateObject(const UI_Document& document, ObjectId objectId) const
    {
        return nullptr != document.Find_Object(objectId) && !Is_ObjectEffectivelyLocked(document, objectId);
    }

    ObjectId UI_ObjectCommandService::Duplicate_Object(UI_Document& document, ObjectId objectId, const ImVec2& offset) const
    {
        if (!Can_DuplicateObject(document, objectId))
            return 0;

        return document.Duplicate_Object(objectId, offset);
    }

    vector<ObjectId> UI_ObjectCommandService::Duplicate_Objects(
        UI_Document& document,
        const vector<ObjectId>& objectIds,
        const ImVec2& offset) const
    {
        vector<ObjectId> duplicatedObjectIds;
        duplicatedObjectIds.reserve(objectIds.size());

        for (ObjectId objectId : objectIds)
        {
            const ObjectId duplicatedObjectId = Duplicate_Object(document, objectId, offset);
            if (duplicatedObjectId != 0)
                duplicatedObjectIds.push_back(duplicatedObjectId);
        }

        return duplicatedObjectIds;
    }

    bool UI_ObjectCommandService::Can_DeleteObject(const UI_Document& document, ObjectId objectId) const
    {
        return nullptr != document.Find_Object(objectId) && !Is_ObjectEffectivelyLocked(document, objectId);
    }

    bool UI_ObjectCommandService::Delete_Object(UI_Document& document, ObjectId objectId) const
    {
        if (!Can_DeleteObject(document, objectId))
            return false;

        return document.Delete_Object(objectId);
    }

    bool UI_ObjectCommandService::Delete_Objects(UI_Document& document, const vector<ObjectId>& objectIds) const
    {
        bool deletedAny = false;
        for (ObjectId objectId : objectIds)
            deletedAny |= Delete_Object(document, objectId);

        return deletedAny;
    }

    bool UI_ObjectCommandService::Can_RenameCanvas(const string& newName) const
    {
        return !newName.empty();
    }

    bool UI_ObjectCommandService::Rename_Canvas(UI_Document& document, const string& newName) const
    {
        if (!Can_RenameCanvas(newName))
            return false;

        document.Set_CanvasName(newName);
        return true;
    }

    bool UI_ObjectCommandService::Can_RenameObject(const UI_Document& document, ObjectId objectId, const string& newName) const
    {
        return !newName.empty() &&
            nullptr != document.Find_Object(objectId) &&
            !Is_ObjectEffectivelyLocked(document, objectId);
    }

    string UI_ObjectCommandService::Resolve_RenameObjectName(
        const UI_Document& document,
        ObjectId objectId,
        const string& newName) const
    {
        return document.Generate_UniqueName(newName, objectId);
    }

    bool UI_ObjectCommandService::Rename_Object(UI_Document& document, ObjectId objectId, const string& newName) const
    {
        if (!Can_RenameObject(document, objectId, newName))
            return false;

        UI_Object* object = document.Find_Object(objectId);
        if (nullptr == object)
            return false;

        object->name = Resolve_RenameObjectName(document, objectId, newName);
        return true;
    }

    bool UI_ObjectCommandService::Can_ReparentObject(const UI_Document& document, ObjectId objectId, ObjectId newParentId) const
    {
        if (Is_ObjectEffectivelyLocked(document, objectId) || Is_ObjectEffectivelyLocked(document, newParentId))
            return false;

        return document.Can_Parent(objectId, newParentId);
    }

    bool UI_ObjectCommandService::Reparent_Object(
        UI_Document& document,
        ObjectId objectId,
        ObjectId newParentId,
        bool keepWorldPosition) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        if (nullptr == object || object->parentId == newParentId)
            return false;

        if (!Can_ReparentObject(document, objectId, newParentId))
            return false;

        return document.Change_Parent(objectId, newParentId, keepWorldPosition);
    }

    bool UI_ObjectCommandService::Is_ObjectEffectivelyVisible(const UI_Document& document, ObjectId objectId) const
    {
        unordered_set<ObjectId> visitedObjectIds;
        const UI_Object* object = document.Find_Object(objectId);

        while (nullptr != object)
        {
            if (!visitedObjectIds.insert(object->id).second)
                break;

            if (!object->visible)
                return false;

            if (object->parentId == 0)
                break;

            object = document.Find_Object(object->parentId);
        }

        return nullptr != document.Find_Object(objectId);
    }

    bool UI_ObjectCommandService::Is_ObjectEffectivelyLocked(const UI_Document& document, ObjectId objectId) const
    {
        unordered_set<ObjectId> visitedObjectIds;
        const UI_Object* object = document.Find_Object(objectId);

        while (nullptr != object)
        {
            if (!visitedObjectIds.insert(object->id).second)
                break;

            if (object->locked)
                return true;

            if (object->parentId == 0)
                break;

            object = document.Find_Object(object->parentId);
        }

        return false;
    }

    vector<ObjectId> UI_ObjectCommandService::Filter_UnlockedObjectIds(
        const UI_Document& document,
        const vector<ObjectId>& objectIds) const
    {
        vector<ObjectId> unlockedObjectIds = objectIds;
        unlockedObjectIds.erase(
            remove_if(
                unlockedObjectIds.begin(),
                unlockedObjectIds.end(),
                [this, &document](ObjectId objectId)
                {
                    return Is_ObjectEffectivelyLocked(document, objectId);
                }),
            unlockedObjectIds.end());

        return unlockedObjectIds;
    }

    bool UI_ObjectCommandService::Can_SetObjectVisible(const UI_Document& document, ObjectId objectId, bool visible) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && object->visible != visible;
    }

    bool UI_ObjectCommandService::Set_ObjectVisible(UI_Document& document, ObjectId objectId, bool visible) const
    {
        UI_Object* object = document.Find_Object(objectId);
        if (nullptr == object || object->visible == visible)
            return false;

        object->visible = visible;
        return true;
    }

    bool UI_ObjectCommandService::Can_SetObjectLocked(const UI_Document& document, ObjectId objectId, bool locked) const
    {
        const UI_Object* object = document.Find_Object(objectId);
        return nullptr != object && object->locked != locked;
    }

    bool UI_ObjectCommandService::Set_ObjectLocked(UI_Document& document, ObjectId objectId, bool locked) const
    {
        UI_Object* object = document.Find_Object(objectId);
        if (nullptr == object || object->locked == locked)
            return false;

        object->locked = locked;
        return true;
    }

    bool UI_ObjectCommandService::Can_AlignObjects(size_t objectCount, UI_AlignmentCommand command) const
    {
        if (objectCount < 2)
            return false;

        if ((command == UI_AlignmentCommand::DistributeHorizontal ||
            command == UI_AlignmentCommand::DistributeVertical) &&
            objectCount < 3)
        {
            return false;
        }

        return true;
    }

    bool UI_ObjectCommandService::Align_Objects(
        UI_Document& document,
        const vector<ObjectId>& objectIds,
        UI_AlignmentCommand command) const
    {
        if (!Can_AlignObjects(objectIds.size(), command))
            return false;

        vector<ObjectRect> objectRects;
        objectRects.reserve(objectIds.size());

        ImVec2 selectionMin = ImVec2(FLT_MAX, FLT_MAX);
        ImVec2 selectionMax = ImVec2(-FLT_MAX, -FLT_MAX);

        for (ObjectId objectId : objectIds)
        {
            const UI_Object* object = document.Find_Object(objectId);
            if (nullptr == object)
                continue;

            ObjectRect rect = {};
            rect.id = objectId;
            const ImVec2 worldCenter = document.Get_WorldCenter(objectId);
            rect.size = document.Get_WorldSize(objectId);
            rect.worldPosition = ImVec2(
                worldCenter.x - rect.size.x * 0.5f,
                worldCenter.y - rect.size.y * 0.5f);
            rect.center = ImVec2(
                rect.worldPosition.x + rect.size.x * 0.5f,
                rect.worldPosition.y + rect.size.y * 0.5f);

            selectionMin.x = min(selectionMin.x, rect.worldPosition.x);
            selectionMin.y = min(selectionMin.y, rect.worldPosition.y);
            selectionMax.x = max(selectionMax.x, rect.worldPosition.x + rect.size.x);
            selectionMax.y = max(selectionMax.y, rect.worldPosition.y + rect.size.y);

            objectRects.push_back(rect);
        }

        if (objectRects.size() < 2)
            return false;

        const ImVec2 selectionCenter = ImVec2(
            (selectionMin.x + selectionMax.x) * 0.5f,
            (selectionMin.y + selectionMax.y) * 0.5f);

        if (command == UI_AlignmentCommand::DistributeHorizontal)
        {
            sort(objectRects.begin(), objectRects.end(), [](const ObjectRect& lhs, const ObjectRect& rhs)
                {
                    if (lhs.center.x != rhs.center.x)
                        return lhs.center.x < rhs.center.x;

                    return lhs.id < rhs.id;
                });
        }
        else if (command == UI_AlignmentCommand::DistributeVertical)
        {
            sort(objectRects.begin(), objectRects.end(), [](const ObjectRect& lhs, const ObjectRect& rhs)
                {
                    if (lhs.center.y != rhs.center.y)
                        return lhs.center.y < rhs.center.y;

                    return lhs.id < rhs.id;
                });
        }

        bool changed = false;
        for (size_t index = 0; index < objectRects.size(); ++index)
        {
            const ObjectRect& rect = objectRects[index];
            UI_Object* object = document.Find_Object(rect.id);
            if (nullptr == object)
                continue;

            ImVec2 nextWorldPosition = rect.worldPosition;
            switch (command)
            {
            case UI_AlignmentCommand::Left:
                nextWorldPosition.x = selectionMin.x;
                break;
            case UI_AlignmentCommand::Right:
                nextWorldPosition.x = selectionMax.x - rect.size.x;
                break;
            case UI_AlignmentCommand::Top:
                nextWorldPosition.y = selectionMin.y;
                break;
            case UI_AlignmentCommand::Bottom:
                nextWorldPosition.y = selectionMax.y - rect.size.y;
                break;
            case UI_AlignmentCommand::CenterX:
                nextWorldPosition.x = selectionCenter.x - rect.size.x * 0.5f;
                break;
            case UI_AlignmentCommand::CenterY:
                nextWorldPosition.y = selectionCenter.y - rect.size.y * 0.5f;
                break;
            case UI_AlignmentCommand::DistributeHorizontal:
            {
                const float firstCenter = objectRects.front().center.x;
                const float lastCenter = objectRects.back().center.x;
                const float step = (lastCenter - firstCenter) / static_cast<float>(objectRects.size() - 1);
                nextWorldPosition.x = firstCenter + step * static_cast<float>(index) - rect.size.x * 0.5f;
                break;
            }
            case UI_AlignmentCommand::DistributeVertical:
            {
                const float firstCenter = objectRects.front().center.y;
                const float lastCenter = objectRects.back().center.y;
                const float step = (lastCenter - firstCenter) / static_cast<float>(objectRects.size() - 1);
                nextWorldPosition.y = firstCenter + step * static_cast<float>(index) - rect.size.y * 0.5f;
                break;
            }
            default:
                break;
            }

            if (fabsf(nextWorldPosition.x - rect.worldPosition.x) <= 0.001f &&
                fabsf(nextWorldPosition.y - rect.worldPosition.y) <= 0.001f)
            {
                continue;
            }

            if (document.Set_WorldPosition(rect.id, nextWorldPosition))
                changed = true;
        }

        return changed;
    }

    void UI_ObjectCommandService::Configure_ButtonLabel(UI_Document& document, ObjectId buttonObjectId) const
    {
        UI_Object* buttonObject = document.Find_Object(buttonObjectId);
        if (nullptr == buttonObject)
            return;

        const ObjectId labelObjectId = document.Create_Object(UI_ObjectType::Text, buttonObjectId);
        UI_Object* labelObject = document.Find_Object(labelObjectId);
        if (nullptr == labelObject)
            return;

        labelObject->name = document.Generate_UniqueName("Label", labelObjectId);
        labelObject->position = ImVec2(0.f, 0.f);
        labelObject->size = buttonObject->size;
        labelObject->scale = ImVec2(1.f, 1.f);
        labelObject->color = ImVec4(1.f, 1.f, 1.f, 1.f);
        labelObject->text = "Button";
        labelObject->fontSize = 24.f;
        labelObject->textAlign = UI_TextAlign::Center;
        labelObject->textWrap = false;
        labelObject->sortOrder = static_cast<int32>(document.Get_Objects().size());
    }
}
