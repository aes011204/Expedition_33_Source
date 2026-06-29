#include "pch.h"
#include "UI_ObjectCommandService.h"
#include "Debug_Profile.h"
#include "UI_DocumentInternalAccess.h"
#include "UI_Editor_Utils.h"

namespace UIEditor
{
    namespace
    {
        struct ObjectRect
        {
            ObjectId id = 0;
            ImVec2 worldPosition = ImVec2(0.f, 0.f);
            ImVec2 center = ImVec2(0.f, 0.f);
            ImVec2 aabbMin = ImVec2(0.f, 0.f);
            ImVec2 aabbMax = ImVec2(0.f, 0.f);
        };

        bool Align_ObjectsWithFrameResolver(
            UI_Document& document,
            const vector<ObjectId>& objectIds,
            UI_AlignmentCommand command,
            const function<UI_WorldObjectFrame(ObjectId)>& resolveFrame,
            const function<bool(ObjectId, const ImVec2&)>& setWorldPosition)
        {
            vector<ObjectRect> objectRects;
            objectRects.reserve(objectIds.size());
            const UI_Document& readDocument = document;

            ImVec2 selectionMin = ImVec2(FLT_MAX, FLT_MAX);
            ImVec2 selectionMax = ImVec2(-FLT_MAX, -FLT_MAX);

            for (ObjectId objectId : objectIds)
            {
                const UI_Object* object = readDocument.Find_Object(objectId);
                if (nullptr == object)
                    continue;

                ObjectRect rect = {};
                rect.id = objectId;
                const UI_WorldObjectFrame frame = resolveFrame(objectId);
                rect.worldPosition = frame.worldPosition;
                rect.aabbMin = frame.worldAabbMin;
                rect.aabbMax = frame.worldAabbMax;
                rect.center = ImVec2(
                    (rect.aabbMin.x + rect.aabbMax.x) * 0.5f,
                    (rect.aabbMin.y + rect.aabbMax.y) * 0.5f);

                selectionMin.x = min(selectionMin.x, rect.aabbMin.x);
                selectionMin.y = min(selectionMin.y, rect.aabbMin.y);
                selectionMax.x = max(selectionMax.x, rect.aabbMax.x);
                selectionMax.y = max(selectionMax.y, rect.aabbMax.y);

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
                const UI_Object* object = readDocument.Find_Object(rect.id);
                if (nullptr == object)
                    continue;

                ImVec2 moveDelta = ImVec2(0.f, 0.f);
                switch (command)
                {
                case UI_AlignmentCommand::Left:
                    moveDelta.x = selectionMin.x - rect.aabbMin.x;
                    break;
                case UI_AlignmentCommand::Right:
                    moveDelta.x = selectionMax.x - rect.aabbMax.x;
                    break;
                case UI_AlignmentCommand::Top:
                    moveDelta.y = selectionMin.y - rect.aabbMin.y;
                    break;
                case UI_AlignmentCommand::Bottom:
                    moveDelta.y = selectionMax.y - rect.aabbMax.y;
                    break;
                case UI_AlignmentCommand::CenterX:
                    moveDelta.x = selectionCenter.x - rect.center.x;
                    break;
                case UI_AlignmentCommand::CenterY:
                    moveDelta.y = selectionCenter.y - rect.center.y;
                    break;
                case UI_AlignmentCommand::DistributeHorizontal:
                {
                    const float firstCenter = objectRects.front().center.x;
                    const float lastCenter = objectRects.back().center.x;
                    const float step = (lastCenter - firstCenter) / static_cast<float>(objectRects.size() - 1);
                    moveDelta.x = firstCenter + step * static_cast<float>(index) - rect.center.x;
                    break;
                }
                case UI_AlignmentCommand::DistributeVertical:
                {
                    const float firstCenter = objectRects.front().center.y;
                    const float lastCenter = objectRects.back().center.y;
                    const float step = (lastCenter - firstCenter) / static_cast<float>(objectRects.size() - 1);
                    moveDelta.y = firstCenter + step * static_cast<float>(index) - rect.center.y;
                    break;
                }
                default:
                    break;
                }

                if (fabsf(moveDelta.x) <= 0.001f && fabsf(moveDelta.y) <= 0.001f)
                    continue;

                const ImVec2 nextWorldPosition = ImVec2(
                    rect.worldPosition.x + moveDelta.x,
                    rect.worldPosition.y + moveDelta.y);

                if (setWorldPosition(rect.id, nextWorldPosition))
                    changed = true;
            }

            return changed;
        }
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

