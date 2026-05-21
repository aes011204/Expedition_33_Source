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

        bool NearlyEqual(float lhs, float rhs)
        {
            return fabsf(lhs - rhs) <= 0.0001f;
        }

        bool NearlyEqual(const ImVec2& lhs, const ImVec2& rhs)
        {
            return NearlyEqual(lhs.x, rhs.x) && NearlyEqual(lhs.y, rhs.y);
        }

        bool Draw_PresetButton(const char* label, bool active, const char* tooltip)
        {
            if (active)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.95f, 0.72f, 0.24f, 0.95f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f, 0.82f, 0.34f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.88f, 0.62f, 0.18f, 1.f));
            }

            const bool clicked = ImGui::Button(label, ImVec2(34.f, 24.f));

            if (active)
                ImGui::PopStyleColor(3);

            if (nullptr != tooltip && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", tooltip);

            return clicked;
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

            UI_Object* selectedObject = _editorManager.Get_SelectedObject();
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

                bool changed = false;
                if (!isEffectivelyLocked)
                    _editorManager.Begin_EditTransaction();
                else
                    ImGui::BeginDisabled(true);

                changed |= ImGui::DragFloat2("Local Position", &selectedObject->position.x, 1.f);
                changed |= ImGui::DragFloat2("Size", &selectedObject->size.x, 1.f);
                changed |= ImGui::DragFloat2("Scale", &selectedObject->scale.x, 0.01f, 0.01f, 10.f, "%.2f");
                changed |= ImGui::DragFloat2("Pivot", &selectedObject->pivot.x, 0.01f, 0.f, 1.f, "%.3f");
                changed |= ImGui::DragFloat2("Anchor Min", &selectedObject->anchorMin.x, 0.01f, 0.f, 1.f, "%.3f");
                changed |= ImGui::DragFloat2("Anchor Max", &selectedObject->anchorMax.x, 0.01f, 0.f, 1.f, "%.3f");
                float rotationDegrees = Radians_ToDegrees(selectedObject->rotationRadians);
                if (ImGui::DragFloat("Rotation (deg)", &rotationDegrees, 1.f, -360.f, 360.f, "%.3f"))
                {
                    selectedObject->rotationRadians = Degrees_ToRadians(rotationDegrees);
                    changed = true;
                }
                changed |= ImGui::DragInt("Sort Order", &selectedObject->sortOrder, 1.f);

                const char* renderGroupItems[] = { "Background", "Foreground" };
                int32 currentRenderGroup = static_cast<int32>(selectedObject->renderGroup);
                currentRenderGroup = min(max(currentRenderGroup, 0), static_cast<int32>(size(renderGroupItems)) - 1);
                if (ImGui::Combo("Render Group", &currentRenderGroup, renderGroupItems, static_cast<int32>(size(renderGroupItems))))
                {
                    selectedObject->renderGroup = static_cast<UI_RenderGroup>(currentRenderGroup);
                    changed = true;
                }

                changed |= ImGui::ColorEdit4("Color", &selectedObject->color.x);

                selectedObject->scale.x = max(0.01f, selectedObject->scale.x);
                selectedObject->scale.y = max(0.01f, selectedObject->scale.y);
                selectedObject->pivot.x = min(max(selectedObject->pivot.x, 0.f), 1.f);
                selectedObject->pivot.y = min(max(selectedObject->pivot.y, 0.f), 1.f);
                selectedObject->anchorMin.x = min(max(selectedObject->anchorMin.x, 0.f), 1.f);
                selectedObject->anchorMin.y = min(max(selectedObject->anchorMin.y, 0.f), 1.f);
                selectedObject->anchorMax.x = min(max(selectedObject->anchorMax.x, 0.f), 1.f);
                selectedObject->anchorMax.y = min(max(selectedObject->anchorMax.y, 0.f), 1.f);
                const ImVec2 parentReferenceSize = _editorManager.Get_Document().Get_ParentAnchorReferenceSize(selectedObject->id);
                const float minSizeX = 1.f - parentReferenceSize.x * max(0.f, selectedObject->anchorMax.x - selectedObject->anchorMin.x);
                const float minSizeY = 1.f - parentReferenceSize.y * max(0.f, selectedObject->anchorMax.y - selectedObject->anchorMin.y);
                selectedObject->size.x = max(minSizeX, selectedObject->size.x);
                selectedObject->size.y = max(minSizeY, selectedObject->size.y);

                Render_LayoutPresets(*selectedObject);

                if (selectedObject->type == UI_ObjectType::Image)
                {
                    changed |= Render_TexturePicker(*selectedObject, createPatternOverlaySourceId);
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
                        ImGui::TextDisabled("Button objects can own Image/Text children.");
                }

                if (selectedObject->type == UI_ObjectType::Text)
                {
                    ImGui::Separator();

                    char textBuffer[1024] = {};
                    strncpy_s(textBuffer, selectedObject->text.c_str(), _TRUNCATE);

                    ImGui::TextUnformatted("Text Content");
                    if (ImGui::InputTextMultiline("##UITextContent", textBuffer, size(textBuffer), ImVec2(-1.f, 88.f)))
                    {
                        selectedObject->text = textBuffer;
                        changed = true;
                    }

                    changed |= ImGui::DragFloat("Font Size", &selectedObject->fontSize, 1.f, 4.f, 256.f, "%.0f");
                    selectedObject->fontSize = max(4.f, selectedObject->fontSize);

                    const char* alignItems[] = { "Left", "Center", "Right" };
                    int32 currentAlign = static_cast<int32>(selectedObject->textAlign);
                    currentAlign = min(max(currentAlign, 0), 2);
                    if (ImGui::Combo("Text Align", &currentAlign, alignItems, static_cast<int32>(size(alignItems))))
                    {
                        selectedObject->textAlign = static_cast<UI_TextAlign>(currentAlign);
                        changed = true;
                    }

                    changed |= ImGui::Checkbox("Wrap Text", &selectedObject->textWrap);

                    changed |= Render_FontPicker(*selectedObject);
                }

                if (changed)
                    _editorManager.Mark_Dirty();

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

                const UI_Document& document = _editorManager.Get_Document();
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

    void UI_Inspector::Render_LayoutPresets(UI_Object& object)
    {
        struct Preset
        {
            const char* label = "";
            const char* tooltip = "";
            ImVec2 value = ImVec2(0.f, 0.f);
        };

        const Preset presets[3][3] =
        {
            {
                { "TL", "Set to (0, 0)", ImVec2(0.f, 0.f) },
                { "T", "Set to (0.5, 0)", ImVec2(0.5f, 0.f) },
                { "TR", "Set to (1, 0)", ImVec2(1.f, 0.f) },
            },
            {
                { "L", "Set to (0, 0.5)", ImVec2(0.f, 0.5f) },
                { "C", "Set to (0.5, 0.5)", ImVec2(0.5f, 0.5f) },
                { "R", "Set to (1, 0.5)", ImVec2(1.f, 0.5f) },
            },
            {
                { "BL", "Set to (0, 1)", ImVec2(0.f, 1.f) },
                { "B", "Set to (0.5, 1)", ImVec2(0.5f, 1.f) },
                { "BR", "Set to (1, 1)", ImVec2(1.f, 1.f) },
            },
        };

        ImGui::Separator();
        ImGui::TextUnformatted("Pivot Presets (Keep Rect)");
        ImGui::PushID("PivotKeepRectPresets");
        for (int32 row = 0; row < 3; ++row)
        {
            for (int32 column = 0; column < 3; ++column)
            {
                const Preset& preset = presets[row][column];
                if (column > 0)
                    ImGui::SameLine();

                if (Draw_PresetButton(preset.label, NearlyEqual(object.pivot, preset.value), preset.tooltip))
                    _editorManager.Set_UIObjectPivotKeepWorldRect(object.id, preset.value);
            }
        }
        ImGui::PopID();

        ImGui::TextUnformatted("Anchor Presets (Keep Rect)");
        ImGui::PushID("AnchorKeepRectPresets");
        for (int32 row = 0; row < 3; ++row)
        {
            for (int32 column = 0; column < 3; ++column)
            {
                const Preset& preset = presets[row][column];
                if (column > 0)
                    ImGui::SameLine();

                const bool active = NearlyEqual(object.anchorMin, preset.value) && NearlyEqual(object.anchorMax, preset.value);
                if (Draw_PresetButton(preset.label, active, preset.tooltip))
                    _editorManager.Set_UIObjectAnchorsKeepWorldRect(object.id, preset.value, preset.value);
            }
        }
        ImGui::PopID();

        const float anchorCenterX = min(max((object.anchorMin.x + object.anchorMax.x) * 0.5f, 0.f), 1.f);
        const float anchorCenterY = min(max((object.anchorMin.y + object.anchorMax.y) * 0.5f, 0.f), 1.f);
        const ImVec2 horizontalMin = ImVec2(0.f, anchorCenterY);
        const ImVec2 horizontalMax = ImVec2(1.f, anchorCenterY);
        const ImVec2 verticalMin = ImVec2(anchorCenterX, 0.f);
        const ImVec2 verticalMax = ImVec2(anchorCenterX, 1.f);
        const ImVec2 fullMin = ImVec2(0.f, 0.f);
        const ImVec2 fullMax = ImVec2(1.f, 1.f);

        ImGui::TextUnformatted("Anchor Stretch (Keep Rect)");
        ImGui::PushID("AnchorStretchKeepRect");
        if (Draw_PresetButton("H", NearlyEqual(object.anchorMin, horizontalMin) && NearlyEqual(object.anchorMax, horizontalMax), "Stretch horizontally, keep current Y anchor"))
            _editorManager.Set_UIObjectAnchorsKeepWorldRect(object.id, horizontalMin, horizontalMax);
        ImGui::SameLine();
        if (Draw_PresetButton("V", NearlyEqual(object.anchorMin, verticalMin) && NearlyEqual(object.anchorMax, verticalMax), "Stretch vertically, keep current X anchor"))
            _editorManager.Set_UIObjectAnchorsKeepWorldRect(object.id, verticalMin, verticalMax);
        ImGui::SameLine();
        if (Draw_PresetButton("HV", NearlyEqual(object.anchorMin, fullMin) && NearlyEqual(object.anchorMax, fullMax), "Stretch both axes"))
            _editorManager.Set_UIObjectAnchorsKeepWorldRect(object.id, fullMin, fullMax);
        ImGui::PopID();
    }

    bool UI_Inspector::Render_TexturePicker(UI_Object& object, ObjectId& outCreatePatternOverlaySourceId)
    {
        ImGui::Separator();

        const char* renderModeItems[] = { "Normal", "Pattern Overlay" };
        int32 currentRenderMode = static_cast<int32>(object.imageRenderMode);
        currentRenderMode = min(max(currentRenderMode, 0), static_cast<int32>(size(renderModeItems)) - 1);
        bool changed = false;
        if (ImGui::Combo("Image Render Mode", &currentRenderMode, renderModeItems, static_cast<int32>(size(renderModeItems))))
        {
            object.imageRenderMode = static_cast<UI_ImageRenderMode>(currentRenderMode);
            changed = true;
        }

        if (object.imageRenderMode == UI_ImageRenderMode::Normal)
        {
            if (ImGui::Button("Create Pattern Overlay From Selected Paint"))
                outCreatePatternOverlaySourceId = object.id;
        }

        ImGui::Separator();

        const string popupId = "UITexturePicker##" + to_string(object.id);
        changed |= Draw_UITextureSlotPicker(
            object.imageRenderMode == UI_ImageRenderMode::PatternOverlay ? "Pattern Texture" : "UI Texture",
            popupId.c_str(),
            object.textureGuid,
            object.texturePath);

        ImGui::Separator();

        const string maskPopupId = "UIMaskTexturePicker##" + to_string(object.id);
        changed |= Draw_UITextureSlotPicker(
            object.imageRenderMode == UI_ImageRenderMode::PatternOverlay ? "Paint Mask" : "UI Mask Texture",
            maskPopupId.c_str(),
            object.maskTextureGuid,
            object.maskTexturePath);

        if (object.imageRenderMode == UI_ImageRenderMode::PatternOverlay)
        {
            ImGui::Separator();

            const string patternMaskPopupId = "UIPatternMaskTexturePicker##" + to_string(object.id);
            changed |= Draw_UITextureSlotPicker(
                "Pattern Mask",
                patternMaskPopupId.c_str(),
                object.patternMaskTextureGuid,
                object.patternMaskTexturePath);
        }

        return changed;
    }

    bool UI_Inspector::Render_FontPicker(UI_Object& object)
    {
        ImGui::Separator();

        const string popupId = "UIFontPicker##" + to_string(object.id);
        return Draw_FontSlotPicker("UI Font", popupId.c_str(), object);
    }

    bool UI_Inspector::Draw_UITextureSlotPicker(const char* label, const char* popupId, string& textureGuid, string& texturePath)
    {
        bool changed = false;

        ImGui::TextUnformatted(label);

        ID3D11ShaderResourceView* previewSRV = _editorManager.Get_TexturePreviewSRV(textureGuid, texturePath);
        if (nullptr != previewSRV)
        {
            ImGui::Image(To_ImTextureID(previewSRV), ImVec2(64.f, 64.f));
        }
        else
        {
            const ImVec2 previewMin = ImGui::GetCursorScreenPos();
            const ImVec2 previewMax = ImVec2(previewMin.x + 64.f, previewMin.y + 64.f);
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            drawList->AddRectFilled(previewMin, previewMax, IM_COL32(36, 39, 46, 255));
            drawList->AddRect(previewMin, previewMax, IM_COL32(88, 96, 108, 255));
            drawList->AddText(ImVec2(previewMin.x + 13.f, previewMin.y + 24.f), IM_COL32(170, 176, 186, 255), "Empty");
            ImGui::Dummy(ImVec2(64.f, 64.f));
        }

        ImGui::SameLine();
        ImGui::BeginGroup();

        if (textureGuid.empty())
        {
            if (texturePath.empty())
            {
                ImGui::TextDisabled("No texture selected");
            }
            else
            {
                ImGui::TextDisabled("Texture path is set without GUID");
                ImGui::TextWrapped("%s", texturePath.c_str());
            }
        }
        else
        {
            const AssetMeta* assetMeta = GAME->Find_AssetByGUID(textureGuid);
            if (nullptr != assetMeta)
            {
                ImGui::Text("%s", Get_AssetFileName(*assetMeta).c_str());
                ImGui::TextWrapped("%s", Make_ClientRuntimeResourcePath(*assetMeta).c_str());
            }
            else
            {
                ImGui::TextDisabled("Missing texture asset");
                if (!texturePath.empty())
                    ImGui::TextWrapped("%s", texturePath.c_str());
            }
        }

        if (ImGui::Button(("Select##Select" + string(popupId)).c_str()))
            ImGui::OpenPopup(popupId);

        ImGui::SameLine();
        if (ImGui::Button(("Clear##Clear" + string(popupId)).c_str()))
        {
            textureGuid.clear();
            texturePath.clear();
            changed = true;
        }

        ImGui::EndGroup();

        if (ImGui::BeginPopup(popupId))
        {
            if (!_editorManager.Is_AssetManagerReady())
                ImGui::TextDisabled("Asset registry is not ready.");

            ImGui::SetNextItemWidth(360.f);
            ImGui::InputText(("Search##" + string(popupId)).c_str(), _textureSearchBuffer, size(_textureSearchBuffer));
            ImGui::Separator();

            if (_uiTextureAssetCacheDirty)
                Refresh_UITextureAssetCache();

            const string searchText = String::ToLowerCopy(_textureSearchBuffer);
            Refresh_FilteredUITextureAssets(searchText);

            if (_filteredUITextureAssets.empty())
            {
                ImGui::TextDisabled("No UI textures found under ../Bin/Resources/Textures/UI.");
            }
            else
            {
                const float listHeight = 360.f;
                if (ImGui::BeginChild(("TextureList##" + string(popupId)).c_str(), ImVec2(520.f, listHeight), true))
                {
                    auto draw_texture_row = [&](const AssetMeta& assetMeta)
                    {
                        ImGui::PushID(assetMeta.guid.c_str());

                        const string assetTexturePath = Make_ClientRuntimeResourcePath(assetMeta);
                        ID3D11ShaderResourceView* rowSRV = _editorManager.Get_TexturePreviewSRV(assetMeta.guid, assetTexturePath);
                        if (nullptr != rowSRV)
                        {
                            ImGui::Image(To_ImTextureID(rowSRV), ImVec2(48.f, 48.f));
                        }
                        else
                        {
                            const ImVec2 thumbMin = ImGui::GetCursorScreenPos();
                            const ImVec2 thumbMax = ImVec2(thumbMin.x + 48.f, thumbMin.y + 48.f);
                            ImDrawList* drawList = ImGui::GetWindowDrawList();

                            drawList->AddRectFilled(thumbMin, thumbMax, IM_COL32(36, 39, 46, 255));
                            drawList->AddRect(thumbMin, thumbMax, IM_COL32(88, 96, 108, 255));
                            ImGui::Dummy(ImVec2(48.f, 48.f));
                        }

                        ImGui::SameLine();
                        ImGui::BeginGroup();

                        const bool isSelected = textureGuid == assetMeta.guid;
                        if (ImGui::Selectable(Get_AssetFileName(assetMeta).c_str(), isSelected))
                        {
                            textureGuid = assetMeta.guid;
                            texturePath = Make_ClientRuntimeResourcePath(assetMeta);
                            changed = true;
                            ImGui::CloseCurrentPopup();
                        }

                        ImGui::TextDisabled("%s", assetTexturePath.c_str());
                        ImGui::EndGroup();
                        ImGui::Separator();
                        ImGui::PopID();
                    };

                    ImGuiListClipper clipper;
                    clipper.Begin(static_cast<int32>(_filteredUITextureAssets.size()));
                    while (clipper.Step())
                    {
                        for (int32 rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd; ++rowIndex)
                        {
                            if (nullptr != _filteredUITextureAssets[rowIndex])
                                draw_texture_row(*_filteredUITextureAssets[rowIndex]);
                        }
                    }
                }
                ImGui::EndChild();
            }

            ImGui::EndPopup();
        }

        return changed;
    }

    void UI_Inspector::Refresh_UITextureAssetCache()
    {
        _uiTextureAssets.clear();
        _filteredUITextureAssets.clear();
        _filteredUITextureAssetsDirty = true;

        if (_editorManager.Is_AssetManagerReady())
        {
            for (const AssetMeta& assetMeta : GAME->Get_AssetsByType("Texture"))
            {
                if (Is_UITextureAsset(assetMeta))
                    _uiTextureAssets.push_back(assetMeta);
            }
        }

        _uiTextureAssetCacheDirty = false;
    }

    void UI_Inspector::Refresh_FilteredUITextureAssets(const string& searchText)
    {
        if (!_filteredUITextureAssetsDirty && _lastTextureSearchText == searchText)
            return;

        _filteredUITextureAssets.clear();
        _filteredUITextureAssets.reserve(_uiTextureAssets.size());

        for (const AssetMeta& assetMeta : _uiTextureAssets)
        {
            if (Matches_Search(assetMeta, searchText))
                _filteredUITextureAssets.push_back(&assetMeta);
        }

        _lastTextureSearchText = searchText;
        _filteredUITextureAssetsDirty = false;
    }

    bool UI_Inspector::Draw_FontSlotPicker(const char* label, const char* popupId, UI_Object& object)
    {
        bool changed = false;

        ImGui::TextUnformatted(label);

        const AssetMeta* selectedAsset = nullptr;
        if (_editorManager.Is_AssetManagerReady() && !object.fontGuid.empty())
            selectedAsset = GAME->Find_AssetByGUID(object.fontGuid);

        if (nullptr != selectedAsset)
        {
            ImGui::Text("%s", Get_AssetFileName(*selectedAsset).c_str());
            ImGui::TextColored(ImVec4(1.f, 0.85f, 0.35f, 1.f), "Applied: %s", object.fontTag.c_str());
            ImGui::TextWrapped("%s", Make_ClientRuntimeResourcePath(*selectedAsset).c_str());
        }
        else if (!object.fontPath.empty())
        {
            ImGui::TextDisabled("Font path is set without GUID");
            if (!object.fontTag.empty())
                ImGui::TextColored(ImVec4(1.f, 0.85f, 0.35f, 1.f), "Applied: %s", object.fontTag.c_str());
            ImGui::TextWrapped("%s", object.fontPath.c_str());
        }
        else if (!object.fontTag.empty())
        {
            ImGui::TextColored(ImVec4(1.f, 0.85f, 0.35f, 1.f), "Applied: %s", object.fontTag.c_str());
            ImGui::TextDisabled("No font asset selected");
        }
        else
        {
            ImGui::TextDisabled("No font selected");
        }

        if (ImGui::Button(("Select##Select" + string(popupId)).c_str()))
            ImGui::OpenPopup(popupId);

        ImGui::SameLine();
        if (ImGui::Button(("Clear##Clear" + string(popupId)).c_str()))
        {
            object.fontGuid.clear();
            object.fontPath.clear();
            object.fontTag.clear();
            changed = true;
        }

        if (ImGui::BeginPopup(popupId))
        {
            if (!_editorManager.Is_AssetManagerReady())
                ImGui::TextDisabled("Asset registry is not ready.");

            ImGui::SetNextItemWidth(360.f);
            ImGui::InputText(("Search##" + string(popupId)).c_str(), _fontSearchBuffer, size(_fontSearchBuffer));
            ImGui::Separator();

            string searchText = String::ToLowerCopy(_fontSearchBuffer);
            vector<AssetMeta> uiFontAssets = _editorManager.Is_AssetManagerReady() ? Collect_UISpriteFontAssets(searchText) : vector<AssetMeta>{};

            if (uiFontAssets.empty())
            {
                ImGui::TextDisabled("No UI fonts found under ../Bin/Resources/Fonts.");
            }
            else
            {
                const float listHeight = 320.f;
                if (ImGui::BeginChild(("FontList##" + string(popupId)).c_str(), ImVec2(520.f, listHeight), true))
                {
                    for (const AssetMeta& assetMeta : uiFontAssets)
                    {
                        const string stableId = Get_FontAssetStableId(assetMeta);
                        ImGui::PushID(stableId.c_str());

                        const bool isSelected = Is_FontAssetApplied(object, assetMeta);
                        if (ImGui::Selectable(Get_AssetFileName(assetMeta).c_str(), isSelected))
                        {
                            Apply_FontAsset(object, assetMeta);
                            changed = true;
                            ImGui::CloseCurrentPopup();
                        }

                        ImGui::TextDisabled("%s", Make_ClientRuntimeResourcePath(assetMeta).c_str());
                        ImGui::TextDisabled("Tag: %s", Make_FontTag(assetMeta).c_str());
                        ImGui::Separator();
                        ImGui::PopID();
                    }
                }
                ImGui::EndChild();
            }

            ImGui::EndPopup();
        }

        return changed;
    }

    Shared<UI_Inspector> UI_Inspector::Create(UI_Editor_Manager& editorManager)
    {
        return make_shared<UI_Inspector>(editorManager);
    }
}
