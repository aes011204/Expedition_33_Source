#pragma once

#include "UI_Editor_Utils.h"

namespace UIEditor
{
    struct UI_AnimationPreviewSample;

    class UI_PreviewTransformResolver final
    {
    public:
        using SampleFunc = function<bool(const UI_Object&, UI_AnimationPreviewSample&)>;

    public:
        UI_PreviewTransformResolver() = default;
        ~UI_PreviewTransformResolver() = default;

    public:
        void Reset(const UI_Document& document, const ImVec2& canvasSize, SampleFunc sampleFunc);

        const UI_Object* Find_Object(ObjectId objectId) const;
        UI_Object Make_PreviewObject(const UI_Object& object) const;

        ImVec2 Get_ResolvedLocalSize(ObjectId objectId) const;
        ImVec2 Get_ResolvedLocalPosition(ObjectId objectId) const;
        ImVec2 Get_ParentReferenceSize(const UI_Object& object) const;
        ImVec2 Transform_LocalPointToWorld(ObjectId objectId, const ImVec2& localPoint) const;
        ImVec2 Get_WorldScale(ObjectId objectId) const;
        float Get_WorldRotation(ObjectId objectId) const;
        ImVec2 Get_WorldSize(ObjectId objectId) const;
        ImVec2 Get_WorldCenter(ObjectId objectId) const;
        ImVec2 Get_WorldPosition(ObjectId objectId) const;
        ImVec2 Get_WorldPivot(ObjectId objectId) const;
        float Get_EffectiveAlphaMultiplier(ObjectId objectId) const;
        ImVec4 Get_EffectiveRenderColor(ObjectId objectId, const ImVec4& color) const;
        UI_WorldObjectFrame Get_WorldFrame(ObjectId objectId) const;

    private:
        ImVec2 Resolve_ResolvedLocalSize(ObjectId objectId, unordered_set<ObjectId>& resolvingIds) const;
        ImVec2 Resolve_ResolvedLocalPosition(ObjectId objectId, unordered_set<ObjectId>& resolvingIds) const;
        ImVec2 Resolve_WorldScale(ObjectId objectId, unordered_set<ObjectId>& resolvingIds) const;
        float Resolve_WorldRotation(ObjectId objectId, unordered_set<ObjectId>& resolvingIds) const;
        float Resolve_EffectiveAlphaMultiplier(ObjectId objectId, unordered_set<ObjectId>& resolvingIds) const;
        ImVec2 Get_PivotOffset(const UI_Object& object, const ImVec2& resolvedSize) const;
        ImVec2 Get_LocalRuntimePointFromPivot(const UI_Object& object, const ImVec2& offsetFromPivot) const;

    private:
        const UI_Document* _document = nullptr;
        ImVec2 _canvasSize = ImVec2(0.f, 0.f);
        SampleFunc _sampleFunc = {};
    };
}
