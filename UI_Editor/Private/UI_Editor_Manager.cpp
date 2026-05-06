#include "pch.h"
#include "UI_Editor_Manager.h"
#include "GameInstance.h"
#include "Texture.h"

namespace
{
    const char* Get_ObjectTypeName(UI_ObjectType objectType)
    {
        switch (objectType)
        {
        case UI_ObjectType::Image:
            return "Image";
        case UI_ObjectType::Text:
            return "Text";
        default:
            return "Unknown";
        }
    }

    ImU32 Get_ObjectFillColor(UI_ObjectType objectType)
    {
        switch (objectType)
        {
        case UI_ObjectType::Image:
            return IM_COL32(76, 125, 194, 110);
        case UI_ObjectType::Text:
            return IM_COL32(104, 178, 120, 110);
        default:
            return IM_COL32(180, 180, 180, 110);
        }
    }

    string Normalize_AssetRelativePath(const FAssetMeta& assetMeta)
    {
        string relativePath = String::ToString(assetMeta.relativePath);
        replace(relativePath.begin(), relativePath.end(), '\\', '/');
        return relativePath;
    }

    string Make_ClientRuntimeResourcePath(const FAssetMeta& assetMeta)
    {
        const string relativePath = Normalize_AssetRelativePath(assetMeta);
        if (relativePath.empty())
            return {};

        return "../Bin/Resources/" + relativePath;
    }

    bool StartsWith(const string& value, const string& prefix)
    {
        return value.size() >= prefix.size() && equal(prefix.begin(), prefix.end(), value.begin());
    }

    wstring Resolve_ClientRuntimeResourcePath(const string& runtimePath)
    {
        if (runtimePath.empty())
            return {};

        string normalizedPath = runtimePath;
        replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

        fs::path inputPath = fs::path(String::ToWString(normalizedPath));
        if (inputPath.is_absolute())
            return inputPath.lexically_normal().wstring();

        const string resourcePrefix = "../Bin/Resources/";
        const string binResourcePrefix = "Bin/Resources/";
        const string resourcesPrefix = "Resources/";

        string resourceRelativePath = normalizedPath;
        if (StartsWith(resourceRelativePath, resourcePrefix))
            resourceRelativePath = resourceRelativePath.substr(resourcePrefix.size());
        else if (StartsWith(resourceRelativePath, binResourcePrefix))
            resourceRelativePath = resourceRelativePath.substr(binResourcePrefix.size());
        else if (StartsWith(resourceRelativePath, resourcesPrefix))
            resourceRelativePath = resourceRelativePath.substr(resourcesPrefix.size());

        const wstring assetRoot = GAME->Get_AssetRoot();
        if (assetRoot.empty())
            return {};

        return (fs::path(assetRoot) / fs::path(String::ToWString(resourceRelativePath))).lexically_normal().wstring();
    }

    string Get_AssetFileName(const FAssetMeta& assetMeta)
    {
        if (!assetMeta.relativePath.empty())
            return String::ToString(fs::path(assetMeta.relativePath).filename().wstring());

        return String::ToString(fs::path(assetMeta.fullPath).filename().wstring());
    }

    bool Is_UITextureAsset(const FAssetMeta& assetMeta)
    {
        if (assetMeta.type != "Texture")
            return false;

        const string relativePath = String::ToLowerCopy(Normalize_AssetRelativePath(assetMeta));
        return StartsWith(relativePath, "textures/ui/");
    }

    bool Is_UISpriteFontAsset(const FAssetMeta& assetMeta)
    {
        if (assetMeta.type != "Font")
            return false;

        const string relativePath = String::ToLowerCopy(Normalize_AssetRelativePath(assetMeta));
        if (!StartsWith(relativePath, "fonts/"))
            return false;

        return String::ToLowerCopy(fs::path(relativePath).extension().string()) == ".spritefont";
    }

    string Make_FontTag(const FAssetMeta& assetMeta)
    {
        const fs::path path = !assetMeta.relativePath.empty() ? fs::path(assetMeta.relativePath) : fs::path(assetMeta.fullPath);
        return String::ToString(path.stem().wstring());
    }

