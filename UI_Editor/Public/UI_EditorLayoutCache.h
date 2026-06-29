#pragma once

#include "Debug_Profile.h"
#include "UI_EditorReadModel.h"
#include "UI_PreviewTransformResolver.h"

namespace UIEditor
{
    struct UI_EditorViewportItem
    {
        const UI_Object* sourceObject = nullptr;
        UI_Object renderObject = {};
        UI_WorldObjectFrame worldFrame = {};
        ImVec2 resolvedLocalSize = ImVec2(0.f, 0.f);
        ImVec2 worldPivot = ImVec2(0.f, 0.f);
        ImVec2 worldSize = ImVec2(0.f, 0.f);
        ImVec2 worldCenter = ImVec2(0.f, 0.f);
        ImVec2 worldScale = ImVec2(1.f, 1.f);
        float worldRotationRadians = 0.f;
        ImVec4 effectiveColor = ImVec4(1.f, 1.f, 1.f, 1.f);
        wstring textPreviewText = L"";
        bool isEffectivelyLocked = false;
    };

    class UI_EditorLayoutCache final
    {
    public:
        using SampleFunc = UI_PreviewTransformResolver::SampleFunc;

    public:
        void Rebuild_IfNeeded(
            const UI_Document& document,
            const UI_EditorReadModel& readModel,
            uint64 layoutRevision,
            uint64 animationRevision,
            SampleFunc sampleFunc)
        {
            if (_readModelRevision == readModel.Get_Revision() &&
                _layoutRevision == layoutRevision &&
                _animationRevision == animationRevision)
            {
                return;
            }

            ENGINE_PROFILE_SCOPE_BASIC("UIEditor.Layout.Rebuild");

            _readModelRevision = readModel.Get_Revision();
            _layoutRevision = layoutRevision;
            _animationRevision = animationRevision;
            _viewportItems.clear();
            _itemIndexById.clear();

            _resolver.Reset(document, document.Get_CanvasSize(), sampleFunc);

            const vector<const UI_Object*>& sortedObjects = readModel.Get_SortedObjects();
            _viewportItems.reserve(sortedObjects.size());
            _itemIndexById.reserve(sortedObjects.size());

            for (const UI_Object* objectPtr : sortedObjects)
            {
                if (nullptr == objectPtr || !readModel.Is_EffectivelyVisible(objectPtr->id))
                    continue;

                UI_EditorViewportItem item = {};
                item.sourceObject = objectPtr;
                item.renderObject = _resolver.Make_PreviewObject(*objectPtr);
                if (item.renderObject.type == UI_ObjectType::Text && !item.renderObject.text.empty())
                    item.textPreviewText = String::ToWString(item.renderObject.text);
                item.effectiveColor = _resolver.Get_EffectiveRenderColor(objectPtr->id, item.renderObject.color);
                item.renderObject.color = item.effectiveColor;
                item.resolvedLocalSize = _resolver.Get_ResolvedLocalSize(objectPtr->id);
                item.worldFrame = _resolver.Get_WorldFrame(objectPtr->id);
                item.worldPivot = _resolver.Get_WorldPivot(objectPtr->id);
                item.worldSize = _resolver.Get_WorldSize(objectPtr->id);
                item.worldCenter = _resolver.Get_WorldCenter(objectPtr->id);
                item.worldScale = _resolver.Get_WorldScale(objectPtr->id);
                item.worldRotationRadians = _resolver.Get_WorldRotation(objectPtr->id);
                item.isEffectivelyLocked = readModel.Is_EffectivelyLocked(objectPtr->id);

                _itemIndexById[item.renderObject.id] = _viewportItems.size();
                _viewportItems.push_back(item);
            }
        }

        const vector<UI_EditorViewportItem>& Get_ViewportItems() const
        {
            return _viewportItems;
        }

        const UI_EditorViewportItem* Find_Item(ObjectId objectId) const
        {
            const auto iter = _itemIndexById.find(objectId);
            if (iter == _itemIndexById.end() || iter->second >= _viewportItems.size())
                return nullptr;

            return &_viewportItems[iter->second];
        }

        const UI_PreviewTransformResolver& Get_Resolver() const
        {
            return _resolver;
        }

    private:
        uint64 _readModelRevision = 0;
        uint64 _layoutRevision = 0;
        uint64 _animationRevision = 0;
        UI_PreviewTransformResolver _resolver = {};
        vector<UI_EditorViewportItem> _viewportItems = {};
        unordered_map<ObjectId, size_t> _itemIndexById = {};
    };
}
