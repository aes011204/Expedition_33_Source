#include "pch.h"
#include "UI_Editor_Manager.h"
#include "GameInstance.h"
#include "ShaderCom.h"
#include "Texture.h"
#include "VIBuffer_Rect.h"
#include "UI_Content_Browser.h"
#include "UI_Hierarchy_View.h"
#include "UI_Inspector.h"
#include "UI_Viewport_View.h"
#include "UI_Editor_Utils.h"
#include <commdlg.h>

namespace
{
    constexpr wchar_t kDefaultJsonFileName[] = L"UI_Layout.json";
    constexpr size_t kMaxHistoryStateCount = 100;

    string Normalize_PathForCompare(const string& path)
    {
        string normalizedPath = String::ToLowerCopy(path);
        replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');
        return normalizedPath;
    }

    fs::path Get_ModuleDirectory()
    {
        wchar_t modulePath[MAX_PATH] = {};
        if (0 == GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
            return {};

        return fs::path(modulePath).parent_path();
    }

    bool Try_CreateDirectory(const fs::path& directory)
    {
        if (directory.empty())
            return false;

        try
        {
            if (fs::exists(directory))
                return fs::is_directory(directory);

            return fs::create_directories(directory);
        }
        catch (const exception& exception)
        {
            LOG_WARN("UI Editor json directory create failed: {} ({})", exception.what(), String::ToString(directory.wstring()));
            return false;
        }
    }

    bool Try_FindResourceRootFromStart(const fs::path& startPath, fs::path& outResourceRoot)
    {
        if (startPath.empty())
            return false;

        try
        {
            fs::path start = fs::absolute(startPath).lexically_normal();
            for (fs::path currentPath = start; !currentPath.empty(); currentPath = currentPath.parent_path())
            {
                const fs::path candidate = (currentPath / L"Client" / L"Bin" / L"Resources").lexically_normal();
                if (fs::exists(candidate) && fs::is_directory(candidate))
                {
                    outResourceRoot = candidate;
                    return true;
                }

                if (currentPath == currentPath.parent_path())
                    break;
            }
        }
        catch (const exception& exception)
        {
            LOG_WARN("UI Editor resource root search failed: {}", exception.what());
        }

        return false;
    }

    wstring Resolve_UIShaderPath()
    {
        vector<fs::path> startPaths;

        const fs::path moduleDirectory = Get_ModuleDirectory();
        if (!moduleDirectory.empty())
            startPaths.push_back(moduleDirectory);

        try
        {
            startPaths.push_back(fs::current_path());
        }
        catch (const exception& exception)
        {
            LOG_WARN("UI Editor shader path current directory query failed: {}", exception.what());
        }

        for (const fs::path& startPath : startPaths)
        {
            fs::path resourceRoot = {};
            if (!Try_FindResourceRootFromStart(startPath, resourceRoot))
                continue;

            const fs::path candidate = (resourceRoot.parent_path() / L"ShaderFiles" / L"Shader_UI.hlsl").lexically_normal();
            if (fs::exists(candidate))
                return candidate.wstring();
        }

        const vector<fs::path> fallbackCandidates = {
            L"..\\Bin\\ShaderFiles\\Shader_UI.hlsl",
            L"..\\..\\Client\\Bin\\ShaderFiles\\Shader_UI.hlsl",
            L"..\\..\\..\\Client\\Bin\\ShaderFiles\\Shader_UI.hlsl",
            L"Client\\Bin\\ShaderFiles\\Shader_UI.hlsl",
        };

        for (const fs::path& candidate : fallbackCandidates)
        {
            try
            {
                const fs::path normalizedCandidate = fs::absolute(candidate).lexically_normal();
                if (fs::exists(normalizedCandidate))
                    return normalizedCandidate.wstring();
            }
            catch (...)
            {
            }
        }

        return {};
    }

    string Make_MaskedTexturePreviewKey(const string& textureGuid, const string& maskTextureGuid)
    {
        return textureGuid + "|" + maskTextureGuid;
    }

    vector<wstring> Split_FontPreviewLines(const wstring& text)
    {
        vector<wstring> lines;
        size_t lineStart = 0;

        for (size_t i = 0; i <= text.size(); ++i)
        {
            if (i == text.size() || text[i] == L'\n')
            {
                size_t lineEnd = i;
                if (lineEnd > lineStart && text[lineEnd - 1] == L'\r')
                    --lineEnd;

                lines.push_back(text.substr(lineStart, lineEnd - lineStart));
                lineStart = i + 1;
            }
        }

        if (lines.empty())
            lines.push_back(L"");

        return lines;
    }

    float Measure_FontPreviewWidth(const SpriteFont& font, const wstring& text, float scale)
    {
        if (text.empty())
            return 0.f;

        XMFLOAT2 measuredSize{};
        XMStoreFloat2(&measuredSize, font.MeasureString(text.c_str()));
        return max(0.f, measuredSize.x * scale);
    }

    float Measure_FontPreviewLineHeight(const SpriteFont& font, float scale)
    {
        XMFLOAT2 measuredSize{};
        XMStoreFloat2(&measuredSize, font.MeasureString(L"Mg"));

        const float measuredHeight = measuredSize.y > 0.f ? measuredSize.y : 32.f;
        return max(1.f, measuredHeight * scale);
    }

    float Resolve_FontPreviewLineX(float boxLeft, float boxRight, float lineWidth, UI_TextAlign textAlign)
    {
        constexpr float textPaddingX = 4.f;
        const float boxWidth = max(0.f, boxRight - boxLeft);

        switch (textAlign)
        {
        case UI_TextAlign::Center:
            return boxLeft + max(textPaddingX, (boxWidth - lineWidth) * 0.5f);

        case UI_TextAlign::Right:
            return boxRight - lineWidth - textPaddingX;

        case UI_TextAlign::Left:
        default:
            return boxLeft + textPaddingX;
        }
    }

    bool Try_GetTexture2DDesc(ID3D11ShaderResourceView* srv, D3D11_TEXTURE2D_DESC& outDesc)
    {
        if (nullptr == srv)
            return false;

        ComPtr<ID3D11Resource> resource = nullptr;
        srv->GetResource(resource.GetAddressOf());
        if (nullptr == resource)
            return false;

        ComPtr<ID3D11Texture2D> texture = nullptr;
        if (FAILED(resource.As(&texture)) || nullptr == texture)
            return false;

        texture->GetDesc(&outDesc);
        return outDesc.Width > 0 && outDesc.Height > 0;
    }

    void Compute_MaskedPreviewSize(const D3D11_TEXTURE2D_DESC& sourceDesc, uint32& outWidth, uint32& outHeight)
    {
        constexpr uint32 kMaxPreviewTextureSize = 1024;

        outWidth = max(1u, sourceDesc.Width);
        outHeight = max(1u, sourceDesc.Height);

        const uint32 maxAxis = max(outWidth, outHeight);
        if (maxAxis <= kMaxPreviewTextureSize)
            return;

        const float scale = static_cast<float>(kMaxPreviewTextureSize) / static_cast<float>(maxAxis);
        outWidth = max(1u, static_cast<uint32>(roundf(static_cast<float>(outWidth) * scale)));
        outHeight = max(1u, static_cast<uint32>(roundf(static_cast<float>(outHeight) * scale)));
    }

    vector<fs::path> Build_DefaultJsonDirectoryCandidates(const fs::path& startPath)
    {
        vector<fs::path> candidates;
        if (startPath.empty())
            return candidates;

        candidates.push_back(startPath / L"..\\Bin\\Resources\\UI\\UI_Json");
        candidates.push_back(startPath / L"..\\Client\\Bin\\Resources\\UI\\UI_Json");
        candidates.push_back(startPath / L"..\\..\\Client\\Bin\\Resources\\UI\\UI_Json");
        candidates.push_back(startPath / L"..\\..\\..\\Client\\Bin\\Resources\\UI\\UI_Json");
        candidates.push_back(startPath / L"Bin\\Resources\\UI\\UI_Json");
        candidates.push_back(startPath / L"Client\\Bin\\Resources\\UI\\UI_Json");

        return candidates;
    }
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

    _defaultJsonDirectory = Resolve_DefaultJsonDirectory();

    Add_Window(UI_Hierarchy_View::Create(*this));
    Add_Window(UI_Viewport_View::Create(*this));
    Add_Window(UI_Inspector::Create(*this));
    Add_Window(UI_Content_Browser::Create(*this));

    Clear_History();
    Clear_Dirty();

    return S_OK;
}

void UI_Editor_Manager::Render()
{
    _fontPreviewCommands.clear();

    Render_DockSpace();
    Handle_Shortcuts();

    for (auto& pair : _windows)
    {
        if (nullptr != pair.second)
            pair.second->Render();
    }

    Render_RenamePopup();
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

void UI_Editor_Manager::Add_Window(const Shared<UI_Editor_Window>& window)
{
    if (nullptr == window)
        return;

    _windows[window->Get_WindowName()] = window;
}

Shared<UI_Editor_Window> UI_Editor_Manager::Find_Window(const wstring& windowName) const
{
    const auto iter = _windows.find(windowName);
    if (iter == _windows.end())
        return nullptr;

    return iter->second;
}

UI_Document& UI_Editor_Manager::Get_Document()
{
    return _document;
}

const UI_Document& UI_Editor_Manager::Get_Document() const
{
    return _document;
}

ObjectId UI_Editor_Manager::Get_SelectedObjectId() const
{
    return _selectedObjectId;
}

void UI_Editor_Manager::Set_SelectedObjectId(ObjectId objectId)
{
    _selectedObjectId = nullptr != _document.Find_Object(objectId) ? objectId : 0;
}

UI_Object* UI_Editor_Manager::Get_SelectedObject()
{
    return _document.Find_Object(_selectedObjectId);
}

const UI_Object* UI_Editor_Manager::Get_SelectedObject() const
{
    return _document.Find_Object(_selectedObjectId);
}

bool UI_Editor_Manager::Is_AssetManagerReady() const
{
    return _assetManagerReady;
}

bool UI_Editor_Manager::Is_Dirty() const
{
    return _isDirty;
}

bool UI_Editor_Manager::Request_Close()
{
    return Confirm_SaveIfDirty(L"close UI Editor");
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
                New_Document();
            }
            if (ImGui::MenuItem("Save"))
            {
                Save_Document();
            }
            if (ImGui::MenuItem("Save As"))
            {
                Save_DocumentAs();
            }
            if (ImGui::MenuItem("Load"))
            {
                Load_Document();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Create"))
        {
            if (ImGui::MenuItem("Image"))
                Create_UIObject(UI_ObjectType::Image);

            if (ImGui::MenuItem("Text"))
                Create_UIObject(UI_ObjectType::Text);

            if (ImGui::MenuItem("Panel"))
                Create_UIObject(UI_ObjectType::Panel);

            if (ImGui::MenuItem("Button"))
                Create_UIObject(UI_ObjectType::Button);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window"))
        {
            for (auto& pair : _windows)
            {
                if (nullptr == pair.second)
                    continue;

                bool isWindowOpen = pair.second->Is_Open();
                const string windowName = String::ToString(pair.first);
                if (ImGui::MenuItem(windowName.c_str(), nullptr, &isWindowOpen))
                    pair.second->Set_Open(isWindowOpen);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, Can_Undo()))
                Undo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, Can_Redo()))
                Redo();
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    ImGui::End();
}

bool UI_Editor_Manager::Save_Document()
{
    if (_currentDocumentPath.empty())
        return Save_DocumentAs();

    if (!_document.Save_ToJsonFile(_currentDocumentPath))
        return false;

    Clear_Dirty();
    return true;
}

bool UI_Editor_Manager::Save_DocumentAs()
{
    wstring filePath = _currentDocumentPath;
    if (!Show_SaveFileDialog(filePath))
        return false;

    Ensure_JsonExtension(filePath);

    if (!_document.Save_ToJsonFile(filePath))
        return false;

    _currentDocumentPath = filePath;
    Clear_Dirty();
    return true;
}

bool UI_Editor_Manager::Load_Document()
{
    if (!Confirm_SaveIfDirty(L"load another UI document"))
        return false;

    wstring filePath = {};
    if (!Show_LoadFileDialog(filePath))
        return false;

    if (!_document.Load_FromJsonFile(filePath))
        return false;

    _currentDocumentPath = filePath;
    _selectedObjectId = 0;
    Clear_RuntimeCaches();
    Resolve_LoadedAssetReferences();
    Clear_History();
    Clear_Dirty();
    return true;
}

bool UI_Editor_Manager::New_Document()
{
    if (!Confirm_SaveIfDirty(L"create a new UI document"))
        return false;

    _document.Reset();
    _currentDocumentPath.clear();
    _selectedObjectId = 0;
    Clear_RuntimeCaches();
    Clear_History();
    Clear_Dirty();
    return true;
}

bool UI_Editor_Manager::Confirm_SaveIfDirty(const wchar_t* actionName)
{
    if (!_isDirty)
        return true;

    const wstring actionText = (nullptr != actionName && L'\0' != actionName[0]) ? actionName : L"continue";
    const wstring message = L"The current UI document has unsaved changes.\n\nSave before you " + actionText + L"?";

    const int result = MessageBoxW(
        nullptr,
        message.c_str(),
        L"UI Editor",
        MB_ICONQUESTION | MB_YESNOCANCEL);

    if (result == IDCANCEL)
        return false;

    if (result == IDYES)
        return Save_Document();

    return true;
}

void UI_Editor_Manager::Handle_Shortcuts()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput)
        return;

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z))
    {
        Undo();
        return;
    }

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y))
    {
        Redo();
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Delete))
    {
        Delete_SelectedUIObject();
        return;
    }

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D))
    {
        Duplicate_SelectedUIObject();
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F2))
    {
        Request_RenameUIObject(_selectedObjectId);
        return;
    }
}