    bool Matches_Search(const FAssetMeta& assetMeta, const string& searchText)
    {
        if (searchText.empty())
            return true;

        string haystack = Normalize_AssetRelativePath(assetMeta);
        haystack += " ";
        haystack += Get_AssetFileName(assetMeta);
        haystack += " ";
        haystack += Make_FontTag(assetMeta);
        haystack += " ";
        haystack += assetMeta.guid;
        haystack = String::ToLowerCopy(haystack);

        return haystack.find(searchText) != string::npos;
    }

    ImTextureID To_ImTextureID(ID3D11ShaderResourceView* srv)
    {
        return static_cast<ImTextureID>(reinterpret_cast<intptr_t>(srv));
    }

    ImVec2 Clamp_ObjectPosition(const UI_Object& object, const ImVec2& canvasSize, const ImVec2& position)
    {
        const ImVec2 objectSize = ImVec2(
            max(1.f, object.size.x * object.scale.x),
            max(1.f, object.size.y * object.scale.y));

        const float maxX = max(0.f, canvasSize.x - objectSize.x);
        const float maxY = max(0.f, canvasSize.y - objectSize.y);

        return ImVec2(
            min(max(position.x, 0.f), maxX),
            min(max(position.y, 0.f), maxY));
    }

    bool Build_ScreenClipRect(const ImVec2& minPosition, const ImVec2& maxPosition, D3D11_RECT& outRect)
    {
        const float left = max(0.f, minPosition.x);
        const float top = max(0.f, minPosition.y);
        const float right = max(left, maxPosition.x);
        const float bottom = max(top, maxPosition.y);

        if (right <= left || bottom <= top)
            return false;

        outRect.left = static_cast<LONG>(left);
        outRect.top = static_cast<LONG>(top);
        outRect.right = static_cast<LONG>(right);
        outRect.bottom = static_cast<LONG>(bottom);

        return outRect.right > outRect.left && outRect.bottom > outRect.top;
    }

    wstring Build_SafeSpriteFontText(const SpriteFont& font, const wstring& text)
    {
        if (text.empty())
            return {};

        const bool hasFallback = font.ContainsCharacter(L'?');
        const bool hasSpace = font.ContainsCharacter(L' ');
        wstring safeText;
        safeText.reserve(text.size());

        for (const wchar_t character : text)
        {
            if (character == L'\r' || character == L'\n')
            {
                safeText.push_back(character);
                continue;
            }

            if (font.ContainsCharacter(character))
            {
                safeText.push_back(character);
                continue;
            }

            if (hasFallback)
                safeText.push_back(L'?');
            else if (hasSpace)
                safeText.push_back(L' ');
        }

        return safeText;
    }

    constexpr float kMinViewportZoomScale = 0.25f;
    constexpr float kMaxViewportZoomScale = 4.f;
    constexpr float kViewportZoomStep = 1.1f;
}

HRESULT UI_Editor_Manager::Initialize(const ComPtr<Device>& device, const ComPtr<Context>& context, bool assetManagerReady)
{
    _device = device;
    _context = context;
    _assetManagerReady = assetManagerReady;

    if (nullptr != _context)
        _fontPreviewSpriteBatch = make_unique<SpriteBatch>(_context.Get());

    if (nullptr != _device)
    {
        D3D11_RASTERIZER_DESC rasterizerDesc = {};
        rasterizerDesc.FillMode = D3D11_FILL_SOLID;
        rasterizerDesc.CullMode = D3D11_CULL_NONE;
        rasterizerDesc.ScissorEnable = TRUE;
        rasterizerDesc.DepthClipEnable = TRUE;
        _device->CreateRasterizerState(&rasterizerDesc, _fontPreviewRasterizerState.GetAddressOf());
    }

    return S_OK;
}

void UI_Editor_Manager::Render()
{
    _fontPreviewCommands.clear();

    Render_DockSpace();
    Render_ContentBrowser();
    Render_Hierarchy();
    Render_Viewport();
    Render_Inspector();
}

