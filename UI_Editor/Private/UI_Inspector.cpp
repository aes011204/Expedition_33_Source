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

        bool Is_UITextureFileExtension(const fs::path& path)
        {
            const string extension = String::ToLowerCopy(path.extension().string());
            return extension == ".png" ||
                extension == ".jpg" ||
                extension == ".jpeg" ||
                extension == ".dds" ||
                extension == ".tga";
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
                        ImGui::TextDisabled("Button objects can own Image/Text children.");
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

    void UI_Inspector::Render_LayoutPresets(const UI_Object& object)
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

    void UI_Inspector::Render_TexturePicker(const UI_Object& object, ObjectId& outCreatePatternOverlaySourceId)
    {
        ImGui::Separator();

        const char* renderModeItems[] = { "Normal", "Pattern Overlay" };
        UI_ImageRenderMode effectiveRenderMode = object.imageRenderMode;
        int32 currentRenderMode = static_cast<int32>(effectiveRenderMode);
        currentRenderMode = min(max(currentRenderMode, 0), static_cast<int32>(size(renderModeItems)) - 1);
        if (ImGui::Combo("Image Render Mode", &currentRenderMode, renderModeItems, static_cast<int32>(size(renderModeItems))))
        {
            effectiveRenderMode = static_cast<UI_ImageRenderMode>(currentRenderMode);
            _editorManager.Set_UIObjectImageRenderMode(object.id, effectiveRenderMode);
        }

        if (effectiveRenderMode == UI_ImageRenderMode::Normal)
        {
            if (ImGui::Button("Create Pattern Overlay From Selected Paint"))
                outCreatePatternOverlaySourceId = object.id;
        }

        ImGui::Separator();

        const string popupId = "UITexturePicker##" + to_string(object.id);
        string nextTextureGuid = object.textureGuid;
        string nextTexturePath = object.texturePath;
        if (Draw_UITextureSlotPicker(
            effectiveRenderMode == UI_ImageRenderMode::PatternOverlay ? "Pattern Texture" : "UI Texture",
            popupId.c_str(),
            nextTextureGuid,
            nextTexturePath))
        {
            _editorManager.Set_UIObjectTextureBinding(object.id, nextTextureGuid, nextTexturePath);
        }

        ImGui::Separator();

        const string maskPopupId = "UIMaskTexturePicker##" + to_string(object.id);
        string nextMaskTextureGuid = object.maskTextureGuid;
        string nextMaskTexturePath = object.maskTexturePath;
        if (Draw_UITextureSlotPicker(
            effectiveRenderMode == UI_ImageRenderMode::PatternOverlay ? "Paint Mask" : "UI Mask Texture",
            maskPopupId.c_str(),
            nextMaskTextureGuid,
            nextMaskTexturePath))
        {
            _editorManager.Set_UIObjectMaskTextureBinding(object.id, nextMaskTextureGuid, nextMaskTexturePath);
        }

        if (effectiveRenderMode == UI_ImageRenderMode::Normal)
        {
            const char* maskTextureModeItems[] = { "Alpha", "BlackWhite" };
            int32 currentMaskTextureMode = static_cast<int32>(object.maskTextureMode);
            currentMaskTextureMode = min(max(currentMaskTextureMode, 0), static_cast<int32>(size(maskTextureModeItems)) - 1);
            if (ImGui::Combo("Mask Texture Mode", &currentMaskTextureMode, maskTextureModeItems, static_cast<int32>(size(maskTextureModeItems))))
                _editorManager.Set_UIObjectMaskTextureMode(object.id, static_cast<UI_MaskTextureMode>(currentMaskTextureMode));
        }

        if (effectiveRenderMode == UI_ImageRenderMode::PatternOverlay)
        {
            ImGui::Separator();

            const string patternMaskPopupId = "UIPatternMaskTexturePicker##" + to_string(object.id);
            string nextPatternMaskTextureGuid = object.patternMaskTextureGuid;
            string nextPatternMaskTexturePath = object.patternMaskTexturePath;
            if (Draw_UITextureSlotPicker(
                "Pattern Mask",
                patternMaskPopupId.c_str(),
                nextPatternMaskTextureGuid,
                nextPatternMaskTexturePath))
            {
                _editorManager.Set_UIObjectPatternMaskTextureBinding(object.id, nextPatternMaskTextureGuid, nextPatternMaskTexturePath);
            }
        }
    }

    void UI_Inspector::Render_FontPicker(const UI_Object& object)
    {
        ImGui::Separator();

        const string popupId = "UIFontPicker##" + to_string(object.id);
        Draw_FontSlotPicker("UI Font", popupId.c_str(), object);
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
                        const string rowId = assetMeta.guid.empty() ? Make_ClientRuntimeResourcePath(assetMeta) : assetMeta.guid;
                        ImGui::PushID(rowId.c_str());

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

                        const bool isSelected =
                            (!assetMeta.guid.empty() && textureGuid == assetMeta.guid) ||
                            (assetMeta.guid.empty() && texturePath == assetTexturePath);
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

        if (_uiTextureAssets.empty())
        {
            const wstring resourceRoot = Resolve_UIEditorClientResourceRoot();
            const fs::path uiTextureRoot = fs::path(resourceRoot) / L"Textures" / L"UI";

            try
            {
                if (!resourceRoot.empty() && fs::exists(uiTextureRoot) && fs::is_directory(uiTextureRoot))
                {
                    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(uiTextureRoot))
                    {
                        if (!entry.is_regular_file() || !Is_UITextureFileExtension(entry.path()))
                            continue;

                        AssetMeta textureAsset = {};
                        textureAsset.type = "Texture";
                        textureAsset.fullPath = fs::absolute(entry.path()).lexically_normal().wstring();
                        textureAsset.relativePath = fs::relative(entry.path(), fs::path(resourceRoot)).lexically_normal().wstring();

                        _uiTextureAssets.push_back(textureAsset);
                    }
                }
            }
            catch (...)
            {
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

    void UI_Inspector::Draw_FontSlotPicker(const char* label, const char* popupId, const UI_Object& object)
    {
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
            _editorManager.Set_UIObjectFontBinding(object.id, "", "", "");

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
                            _editorManager.Set_UIObjectFontBinding(
                                object.id,
                                assetMeta.guid,
                                Make_ClientRuntimeResourcePath(assetMeta),
                                Make_FontTag(assetMeta));
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
    }

    Shared<UI_Inspector> UI_Inspector::Create(UI_Editor_Manager& editorManager)
    {
        return make_shared<UI_Inspector>(editorManager);
    }
}
