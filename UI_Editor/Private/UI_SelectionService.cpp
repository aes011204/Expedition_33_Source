#include "pch.h"
#include "UI_SelectionService.h"

namespace UIEditor
{
    bool UI_SelectionService::Is_ObjectSelected(ObjectId objectId) const
    {
        if (objectId == 0)
            return _state.selectedObjectId == 0 && _state.selectedObjectIds.empty();

        return find(_state.selectedObjectIds.begin(), _state.selectedObjectIds.end(), objectId) != _state.selectedObjectIds.end();
    }

    UI_Object* UI_SelectionService::Get_SelectedObject(UI_Document& document) const
    {
        return document.Find_Object(_state.selectedObjectId);
    }

    const UI_Object* UI_SelectionService::Get_SelectedObject(const UI_Document& document) const
    {
        return document.Find_Object(_state.selectedObjectId);
    }

    UI_SelectionState UI_SelectionService::Make_CurrentSelectionState() const
    {
        return _state;
    }

    UI_SelectionState UI_SelectionService::Make_SingleSelectionState(const UI_Document& document, ObjectId objectId) const
    {
        UI_SelectionState selectionState = {};
        selectionState.selectedObjectId = nullptr != document.Find_Object(objectId) ? objectId : 0;
        if (selectionState.selectedObjectId != 0)
            selectionState.selectedObjectIds.push_back(selectionState.selectedObjectId);

        return selectionState;
    }

    UI_SelectionState UI_SelectionService::Make_SelectionState(
        const UI_Document& document,
        const vector<ObjectId>& objectIds,
        ObjectId primaryObjectId) const
    {
        UI_SelectionState selectionState = {};
        selectionState.selectedObjectIds = Normalize_Selection(document, objectIds);

        if (primaryObjectId != 0 && document.Find_Object(primaryObjectId) != nullptr)
            selectionState.selectedObjectId = primaryObjectId;
        else if (!selectionState.selectedObjectIds.empty())
            selectionState.selectedObjectId = selectionState.selectedObjectIds.back();

        if (selectionState.selectedObjectId != 0 &&
            find(selectionState.selectedObjectIds.begin(), selectionState.selectedObjectIds.end(), selectionState.selectedObjectId) == selectionState.selectedObjectIds.end())
        {
            selectionState.selectedObjectIds.push_back(selectionState.selectedObjectId);
        }

        return selectionState;
    }

    UI_SelectionState UI_SelectionService::Make_AddToSelectionState(const UI_Document& document, ObjectId objectId) const
    {
        if (objectId == 0 || nullptr == document.Find_Object(objectId))
            return _state;

        vector<ObjectId> nextSelection = _state.selectedObjectIds;
        if (find(nextSelection.begin(), nextSelection.end(), objectId) == nextSelection.end())
            nextSelection.push_back(objectId);

        return Make_SelectionState(document, nextSelection, objectId);
    }

    UI_SelectionState UI_SelectionService::Make_ToggleSelectionState(const UI_Document& document, ObjectId objectId) const
    {
        if (objectId == 0 || nullptr == document.Find_Object(objectId))
            return _state;

        vector<ObjectId> nextSelection = _state.selectedObjectIds;
        const auto iter = find(nextSelection.begin(), nextSelection.end(), objectId);
        if (iter == nextSelection.end())
        {
            nextSelection.push_back(objectId);
            return Make_SelectionState(document, nextSelection, objectId);
        }

        nextSelection.erase(iter);
        const ObjectId nextPrimaryObjectId = nextSelection.empty()
            ? 0
            : (_state.selectedObjectId == objectId ? nextSelection.back() : _state.selectedObjectId);

        return Make_SelectionState(document, nextSelection, nextPrimaryObjectId);
    }

    UI_SelectionState UI_SelectionService::Make_ClearSelectionState() const
    {
        return {};
    }

    void UI_SelectionService::Apply_SelectionState(UI_SelectionState selectionState)
    {
        _state = move(selectionState);
    }

    void UI_SelectionService::Restore_SelectionState(const UI_Document& document, ObjectId selectedObjectId, const vector<ObjectId>& selectedObjectIds)
    {
        Apply_SelectionState(Make_SelectionState(document, selectedObjectIds, selectedObjectId));
    }

    vector<ObjectId> UI_SelectionService::Normalize_Selection(const UI_Document& document, const vector<ObjectId>& objectIds) const
    {
        vector<ObjectId> normalizedObjectIds;
        unordered_set<ObjectId> visitedObjectIds;

        for (ObjectId objectId : objectIds)
        {
            if (objectId == 0 || nullptr == document.Find_Object(objectId))
                continue;

            if (!visitedObjectIds.insert(objectId).second)
                continue;

            normalizedObjectIds.push_back(objectId);
        }

        return normalizedObjectIds;
    }

    vector<ObjectId> UI_SelectionService::Collect_RootObjectIds(const UI_Document& document, const vector<ObjectId>& objectIds) const
    {
        const vector<ObjectId> normalizedObjectIds = Normalize_Selection(document, objectIds);
        unordered_set<ObjectId> selectedObjectIdSet(normalizedObjectIds.begin(), normalizedObjectIds.end());
        vector<ObjectId> rootObjectIds;

        for (ObjectId objectId : normalizedObjectIds)
        {
            bool hasSelectedAncestor = false;
            const UI_Object* object = document.Find_Object(objectId);
            while (nullptr != object && object->parentId != 0)
            {
                if (selectedObjectIdSet.find(object->parentId) != selectedObjectIdSet.end())
                {
                    hasSelectedAncestor = true;
                    break;
                }

                object = document.Find_Object(object->parentId);
            }

            if (!hasSelectedAncestor)
                rootObjectIds.push_back(objectId);
        }

        return rootObjectIds;
    }

    vector<ObjectId> UI_SelectionService::Get_RootSelectedObjectIds(const UI_Document& document) const
    {
        return Collect_RootObjectIds(document, _state.selectedObjectIds);
    }
}