void UI_Editor_Manager::Render_FontPreviews()
{
    if (nullptr == _fontPreviewSpriteBatch || nullptr == _context || _fontPreviewCommands.empty())
        return;

    const ImGuiIO& io = ImGui::GetIO();
    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.f;
    viewport.TopLeftY = 0.f;
    viewport.Width = max(1.f, io.DisplaySize.x);
    viewport.Height = max(1.f, io.DisplaySize.y);
    viewport.MinDepth = 0.f;
    viewport.MaxDepth = 1.f;
    _fontPreviewSpriteBatch->SetViewport(viewport);

    for (const FontPreviewCommand& command : _fontPreviewCommands)
    {
        SpriteFont* font = Get_FontPreview(command.fontGuid, command.fontPath);
        if (nullptr == font || command.text.empty())
            continue;

        const wstring safeText = Build_SafeSpriteFontText(*font, command.text);
        if (safeText.empty())
            continue;

        bool isBatchOpen = false;
        try
        {
            _context->RSSetScissorRects(1, &command.clipRect);

            _fontPreviewSpriteBatch->Begin(
                SpriteSortMode_Deferred,
                nullptr,
                nullptr,
                nullptr,
                _fontPreviewRasterizerState.Get(),
                nullptr,
                XMMatrixIdentity());
            isBatchOpen = true;

            font->DrawString(
                _fontPreviewSpriteBatch.get(),
                safeText.c_str(),
                command.position,
                XMVectorSet(command.color.x, command.color.y, command.color.z, command.color.w),
                command.rotationRadians,
                command.pivot,
                command.scale,
                SpriteEffects_None,
                0.f);

            _fontPreviewSpriteBatch->End();
            isBatchOpen = false;
        }
        catch (...)
        {
            if (isBatchOpen)
            {
                try
                {
                    _fontPreviewSpriteBatch->End();
                }
                catch (...)
                {
                }
            }
        }
    }
}

void UI_Editor_Manager::Render_DockSpace()
{
    ImGuiViewport* pViewport = ImGui::GetMainViewport();
    if (nullptr == pViewport)
        return;

    ImGui::SetNextWindowPos(pViewport->WorkPos);
    ImGui::SetNextWindowSize(pViewport->WorkSize);
    ImGui::SetNextWindowViewport(pViewport->ID);

    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    WindowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));

    bool isOpen = true;
    ImGui::Begin("UIEditorDockSpace", &isOpen, WindowFlags);
    ImGui::PopStyleVar(3);

    const ImGuiID DockSpaceId = ImGui::GetID("UIEditorDockSpaceId");
    ImGui::DockSpace(DockSpaceId, ImVec2(0.f, 0.f), ImGuiDockNodeFlags_None);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New"))
            {

            }
            if (ImGui::MenuItem("Save"))
            {

            }
            if (ImGui::MenuItem("Load"))
            {

            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }


    ImGui::End();
}

void UI_Editor_Manager::Render_Hierarchy()
{
    ImGui::Begin("Hierarchy");

    const vector<UI_Object>& objects = _document.Get_Objects();
    for (const UI_Object& object : objects)
    {
        const bool isSelected = (_selectedObjectId == object.id);
        const string itemLabel = object.name + "##UIObject" + to_string(object.id);

        if (ImGui::Selectable(itemLabel.c_str(), isSelected))
            _selectedObjectId = object.id;
    }

    ImGui::End();
}