void UI_Editor_Manager::Render_RenamePopup()
{
    if (_openRenamePopup)
    {
        ImGui::OpenPopup("Rename Selected UI Object");
        _openRenamePopup = false;
    }

    if (ImGui::BeginPopupModal("Rename Selected UI Object", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        UI_Object* object = _document.Find_Object(_renameRequestObjectId);
        if (nullptr == object)
        {
            _renameRequestObjectId = 0;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        ImGui::Text("ID: %u", object->id);
        ImGui::SetNextItemWidth(280.f);
        const bool submitted = ImGui::InputText("Name", _renameBuffer, size(_renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

        if (submitted || ImGui::Button("OK"))
        {
            Rename_UIObject(_renameRequestObjectId, _renameBuffer);
            _renameRequestObjectId = 0;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            _renameRequestObjectId = 0;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void UI_Editor_Manager::Clear_RuntimeCaches()
{
    _textureThumbCache.clear();
    _textureNoThumbGuids.clear();
    _maskedTexturePreviewCache.clear();
    _maskedTexturePreviewFailedKeys.clear();
    _fontPreviewCache.clear();
    _fontPreviewFailedGuids.clear();
    _fontPreviewCommands.clear();
}

bool UI_Editor_Manager::Show_SaveFileDialog(wstring& outFilePath) const
{
    wchar_t filePathBuffer[MAX_PATH] = {};
    if (!outFilePath.empty())
    {
        wcsncpy_s(filePathBuffer, outFilePath.c_str(), _TRUNCATE);
    }
    else
    {
        wcsncpy_s(filePathBuffer, kDefaultJsonFileName, _TRUNCATE);
    }

    const wstring initialDirectory = !_defaultJsonDirectory.empty() ? _defaultJsonDirectory : Resolve_DefaultJsonDirectory();

    OPENFILENAMEW openFileName = {};
    openFileName.lStructSize = sizeof(openFileName);
    openFileName.lpstrFilter = L"JSON Files\0*.json\0All Files\0*.*\0";
    openFileName.lpstrFile = filePathBuffer;
    openFileName.nMaxFile = MAX_PATH;
    if (!initialDirectory.empty())
        openFileName.lpstrInitialDir = initialDirectory.c_str();
    openFileName.lpstrDefExt = L"json";
    openFileName.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (FALSE == GetSaveFileNameW(&openFileName))
        return false;

    outFilePath = filePathBuffer;
    Ensure_JsonExtension(outFilePath);
    return !outFilePath.empty();
}

bool UI_Editor_Manager::Show_LoadFileDialog(wstring& outFilePath) const
{
    wchar_t filePathBuffer[MAX_PATH] = {};
    const wstring initialDirectory = !_defaultJsonDirectory.empty() ? _defaultJsonDirectory : Resolve_DefaultJsonDirectory();

    OPENFILENAMEW openFileName = {};
    openFileName.lStructSize = sizeof(openFileName);
    openFileName.lpstrFilter = L"JSON Files\0*.json\0All Files\0*.*\0";
    openFileName.lpstrFile = filePathBuffer;
    openFileName.nMaxFile = MAX_PATH;
    if (!initialDirectory.empty())
        openFileName.lpstrInitialDir = initialDirectory.c_str();
    openFileName.lpstrDefExt = L"json";
    openFileName.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (FALSE == GetOpenFileNameW(&openFileName))
        return false;

    outFilePath = filePathBuffer;
    return !outFilePath.empty();
}

wstring UI_Editor_Manager::Resolve_DefaultJsonDirectory() const
{
    vector<fs::path> startPaths;

    const fs::path moduleDirectory = Get_ModuleDirectory();
    if (!moduleDirectory.empty())
        startPaths.push_back(moduleDirectory);

    try
    {
        startPaths.push_back(fs::current_path());
    }
    catch (const exception& exception)
    {
        LOG_WARN("UI Editor current path query failed: {}", exception.what());
    }

    for (const fs::path& startPath : startPaths)
    {
        fs::path resourceRoot = {};
        if (Try_FindResourceRootFromStart(startPath, resourceRoot))
        {
            const fs::path jsonDirectory = (resourceRoot / L"UI" / L"UI_Json").lexically_normal();
            if (Try_CreateDirectory(jsonDirectory))
                return jsonDirectory.wstring();
        }
    }

    vector<fs::path> fallbackCandidates;
    for (const fs::path& startPath : startPaths)
    {
        vector<fs::path> candidates = Build_DefaultJsonDirectoryCandidates(startPath);
        fallbackCandidates.insert(fallbackCandidates.end(), candidates.begin(), candidates.end());
    }

    for (const fs::path& candidate : fallbackCandidates)
    {
        try
        {
            const fs::path normalizedCandidate = fs::absolute(candidate).lexically_normal();
            if (fs::exists(normalizedCandidate) && fs::is_directory(normalizedCandidate))
                return normalizedCandidate.wstring();
        }
        catch (...)
        {
        }
    }

    for (const fs::path& candidate : fallbackCandidates)
    {
        try
        {
            const fs::path normalizedCandidate = fs::absolute(candidate).lexically_normal();
            if (Try_CreateDirectory(normalizedCandidate))
                return normalizedCandidate.wstring();
        }
        catch (const exception& exception)
        {
            LOG_WARN("UI Editor fallback json directory failed: {} ({})", exception.what(), String::ToString(candidate.wstring()));
        }
    }

    LOG_WARN("UI Editor default json directory could not be resolved.");
    return {};
}

void UI_Editor_Manager::Ensure_JsonExtension(wstring& filePath) const
{
    if (filePath.empty())
        return;

    fs::path path = fs::path(filePath);
    if (path.extension().empty())
        path.replace_extension(L".json");

    filePath = path.wstring();
}

void UI_Editor_Manager::Resolve_LoadedAssetReferences()
{
    if (!_assetManagerReady)
        return;

    vector<UI_Object>& objects = _document.Get_MutableObjects();

    for (UI_Object& object : objects)
    {
        if (object.type == UI_ObjectType::Image)
        {
            auto resolveTextureGuid = [](string& guid, const string& path, const char* label)
            {
                if (!guid.empty() || path.empty())
                    return;

                const string targetPath = Normalize_PathForCompare(path);
                for (const AssetMeta& assetMeta : GAME->Get_AssetsByType("Texture"))
                {
                    if (Normalize_PathForCompare(Make_ClientRuntimeResourcePath(assetMeta)) == targetPath)
                    {
                        guid = assetMeta.guid;
                        break;
                    }
                }

                if (guid.empty())
                    LOG_WARN("UI Editor load: {} texture not found for {}", label, path);
            };

            resolveTextureGuid(object.textureGuid, object.texturePath, "image");
            resolveTextureGuid(object.maskTextureGuid, object.maskTexturePath, "mask");
        }
        else if (object.type == UI_ObjectType::Text)
        {
            if (object.fontGuid.empty() && !object.fontPath.empty())
            {
                const string targetPath = Normalize_PathForCompare(object.fontPath);
                for (const AssetMeta& assetMeta : GAME->Get_AssetsByType("Font"))
                {
                    if (Normalize_PathForCompare(Make_ClientRuntimeResourcePath(assetMeta)) == targetPath)
                    {
                        object.fontGuid = assetMeta.guid;
                        if (object.fontTag.empty())
                            object.fontTag = Make_FontTag(assetMeta);
                        break;
                    }
                }

                if (object.fontGuid.empty())
                    LOG_WARN("UI Editor load: font not found for {}", object.fontPath);
            }

            if (object.fontTag.empty() && !object.fontPath.empty())
                object.fontTag = String::ToString(fs::path(String::ToWString(object.fontPath)).stem().wstring());
        }
    }
}

bool UI_Editor_Manager::Queue_FontPreview(
    const string& fontGuid,
    const string& fontPath,
    const wstring& text,
    const Vec2& boxMin,
    const Vec2& boxMax,
    const Vec4& color,
    float rotationRadians,
    UI_TextAlign textAlign,
    float scale,
    const D3D11_RECT& clipRect)
{
    SpriteFont* font = Get_FontPreview(fontGuid, fontPath);
    if (nullptr == font || text.empty())
        return false;

    const vector<wstring> lines = Split_FontPreviewLines(text);
    const float lineHeight = Measure_FontPreviewLineHeight(*font, scale);
    float textY = boxMin.y + 3.f;
    bool queuedAny = false;

    for (const wstring& line : lines)
    {
        if (textY >= boxMax.y)
            break;

        const wstring safeLine = Build_SafeSpriteFontText(*font, line);
        if (!safeLine.empty())
        {
            const float lineWidth = Measure_FontPreviewWidth(*font, safeLine, scale);
            const float textX = Resolve_FontPreviewLineX(boxMin.x, boxMax.x, lineWidth, textAlign);

            FontPreviewCommand command = {};
            command.fontGuid = fontGuid;
            command.fontPath = fontPath;
            command.text = safeLine;
            command.position = Vec2(textX, textY);
            command.color = color;
            command.rotationRadians = rotationRadians;
            command.pivot = Vec2(0.f, 0.f);
            command.scale = scale;
            command.clipRect = clipRect;
            _fontPreviewCommands.push_back(command);
            queuedAny = true;
        }

        textY += lineHeight;
    }

    return queuedAny;
}

ObjectId UI_Editor_Manager::Resolve_CreateParentId() const
{
    if (_selectedObjectId == 0)
        return 0;

    const UI_Object* selectedObject = _document.Find_Object(_selectedObjectId);
    if (nullptr == selectedObject)
        return 0;

    if (Is_ContainerObjectType(selectedObject->type))
        return selectedObject->id;

    return selectedObject->parentId;
}

void UI_Editor_Manager::Create_UIObject(UI_ObjectType objectType)
{
    Push_UndoSnapshot();

    const ObjectId parentId = Resolve_CreateParentId();
    const ObjectId createdObjectId = _document.Create_Object(objectType, parentId);
    if (createdObjectId == 0)
        return;

    if (objectType == UI_ObjectType::Button)
    {
        UI_Object* buttonObject = _document.Find_Object(createdObjectId);
        if (nullptr != buttonObject)
        {
            const ObjectId labelObjectId = _document.Create_Object(UI_ObjectType::Text, createdObjectId);
            UI_Object* labelObject = _document.Find_Object(labelObjectId);
            if (nullptr != labelObject)
            {
                labelObject->name = _document.Generate_UniqueName("Label", labelObjectId);
                labelObject->position = ImVec2(0.f, 0.f);
                labelObject->size = buttonObject->size;
                labelObject->scale = ImVec2(1.f, 1.f);
                labelObject->color = ImVec4(1.f, 1.f, 1.f, 1.f);
                labelObject->text = "Button";
                labelObject->fontSize = 24.f;
                labelObject->textAlign = UI_TextAlign::Center;
                labelObject->textWrap = false;
                labelObject->sortOrder = static_cast<int32>(_document.Get_Objects().size());
            }
        }
    }

    _selectedObjectId = createdObjectId;
    Mark_Dirty();
}

void UI_Editor_Manager::Duplicate_UIObject(ObjectId objectId)
{
    if (nullptr == _document.Find_Object(objectId))
        return;

    Push_UndoSnapshot();

    const ObjectId duplicatedObjectId = _document.Duplicate_Object(objectId, ImVec2(20.f, 20.f));
    if (duplicatedObjectId == 0)
        return;

    _selectedObjectId = duplicatedObjectId;
    Mark_Dirty();
}

void UI_Editor_Manager::Delete_UIObject(ObjectId objectId)
{
    if (nullptr == _document.Find_Object(objectId))
        return;

    Push_UndoSnapshot();

    if (!_document.Delete_Object(objectId))
        return;

    if (nullptr == _document.Find_Object(_selectedObjectId))
        _selectedObjectId = 0;

    Mark_Dirty();
}

void UI_Editor_Manager::Duplicate_SelectedUIObject()
{
    if (_selectedObjectId == 0)
        return;

    Duplicate_UIObject(_selectedObjectId);
}

void UI_Editor_Manager::Delete_SelectedUIObject()
{
    if (_selectedObjectId == 0)
        return;

    Delete_UIObject(_selectedObjectId);
}

bool UI_Editor_Manager::Rename_UIObject(ObjectId objectId, const string& newName)
{
    UI_Object* object = _document.Find_Object(objectId);
    if (nullptr == object || newName.empty())
        return false;

    const string uniqueName = _document.Generate_UniqueName(newName, objectId);
    if (object->name == uniqueName)
        return true;

    Push_UndoSnapshot();

    object->name = uniqueName;
    Mark_Dirty();
    return true;
}

void UI_Editor_Manager::Request_RenameUIObject(ObjectId objectId)
{
    UI_Object* object = _document.Find_Object(objectId);
    if (nullptr == object)
        return;

    _renameRequestObjectId = objectId;
    strncpy_s(_renameBuffer, object->name.c_str(), _TRUNCATE);
    _openRenamePopup = true;
}

bool UI_Editor_Manager::Reparent_UIObject(ObjectId objectId, ObjectId newParentId)
{
    const UI_Object* object = _document.Find_Object(objectId);
    if (nullptr == object || object->parentId == newParentId)
        return false;

    Push_UndoSnapshot();

    if (!_document.Change_Parent(objectId, newParentId, true))
        return false;

    _selectedObjectId = objectId;
    Mark_Dirty();
    return true;
}

bool UI_Editor_Manager::Can_ReparentUIObject(ObjectId objectId, ObjectId newParentId) const
{
    return _document.Can_Parent(objectId, newParentId);
}

bool UI_Editor_Manager::Set_UIObjectWorldPosition(ObjectId objectId, const ImVec2& worldPosition)
{
    if (!_isEditTransactionOpen)
        Push_UndoSnapshot();

    if (!_document.Set_WorldPosition(objectId, worldPosition))
        return false;

    Mark_Dirty();
    return true;
}

bool UI_Editor_Manager::Set_UIObjectWorldRect(ObjectId objectId, const ImVec2& worldPosition, const ImVec2& worldSize)
{
    UI_Object* object = _document.Find_Object(objectId);
    if (nullptr == object)
        return false;

    if (!_isEditTransactionOpen)
        Push_UndoSnapshot();

    const float safeScaleX = max(0.01f, object->scale.x);
    const float safeScaleY = max(0.01f, object->scale.y);
    object->size = ImVec2(max(1.f, worldSize.x) / safeScaleX, max(1.f, worldSize.y) / safeScaleY);

    if (!_document.Set_WorldPosition(objectId, worldPosition))
        return false;

    Mark_Dirty();
    return true;
}

ImVec2 UI_Editor_Manager::Get_UIObjectWorldPosition(ObjectId objectId) const
{
    return _document.Get_WorldPosition(objectId);
}

bool UI_Editor_Manager::Undo()
{
    if (!Can_Undo())
        return false;

    if (_isEditTransactionOpen)
        Commit_EditTransactionIfChanged();

    const HistoryState currentState = Capture_HistoryState();
    const HistoryState undoState = _undoStack.back();
    _undoStack.pop_back();
    Push_HistoryState(_redoStack, currentState);
    Apply_HistoryState(undoState);
    return true;
}

bool UI_Editor_Manager::Redo()
{
    if (!Can_Redo())
        return false;

    if (_isEditTransactionOpen)
        Commit_EditTransactionIfChanged();

    const HistoryState currentState = Capture_HistoryState();
    const HistoryState redoState = _redoStack.back();
    _redoStack.pop_back();
    Push_HistoryState(_undoStack, currentState);
    Apply_HistoryState(redoState);
    return true;
}

bool UI_Editor_Manager::Can_Undo() const
{
    return !_undoStack.empty();
}

bool UI_Editor_Manager::Can_Redo() const
{
    return !_redoStack.empty();
}

void UI_Editor_Manager::Begin_EditTransaction()
{
    if (_isEditTransactionOpen)
        return;

    _editTransactionStartState = Capture_HistoryState();
    _isEditTransactionOpen = true;
}

void UI_Editor_Manager::Commit_EditTransactionIfChanged()
{
    if (!_isEditTransactionOpen)
        return;

    const HistoryState currentState = Capture_HistoryState();
    if (!Is_SameHistoryState(_editTransactionStartState, currentState))
    {
        Push_HistoryState(_undoStack, _editTransactionStartState);
        _redoStack.clear();
        _isDirty = !Is_CurrentDocumentClean();
    }

    _isEditTransactionOpen = false;
    _editTransactionStartState = {};
}

void UI_Editor_Manager::Mark_Dirty()
{
    _isDirty = !Is_CurrentDocumentClean();
}

void UI_Editor_Manager::Clear_Dirty()
{
    _cleanDocumentSignature = Make_DocumentSignature(_document);
    _isDirty = false;
}

string UI_Editor_Manager::Make_DocumentSignature(const UI_Document& document) const
{
    return document.To_Json().dump();
}

UI_Editor_Manager::HistoryState UI_Editor_Manager::Capture_HistoryState() const
{
    HistoryState historyState = {};
    historyState.document = _document;
    historyState.selectedObjectId = _selectedObjectId;
    historyState.isDirty = _isDirty;
    historyState.documentSignature = Make_DocumentSignature(_document);
    return historyState;
}

bool UI_Editor_Manager::Is_SameHistoryState(const HistoryState& lhs, const HistoryState& rhs) const
{
    return lhs.selectedObjectId == rhs.selectedObjectId && lhs.documentSignature == rhs.documentSignature;
}

bool UI_Editor_Manager::Is_CurrentDocumentClean() const
{
    return !_cleanDocumentSignature.empty() && Make_DocumentSignature(_document) == _cleanDocumentSignature;
}

void UI_Editor_Manager::Push_UndoSnapshot()
{
    if (_isEditTransactionOpen)
        return;

    Push_HistoryState(_undoStack, Capture_HistoryState());
    _redoStack.clear();
}

void UI_Editor_Manager::Push_HistoryState(vector<HistoryState>& historyStack, const HistoryState& historyState) const
{
    if (!historyStack.empty() && Is_SameHistoryState(historyStack.back(), historyState))
        return;

    historyStack.push_back(historyState);
    if (historyStack.size() > kMaxHistoryStateCount)
        historyStack.erase(historyStack.begin());
}

void UI_Editor_Manager::Clear_History()
{
    _undoStack.clear();
    _redoStack.clear();
    _isEditTransactionOpen = false;
    _editTransactionStartState = {};
}

void UI_Editor_Manager::Apply_HistoryState(const HistoryState& historyState)
{
    _document = historyState.document;
    _selectedObjectId = nullptr != _document.Find_Object(historyState.selectedObjectId) ? historyState.selectedObjectId : 0;
    _isDirty = !Is_CurrentDocumentClean();
    Clear_RuntimeCaches();
    Resolve_LoadedAssetReferences();
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

ID3D11ShaderResourceView* UI_Editor_Manager::Get_MaskedTexturePreviewSRV(const string& textureGuid, const string& maskTextureGuid)
{
    if (!_assetManagerReady || textureGuid.empty() || maskTextureGuid.empty() || nullptr == _device || nullptr == _context)
        return nullptr;

    const string cacheKey = Make_MaskedTexturePreviewKey(textureGuid, maskTextureGuid);
    const auto cachedIter = _maskedTexturePreviewCache.find(cacheKey);
    if (cachedIter != _maskedTexturePreviewCache.end())
        return cachedIter->second.Get();

    if (_maskedTexturePreviewFailedKeys.find(cacheKey) != _maskedTexturePreviewFailedKeys.end())
        return nullptr;

    ID3D11ShaderResourceView* textureSRV = Get_TexturePreviewSRV(textureGuid);
    ID3D11ShaderResourceView* maskSRV = Get_TexturePreviewSRV(maskTextureGuid);
    if (nullptr == textureSRV || nullptr == maskSRV)
    {
        _maskedTexturePreviewFailedKeys.insert(cacheKey);
        return nullptr;
    }

    return Create_MaskedTexturePreviewSRV(cacheKey, textureSRV, maskSRV);
}

bool UI_Editor_Manager::Ensure_ImagePreviewRenderer()
{
    if (nullptr != _imagePreviewShader && nullptr != _imagePreviewVIBuffer)
        return true;

    if (nullptr == _device || nullptr == _context)
        return false;

    const wstring shaderPath = Resolve_UIShaderPath();
    if (shaderPath.empty())
    {
        LOG_WARN("UI Editor masked preview shader path could not be resolved.");
        return false;
    }

    _imagePreviewShader = ShaderCom::Create(_device, _context, shaderPath.c_str(), VTXTEX::Elements, VTXTEX::numElements);
    _imagePreviewVIBuffer = VIBuffer_Rect::Create(_device, _context);

    if (nullptr == _imagePreviewShader || nullptr == _imagePreviewVIBuffer)
    {
        _imagePreviewShader.reset();
        _imagePreviewVIBuffer.reset();
        LOG_WARN("UI Editor masked preview renderer create failed.");
        return false;
    }

    return true;
}

ID3D11ShaderResourceView* UI_Editor_Manager::Create_MaskedTexturePreviewSRV(
    const string& cacheKey,
    ID3D11ShaderResourceView* textureSRV,
    ID3D11ShaderResourceView* maskSRV)
{
    if (cacheKey.empty() || nullptr == textureSRV || nullptr == maskSRV)
        return nullptr;

    if (!Ensure_ImagePreviewRenderer())
    {
        _maskedTexturePreviewFailedKeys.insert(cacheKey);
        return nullptr;
    }

    D3D11_TEXTURE2D_DESC sourceDesc = {};
    if (!Try_GetTexture2DDesc(textureSRV, sourceDesc))
    {
        _maskedTexturePreviewFailedKeys.insert(cacheKey);
        return nullptr;
    }

    uint32 previewWidth = 1;
    uint32 previewHeight = 1;
    Compute_MaskedPreviewSize(sourceDesc, previewWidth, previewHeight);

    D3D11_TEXTURE2D_DESC renderTextureDesc = {};
    renderTextureDesc.Width = previewWidth;
    renderTextureDesc.Height = previewHeight;
    renderTextureDesc.MipLevels = 1;
    renderTextureDesc.ArraySize = 1;
    renderTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    renderTextureDesc.SampleDesc.Count = 1;
    renderTextureDesc.Usage = D3D11_USAGE_DEFAULT;
    renderTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    ComPtr<ID3D11Texture2D> renderTexture = nullptr;
    if (FAILED(_device->CreateTexture2D(&renderTextureDesc, nullptr, renderTexture.GetAddressOf())))
    {
        _maskedTexturePreviewFailedKeys.insert(cacheKey);
        return nullptr;
    }

    ComPtr<ID3D11RenderTargetView> renderTargetView = nullptr;
    if (FAILED(_device->CreateRenderTargetView(renderTexture.Get(), nullptr, renderTargetView.GetAddressOf())))
    {
        _maskedTexturePreviewFailedKeys.insert(cacheKey);
        return nullptr;
    }

    ComPtr<ID3D11ShaderResourceView> previewSRV = nullptr;
    if (FAILED(_device->CreateShaderResourceView(renderTexture.Get(), nullptr, previewSRV.GetAddressOf())))
    {
        _maskedTexturePreviewFailedKeys.insert(cacheKey);
        return nullptr;
    }

    ComPtr<ID3D11RenderTargetView> previousRTV = nullptr;
    ComPtr<ID3D11DepthStencilView> previousDSV = nullptr;
    _context->OMGetRenderTargets(1, previousRTV.GetAddressOf(), previousDSV.GetAddressOf());

    UINT previousViewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    D3D11_VIEWPORT previousViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
    _context->RSGetViewports(&previousViewportCount, previousViewports);

    const float clearColor[4] = { 0.f, 0.f, 0.f, 0.f };
    _context->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), nullptr);
    _context->ClearRenderTargetView(renderTargetView.Get(), clearColor);

    D3D11_VIEWPORT previewViewport = {};
    previewViewport.TopLeftX = 0.f;
    previewViewport.TopLeftY = 0.f;
    previewViewport.Width = static_cast<float>(previewWidth);
    previewViewport.Height = static_cast<float>(previewHeight);
    previewViewport.MinDepth = 0.f;
    previewViewport.MaxDepth = 1.f;
    _context->RSSetViewports(1, &previewViewport);

    Matrix worldMatrix = Matrix::CreateScale(static_cast<float>(previewWidth), static_cast<float>(previewHeight), 1.f);
    Matrix viewMatrix = Matrix::Identity;
    Matrix projMatrix;
    XMStoreFloat4x4(&projMatrix, XMMatrixOrthographicLH(static_cast<float>(previewWidth), static_cast<float>(previewHeight), 0.f, 1.f));
    const Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);

    bool renderSucceeded =
        SUCCEEDED(_imagePreviewShader->Bind_Matrix("g_WorldMatrix", &worldMatrix)) &&
        SUCCEEDED(_imagePreviewShader->Bind_Matrix("g_ViewMatrix", &viewMatrix)) &&
        SUCCEEDED(_imagePreviewShader->Bind_Matrix("g_ProjMatrix", &projMatrix)) &&
        SUCCEEDED(_imagePreviewShader->Bind_RawValue("g_Color", &color, sizeof(Vec4))) &&
        SUCCEEDED(_imagePreviewShader->Bind_SRV("g_Texture", textureSRV)) &&
        SUCCEEDED(_imagePreviewShader->Bind_SRV("g_MaskTexture", maskSRV)) &&
        SUCCEEDED(_imagePreviewShader->Begin(1));

    if (renderSucceeded)
    {
        const float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
        _context->OMSetBlendState(nullptr, blendFactor, 0xffffffff);

        renderSucceeded =
            SUCCEEDED(_imagePreviewVIBuffer->Bind_Resources()) &&
            SUCCEEDED(_imagePreviewVIBuffer->Render());
    }

    ID3D11RenderTargetView* restoredRTVs[] = { previousRTV.Get() };
    _context->OMSetRenderTargets(1, restoredRTVs, previousDSV.Get());
    if (previousViewportCount > 0)
        _context->RSSetViewports(previousViewportCount, previousViewports);

    if (!renderSucceeded)
    {
        _maskedTexturePreviewFailedKeys.insert(cacheKey);
        return nullptr;
    }

    _maskedTexturePreviewCache.emplace(cacheKey, previewSRV);
    return _maskedTexturePreviewCache[cacheKey].Get();
}

Unique<UI_Editor_Manager> UI_Editor_Manager::Create(const ComPtr<Device>& device, const ComPtr<Context>& context, bool assetManagerReady)
{
    Unique<UI_Editor_Manager> pInstance{ new UI_Editor_Manager() };

    if (FAILED(pInstance->Initialize(device, context, assetManagerReady)))
        return nullptr;

    return pInstance;
}
