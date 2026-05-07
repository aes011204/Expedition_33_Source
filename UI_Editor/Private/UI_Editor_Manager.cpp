#include "pch.h"
#include "UI_Editor_Manager.h"
#include "GameInstance.h"
#include "Texture.h"
#include "UI_Content_Browser.h"
#include "UI_Hierarchy_View.h"
#include "UI_Inspector.h"
#include "UI_Viewport_View.h"
#include "UI_Editor_Utils.h"
#include <commdlg.h>

namespace
{
    constexpr wchar_t kDefaultJsonFileName[] = L"UI_Layout.json";

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

    return S_OK;
}

void UI_Editor_Manager::Render()
{
    _fontPreviewCommands.clear();

    Render_DockSpace();

    for (auto& pair : _windows)
    {
        if (nullptr != pair.second)
            pair.second->Render();
    }
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
    _selectedObjectId = objectId;
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
            ImGui::MenuItem("Undo", nullptr, false, false);
            ImGui::MenuItem("Redo", nullptr, false, false);
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

    return _document.Save_ToJsonFile(_currentDocumentPath);
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
    return true;
}

bool UI_Editor_Manager::Load_Document()
{
    wstring filePath = {};
    if (!Show_LoadFileDialog(filePath))
        return false;

    if (!_document.Load_FromJsonFile(filePath))
        return false;

    _currentDocumentPath = filePath;
    _selectedObjectId = 0;
    Resolve_LoadedAssetReferences();
    return true;
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
        if (object.type == UI_ObjectType::Image && object.textureGuid.empty() && !object.texturePath.empty())
        {
            const string targetPath = Normalize_PathForCompare(object.texturePath);
            for (const FAssetMeta& assetMeta : GAME->Get_AssetsByType("Texture"))
            {
                if (Normalize_PathForCompare(Make_ClientRuntimeResourcePath(assetMeta)) == targetPath)
                {
                    object.textureGuid = assetMeta.guid;
                    break;
                }
            }

            if (object.textureGuid.empty())
                LOG_WARN("UI Editor load: texture not found for {}", object.texturePath);
        }
        else if (object.type == UI_ObjectType::Text)
        {
            if (object.fontGuid.empty() && !object.fontPath.empty())
            {
                const string targetPath = Normalize_PathForCompare(object.fontPath);
                for (const FAssetMeta& assetMeta : GAME->Get_AssetsByType("Font"))
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
    const Vec2& position,
    const Vec4& color,
    float rotationRadians,
    const Vec2& pivot,
    float scale,
    const D3D11_RECT& clipRect)
{
    SpriteFont* font = Get_FontPreview(fontGuid, fontPath);
    if (nullptr == font || text.empty())
        return false;

    const wstring safeText = Build_SafeSpriteFontText(*font, text);
    if (safeText.empty())
        return false;

    FontPreviewCommand command = {};
    command.fontGuid = fontGuid;
    command.fontPath = fontPath;
    command.text = safeText;
    command.position = position;
    command.color = color;
    command.rotationRadians = rotationRadians;
    command.pivot = pivot;
    command.scale = scale;
    command.clipRect = clipRect;
    _fontPreviewCommands.push_back(command);

    return true;
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
    const ObjectId parentId = Resolve_CreateParentId();
    _selectedObjectId = _document.Create_Object(objectType, parentId);
}

void UI_Editor_Manager::Duplicate_UIObject(ObjectId objectId)
{
    const ObjectId duplicatedObjectId = _document.Duplicate_Object(objectId, ImVec2(20.f, 20.f));
    if (duplicatedObjectId == 0)
        return;

    _selectedObjectId = duplicatedObjectId;
}

void UI_Editor_Manager::Delete_UIObject(ObjectId objectId)
{
    if (!_document.Delete_Object(objectId))
        return;

    if (nullptr == _document.Find_Object(_selectedObjectId))
        _selectedObjectId = 0;
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
    Unique<UI_Editor_Manager> pInstance{ new UI_Editor_Manager() };

    if (FAILED(pInstance->Initialize(device, context, assetManagerReady)))
        return nullptr;

    return pInstance;
}