void UI_Editor_Manager::Render_Viewport()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::SetCursorPosX(8.f);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Zoom: %.0f%%", _viewportZoomScale * 100.f);
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset Zoom"))
        _viewportZoomScale = 1.f;
    ImGui::Separator();

    ImVec2 available = ImGui::GetContentRegionAvail();
    available.x = max(1.f, available.x);
    available.y = max(1.f, available.y);

    const ImVec2 canvasSize = _document.Get_CanvasSize();

    ImGuiIO& io = ImGui::GetIO();
    const bool canZoomViewport = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (canZoomViewport && io.MouseWheel != 0.f)
    {
        if (io.MouseWheel > 0.f)
        {
            for (int32 i = 0; i < static_cast<int32>(io.MouseWheel); ++i)
                _viewportZoomScale *= kViewportZoomStep;
        }
        else
        {
            for (int32 i = 0; i < static_cast<int32>(-io.MouseWheel); ++i)
                _viewportZoomScale /= kViewportZoomStep;
        }
    }
    _viewportZoomScale = min(max(_viewportZoomScale, kMinViewportZoomScale), kMaxViewportZoomScale);

    float fitScale = min(available.x / canvasSize.x, available.y / canvasSize.y);
    fitScale = min(1.f, max(0.01f, fitScale));

    const float scale = max(0.01f, fitScale * _viewportZoomScale);
    const ImVec2 canvasScreenSize = ImVec2(canvasSize.x * scale, canvasSize.y * scale);
    const ImVec2 canvasOffset = ImVec2(
        max(0.f, (available.x - canvasScreenSize.x) * 0.5f),
        max(0.f, (available.y - canvasScreenSize.y) * 0.5f));

    const float gridSize = 16.f;

    const ImVec2 contentCursorPos = ImGui::GetCursorPos();
    const ImVec2 viewAreaMin = ImGui::GetCursorScreenPos();
    const ImVec2 viewAreaMax = ImVec2(viewAreaMin.x + available.x, viewAreaMin.y + available.y);
    ImGui::SetCursorPos(ImVec2(contentCursorPos.x + canvasOffset.x, contentCursorPos.y + canvasOffset.y));
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    const ImVec2 viewportMin = viewAreaMin;
    const ImVec2 viewportMax = viewAreaMax;
    ImVec2 canvasMax = ImVec2(canvasPos.x + canvasScreenSize.x, canvasPos.y + canvasScreenSize.y);

    const ImU32 viewportColor = IM_COL32(15, 17, 20, 255);
    const ImU32 canvasColor = IM_COL32(26, 28, 32, 255);
    const ImU32 gridColor   = IM_COL32(52, 56, 64, 160);
    const ImU32 borderColor = IM_COL32(92, 100, 112, 255);
    const ImU32 canvasBoundaryColor = IM_COL32(150, 160, 174, 220);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(viewportMin, viewportMax, viewportColor);
    drawList->AddRectFilled(canvasPos, canvasMax, canvasColor);

    for (float x = 0.f; x <= canvasSize.x; x += gridSize)
    {
        float screenX = canvasPos.x + x * scale;
        drawList->AddLine(ImVec2(screenX, canvasPos.y), ImVec2(screenX, canvasMax.y), gridColor);
    }
    for (float y = 0.f; y <= canvasSize.y; y += gridSize)
    {
        float screenY = canvasPos.y + y * scale;
        drawList->AddLine(ImVec2(canvasPos.x, screenY), ImVec2(canvasMax.x, screenY), gridColor);
    }

    ImGui::InvisibleButton("##UIEditorViewportCanvas", canvasScreenSize, ImGuiButtonFlags_MouseButtonLeft);
    const bool isViewportHovered = ImGui::IsItemHovered();
    const bool isViewportActive = ImGui::IsItemActive();
    const ImVec2 mousePos = ImGui::GetMousePos();
    const ImVec2 mouseCanvasPos = ImVec2((mousePos.x - canvasPos.x) / scale, (mousePos.y - canvasPos.y) / scale);

    const vector<UI_Object>& objects = _document.Get_Objects();
    for (const UI_Object& object : objects)
    {
        const ImVec2 objectSize = ImVec2(object.size.x * object.scale.x, object.size.y * object.scale.y);
        const ImVec2 objectMin = ImVec2(canvasPos.x + object.position.x * scale, canvasPos.y + object.position.y * scale);
        const ImVec2 objectMax = ImVec2(objectMin.x + objectSize.x * scale, objectMin.y + objectSize.y * scale);
        const bool isSelected = (_selectedObjectId == object.id);

        const ImU32 fillColor = Get_ObjectFillColor(object.type);
        const ImU32 objectBorderColor = isSelected ? IM_COL32(255, 217, 102, 255) : IM_COL32(210, 214, 220, 220);
        const float borderThickness = isSelected ? 2.5f : 1.5f;
        ID3D11ShaderResourceView* previewSRV = nullptr;

        if (object.type == UI_ObjectType::Image && !object.textureGuid.empty())
            previewSRV = Get_TexturePreviewSRV(object.textureGuid);

        if (nullptr != previewSRV)
            drawList->AddImage(To_ImTextureID(previewSRV), objectMin, objectMax);
        else
            drawList->AddRectFilled(objectMin, objectMax, fillColor);

        drawList->AddRect(objectMin, objectMax, objectBorderColor, 0.f, 0, borderThickness);

        bool usesSpriteFontPreview = false;
        if (object.type == UI_ObjectType::Text && (!object.fontGuid.empty() || !object.fontPath.empty()) && !object.text.empty())
        {
            SpriteFont* previewFont = Get_FontPreview(object.fontGuid, object.fontPath);
            if (nullptr != previewFont)
            {
                const wstring safeText = Build_SafeSpriteFontText(*previewFont, String::ToWString(object.text));
                if (!safeText.empty())
                {
                    FontPreviewCommand command = {};
                    command.fontGuid = object.fontGuid;
                    command.fontPath = object.fontPath;
                    command.text = safeText;
                    command.position = Vec2(objectMin.x + 4.f, objectMin.y + 3.f);
                    command.color = Vec4(object.color.x, object.color.y, object.color.z, object.color.w);
                    command.rotationRadians = object.rotationRadians;
                    command.pivot = Vec2(0.f, 0.f);
                    command.scale = (max(4.f, object.fontSize) / 32.f) * max(0.01f, object.scale.x) * scale;
                    if (Build_ScreenClipRect(objectMin, objectMax, command.clipRect))
                    {
                        _fontPreviewCommands.push_back(command);
                        usesSpriteFontPreview = true;
                    }
                }
            }
        }

        if (!usesSpriteFontPreview && nullptr == previewSRV && objectMax.x - objectMin.x > 32.f && objectMax.y - objectMin.y > 18.f)
        {
            const string previewText = object.type == UI_ObjectType::Text && !object.text.empty() ? object.text : object.name;
            const ImU32 previewTextColor = object.type == UI_ObjectType::Text ? ImGui::ColorConvertFloat4ToU32(object.color) : IM_COL32(245, 247, 250, 230);
            const ImVec2 textPosition = ImVec2(objectMin.x + 4.f, objectMin.y + 3.f);

            if (object.type == UI_ObjectType::Text)
                drawList->AddText(ImGui::GetFont(), max(4.f, object.fontSize) * max(0.01f, object.scale.x) * scale, textPosition, previewTextColor, previewText.c_str());
            else
                drawList->AddText(textPosition, previewTextColor, previewText.c_str());

            if (object.type == UI_ObjectType::Text && isSelected && !object.fontTag.empty() && objectMax.y - objectMin.y > 34.f)
            {
                const string fontPreviewText = "Font: " + object.fontTag;
                drawList->AddText(ImVec2(objectMin.x + 4.f, objectMax.y - 17.f), IM_COL32(255, 217, 102, 240), fontPreviewText.c_str());
            }
        }
    }

    if (isViewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        ObjectId pickedObjectId = 0;
        for (auto iter = objects.rbegin(); iter != objects.rend(); ++iter)
        {
            const UI_Object& object = *iter;
            const ImVec2 objectSize = ImVec2(object.size.x * object.scale.x, object.size.y * object.scale.y);
            const bool isInsideObject =
                mouseCanvasPos.x >= object.position.x && mouseCanvasPos.x <= object.position.x + objectSize.x &&
                mouseCanvasPos.y >= object.position.y && mouseCanvasPos.y <= object.position.y + objectSize.y;

            if (isInsideObject)
            {
                pickedObjectId = object.id;
                break;
            }
        }

        _selectedObjectId = pickedObjectId;
        _draggingObjectId = pickedObjectId;
        _dragStartMouseCanvasPos = mouseCanvasPos;

        const UI_Object* pickedObject = _document.Find_Object(pickedObjectId);
        if (nullptr != pickedObject)
            _dragStartObjectPosition = pickedObject->position;
    }

    if (_draggingObjectId != 0 && isViewportActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.f))
    {
        UI_Object* draggingObject = _document.Find_Object(_draggingObjectId);
        if (nullptr != draggingObject)
        {
            const ImVec2 delta = ImVec2(
                mouseCanvasPos.x - _dragStartMouseCanvasPos.x,
                mouseCanvasPos.y - _dragStartMouseCanvasPos.y);

            draggingObject->position = ImVec2(
                _dragStartObjectPosition.x + delta.x,
                _dragStartObjectPosition.y + delta.y);
            draggingObject->position = Clamp_ObjectPosition(*draggingObject, canvasSize, draggingObject->position);
        }
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        _draggingObjectId = 0;

    drawList->AddRect(canvasPos, canvasMax, canvasBoundaryColor, 0.f, 0, 2.f);
    drawList->AddRect(canvasPos, viewportMax, borderColor);

    ImGui::End();
    ImGui::PopStyleVar();
}

