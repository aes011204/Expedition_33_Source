#include "pch.h"
#include "UI_Editor_Manager.h"

#include "GameInstance.h"
#include "UI_Content_Browser.h"
#include "UI_Editor_Utils.h"
#include "UI_Hierarchy_View.h"
#include "UI_Inspector.h"
#include "UI_Viewport_View.h"

namespace UIEditor
{
    namespace
    {
        string Normalize_PathForCompare(const string& path)
        {
            string normalizedPath = String::ToLowerCopy(path);
            replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');
            return normalizedPath;
        }
    }

    HRESULT UI_Editor_Manager::Initialize(const ComPtr<Device>& device, const ComPtr<Context>& context, bool assetManagerReady)
    {
        _assetManagerReady = assetManagerReady;

        if (FAILED(_previewService.Initialize(device, context, assetManagerReady)))
            return E_FAIL;

        if (FAILED(_fileService.Initialize()))
            return E_FAIL;

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
        _previewService.Begin_Frame();

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
        _previewService.Render_FontPreviews();
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
        return _historyService.Is_Dirty();
    }

    bool UI_Editor_Manager::Request_Close()
    {
        return Confirm_SaveIfDirty(L"close UI Editor");
    }

    ID3D11ShaderResourceView* UI_Editor_Manager::Get_TexturePreviewSRV(const string& textureGuid)
    {
        return _previewService.Get_TexturePreviewSRV(textureGuid);
    }

    ID3D11ShaderResourceView* UI_Editor_Manager::Get_MaskedTexturePreviewSRV(const string& textureGuid, const string& maskTextureGuid)
    {
        return _previewService.Get_MaskedTexturePreviewSRV(textureGuid, maskTextureGuid);
    }

