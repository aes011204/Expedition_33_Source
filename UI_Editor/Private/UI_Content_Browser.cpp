#include "pch.h"
#include "UI_Content_Browser.h"
#include "GameInstance.h"
#include "UI_Editor_Manager.h"
#include "UI_Editor_Utils.h"

namespace UIEditor
{
    UI_Content_Browser::UI_Content_Browser(UI_Editor_Manager& editorManager)
        : UI_Editor_Window{ L"Content Browser" }
        , _editorManager{ editorManager }
    {
    }

    void UI_Content_Browser::Render()
    {
        if (!Is_Open())
            return;

        bool isOpen = Is_Open();
        const string windowName = String::ToString(Get_WindowName());

        if (ImGui::Begin(windowName.c_str(), &isOpen))
        {
            _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
            _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

            ImGui::TextUnformatted("Search");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1.f);
            ImGui::InputText("##ContentBrowserSearch", _contentBrowserSearchBuffer, size(_contentBrowserSearchBuffer));

            const string searchText = String::ToLowerCopy(_contentBrowserSearchBuffer);
            UI_Object* selectedObject = _editorManager.Get_SelectedObject();

            if (!_editorManager.Is_AssetManagerReady())
            {
                ImGui::Separator();
                ImGui::TextDisabled("Asset registry is not ready.");
            }
            else if (ImGui::BeginTabBar("ContentBrowserTabs"))
            {
                if (ImGui::BeginTabItem("UI Textures"))
                {
                    Render_TextureTab(searchText, selectedObject);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Fonts"))
                {
                    Render_FontTab(searchText, selectedObject);
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }

        Set_Open(isOpen);
        ImGui::End();
    }

    void UI_Content_Browser::Render_TextureTab(const string& searchText, UI_Object* selectedObject)
    {
        vector<FAssetMeta> uiTextureAssets;
        for (const FAssetMeta& assetMeta : GAME->Get_AssetsByType("Texture"))
        {
            if (Is_UITextureAsset(assetMeta) && Matches_Search(assetMeta, searchText))
                uiTextureAssets.push_back(assetMeta);
        }

        const bool canApplyTexture = nullptr != selectedObject && selectedObject->type == UI_ObjectType::Image;

        if (uiTextureAssets.empty())
        {
            ImGui::TextDisabled("No UI textures found under ../Bin/Resources/Textures/UI.");
            return;
        }

        const float thumbnailSize = 72.f;
        const float cellWidth = 128.f;
        const float regionWidth = max(1.f, ImGui::GetContentRegionAvail().x);
        const int columnCount = max(1, static_cast<int>(regionWidth / cellWidth));
        int columnIndex = 0;

        if (ImGui::BeginChild("UITextureContentBrowser", ImVec2(0.f, 0.f), false))
        {
            for (const FAssetMeta& assetMeta : uiTextureAssets)
            {
                ImGui::PushID(assetMeta.guid.c_str());
                ImGui::BeginGroup();

                ID3D11ShaderResourceView* previewSRV = _editorManager.Get_TexturePreviewSRV(assetMeta.guid);
                if (nullptr != previewSRV)
                {
                    ImGui::Image(To_ImTextureID(previewSRV), ImVec2(thumbnailSize, thumbnailSize));
                }
                else
                {
                    const ImVec2 thumbMin = ImGui::GetCursorScreenPos();
                    const ImVec2 thumbMax = ImVec2(thumbMin.x + thumbnailSize, thumbMin.y + thumbnailSize);
                    ImDrawList* drawList = ImGui::GetWindowDrawList();

                    drawList->AddRectFilled(thumbMin, thumbMax, IM_COL32(36, 39, 46, 255));
                    drawList->AddRect(thumbMin, thumbMax, IM_COL32(88, 96, 108, 255));
                    drawList->AddText(ImVec2(thumbMin.x + 13.f, thumbMin.y + 28.f), IM_COL32(170, 176, 186, 255), "Empty");
                    ImGui::Dummy(ImVec2(thumbnailSize, thumbnailSize));
                }

                if (canApplyTexture && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    Apply_TextureAsset(*selectedObject, assetMeta);

                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + cellWidth - 12.f);
                ImGui::TextUnformatted(Get_AssetFileName(assetMeta).c_str());
                ImGui::PopTextWrapPos();

                if (canApplyTexture)
                {
                    if (ImGui::SmallButton("Apply"))
                        Apply_TextureAsset(*selectedObject, assetMeta);
                }
                else
                {
                    ImGui::TextDisabled("Image only");
                }

                ImGui::EndGroup();
                ImGui::PopID();

                ++columnIndex;
                if (columnIndex < columnCount)
                    ImGui::SameLine();
                else
                    columnIndex = 0;
            }
        }
        ImGui::EndChild();
    }

    void UI_Content_Browser::Render_FontTab(const string& searchText, UI_Object* selectedObject)
    {
        vector<FAssetMeta> uiFontAssets;
        for (const FAssetMeta& assetMeta : GAME->Get_AssetsByType("Font"))
        {
            if (Is_UISpriteFontAsset(assetMeta) && Matches_Search(assetMeta, searchText))
                uiFontAssets.push_back(assetMeta);
        }

        const bool canApplyFont = nullptr != selectedObject && selectedObject->type == UI_ObjectType::Text;

        if (uiFontAssets.empty())
        {
            ImGui::TextDisabled("No UI fonts found under ../Bin/Resources/Fonts.");
            return;
        }

        if (ImGui::BeginChild("UIFontContentBrowser", ImVec2(0.f, 0.f), false))
        {
            for (const FAssetMeta& assetMeta : uiFontAssets)
            {
                ImGui::PushID(assetMeta.guid.c_str());

                const bool isCurrentFont = nullptr != selectedObject && selectedObject->fontGuid == assetMeta.guid;
                if (ImGui::Selectable(Get_AssetFileName(assetMeta).c_str(), isCurrentFont, ImGuiSelectableFlags_AllowDoubleClick))
                {
                    if (canApplyFont && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        Apply_FontAsset(*selectedObject, assetMeta);
                }

                ImGui::TextDisabled("Tag: %s", Make_FontTag(assetMeta).c_str());
                ImGui::TextDisabled("%s", Make_ClientRuntimeResourcePath(assetMeta).c_str());

                if (canApplyFont)
                {
                    if (ImGui::SmallButton("Apply"))
                        Apply_FontAsset(*selectedObject, assetMeta);
                }
                else
                {
                    ImGui::TextDisabled("Text only");
                }

                ImGui::Separator();
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
    }

    Shared<UI_Content_Browser> UI_Content_Browser::Create(UI_Editor_Manager& editorManager)
    {
        return make_shared<UI_Content_Browser>(editorManager);
    }
}