void UI_Editor_Manager::Render_Inspector()
{
    ImGui::Begin("Inspector");

    UI_Object* selectedObject = _document.Find_Object(_selectedObjectId);
    if (nullptr != selectedObject)
    {
        ImGui::Text("ID: %u", selectedObject->id);
        ImGui::Text("Name: %s", selectedObject->name.c_str());
        ImGui::Text("Type: %s", Get_ObjectTypeName(selectedObject->type));
        ImGui::Separator();

        ImGui::DragFloat2("Position", &selectedObject->position.x, 1.f);
        ImGui::DragFloat2("Size", &selectedObject->size.x, 1.f, 1.f, 4096.f);
        ImGui::DragFloat2("Scale", &selectedObject->scale.x, 0.01f, 0.01f, 10.f, "%.2f");
        ImGui::DragFloat("Rotation", &selectedObject->rotationRadians, 0.01f, -6.283f, 6.283f, "%.3f");
        ImGui::DragInt("Sort Order", &selectedObject->sortOrder, 1.f);
        ImGui::ColorEdit4("Color", &selectedObject->color.x);

        selectedObject->size.x = max(1.f, selectedObject->size.x);
        selectedObject->size.y = max(1.f, selectedObject->size.y);
        selectedObject->scale.x = max(0.01f, selectedObject->scale.x);
        selectedObject->scale.y = max(0.01f, selectedObject->scale.y);
        selectedObject->position = Clamp_ObjectPosition(*selectedObject, _document.Get_CanvasSize(), selectedObject->position);

        if (selectedObject->type == UI_ObjectType::Image)
        {
            Render_TexturePicker(*selectedObject);
        }
        else if (selectedObject->type == UI_ObjectType::Text)
        {
            ImGui::Separator();

            char textBuffer[1024] = {};
            strncpy_s(textBuffer, selectedObject->text.c_str(), _TRUNCATE);

            ImGui::TextUnformatted("Text Content");
            if (ImGui::InputTextMultiline("##UITextContent", textBuffer, size(textBuffer), ImVec2(-1.f, 88.f)))
                selectedObject->text = textBuffer;

            ImGui::DragFloat("Font Size", &selectedObject->fontSize, 1.f, 4.f, 256.f, "%.0f");
            selectedObject->fontSize = max(4.f, selectedObject->fontSize);

            Render_FontPicker(*selectedObject);
        }
    }

    ImGui::End();
}

