#include "pch.h"
#include "UI_Animation_View.h"

#include "UI_Editor_Manager.h"

namespace UIEditor
{
    namespace
    {
        const char* const kEditorModeNames[] = {
            "Keyframe Animation",
            "State Animation",
            "Sequence Preview",
            "UI Effect",
        };
    }

    UI_Animation_View::UI_Animation_View(UI_Editor_Manager& editorManager)
        : UI_Editor_Window{ L"Animation" }
        , _editorManager{ editorManager }
    {
        Sync_AssetNameBuffer();
        Sync_StateAssetNameBuffer();
        Sync_StateEventPathBuffers();
        Sync_TargetNameBuffer();
    }

    void UI_Animation_View::Render()
    {
        if (!Is_Open())
        {
            _isFocused = false;
            _isHovered = false;
            return;
        }

        bool isOpen = Is_Open();
        const string windowName = String::ToString(Get_WindowName());
        _isFocused = false;
        _isHovered = false;

        if (ImGui::Begin(windowName.c_str(), &isOpen))
        {
            _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
            _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
            Handle_Shortcuts();

            int32 editorModeIndex = static_cast<int32>(_editorMode);
            ImGui::SetNextItemWidth(190.f);
            if (ImGui::Combo("Editor Mode", &editorModeIndex, kEditorModeNames, static_cast<int32>(size(kEditorModeNames))))
            {
                _editorMode = static_cast<UI_AnimationEditorMode>(editorModeIndex);
                _editorManager.Stop_AnimationPreview();
            }
            ImGui::Separator();

            if (_editorMode == UI_AnimationEditorMode::StateAnimation)
            {
                Render_StateAnimationEditor();
            }
            else if (_editorMode == UI_AnimationEditorMode::SequencePreview)
            {
                Render_SequencePreviewEditor();
            }
            else if (_editorMode == UI_AnimationEditorMode::UIEffect)
            {
                Render_UIEffectEditor();
            }
            else
            {
                Render_KeyframeAnimationEditor();
            }
        }

        Set_Open(isOpen);
        ImGui::End();
    }

    void UI_Animation_View::Sync_AssetNameBuffer()
    {
        memset(_assetNameBuffer, 0, sizeof(_assetNameBuffer));
        strncpy_s(_assetNameBuffer, _assetService.Get_Asset().name.c_str(), _TRUNCATE);
    }

    void UI_Animation_View::Sync_StateAssetNameBuffer()
    {
        memset(_stateAssetNameBuffer, 0, sizeof(_stateAssetNameBuffer));
        strncpy_s(_stateAssetNameBuffer, _stateAssetService.Get_Asset().name.c_str(), _TRUNCATE);
    }

    void UI_Animation_View::Sync_StateEventPathBuffers()
    {
        const UI_StateAnimationAsset& asset = _stateAssetService.Get_Asset();
        for (int32 index = 0; index < static_cast<int32>(UI_StateAnimationEvent::Count); ++index)
        {
            array<char, 260>& buffer = _stateEventPathBuffers[static_cast<size_t>(index)];
            memset(buffer.data(), 0, buffer.size());
            strncpy_s(buffer.data(), buffer.size(), asset.eventAnimationPaths[static_cast<size_t>(index)].c_str(), _TRUNCATE);
        }
    }

    void UI_Animation_View::Sync_TargetNameBuffer()
    {
        memset(_targetNameBuffer, 0, sizeof(_targetNameBuffer));
        const UI_AnimationTargetBinding* target = _assetService.Get_SelectedTarget();
        if (nullptr != target)
            strncpy_s(_targetNameBuffer, target->targetName.c_str(), _TRUNCATE);
    }