    void UI_Editor_Manager::Render_DockSpace()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (nullptr == viewport)
            return;

        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));

        bool isOpen = true;
        ImGui::Begin("UIEditorDockSpace", &isOpen, windowFlags);
        ImGui::PopStyleVar(3);

        const ImGuiID dockSpaceId = ImGui::GetID("UIEditorDockSpaceId");
        ImGui::DockSpace(dockSpaceId, ImVec2(0.f, 0.f), ImGuiDockNodeFlags_None);

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New"))
                    New_Document();
                if (ImGui::MenuItem("Save"))
                    Save_Document();
                if (ImGui::MenuItem("Save As"))
                    Save_DocumentAs();
                if (ImGui::MenuItem("Load"))
                    Load_Document();

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
        if (!_fileService.Save_Document(_document, false))
            return false;

        Clear_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Save_DocumentAs()
    {
        if (!_fileService.Save_Document(_document, true))
            return false;

        Clear_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Load_Document()
    {
        if (!Confirm_SaveIfDirty(L"load another UI document"))
            return false;

        if (!_fileService.Load_Document(_document))
            return false;

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
        _fileService.Clear_CurrentDocumentPath();
        _selectedObjectId = 0;
        Clear_RuntimeCaches();
        Clear_History();
        Clear_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Confirm_SaveIfDirty(const wchar_t* actionName)
    {
        const UI_FileConfirmResult result = _fileService.Confirm_SaveIfDirty(Is_Dirty(), actionName, _document);
        if (result == UI_FileConfirmResult::Saved)
            Clear_Dirty();

        return result != UI_FileConfirmResult::Cancel;
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
            if (_selectedObjectId == 0)
                Request_RenameCanvas();
            else
                Request_RenameUIObject(_selectedObjectId);
            return;
        }
    }

    void UI_Editor_Manager::Render_RenamePopup()
    {
        const char* popupTitle = _isRenamingCanvas ? "Rename Canvas" : "Rename Selected UI Object";

        if (_openRenamePopup)
        {
            ImGui::OpenPopup(popupTitle);
            _openRenamePopup = false;
        }

        if (ImGui::BeginPopupModal(popupTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            UI_Object* object = _isRenamingCanvas ? nullptr : _document.Find_Object(_renameRequestObjectId);
            if (!_isRenamingCanvas && nullptr == object)
            {
                _renameRequestObjectId = 0;
                _isRenamingCanvas = false;
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return;
            }

            if (_isRenamingCanvas)
            {
                ImGui::TextUnformatted("Type: Canvas");
            }
            else
            {
                ImGui::Text("ID: %u", object->id);
            }

            ImGui::SetNextItemWidth(280.f);
            const bool submitted = ImGui::InputText("Name", _renameBuffer, size(_renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

            if (submitted || ImGui::Button("OK"))
            {
                if (_isRenamingCanvas)
                    Rename_Canvas(_renameBuffer);
                else
                    Rename_UIObject(_renameRequestObjectId, _renameBuffer);

                _renameRequestObjectId = 0;
                _isRenamingCanvas = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                _renameRequestObjectId = 0;
                _isRenamingCanvas = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void UI_Editor_Manager::Clear_RuntimeCaches()
    {
        _previewService.Clear_RuntimeCaches();
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
        return _previewService.Queue_FontPreview(
            fontGuid,
            fontPath,
            text,
            boxMin,
            boxMax,
            color,
            rotationRadians,
            textAlign,
            scale,
            clipRect);
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

    bool UI_Editor_Manager::Rename_Canvas(const string& newName)
    {
        if (newName.empty())
            return false;

        if (_document.Get_CanvasName() == newName)
            return true;

        Push_UndoSnapshot();
        _document.Set_CanvasName(newName);
        Mark_Dirty();
        return true;
    }

    void UI_Editor_Manager::Request_RenameCanvas()
    {
        _renameRequestObjectId = 0;
        _isRenamingCanvas = true;
        strncpy_s(_renameBuffer, _document.Get_CanvasName().c_str(), _TRUNCATE);
        _openRenamePopup = true;
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
        _isRenamingCanvas = false;
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
        if (!_historyService.Is_EditTransactionOpen())
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

        if (!_historyService.Is_EditTransactionOpen())
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
        if (!_historyService.Undo(_document, _selectedObjectId))
            return false;

        Clear_RuntimeCaches();
        Resolve_LoadedAssetReferences();
        return true;
    }

    bool UI_Editor_Manager::Redo()
    {
        if (!_historyService.Redo(_document, _selectedObjectId))
            return false;

        Clear_RuntimeCaches();
        Resolve_LoadedAssetReferences();
        return true;
    }

    bool UI_Editor_Manager::Can_Undo() const
    {
        return _historyService.Can_Undo();
    }

    bool UI_Editor_Manager::Can_Redo() const
    {
        return _historyService.Can_Redo();
    }

    void UI_Editor_Manager::Begin_EditTransaction()
    {
        _historyService.Begin_EditTransaction(_document, _selectedObjectId);
    }

    void UI_Editor_Manager::Commit_EditTransactionIfChanged()
    {
        _historyService.Commit_EditTransactionIfChanged(_document, _selectedObjectId);
    }

    void UI_Editor_Manager::Mark_Dirty()
    {
        _historyService.Mark_Dirty(_document);
    }

    void UI_Editor_Manager::Clear_Dirty()
    {
        _historyService.Clear_Dirty(_document);
    }

    void UI_Editor_Manager::Push_UndoSnapshot()
    {
        _historyService.Push_UndoSnapshot(_document, _selectedObjectId);
    }

    void UI_Editor_Manager::Clear_History()
    {
        _historyService.Clear_History();
    }

    Unique<UI_Editor_Manager> UI_Editor_Manager::Create(
        const ComPtr<Device>& device,
        const ComPtr<Context>& context,
        bool assetManagerReady)
    {
        Unique<UI_Editor_Manager> instance{ new UI_Editor_Manager() };

        if (FAILED(instance->Initialize(device, context, assetManagerReady)))
            return nullptr;

        return instance;
    }
}
