#pragma once

#include "Debug_Profile.h"
#include "UI_Document.h"

namespace UIEditor
{
    struct UI_HierarchyRow
    {
        ObjectId objectId = 0;
        ObjectId parentId = 0;
        int32 depth = 0;
        bool hasChildren = false;
        bool isExpanded = false;
        const UI_Object* object = nullptr;
    };

    class UI_EditorReadModel final
    {
    public:
        void Rebuild_IfNeeded(const UI_Document& document, uint64 revision)
        {
            if (_revision == revision)
                return;

            ENGINE_PROFILE_SCOPE_BASIC("UIEditor.ReadModel.Rebuild");

            _revision = revision;
            _hierarchyRowsRevision = 0;
            _objectById.clear();
            _childrenByParentId.clear();
            _effectiveVisibleById.clear();
            _effectiveLockedById.clear();
            _sortedObjects.clear();
            _hierarchyRows.clear();

            const vector<UI_Object>& objects = document.Get_Objects();
            _objectById.reserve(objects.size());
            _childrenByParentId.reserve(objects.size());
            _effectiveVisibleById.reserve(objects.size());
            _effectiveLockedById.reserve(objects.size());
            _sortedObjects.reserve(objects.size());

            for (const UI_Object& object : objects)
            {
                _objectById[object.id] = &object;
                _childrenByParentId[object.parentId].push_back(&object);
                _sortedObjects.push_back(&object);
            }

            stable_sort(
                _sortedObjects.begin(),
                _sortedObjects.end(),
                [](const UI_Object* lhs, const UI_Object* rhs)
                {
                    if (lhs->sortOrder != rhs->sortOrder)
                        return lhs->sortOrder < rhs->sortOrder;

                    return lhs->id < rhs->id;
                });

            for (const UI_Object& object : objects)
            {
                unordered_set<ObjectId> visibleResolvingIds;
                Resolve_EffectiveVisible(object.id, visibleResolvingIds);

                unordered_set<ObjectId> lockedResolvingIds;
                Resolve_EffectiveLocked(object.id, lockedResolvingIds);
            }
        }

        void Rebuild_HierarchyRowsIfNeeded(const unordered_set<ObjectId>& expandedObjectIds, uint64 hierarchyRowsRevision) const
        {
            if (_hierarchyRowsRevision == hierarchyRowsRevision)
                return;

            _hierarchyRowsRevision = hierarchyRowsRevision;
            _hierarchyRows.clear();
            _hierarchyRows.reserve(_objectById.size());
            Append_HierarchyRows(0, 0, expandedObjectIds);
        }

        uint64 Get_Revision() const
        {
            return _revision;
        }

        const UI_Object* Find_Object(ObjectId objectId) const
        {
            const auto iter = _objectById.find(objectId);
            return iter != _objectById.end() ? iter->second : nullptr;
        }

        const vector<const UI_Object*>& Get_Children(ObjectId parentId) const
        {
            const auto iter = _childrenByParentId.find(parentId);
            return iter != _childrenByParentId.end() ? iter->second : Empty_ObjectList();
        }

        bool Has_Children(ObjectId parentId) const
        {
            const auto iter = _childrenByParentId.find(parentId);
            return iter != _childrenByParentId.end() && !iter->second.empty();
        }

        bool Is_EffectivelyVisible(ObjectId objectId) const
        {
            const auto iter = _effectiveVisibleById.find(objectId);
            return iter != _effectiveVisibleById.end() && iter->second;
        }

        bool Is_EffectivelyLocked(ObjectId objectId) const
        {
            const auto iter = _effectiveLockedById.find(objectId);
            return iter != _effectiveLockedById.end() && iter->second;
        }

        const vector<const UI_Object*>& Get_SortedObjects() const
        {
            return _sortedObjects;
        }

        const vector<UI_HierarchyRow>& Get_HierarchyRows() const
        {
            return _hierarchyRows;
        }

    private:
        bool Resolve_EffectiveVisible(ObjectId objectId, unordered_set<ObjectId>& resolvingIds)
        {
            const auto cachedIter = _effectiveVisibleById.find(objectId);
            if (cachedIter != _effectiveVisibleById.end())
                return cachedIter->second;

            const UI_Object* object = Find_Object(objectId);
            if (nullptr == object)
                return false;

            if (!resolvingIds.insert(objectId).second)
                return object->visible;

            const UI_Object* parentObject = object->parentId == 0 ? nullptr : Find_Object(object->parentId);
            const bool isVisible = object->visible &&
                (object->parentId == 0 || nullptr == parentObject || Resolve_EffectiveVisible(object->parentId, resolvingIds));
            resolvingIds.erase(objectId);
            _effectiveVisibleById[objectId] = isVisible;
            return isVisible;
        }

        bool Resolve_EffectiveLocked(ObjectId objectId, unordered_set<ObjectId>& resolvingIds)
        {
            const auto cachedIter = _effectiveLockedById.find(objectId);
            if (cachedIter != _effectiveLockedById.end())
                return cachedIter->second;

            const UI_Object* object = Find_Object(objectId);
            if (nullptr == object)
                return false;

            if (!resolvingIds.insert(objectId).second)
                return object->locked;

            const bool isLocked = object->locked ||
                (object->parentId != 0 && Resolve_EffectiveLocked(object->parentId, resolvingIds));
            resolvingIds.erase(objectId);
            _effectiveLockedById[objectId] = isLocked;
            return isLocked;
        }

        void Append_HierarchyRows(ObjectId parentId, int32 depth, const unordered_set<ObjectId>& expandedObjectIds) const
        {
            const vector<const UI_Object*>& children = Get_Children(parentId);
            for (const UI_Object* object : children)
            {
                if (nullptr == object)
                    continue;

                const bool hasChildren = Has_Children(object->id);
                const bool isExpanded = expandedObjectIds.find(object->id) != expandedObjectIds.end();
                _hierarchyRows.push_back(UI_HierarchyRow{ object->id, parentId, depth, hasChildren, isExpanded, object });

                if (hasChildren && isExpanded)
                    Append_HierarchyRows(object->id, depth + 1, expandedObjectIds);
            }
        }

        static const vector<const UI_Object*>& Empty_ObjectList()
        {
            static const vector<const UI_Object*> emptyList;
            return emptyList;
        }

    private:
        uint64 _revision = 0;
        mutable uint64 _hierarchyRowsRevision = 0;
        unordered_map<ObjectId, const UI_Object*> _objectById = {};
        unordered_map<ObjectId, vector<const UI_Object*>> _childrenByParentId = {};
        unordered_map<ObjectId, bool> _effectiveVisibleById = {};
        unordered_map<ObjectId, bool> _effectiveLockedById = {};
        vector<const UI_Object*> _sortedObjects = {};
        mutable vector<UI_HierarchyRow> _hierarchyRows = {};
    };
}
