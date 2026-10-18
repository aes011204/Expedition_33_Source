#include "pch.h"
#include "UI_ClipboardService.h"
#include "UI_DocumentInternalAccess.h"

namespace UIEditor
{
    bool UI_ClipboardService::Has_CopiedUIObjects() const
    {
        return !_clipboardObjects.empty() && !_clipboardRootObjectIds.empty();
    }

    bool UI_ClipboardService::Copy_FromDocument(const UI_Document& document, const vector<ObjectId>& rootObjectIds)
    {
        if (rootObjectIds.empty())
            return false;

        vector<ObjectId> copiedObjectIds;
        unordered_set<ObjectId> copiedObjectIdSet;
        for (ObjectId rootObjectId : rootObjectIds)
        {
            vector<ObjectId> objectAndChildrenIds;
            Collect_ObjectAndChildren(document, rootObjectId, objectAndChildrenIds);

            for (ObjectId objectId : objectAndChildrenIds)
            {
                if (copiedObjectIdSet.insert(objectId).second)
                    copiedObjectIds.push_back(objectId);
            }
        }

        if (copiedObjectIds.empty())
            return false;

        unordered_set<ObjectId> rootObjectIdSet(rootObjectIds.begin(), rootObjectIds.end());
        vector<UI_Object> clipboardObjects;
        clipboardObjects.reserve(copiedObjectIds.size());

        for (ObjectId objectId : copiedObjectIds)
        {
            const UI_Object* sourceObject = document.Find_Object(objectId);
            if (nullptr == sourceObject)
                continue;

            UI_Object clipboardObject = *sourceObject;
            if (rootObjectIdSet.find(clipboardObject.id) != rootObjectIdSet.end())
                clipboardObject.parentId = 0;

            clipboardObjects.push_back(move(clipboardObject));
        }

        if (clipboardObjects.empty())
            return false;

        _clipboardObjects = move(clipboardObjects);
        _clipboardRootObjectIds = rootObjectIds;
        _clipboardPasteCount = 0;
        return true;
    }

    bool UI_ClipboardService::Paste_ToDocument(UI_Document& document, ObjectId parentId, vector<ObjectId>& outCreatedRootObjectIds)
    {
        outCreatedRootObjectIds.clear();
        if (!Has_CopiedUIObjects())
            return false;

        const float pasteOffset = 20.f * static_cast<float>(_clipboardPasteCount + 1);
        outCreatedRootObjectIds = UI_DocumentInternalAccess::Import_PrefabObjects(
            document,
            _clipboardObjects,
            _clipboardRootObjectIds,
            parentId,
            ImVec2(pasteOffset, pasteOffset));

        if (outCreatedRootObjectIds.empty())
            return false;

        ++_clipboardPasteCount;
        return true;
    }

    void UI_ClipboardService::Collect_ObjectAndChildren(
        const UI_Document& document,
        ObjectId objectId,
        vector<ObjectId>& outObjectIds) const
    {
        if (objectId == 0 || nullptr == document.Find_Object(objectId))
            return;

        outObjectIds.push_back(objectId);
        for (const UI_Object& object : document.Get_Objects())
        {
            if (object.parentId == objectId)
                Collect_ObjectAndChildren(document, object.id, outObjectIds);
        }
    }
}
