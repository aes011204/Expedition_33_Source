#include "pch.h"
#include "UI_Inspector.h"
#include "GameInstance.h"
#include "UI_Editor_Manager.h"
#include "UI_Editor_Utils.h"

namespace UIEditor
{
    namespace
    {
        constexpr float kPi = 3.14159265358979323846f;

        float Radians_ToDegrees(float radians)
        {
            return radians * 180.f / kPi;
        }

        float Degrees_ToRadians(float degrees)
        {
            return degrees * kPi / 180.f;
        }

    }

    UI_Inspector::UI_Inspector(UI_Editor_Manager& editorManager)
        : UI_Editor_Window{ L"Inspector" }
        , _editorManager{ editorManager }
    {
    }

    void UI_Inspector::Render()
    {
        if (!Is_Open())
            return;

        bool isOpen = Is_Open();
        const string windowName = String::ToString(Get_WindowName());

        if (ImGui::Begin(windowName.c_str(), &isOpen))
        {
            _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
            _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

            const UI_Document& document = _editorManager.Get_Document();
            const UI_Object* selectedObject = document.Find_Object(_editorManager.Get_SelectedObjectId());
            if (nullptr != selectedObject)
            {
                ObjectId createPatternOverlaySourceId = 0;
                ImGui::PushID(static_cast<int32>(selectedObject->id));

                ImGui::Text("ID: %u", selectedObject->id);
                ImGui::Text("Name: %s", selectedObject->name.c_str());
                ImGui::Text("Type: %s", Get_ObjectTypeName(selectedObject->type));
                ImGui::Separator();

                bool visible = selectedObject->visible;
                if (ImGui::Checkbox("Visible in Editor", &visible))
                    _editorManager.Set_ObjectVisible(selectedObject->id, visible);

                bool locked = selectedObject->locked;
                if (ImGui::Checkbox("Locked", &locked))
                    _editorManager.Set_ObjectLocked(selectedObject->id, locked);

                const bool isEffectivelyLocked = _editorManager.Is_ObjectEffectivelyLocked(selectedObject->id);
                if (isEffectivelyLocked)
                    ImGui::TextDisabled("Editing is locked by this object or one of its parents.");

                ImGui::Separator();

                if (!isEffectivelyLocked)
                    _editorManager.Begin_EditTransaction();
                else
                    ImGui::BeginDisabled(true);

                ImVec2 nextPosition = selectedObject->position;
                if (ImGui::DragFloat2("Local Position", &nextPosition.x, 1.f))
                    _editorManager.Set_UIObjectLocalPosition(selectedObject->id, nextPosition);

                ImVec2 nextSize = selectedObject->size;
                if (ImGui::DragFloat2("Size", &nextSize.x, 1.f))
                    _editorManager.Set_UIObjectSize(selectedObject->id, nextSize);

                ImVec2 nextScale = selectedObject->scale;
                if (ImGui::DragFloat2("Scale", &nextScale.x, 0.01f, 0.01f, 10.f, "%.2f"))
                    _editorManager.Set_UIObjectScale(selectedObject->id, nextScale);

                ImVec2 nextPivot = selectedObject->pivot;
                if (ImGui::DragFloat2("Pivot", &nextPivot.x, 0.01f, 0.f, 1.f, "%.3f"))
                    _editorManager.Set_UIObjectPivotKeepWorldRect(selectedObject->id, nextPivot);

                ImVec2 nextAnchorMin = selectedObject->anchorMin;
                if (ImGui::DragFloat2("Anchor Min", &nextAnchorMin.x, 0.01f, 0.f, 1.f, "%.3f"))
                    _editorManager.Set_UIObjectAnchorsKeepWorldRect(selectedObject->id, nextAnchorMin, selectedObject->anchorMax);

                ImVec2 nextAnchorMax = selectedObject->anchorMax;
                if (ImGui::DragFloat2("Anchor Max", &nextAnchorMax.x, 0.01f, 0.f, 1.f, "%.3f"))
                    _editorManager.Set_UIObjectAnchorsKeepWorldRect(selectedObject->id, selectedObject->anchorMin, nextAnchorMax);

                float rotationDegrees = Radians_ToDegrees(selectedObject->rotationRadians);
                if (ImGui::DragFloat("Rotation (deg)", &rotationDegrees, 1.f, -360.f, 360.f, "%.3f"))
                    _editorManager.Set_UIObjectRotationRadians(selectedObject->id, Degrees_ToRadians(rotationDegrees));

                int32 nextSortOrder = selectedObject->sortOrder;
                if (ImGui::DragInt("Sort Order", &nextSortOrder, 1.f))
                    _editorManager.Set_UIObjectSortOrder(selectedObject->id, nextSortOrder);

                const char* renderGroupItems[] = { "Background", "Foreground" };
                int32 currentRenderGroup = static_cast<int32>(selectedObject->renderGroup);
                currentRenderGroup = min(max(currentRenderGroup, 0), static_cast<int32>(size(renderGroupItems)) - 1);
                if (ImGui::Combo("Render Group", &currentRenderGroup, renderGroupItems, static_cast<int32>(size(renderGroupItems))))
                    _editorManager.Set_UIObjectRenderGroup(selectedObject->id, static_cast<UI_RenderGroup>(currentRenderGroup));

                ImVec4 nextColor = selectedObject->color;
                if (ImGui::ColorEdit4("Color", &nextColor.x))
                    _editorManager.Set_UIObjectColor(selectedObject->id, nextColor);

                float nextAlphaMultiplier = selectedObject->alphaMultiplier;
                if (ImGui::SliderFloat("Alpha Multiplier", &nextAlphaMultiplier, 0.f, 1.f, "%.3f"))
                    _editorManager.Set_UIObjectAlphaMultiplier(selectedObject->id, nextAlphaMultiplier);

                bool alphaMultiplierHidden = selectedObject->alphaMultiplier <= 0.0001f;
                if (ImGui::Checkbox("Force Alpha 0", &alphaMultiplierHidden))
                    _editorManager.Set_UIObjectAlphaMultiplier(selectedObject->id, alphaMultiplierHidden ? 0.f : 1.f);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Sets this object and its children to AlphaMultiplier 0 or 1 without changing their Color alpha.");

                Render_LayoutPresets(*selectedObject);

                if (selectedObject->type == UI_ObjectType::Image)
                {
                    Render_TexturePicker(*selectedObject, createPatternOverlaySourceId);
                }
                else if (Is_ContainerObjectType(selectedObject->type))
                {
                    ImGui::Separator();

                    int32 childCount = 0;
                    for (const UI_Object& object : _editorManager.Get_Document().Get_Objects())
                    {
                        if (object.parentId == selectedObject->id)
                            ++childCount;
                    }

                    ImGui::Text("Container Children: %d", childCount);
                    if (selectedObject->type == UI_ObjectType::Button)
                    {
                        ImGui::TextDisabled("Button objects can own Image/Text children.");
                        Render_StateAnimationPicker(*selectedObject);
                    }
                }

                if (selectedObject->type == UI_ObjectType::Text)
                {
                    ImGui::Separator();

                    char textBuffer[1024] = {};
                    strncpy_s(textBuffer, selectedObject->text.c_str(), _TRUNCATE);

                    ImGui::TextUnformatted("Text Content");
                    if (ImGui::InputTextMultiline("##UITextContent", textBuffer, size(textBuffer), ImVec2(-1.f, 88.f)))
                        _editorManager.Set_UIObjectText(selectedObject->id, textBuffer);

                    float nextFontSize = selectedObject->fontSize;
                    if (ImGui::DragFloat("Font Size", &nextFontSize, 1.f, 4.f, 256.f, "%.0f"))
                        _editorManager.Set_UIObjectFontSize(selectedObject->id, nextFontSize);

                    const char* alignItems[] = { "Left", "Center", "Right" };
                    int32 currentAlign = static_cast<int32>(selectedObject->textAlign);
                    currentAlign = min(max(currentAlign, 0), 2);
                    if (ImGui::Combo("Text Align", &currentAlign, alignItems, static_cast<int32>(size(alignItems))))
                        _editorManager.Set_UIObjectTextAlign(selectedObject->id, static_cast<UI_TextAlign>(currentAlign));

                    bool nextTextWrap = selectedObject->textWrap;
                    if (ImGui::Checkbox("Wrap Text", &nextTextWrap))
                        _editorManager.Set_UIObjectTextWrap(selectedObject->id, nextTextWrap);

                    Render_FontPicker(*selectedObject);
                }

                if (isEffectivelyLocked)
                    ImGui::EndDisabled();

                if (!ImGui::IsAnyItemActive())
                    _editorManager.Commit_EditTransactionIfChanged();

                ImGui::PopID();

                if (createPatternOverlaySourceId != 0)
                    _editorManager.Create_PatternOverlayFromImage(createPatternOverlaySourceId);
            }
            else
            {
                if (!ImGui::IsAnyItemActive())
                    _editorManager.Commit_EditTransactionIfChanged();

                ImGui::Text("Name: %s", document.Get_CanvasName().c_str());
                ImGui::TextUnformatted("Type: Canvas");
                ImGui::Separator();

                if (ImGui::Button("Rename"))
                    _editorManager.Request_RenameCanvas();
            }
        }

        Set_Open(isOpen);
        ImGui::End();
    }

    Shared<UI_Inspector> UI_Inspector::Create(UI_Editor_Manager& editorManager)
    {
        return make_shared<UI_Inspector>(editorManager);
    }
}
