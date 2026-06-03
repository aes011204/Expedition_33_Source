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

        bool NearlyEqual(const ImVec2& lhs, const ImVec2& rhs)
        {
            return fabsf(lhs.x - rhs.x) <= 0.001f && fabsf(lhs.y - rhs.y) <= 0.001f;
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

        const ImVec2 sourceWorldPosition = Get_WorldPosition(objectId);
        const ObjectId duplicatedObjectId = Duplicate_ObjectRecursive(*sourceObject, sourceObject->parentId);
        if (duplicatedObjectId != 0)
        {
            Set_WorldPosition(
                duplicatedObjectId,
                ImVec2(sourceWorldPosition.x + offset.x, sourceWorldPosition.y + offset.y));
        }

        return duplicatedObjectId;
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

        if (!NearlyEqual(rootOffset, ImVec2(0.f, 0.f)))
        {
            for (ObjectId importedRootObjectId : importedRootObjectIds)
            {
                const ImVec2 currentWorldPosition = Get_WorldPosition(importedRootObjectId);
                Set_WorldPosition(
                    importedRootObjectId,
                    ImVec2(currentWorldPosition.x + rootOffset.x, currentWorldPosition.y + rootOffset.y));
            }
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
        const ImVec2 oldWorldSize = Get_WorldSize(objectId);
        object->parentId = newParentId;

        if (keepWorldPosition)
            Set_WorldRect(objectId, oldWorldPosition, oldWorldSize);

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

    ObjectId UI_Document::Duplicate_ObjectRecursive(UI_Object sourceObject, ObjectId parentId)
    {
        const ObjectId sourceObjectId = sourceObject.id;

        UI_Object duplicateObject = sourceObject;
        duplicateObject.id = _nextObjectId++;
        duplicateObject.parentId = parentId;
        duplicateObject.name = Generate_DuplicateName(sourceObject.name);
        duplicateObject.sortOrder = static_cast<int32>(_objects.size());

        _objects.push_back(duplicateObject);

        for (size_t i = 0; i < _objects.size(); ++i)
        {
            if (_objects[i].parentId == sourceObjectId)
            {
                UI_Object childSourceObject = _objects[i];
                Duplicate_ObjectRecursive(childSourceObject, duplicateObject.id);
            }
        }

        return duplicateObject.id;
    }
}