    ObjectId UI_ObjectCommandService::Resolve_CreateParentId(const UI_EditorReadModel& readModel, ObjectId selectedObjectId) const
    {
        if (selectedObjectId == 0)
            return 0;

        if (Is_ObjectEffectivelyLocked(readModel, selectedObjectId))
            return 0;

        const UI_Object* selectedObject = readModel.Find_Object(selectedObjectId);
        if (nullptr == selectedObject)
            return 0;

        if (Is_ContainerObjectType(selectedObject->type))
            return selectedObject->id;

        return Is_ObjectEffectivelyLocked(readModel, selectedObject->parentId) ? 0 : selectedObject->parentId;
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

    ObjectId UI_ObjectCommandService::Resolve_PasteParentId(
        const UI_Document& document,
        const UI_EditorReadModel& readModel,
        ObjectId selectedObjectId) const
    {
        const UI_Object* selectedObject = readModel.Find_Object(selectedObjectId);
        if (nullptr == selectedObject)
            return 0;

        const ObjectId parentId = selectedObject->parentId;
        if (parentId != 0 && Is_ObjectEffectivelyLocked(readModel, parentId))
            return 0;

        return document.Can_HaveChildren(parentId) ? parentId : 0;
    }

    ObjectId UI_ObjectCommandService::Create_Object(UI_Document& document, UI_ObjectType objectType, ObjectId parentId) const
    {
        const ObjectId createdObjectId = UI_DocumentInternalAccess::Create_Object(document, objectType, parentId);
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

        return UI_DocumentInternalAccess::Import_PrefabObjects(document, prefab.objects, prefab.rootObjectIds, parentId, rootOffset);
    }

    bool UI_ObjectCommandService::Can_DuplicateObject(const UI_Document& document, ObjectId objectId) const
    {
        return nullptr != document.Find_Object(objectId) && !Is_ObjectEffectivelyLocked(document, objectId);
    }

    bool UI_ObjectCommandService::Can_DuplicateObject(const UI_EditorReadModel& readModel, ObjectId objectId) const
    {
        return nullptr != readModel.Find_Object(objectId) && !Is_ObjectEffectivelyLocked(readModel, objectId);
    }

    ObjectId UI_ObjectCommandService::Duplicate_Object(UI_Document& document, ObjectId objectId, const ImVec2& offset) const
    {
        if (!Can_DuplicateObject(document, objectId))
            return 0;

        return UI_DocumentInternalAccess::Duplicate_Object(document, objectId, offset);
    }

    ObjectId UI_ObjectCommandService::Duplicate_Object(
        UI_Document& document,
        const UI_EditorReadModel& readModel,
        ObjectId objectId,
        const ImVec2& offset) const
    {
        if (!Can_DuplicateObject(readModel, objectId))
            return 0;

        return UI_DocumentInternalAccess::Duplicate_Object(document, objectId, offset);
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

    vector<ObjectId> UI_ObjectCommandService::Duplicate_Objects(
        UI_Document& document,
        const UI_EditorReadModel& readModel,
        const vector<ObjectId>& objectIds,
        const ImVec2& offset) const
    {
        vector<ObjectId> duplicatedObjectIds;
        duplicatedObjectIds.reserve(objectIds.size());

        for (ObjectId objectId : objectIds)
        {
            const ObjectId duplicatedObjectId = Duplicate_Object(document, readModel, objectId, offset);
            if (duplicatedObjectId != 0)
                duplicatedObjectIds.push_back(duplicatedObjectId);
        }

        return duplicatedObjectIds;
    }

    bool UI_ObjectCommandService::Can_DeleteObject(const UI_Document& document, ObjectId objectId) const
    {
        return nullptr != document.Find_Object(objectId) && !Is_ObjectEffectivelyLocked(document, objectId);
    }

    bool UI_ObjectCommandService::Can_DeleteObject(const UI_EditorReadModel& readModel, ObjectId objectId) const
    {
        return nullptr != readModel.Find_Object(objectId) && !Is_ObjectEffectivelyLocked(readModel, objectId);
    }

    bool UI_ObjectCommandService::Delete_Object(UI_Document& document, ObjectId objectId) const
    {
        if (!Can_DeleteObject(document, objectId))
            return false;

        return UI_DocumentInternalAccess::Delete_Object(document, objectId);
    }

    bool UI_ObjectCommandService::Delete_Object(UI_Document& document, const UI_EditorReadModel& readModel, ObjectId objectId) const
    {
        if (!Can_DeleteObject(readModel, objectId))
            return false;

        return UI_DocumentInternalAccess::Delete_Object(document, objectId);
    }

    bool UI_ObjectCommandService::Delete_Objects(UI_Document& document, const vector<ObjectId>& objectIds) const
    {
        bool deletedAny = false;
        for (ObjectId objectId : objectIds)
            deletedAny |= Delete_Object(document, objectId);

        return deletedAny;
    }

    bool UI_ObjectCommandService::Delete_Objects(
        UI_Document& document,
        const UI_EditorReadModel& readModel,
        const vector<ObjectId>& objectIds) const
    {
        bool deletedAny = false;
        for (ObjectId objectId : objectIds)
            deletedAny |= Delete_Object(document, readModel, objectId);

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

        UI_DocumentInternalAccess::Set_CanvasName(document, newName);
        return true;
    }

    bool UI_ObjectCommandService::Can_RenameObject(const UI_Document& document, ObjectId objectId, const string& newName) const
    {
        return !newName.empty() &&
            nullptr != document.Find_Object(objectId) &&
            !Is_ObjectEffectivelyLocked(document, objectId);
    }

    bool UI_ObjectCommandService::Can_RenameObject(
        const UI_EditorReadModel& readModel,
        ObjectId objectId,
        const string& newName) const
    {
        return !newName.empty() &&
            nullptr != readModel.Find_Object(objectId) &&
            !Is_ObjectEffectivelyLocked(readModel, objectId);
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

        UI_Object* object = UI_DocumentInternalAccess::Find_Object(document, objectId);
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

    bool UI_ObjectCommandService::Can_ReparentObject(
        const UI_Document& document,
        const UI_EditorReadModel& readModel,
        ObjectId objectId,
        ObjectId newParentId) const
    {
        if (Is_ObjectEffectivelyLocked(readModel, objectId) || Is_ObjectEffectivelyLocked(readModel, newParentId))
            return false;

        return document.Can_Parent(objectId, newParentId);
    }

    bool UI_ObjectCommandService::Reparent_Object(
        UI_Document& document,
        ObjectId objectId,
        ObjectId newParentId,
        bool keepWorldPosition) const
    {
        const UI_Object* object = static_cast<const UI_Document&>(document).Find_Object(objectId);
        if (nullptr == object || object->parentId == newParentId)
            return false;

        if (!Can_ReparentObject(document, objectId, newParentId))
            return false;

        return UI_DocumentInternalAccess::Change_Parent(document, objectId, newParentId, keepWorldPosition);
    }

    bool UI_ObjectCommandService::Reparent_Object(
        UI_Document& document,
        const UI_EditorReadModel& readModel,
        ObjectId objectId,
        ObjectId newParentId,
        bool keepWorldPosition) const
    {
        const UI_Object* object = readModel.Find_Object(objectId);
        if (nullptr == object || object->parentId == newParentId)
            return false;

        if (!Can_ReparentObject(document, readModel, objectId, newParentId))
            return false;

        return UI_DocumentInternalAccess::Change_Parent(document, objectId, newParentId, keepWorldPosition);
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

    bool UI_ObjectCommandService::Is_ObjectEffectivelyVisible(const UI_EditorReadModel& readModel, ObjectId objectId) const
    {
        return readModel.Is_EffectivelyVisible(objectId);
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

    bool UI_ObjectCommandService::Is_ObjectEffectivelyLocked(const UI_EditorReadModel& readModel, ObjectId objectId) const
    {
        return readModel.Is_EffectivelyLocked(objectId);
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

    vector<ObjectId> UI_ObjectCommandService::Filter_UnlockedObjectIds(
        const UI_EditorReadModel& readModel,
        const vector<ObjectId>& objectIds) const
    {
        vector<ObjectId> unlockedObjectIds = objectIds;
        unlockedObjectIds.erase(
            remove_if(
                unlockedObjectIds.begin(),
                unlockedObjectIds.end(),
                [this, &readModel](ObjectId objectId)
                {
                    return Is_ObjectEffectivelyLocked(readModel, objectId);
                }),
            unlockedObjectIds.end());

        return unlockedObjectIds;
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

        return Align_ObjectsWithFrameResolver(
            document,
            objectIds,
            command,
            [&document](ObjectId objectId)
            {
                return Build_UIWorldObjectFrame(document, objectId);
            },
            [&document](ObjectId objectId, const ImVec2& worldPosition)
            {
                return UI_DocumentInternalAccess::Set_WorldPosition(document, objectId, worldPosition);
            });
    }

    bool UI_ObjectCommandService::Align_Objects(
        UI_Document& document,
        const UI_EditorLayoutCache& layoutCache,
        const vector<ObjectId>& objectIds,
        UI_AlignmentCommand command) const
    {
        ENGINE_PROFILE_SCOPE_BASIC("UIEditor.Command.Align");

        if (!Can_AlignObjects(objectIds.size(), command))
            return false;

        return Align_ObjectsWithFrameResolver(
            document,
            objectIds,
            command,
            [&document, &layoutCache](ObjectId objectId)
            {
                const UI_EditorViewportItem* viewportItem = layoutCache.Find_Item(objectId);
                return nullptr != viewportItem
                    ? viewportItem->worldFrame
                    : Build_UIWorldObjectFrame(document, objectId);
            },
            [&document](ObjectId objectId, const ImVec2& worldPosition)
            {
                return UI_DocumentInternalAccess::Set_WorldPosition(document, objectId, worldPosition);
            });
    }

    void UI_ObjectCommandService::Configure_ButtonLabel(UI_Document& document, ObjectId buttonObjectId) const
    {
        UI_Object* buttonObject = UI_DocumentInternalAccess::Find_Object(document, buttonObjectId);
        if (nullptr == buttonObject)
            return;

        const ObjectId labelObjectId = UI_DocumentInternalAccess::Create_Object(document, UI_ObjectType::Text, buttonObjectId);
        UI_Object* labelObject = UI_DocumentInternalAccess::Find_Object(document, labelObjectId);
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