void UI_Editor_Manager::Render_ContentBrowser()
{
    ImGui::Begin("Content Browser");

    ImGui::TextUnformatted("Search");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputText("##ContentBrowserSearch", _contentBrowserSearchBuffer, size(_contentBrowserSearchBuffer));

    const string searchText = String::ToLowerCopy(_contentBrowserSearchBuffer);
    UI_Object* selectedObject = _document.Find_Object(_selectedObjectId);

    if (!_assetManagerReady)
    {
        ImGui::Separator();
        ImGui::TextDisabled("Asset registry is not ready.");
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("ContentBrowserTabs"))
    {
        if (ImGui::BeginTabItem("UI Textures"))
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
            }
            else
            {
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

                        ID3D11ShaderResourceView* previewSRV = Get_TexturePreviewSRV(assetMeta.guid);
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

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Fonts"))
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
            }
            else
            {
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

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void UI_Editor_Manager::Render_TexturePicker(UI_Object& object)
{
    ImGui::Separator();

    const string popupId = "UITexturePicker##" + to_string(object.id);
    Draw_UITextureSlotPicker("UI Texture", popupId.c_str(), object);
}

void UI_Editor_Manager::Render_FontPicker(UI_Object& object)
{
    ImGui::Separator();

    const string popupId = "UIFontPicker##" + to_string(object.id);
    Draw_FontSlotPicker("UI Font", popupId.c_str(), object);
}

bool UI_Editor_Manager::Draw_UITextureSlotPicker(const char* label, const char* popupId, UI_Object& object)
{
    bool changed = false;

    ImGui::TextUnformatted(label);

    ID3D11ShaderResourceView* previewSRV = Get_TexturePreviewSRV(object.textureGuid);
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

    if (object.textureGuid.empty())
    {
        if (object.texturePath.empty())
        {
            ImGui::TextDisabled("No texture selected");
        }
        else
        {
            ImGui::TextDisabled("Texture path is set without GUID");
            ImGui::TextWrapped("%s", object.texturePath.c_str());
        }
    }
    else
    {
        const FAssetMeta* assetMeta = GAME->Find_AssetByGUID(object.textureGuid);
        if (nullptr != assetMeta)
        {
            ImGui::Text("%s", Get_AssetFileName(*assetMeta).c_str());
            ImGui::TextWrapped("%s", Make_ClientRuntimeResourcePath(*assetMeta).c_str());
        }
        else
        {
            ImGui::TextDisabled("Missing texture asset");
            if (!object.texturePath.empty())
                ImGui::TextWrapped("%s", object.texturePath.c_str());
        }
    }

    if (ImGui::Button(("Select##Select" + string(popupId)).c_str()))
        ImGui::OpenPopup(popupId);

    ImGui::SameLine();
    if (ImGui::Button(("Clear##Clear" + string(popupId)).c_str()))
    {
        object.textureGuid.clear();
        object.texturePath.clear();
        changed = true;
    }

    ImGui::EndGroup();

    if (ImGui::BeginPopup(popupId))
    {
        if (!_assetManagerReady)
            ImGui::TextDisabled("Asset registry is not ready.");

        ImGui::SetNextItemWidth(360.f);
        ImGui::InputText(("Search##" + string(popupId)).c_str(), _textureSearchBuffer, size(_textureSearchBuffer));
        ImGui::Separator();

        string searchText = String::ToLowerCopy(_textureSearchBuffer);
        vector<FAssetMeta> uiTextureAssets;
        if (_assetManagerReady)
        {
            for (const FAssetMeta& assetMeta : GAME->Get_AssetsByType("Texture"))
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
                for (const FAssetMeta& assetMeta : uiTextureAssets)
                {
                    ImGui::PushID(assetMeta.guid.c_str());

                    ID3D11ShaderResourceView* rowSRV = Get_TexturePreviewSRV(assetMeta.guid);
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

                    const bool isSelected = object.textureGuid == assetMeta.guid;
                    if (ImGui::Selectable(Get_AssetFileName(assetMeta).c_str(), isSelected))
                    {
                        Apply_TextureAsset(object, assetMeta);
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

bool UI_Editor_Manager::Draw_FontSlotPicker(const char* label, const char* popupId, UI_Object& object)
{
    bool changed = false;

    ImGui::TextUnformatted(label);

    const FAssetMeta* selectedAsset = nullptr;
    if (_assetManagerReady && !object.fontGuid.empty())
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
        if (!_assetManagerReady)
            ImGui::TextDisabled("Asset registry is not ready.");

        ImGui::SetNextItemWidth(360.f);
        ImGui::InputText(("Search##" + string(popupId)).c_str(), _fontSearchBuffer, size(_fontSearchBuffer));
        ImGui::Separator();

        string searchText = String::ToLowerCopy(_fontSearchBuffer);
        vector<FAssetMeta> uiFontAssets;
        if (_assetManagerReady)
        {
            for (const FAssetMeta& assetMeta : GAME->Get_AssetsByType("Font"))
            {
                if (Is_UISpriteFontAsset(assetMeta) && Matches_Search(assetMeta, searchText))
                    uiFontAssets.push_back(assetMeta);
            }
        }

        if (uiFontAssets.empty())
        {
            ImGui::TextDisabled("No UI fonts found under ../Bin/Resources/Fonts.");
        }
        else
        {
            const float listHeight = 320.f;
            if (ImGui::BeginChild(("FontList##" + string(popupId)).c_str(), ImVec2(520.f, listHeight), true))
            {
                for (const FAssetMeta& assetMeta : uiFontAssets)
                {
                    ImGui::PushID(assetMeta.guid.c_str());

                    const bool isSelected = object.fontGuid == assetMeta.guid;
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

void UI_Editor_Manager::Apply_TextureAsset(UI_Object& object, const FAssetMeta& assetMeta)
{
    object.textureGuid = assetMeta.guid;
    object.texturePath = Make_ClientRuntimeResourcePath(assetMeta);
}

void UI_Editor_Manager::Apply_FontAsset(UI_Object& object, const FAssetMeta& assetMeta)
{
    object.fontGuid = assetMeta.guid;
    object.fontPath = Make_ClientRuntimeResourcePath(assetMeta);
    object.fontTag = Make_FontTag(assetMeta);
}

SpriteFont* UI_Editor_Manager::Get_FontPreview(const string& fontGuid, const string& fontPath)
{
    if (nullptr == _device)
        return nullptr;

    wstring resolvedFontPath = {};
    string cacheKey = {};

    if (!fontGuid.empty())
    {
        resolvedFontPath = GAME->Resolve_AssetPath(fontGuid);
        cacheKey = "guid:" + fontGuid;
    }
    else if (!fontPath.empty())
    {
        resolvedFontPath = Resolve_ClientRuntimeResourcePath(fontPath);
        cacheKey = "path:" + String::ToString(resolvedFontPath);
    }

    if (resolvedFontPath.empty() || cacheKey.empty())
        return nullptr;

    const auto cachedIter = _fontPreviewCache.find(cacheKey);
    if (cachedIter != _fontPreviewCache.end())
        return cachedIter->second.get();

    if (_fontPreviewFailedGuids.find(cacheKey) != _fontPreviewFailedGuids.end())
        return nullptr;

    if (!fs::exists(resolvedFontPath))
    {
        _fontPreviewFailedGuids.insert(cacheKey);
        return nullptr;
    }

    try
    {
        Unique<SpriteFont> font = make_unique<SpriteFont>(_device.Get(), resolvedFontPath.c_str());
        if (font->ContainsCharacter(L'?'))
            font->SetDefaultCharacter(L'?');

        SpriteFont* rawFont = font.get();
        _fontPreviewCache.emplace(cacheKey, move(font));
        return rawFont;
    }
    catch (...)
    {
        _fontPreviewFailedGuids.insert(cacheKey);
        return nullptr;
    }
}

ID3D11ShaderResourceView* UI_Editor_Manager::Get_TexturePreviewSRV(const string& textureGuid)
{
    if (!_assetManagerReady || textureGuid.empty() || nullptr == _device || nullptr == _context)
        return nullptr;

    const auto cachedIter = _textureThumbCache.find(textureGuid);
    if (cachedIter != _textureThumbCache.end())
        return cachedIter->second.Get();

    if (_textureNoThumbGuids.find(textureGuid) != _textureNoThumbGuids.end())
        return nullptr;

    const wstring texturePath = GAME->Resolve_AssetPath(textureGuid);
    if (texturePath.empty())
    {
        _textureNoThumbGuids.insert(textureGuid);
        return nullptr;
    }

    Shared<Texture> texture = Texture::Create(_device, _context, texturePath.c_str(), 1);
    if (nullptr == texture || texture->Get_SRVs().empty() || nullptr == texture->Get_SRVs().front().Get())
    {
        _textureNoThumbGuids.insert(textureGuid);
        return nullptr;
    }

    _textureThumbCache.emplace(textureGuid, texture->Get_SRVs().front());
    return _textureThumbCache[textureGuid].Get();
}

Unique<UI_Editor_Manager> UI_Editor_Manager::Create(const ComPtr<Device>& device, const ComPtr<Context>& context, bool assetManagerReady)
{
    Unique<UI_Editor_Manager> pInstance{new UI_Editor_Manager()};

    if (FAILED(pInstance->Initialize(device, context, assetManagerReady)))
        return nullptr;

    return pInstance;
}
