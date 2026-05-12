#include "pch.h"
#include "UI_Inspector.h"
#include "GameInstance.h"
#include "UI_Editor_Manager.h"
#include "UI_Editor_Utils.h"

namespace UIEditor
{
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
                changed |= ImGui::DragFloat2("Size", &selectedObject->size.x, 1.f, 1.f, 4096.f);
                changed |= ImGui::DragFloat2("Scale", &selectedObject->scale.x, 0.01f, 0.01f, 10.f, "%.2f");
                changed |= ImGui::DragFloat("Rotation", &selectedObject->rotationRadians, 0.01f, -6.283f, 6.283f, "%.3f");
                changed |= ImGui::DragInt("Sort Order", &selectedObject->sortOrder, 1.f);
                changed |= ImGui::ColorEdit4("Color", &selectedObject->color.x);

                selectedObject->size.x = max(1.f, selectedObject->size.x);
                selectedObject->size.y = max(1.f, selectedObject->size.y);
                selectedObject->scale.x = max(0.01f, selectedObject->scale.x);
                selectedObject->scale.y = max(0.01f, selectedObject->scale.y);
                selectedObject->position = Clamp_ObjectPosition(*selectedObject, _editorManager.Get_Document().Get_CanvasSize(), selectedObject->position);

                if (selectedObject->type == UI_ObjectType::Image)
                {
                    changed |= Render_TexturePicker(*selectedObject);
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

    bool UI_Inspector::Render_TexturePicker(UI_Object& object)
    {
        ImGui::Separator();

        const string popupId = "UITexturePicker##" + to_string(object.id);
        bool changed = Draw_UITextureSlotPicker("UI Texture", popupId.c_str(), object.textureGuid, object.texturePath);

        ImGui::Separator();

        const string maskPopupId = "UIMaskTexturePicker##" + to_string(object.id);
        changed |= Draw_UITextureSlotPicker("UI Mask Texture", maskPopupId.c_str(), object.maskTextureGuid, object.maskTexturePath);

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

        ID3D11ShaderResourceView* previewSRV = _editorManager.Get_TexturePreviewSRV(textureGuid);
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

            string searchText = String::ToLowerCopy(_textureSearchBuffer);
            vector<AssetMeta> uiTextureAssets;
            if (_editorManager.Is_AssetManagerReady())
            {
                for (const AssetMeta& assetMeta : GAME->Get_AssetsByType("Texture"))
                {
                    if (Is_UITextureAsset(assetMeta) && Matches_Search(assetMeta, searchText))
                        uiTextureAssets.push_back(assetMeta);
                }
            }

            if (uiTextureAssets.empty())
            {
                ImGui::TextDisabled("No UI textures found under ../Bin/Resources/Textures/UI.");
            }
            else
            {
                const float listHeight = 360.f;
                if (ImGui::BeginChild(("TextureList##" + string(popupId)).c_str(), ImVec2(520.f, listHeight), true))
                {
                    for (const AssetMeta& assetMeta : uiTextureAssets)
                    {
                        ImGui::PushID(assetMeta.guid.c_str());

                        ID3D11ShaderResourceView* rowSRV = _editorManager.Get_TexturePreviewSRV(assetMeta.guid);
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

                        ImGui::TextDisabled("%s", Make_ClientRuntimeResourcePath(assetMeta).c_str());
                        ImGui::EndGroup();
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