    void UI_Animation_View::Handle_TimelineClick(const UI_AnimationTimelineClickResult& clickResult)
    {
        if (!clickResult.clicked)
            return;

        bool selectedKey = false;
        if (clickResult.targetIndex >= 0 && clickResult.trackIndex >= 0)
        {
            if (clickResult.hitKey)
                selectedKey = _assetService.Select_Key(clickResult.targetIndex, clickResult.trackIndex, clickResult.keyIndex);
            else
                _assetService.Select_Track(clickResult.targetIndex, clickResult.trackIndex);
            Sync_TargetNameBuffer();
        }
        else if (clickResult.trackIndex >= 0)
        {
            if (clickResult.hitKey)
                selectedKey = _assetService.Select_Key(-1, clickResult.trackIndex, clickResult.keyIndex);
            else
                _assetService.Select_Track(-1, clickResult.trackIndex);
        }
        else if (clickResult.targetIndex >= 0)
        {
            _assetService.Select_Target(clickResult.targetIndex);
            Sync_TargetNameBuffer();
        }

        if (clickResult.draggedKey)
        {
            if (!selectedKey)
                return;

            int32 targetFrame = clickResult.frame;
            if (_assetService.TryMoveSelectedKeyFrame(clickResult.frame, targetFrame))
                Set_CurrentFrame(targetFrame);
            return;
        }

        Set_CurrentFrame(clickResult.frame);
    }

    void UI_Animation_View::Handle_SequenceTimelineClick(const UI_AnimationSequenceTimelineClickResult& clickResult)
    {
        if (!clickResult.clicked)
            return;

        _sequenceService.Select_Clip(clickResult.clipIndex);
        _sequenceService.Set_CurrentFrame(clickResult.frame);
        Request_SequenceStaticPreviewRefresh();
    }

    void UI_Animation_View::Render_UIEffectEditor()
    {
        const UI_Object* selectedObject = _editorManager.Get_SelectedObject();
        const bool hasSelectedImage = nullptr != selectedObject && selectedObject->type == UI_ObjectType::Image;
        const bool hasHoverBrushTarget = nullptr != selectedObject &&
            (selectedObject->type == UI_ObjectType::Button || selectedObject->type == UI_ObjectType::Panel);
        const bool hasBrushAtmosphereTarget = hasSelectedImage || hasHoverBrushTarget;

        if (!hasBrushAtmosphereTarget)
        {
            ImGui::TextDisabled("Select an Image, Button, or Panel UI object.");
            ImGui::Separator();
            Render_KeyframeAnimationEditor();
            return;
        }

        const ObjectId selectedObjectId = selectedObject->id;
        const bool isLocked = _editorManager.Is_ObjectEffectivelyLocked(selectedObjectId);
        const bool isPatternOverlay = hasSelectedImage && selectedObject->imageRenderMode == UI_ImageRenderMode::PatternOverlay;

        if (hasSelectedImage && selectedObject->imageRenderMode == UI_ImageRenderMode::Normal)
        {
            if (ImGui::Button("Create Brush Overlay"))
                _editorManager.Create_PatternOverlayFromImage(selectedObjectId);
        }
        else
        {
            ImGui::BeginDisabled(true);
            ImGui::Button("Create Brush Overlay");
            ImGui::EndDisabled();
        }

        if (hasHoverBrushTarget && !isLocked)
        {
            if (ImGui::Button("Create Button Ink Flow Layer"))
                _editorManager.Create_HoverBrushPreset(selectedObjectId);
        }
        else
        {
            ImGui::BeginDisabled(true);
            ImGui::Button("Create Button Ink Flow Layer");
            ImGui::EndDisabled();
        }

        if (hasBrushAtmosphereTarget && !isLocked)
        {
            if (ImGui::Button("Create Brush Atmosphere Layers"))
                _editorManager.Create_BrushAtmospherePreset(selectedObjectId);
        }
        else
        {
            ImGui::BeginDisabled(true);
            ImGui::Button("Create Brush Atmosphere Layers");
            ImGui::EndDisabled();
        }

        if (!hasSelectedImage)
        {
            ImGui::Separator();
            Render_KeyframeAnimationEditor();
            return;
        }

        selectedObject = _editorManager.Get_SelectedObject();
        if (nullptr == selectedObject || selectedObject->id != selectedObjectId || selectedObject->type != UI_ObjectType::Image)
        {
            ImGui::Separator();
            Render_KeyframeAnimationEditor();
            return;
        }

        ImGui::Separator();
        ImGui::BeginDisabled(isLocked);
        _editorManager.Begin_EditTransaction();

        ImVec4 nextColor = selectedObject->color;
        if (ImGui::ColorEdit4("Color", &nextColor.x))
            _editorManager.Set_UIObjectColor(selectedObjectId, nextColor);

        float nextAlphaMultiplier = selectedObject->alphaMultiplier;
        if (ImGui::SliderFloat("Alpha Multiplier", &nextAlphaMultiplier, 0.f, 1.f, "%.3f"))
            _editorManager.Set_UIObjectAlphaMultiplier(selectedObjectId, nextAlphaMultiplier);

        bool nextUseFillRatio = selectedObject->useFillRatio;
        if (ImGui::Checkbox("Use Fill Ratio", &nextUseFillRatio))
            _editorManager.Set_UIObjectUseFillRatio(selectedObjectId, nextUseFillRatio);

        ImGui::BeginDisabled(!nextUseFillRatio);
        float nextFillRatio = selectedObject->fillRatio;
        if (ImGui::SliderFloat("Fill Ratio", &nextFillRatio, kFillRatioMin, kFillRatioMax, "%.3f"))
            _editorManager.Set_UIObjectFillRatio(selectedObjectId, nextFillRatio);

        const char* fillOriginItems[] = { "Left To Right", "Right To Left", "Top To Bottom", "Bottom To Top" };
        int32 currentFillOrigin = static_cast<int32>(selectedObject->fillOrigin);
        currentFillOrigin = min(max(currentFillOrigin, 0), static_cast<int32>(size(fillOriginItems)) - 1);
        if (ImGui::Combo("Fill Origin", &currentFillOrigin, fillOriginItems, static_cast<int32>(size(fillOriginItems))))
            _editorManager.Set_UIObjectFillOrigin(selectedObjectId, static_cast<UI_FillOrigin>(currentFillOrigin));
        ImGui::EndDisabled();

        if (isPatternOverlay)
        {
            ImGui::Separator();
            ImGui::TextUnformatted("Pattern UV");

            ImVec2 nextPatternUvOffset = selectedObject->patternUvOffset;
            if (ImGui::DragFloat2("Pattern UV Offset", &nextPatternUvOffset.x, 0.01f, kPatternUvOffsetMin, kPatternUvOffsetMax, "%.3f"))
                _editorManager.Set_UIObjectPatternUvOffset(selectedObjectId, nextPatternUvOffset);

            ImVec2 nextPatternUvScale = selectedObject->patternUvScale;
            if (ImGui::DragFloat2("Pattern UV Scale", &nextPatternUvScale.x, 0.01f, kPatternUvScaleMin, kPatternUvScaleMax, "%.3f"))
                _editorManager.Set_UIObjectPatternUvScale(selectedObjectId, nextPatternUvScale);

            if (ImGui::Button("Reset Pattern UV"))
                _editorManager.Set_UIObjectPatternUv(selectedObjectId, Get_DefaultPatternUvOffset(), Get_DefaultPatternUvScale());
        }

        if (!ImGui::IsAnyItemActive())
            _editorManager.Commit_EditTransactionIfChanged();
        ImGui::EndDisabled();

        ImGui::Separator();
        Render_KeyframeAnimationEditor();
    }

    Shared<UI_Animation_View> UI_Animation_View::Create(UI_Editor_Manager& editorManager)
    {
        return make_shared<UI_Animation_View>(editorManager);
    }
}
