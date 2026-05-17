#include "pch.h"
#include "UI_Editor_Manager.h"

#include "GameInstance.h"
#include "UI_Animation_View.h"
#include "UI_Editor_Utils.h"
#include "UI_Hierarchy_View.h"
#include "UI_Inspector.h"
#include "UI_Prefab_Browser.h"
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

        float Clamp01(float value)
        {
            return min(max(value, 0.f), 1.f);
        }

        ImVec2 Clamp01(const ImVec2& value)
        {
            return ImVec2(Clamp01(value.x), Clamp01(value.y));
        }

        bool NearlyEqual(float lhs, float rhs)
        {
            return fabsf(lhs - rhs) <= 0.0001f;
        }

        bool NearlyEqual(const ImVec2& lhs, const ImVec2& rhs)
        {
            return NearlyEqual(lhs.x, rhs.x) && NearlyEqual(lhs.y, rhs.y);
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
        Add_Window(UI_Animation_View::Create(*this));
        Add_Window(UI_Prefab_Browser::Create(*this));

        Clear_History();
        Clear_Dirty();

        return S_OK;
    }

    void UI_Editor_Manager::Render()
    {
        _previewService.Begin_Frame();
        _animationPreviewService.Update(ImGui::GetIO().DeltaTime);

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
        Set_SingleSelection(objectId);
    }

    void UI_Editor_Manager::Set_SingleSelection(ObjectId objectId)
    {
        const ObjectId nextSelectedObjectId = nullptr != _document.Find_Object(objectId) ? objectId : 0;
        if (_selectedObjectId != nextSelectedObjectId)
            _animationPreviewService.Stop();

        _selectedObjectId = nextSelectedObjectId;
        _selectedObjectIds.clear();
        if (_selectedObjectId != 0)
            _selectedObjectIds.push_back(_selectedObjectId);
    }

    void UI_Editor_Manager::Set_SelectedObjectIds(const vector<ObjectId>& objectIds, ObjectId primaryObjectId)
    {
        Apply_Selection(objectIds, primaryObjectId);
    }

    const vector<ObjectId>& UI_Editor_Manager::Get_SelectedObjectIds() const
    {
        return _selectedObjectIds;
    }

    vector<ObjectId> UI_Editor_Manager::Get_RootSelectedObjectIds() const
    {
        return Collect_RootObjectIds(_selectedObjectIds);
    }

    bool UI_Editor_Manager::Is_ObjectSelected(ObjectId objectId) const
    {
        if (objectId == 0)
            return _selectedObjectId == 0 && _selectedObjectIds.empty();

        return find(_selectedObjectIds.begin(), _selectedObjectIds.end(), objectId) != _selectedObjectIds.end();
    }

    bool UI_Editor_Manager::Has_SelectedObjects() const
    {
        return !_selectedObjectIds.empty();
    }

    size_t UI_Editor_Manager::Get_SelectedObjectCount() const
    {
        return _selectedObjectIds.size();
    }

    void UI_Editor_Manager::Add_ToSelection(ObjectId objectId)
    {
        if (objectId == 0 || nullptr == _document.Find_Object(objectId))
            return;

        vector<ObjectId> nextSelection = _selectedObjectIds;
        if (find(nextSelection.begin(), nextSelection.end(), objectId) == nextSelection.end())
            nextSelection.push_back(objectId);

        Apply_Selection(nextSelection, objectId);
    }

    void UI_Editor_Manager::Toggle_Selection(ObjectId objectId)
    {
        if (objectId == 0 || nullptr == _document.Find_Object(objectId))
            return;

        vector<ObjectId> nextSelection = _selectedObjectIds;
        const auto iter = find(nextSelection.begin(), nextSelection.end(), objectId);
        if (iter == nextSelection.end())
        {
            nextSelection.push_back(objectId);
            Apply_Selection(nextSelection, objectId);
            return;
        }

        nextSelection.erase(iter);
        const ObjectId nextPrimaryObjectId = nextSelection.empty()
            ? 0
            : (_selectedObjectId == objectId ? nextSelection.back() : _selectedObjectId);
        Apply_Selection(nextSelection, nextPrimaryObjectId);
    }

    void UI_Editor_Manager::Clear_Selection()
    {
        Apply_Selection({}, 0);
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

    ID3D11ShaderResourceView* UI_Editor_Manager::Get_TexturePreviewSRV(const string& textureGuid, const string& texturePath)
    {
        return _previewService.Get_TexturePreviewSRV(textureGuid, texturePath);
    }

    ID3D11ShaderResourceView* UI_Editor_Manager::Get_MaskedTexturePreviewSRV(
        const string& textureGuid,
        const string& texturePath,
        const string& maskTextureGuid,
        const string& maskTexturePath)
    {
        return _previewService.Get_MaskedTexturePreviewSRV(textureGuid, texturePath, maskTextureGuid, maskTexturePath);
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

                ImGui::Separator();
                if (ImGui::MenuItem("Copy", "Ctrl+C", false, Has_SelectedObjects()))
                    Copy_SelectedUIObjects();
                if (ImGui::MenuItem("Paste", "Ctrl+V", false, Has_CopiedUIObjects()))
                    Paste_UIObjects();
                if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, Has_SelectedObjects()))
                    Duplicate_SelectedUIObject();

                ImGui::Separator();
                if (ImGui::BeginMenu("Align", Get_SelectedObjectCount() >= 2))
                {
                    if (ImGui::MenuItem("Left"))
                        Align_SelectedObjects(UI_AlignmentCommand::Left);
                    if (ImGui::MenuItem("Right"))
                        Align_SelectedObjects(UI_AlignmentCommand::Right);
                    if (ImGui::MenuItem("Top"))
                        Align_SelectedObjects(UI_AlignmentCommand::Top);
                    if (ImGui::MenuItem("Bottom"))
                        Align_SelectedObjects(UI_AlignmentCommand::Bottom);
                    if (ImGui::MenuItem("Center X"))
                        Align_SelectedObjects(UI_AlignmentCommand::CenterX);
                    if (ImGui::MenuItem("Center Y"))
                        Align_SelectedObjects(UI_AlignmentCommand::CenterY);
                    ImGui::Separator();
                    if (ImGui::MenuItem("Distribute Horizontal", nullptr, false, Get_SelectedObjectCount() >= 3))
                        Align_SelectedObjects(UI_AlignmentCommand::DistributeHorizontal);
                    if (ImGui::MenuItem("Distribute Vertical", nullptr, false, Get_SelectedObjectCount() >= 3))
                        Align_SelectedObjects(UI_AlignmentCommand::DistributeVertical);
                    ImGui::EndMenu();
                }
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

        Clear_Selection();
        _animationPreviewService.Stop();
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
        Clear_Selection();
        _animationPreviewService.Stop();
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
        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantTextInput ||
            ImGui::IsAnyItemActive() ||
            ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup))
        {
            return;
        }

        const Shared<UI_Editor_Window> animationWindow = Find_Window(L"Animation");
        if (nullptr != animationWindow && animationWindow->Is_Open() && animationWindow->Is_Focused())
            return;

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
        {
            Undo();
            return;
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
        {
            Redo();
            return;
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false))
        {
            Copy_SelectedUIObjects();
            return;
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false))
        {
            Paste_UIObjects();
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
        {
            Delete_SelectedUIObject();
            return;
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
        {
            Duplicate_SelectedUIObject();
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_F2, false))
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
        const Vec2& pivot,
        UI_TextAlign textAlign,
        float scale,
        bool textWrap,
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
            pivot,
            textAlign,
            scale,
            textWrap,
            clipRect);
    }

    ObjectId UI_Editor_Manager::Resolve_CreateParentId() const
    {
        if (_selectedObjectId == 0)
            return 0;

        if (Is_ObjectEffectivelyLocked(_selectedObjectId))
            return 0;

        const UI_Object* selectedObject = _document.Find_Object(_selectedObjectId);
        if (nullptr == selectedObject)
            return 0;

        if (Is_ContainerObjectType(selectedObject->type))
            return selectedObject->id;

        return Is_ObjectEffectivelyLocked(selectedObject->parentId) ? 0 : selectedObject->parentId;
    }

    vector<ObjectId> UI_Editor_Manager::Normalize_Selection(const vector<ObjectId>& objectIds) const
    {
        vector<ObjectId> normalizedObjectIds;
        unordered_set<ObjectId> visitedObjectIds;

        for (ObjectId objectId : objectIds)
        {
            if (objectId == 0 || nullptr == _document.Find_Object(objectId))
                continue;

            if (!visitedObjectIds.insert(objectId).second)
                continue;

            normalizedObjectIds.push_back(objectId);
        }

        return normalizedObjectIds;
    }

    vector<ObjectId> UI_Editor_Manager::Collect_RootObjectIds(const vector<ObjectId>& objectIds) const
    {
        const vector<ObjectId> normalizedObjectIds = Normalize_Selection(objectIds);
        unordered_set<ObjectId> selectedObjectIdSet(normalizedObjectIds.begin(), normalizedObjectIds.end());
        vector<ObjectId> rootObjectIds;

        for (ObjectId objectId : normalizedObjectIds)
        {
            bool hasSelectedAncestor = false;
            const UI_Object* object = _document.Find_Object(objectId);
            while (nullptr != object && object->parentId != 0)
            {
                if (selectedObjectIdSet.find(object->parentId) != selectedObjectIdSet.end())
                {
                    hasSelectedAncestor = true;
                    break;
                }

                object = _document.Find_Object(object->parentId);
            }

            if (!hasSelectedAncestor)
                rootObjectIds.push_back(objectId);
        }

        return rootObjectIds;
    }

    void UI_Editor_Manager::Collect_ObjectAndChildren(ObjectId objectId, vector<ObjectId>& outObjectIds) const
    {
        if (objectId == 0 || nullptr == _document.Find_Object(objectId))
            return;

        outObjectIds.push_back(objectId);
        for (const UI_Object& object : _document.Get_Objects())
        {
            if (object.parentId == objectId)
                Collect_ObjectAndChildren(object.id, outObjectIds);
        }
    }

    ObjectId UI_Editor_Manager::Resolve_PasteParentId() const
    {
        const UI_Object* selectedObject = _document.Find_Object(_selectedObjectId);
        if (nullptr == selectedObject)
            return 0;

        const ObjectId parentId = selectedObject->parentId;
        if (parentId != 0 && Is_ObjectEffectivelyLocked(parentId))
            return 0;

        return _document.Can_HaveChildren(parentId) ? parentId : 0;
    }

    void UI_Editor_Manager::Apply_Selection(const vector<ObjectId>& objectIds, ObjectId primaryObjectId)
    {
        vector<ObjectId> normalizedObjectIds = Normalize_Selection(objectIds);
        ObjectId nextSelectedObjectId = 0;

        if (primaryObjectId != 0 && _document.Find_Object(primaryObjectId) != nullptr)
            nextSelectedObjectId = primaryObjectId;
        else if (!normalizedObjectIds.empty())
            nextSelectedObjectId = normalizedObjectIds.back();

        if (nextSelectedObjectId != 0 &&
            find(normalizedObjectIds.begin(), normalizedObjectIds.end(), nextSelectedObjectId) == normalizedObjectIds.end())
        {
            normalizedObjectIds.push_back(nextSelectedObjectId);
        }

        if (_selectedObjectId != nextSelectedObjectId)
        {
            Commit_EditTransactionIfChanged();
            _animationPreviewService.Stop();
        }

        _selectedObjectIds = move(normalizedObjectIds);
        _selectedObjectId = nextSelectedObjectId;
    }

    void UI_Editor_Manager::Prune_InvalidSelection()
    {
        Apply_Selection(_selectedObjectIds, _selectedObjectId);
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

        Set_SingleSelection(createdObjectId);
        Mark_Dirty();
    }

    bool UI_Editor_Manager::Create_UIObjectsFromPrefab(const UI_PrefabAsset& prefab)
    {
        if (prefab.objects.empty() || prefab.rootObjectIds.empty())
            return false;

        Push_UndoSnapshot();

        const ObjectId parentId = Resolve_CreateParentId();
        const vector<ObjectId> createdRootObjectIds = _document.Import_PrefabObjects(
            prefab.objects,
            prefab.rootObjectIds,
            parentId,
            ImVec2(100.f, 100.f));

        if (createdRootObjectIds.empty())
            return false;

        Set_SelectedObjectIds(createdRootObjectIds, createdRootObjectIds.back());
        Resolve_LoadedAssetReferences();
        Clear_RuntimeCaches();
        Mark_Dirty();
        return true;
    }

    void UI_Editor_Manager::Duplicate_UIObject(ObjectId objectId)
    {
        if (nullptr == _document.Find_Object(objectId) || Is_ObjectEffectivelyLocked(objectId))
            return;

        Push_UndoSnapshot();

        const ObjectId duplicatedObjectId = _document.Duplicate_Object(objectId, ImVec2(20.f, 20.f));
        if (duplicatedObjectId == 0)
            return;

        Set_SingleSelection(duplicatedObjectId);
        Mark_Dirty();
    }

    void UI_Editor_Manager::Delete_UIObject(ObjectId objectId)
    {
        if (nullptr == _document.Find_Object(objectId) || Is_ObjectEffectivelyLocked(objectId))
            return;

        Push_UndoSnapshot();

        if (!_document.Delete_Object(objectId))
            return;

        Prune_InvalidSelection();

        _animationPreviewService.Stop();
        Mark_Dirty();
    }

    void UI_Editor_Manager::Duplicate_SelectedUIObject()
    {
        if (_selectedObjectIds.empty())
            return;

        vector<ObjectId> rootObjectIds = Collect_RootObjectIds(_selectedObjectIds);
        rootObjectIds.erase(
            remove_if(
                rootObjectIds.begin(),
                rootObjectIds.end(),
                [this](ObjectId objectId)
                {
                    return Is_ObjectEffectivelyLocked(objectId);
                }),
            rootObjectIds.end());

        if (rootObjectIds.empty())
            return;

        Push_UndoSnapshot();

        vector<ObjectId> duplicatedObjectIds;
        duplicatedObjectIds.reserve(rootObjectIds.size());
        for (ObjectId objectId : rootObjectIds)
        {
            const ObjectId duplicatedObjectId = _document.Duplicate_Object(objectId, ImVec2(20.f, 20.f));
            if (duplicatedObjectId != 0)
                duplicatedObjectIds.push_back(duplicatedObjectId);
        }

        if (duplicatedObjectIds.empty())
            return;

        Set_SelectedObjectIds(duplicatedObjectIds, duplicatedObjectIds.back());
        Mark_Dirty();
    }

    void UI_Editor_Manager::Delete_SelectedUIObject()
    {
        if (_selectedObjectIds.empty())
            return;

        vector<ObjectId> rootObjectIds = Collect_RootObjectIds(_selectedObjectIds);
        rootObjectIds.erase(
            remove_if(
                rootObjectIds.begin(),
                rootObjectIds.end(),
                [this](ObjectId objectId)
                {
                    return Is_ObjectEffectivelyLocked(objectId);
                }),
            rootObjectIds.end());

        if (rootObjectIds.empty())
            return;

        Push_UndoSnapshot();

        bool deletedAny = false;
        for (ObjectId objectId : rootObjectIds)
            deletedAny |= _document.Delete_Object(objectId);

        if (!deletedAny)
            return;

        Prune_InvalidSelection();
        _animationPreviewService.Stop();
        Mark_Dirty();
    }

    bool UI_Editor_Manager::Copy_SelectedUIObjects()
    {
        const vector<ObjectId> rootObjectIds = Collect_RootObjectIds(_selectedObjectIds);
        if (rootObjectIds.empty())
            return false;

        vector<ObjectId> copiedObjectIds;
        unordered_set<ObjectId> copiedObjectIdSet;
        for (ObjectId rootObjectId : rootObjectIds)
        {
            vector<ObjectId> objectAndChildrenIds;
            Collect_ObjectAndChildren(rootObjectId, objectAndChildrenIds);

            for (ObjectId objectId : objectAndChildrenIds)
            {
                if (copiedObjectIdSet.insert(objectId).second)
                    copiedObjectIds.push_back(objectId);
            }
        }

        if (copiedObjectIds.empty())
            return false;

        unordered_set<ObjectId> rootObjectIdSet(rootObjectIds.begin(), rootObjectIds.end());
        vector<UI_Object> clipboardObjects;
        clipboardObjects.reserve(copiedObjectIds.size());

        for (ObjectId objectId : copiedObjectIds)
        {
            const UI_Object* sourceObject = _document.Find_Object(objectId);
            if (nullptr == sourceObject)
                continue;

            UI_Object clipboardObject = *sourceObject;
            if (rootObjectIdSet.find(clipboardObject.id) != rootObjectIdSet.end())
                clipboardObject.parentId = 0;

            clipboardObjects.push_back(move(clipboardObject));
        }

        if (clipboardObjects.empty())
            return false;

        _clipboardObjects = move(clipboardObjects);
        _clipboardRootObjectIds = rootObjectIds;
        _clipboardPasteCount = 0;
        return true;
    }

    bool UI_Editor_Manager::Paste_UIObjects()
    {
        if (_clipboardObjects.empty() || _clipboardRootObjectIds.empty())
            return false;

        const ObjectId parentId = Resolve_PasteParentId();
        const float pasteOffset = 20.f * static_cast<float>(_clipboardPasteCount + 1);

        Push_UndoSnapshot();

        const vector<ObjectId> createdRootObjectIds = _document.Import_PrefabObjects(
            _clipboardObjects,
            _clipboardRootObjectIds,
            parentId,
            ImVec2(pasteOffset, pasteOffset));

        if (createdRootObjectIds.empty())
            return false;

        ++_clipboardPasteCount;
        Set_SelectedObjectIds(createdRootObjectIds, createdRootObjectIds.back());
        Resolve_LoadedAssetReferences();
        Clear_RuntimeCaches();
        Mark_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Has_CopiedUIObjects() const
    {
        return !_clipboardObjects.empty() && !_clipboardRootObjectIds.empty();
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
        if (nullptr == object || newName.empty() || Is_ObjectEffectivelyLocked(objectId))
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
        if (nullptr == object || Is_ObjectEffectivelyLocked(objectId))
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

        if (Is_ObjectEffectivelyLocked(objectId) || Is_ObjectEffectivelyLocked(newParentId))
            return false;

        Push_UndoSnapshot();

        if (!_document.Change_Parent(objectId, newParentId, true))
            return false;

        Set_SingleSelection(objectId);
        Mark_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Can_ReparentUIObject(ObjectId objectId, ObjectId newParentId) const
    {
        if (Is_ObjectEffectivelyLocked(objectId) || Is_ObjectEffectivelyLocked(newParentId))
            return false;

        return _document.Can_Parent(objectId, newParentId);
    }

    bool UI_Editor_Manager::Is_ObjectEffectivelyVisible(ObjectId objectId) const
    {
        unordered_set<ObjectId> visitedObjectIds;
        const UI_Object* object = _document.Find_Object(objectId);

        while (nullptr != object)
        {
            if (!visitedObjectIds.insert(object->id).second)
                break;

            if (!object->visible)
                return false;

            if (object->parentId == 0)
                break;

            object = _document.Find_Object(object->parentId);
        }

        return nullptr != _document.Find_Object(objectId);
    }

    bool UI_Editor_Manager::Is_ObjectEffectivelyLocked(ObjectId objectId) const
    {
        unordered_set<ObjectId> visitedObjectIds;
        const UI_Object* object = _document.Find_Object(objectId);

        while (nullptr != object)
        {
            if (!visitedObjectIds.insert(object->id).second)
                break;

            if (object->locked)
                return true;

            if (object->parentId == 0)
                break;

            object = _document.Find_Object(object->parentId);
        }

        return false;
    }

    bool UI_Editor_Manager::Set_ObjectVisible(ObjectId objectId, bool visible)
    {
        UI_Object* object = _document.Find_Object(objectId);
        if (nullptr == object || object->visible == visible)
            return false;

        Push_UndoSnapshot();
        object->visible = visible;
        Mark_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Set_ObjectLocked(ObjectId objectId, bool locked)
    {
        UI_Object* object = _document.Find_Object(objectId);
        if (nullptr == object || object->locked == locked)
            return false;

        Push_UndoSnapshot();
        object->locked = locked;
        Mark_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Set_UIObjectWorldPosition(ObjectId objectId, const ImVec2& worldPosition)
    {
        if (Is_ObjectEffectivelyLocked(objectId))
            return false;

        if (!_historyService.Is_EditTransactionOpen())
            Push_UndoSnapshot();

        if (!_document.Set_WorldPosition(objectId, worldPosition))
            return false;

        Mark_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Set_UIObjectWorldRect(ObjectId objectId, const ImVec2& worldPosition, const ImVec2& worldSize)
    {
        if (Is_ObjectEffectivelyLocked(objectId))
            return false;

        if (nullptr == _document.Find_Object(objectId))
            return false;

        if (!_historyService.Is_EditTransactionOpen())
            Push_UndoSnapshot();

        if (!_document.Set_WorldRect(objectId, worldPosition, worldSize))
            return false;

        Mark_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Set_UIObjectPivotKeepWorldRect(ObjectId objectId, const ImVec2& pivot)
    {
        if (Is_ObjectEffectivelyLocked(objectId))
            return false;

        UI_Object* object = _document.Find_Object(objectId);
        if (nullptr == object)
            return false;

        const ImVec2 clampedPivot = Clamp01(pivot);
        if (NearlyEqual(object->pivot, clampedPivot))
            return false;

        if (!_historyService.Is_EditTransactionOpen())
            Push_UndoSnapshot();

        if (!_document.Set_PivotKeepWorldRect(objectId, clampedPivot))
            return false;

        Mark_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Set_UIObjectAnchorsKeepWorldRect(ObjectId objectId, const ImVec2& anchorMin, const ImVec2& anchorMax)
    {
        if (Is_ObjectEffectivelyLocked(objectId))
            return false;

        UI_Object* object = _document.Find_Object(objectId);
        if (nullptr == object)
            return false;

        const ImVec2 clampedAnchorMin = Clamp01(anchorMin);
        const ImVec2 clampedAnchorMax = Clamp01(anchorMax);
        if (NearlyEqual(object->anchorMin, clampedAnchorMin) && NearlyEqual(object->anchorMax, clampedAnchorMax))
            return false;

        if (!_historyService.Is_EditTransactionOpen())
            Push_UndoSnapshot();

        if (!_document.Set_AnchorsKeepWorldRect(objectId, clampedAnchorMin, clampedAnchorMax))
            return false;

        Mark_Dirty();
        return true;
    }

    ImVec2 UI_Editor_Manager::Get_UIObjectWorldPosition(ObjectId objectId) const
    {
        const ImVec2 worldCenter = _document.Get_WorldCenter(objectId);
        const ImVec2 worldSize = _document.Get_WorldSize(objectId);
        return ImVec2(
            worldCenter.x - worldSize.x * 0.5f,
            worldCenter.y - worldSize.y * 0.5f);
    }

    bool UI_Editor_Manager::Align_SelectedObjects(UI_AlignmentCommand command)
    {
        vector<ObjectId> targetObjectIds = Collect_RootObjectIds(_selectedObjectIds);
        targetObjectIds.erase(
            remove_if(
                targetObjectIds.begin(),
                targetObjectIds.end(),
                [this](ObjectId objectId)
                {
                    return Is_ObjectEffectivelyLocked(objectId);
                }),
            targetObjectIds.end());

        if (targetObjectIds.size() < 2)
            return false;

        if ((command == UI_AlignmentCommand::DistributeHorizontal ||
            command == UI_AlignmentCommand::DistributeVertical) &&
            targetObjectIds.size() < 3)
        {
            return false;
        }

        struct ObjectRect
        {
            ObjectId id = 0;
            ImVec2 worldPosition = ImVec2(0.f, 0.f);
            ImVec2 size = ImVec2(0.f, 0.f);
            ImVec2 center = ImVec2(0.f, 0.f);
        };

        vector<ObjectRect> objectRects;
        objectRects.reserve(targetObjectIds.size());

        ImVec2 selectionMin = ImVec2(FLT_MAX, FLT_MAX);
        ImVec2 selectionMax = ImVec2(-FLT_MAX, -FLT_MAX);

        for (ObjectId objectId : targetObjectIds)
        {
            const UI_Object* object = _document.Find_Object(objectId);
            if (nullptr == object)
                continue;

            ObjectRect rect = {};
            rect.id = objectId;
            const ImVec2 worldCenter = _document.Get_WorldCenter(objectId);
            rect.size = _document.Get_WorldSize(objectId);
            rect.worldPosition = ImVec2(
                worldCenter.x - rect.size.x * 0.5f,
                worldCenter.y - rect.size.y * 0.5f);
            rect.center = ImVec2(
                rect.worldPosition.x + rect.size.x * 0.5f,
                rect.worldPosition.y + rect.size.y * 0.5f);

            selectionMin.x = min(selectionMin.x, rect.worldPosition.x);
            selectionMin.y = min(selectionMin.y, rect.worldPosition.y);
            selectionMax.x = max(selectionMax.x, rect.worldPosition.x + rect.size.x);
            selectionMax.y = max(selectionMax.y, rect.worldPosition.y + rect.size.y);

            objectRects.push_back(rect);
        }

        if (objectRects.size() < 2)
            return false;

        const ImVec2 selectionCenter = ImVec2(
            (selectionMin.x + selectionMax.x) * 0.5f,
            (selectionMin.y + selectionMax.y) * 0.5f);

        if (command == UI_AlignmentCommand::DistributeHorizontal)
        {
            sort(objectRects.begin(), objectRects.end(), [](const ObjectRect& lhs, const ObjectRect& rhs)
                {
                    if (lhs.center.x != rhs.center.x)
                        return lhs.center.x < rhs.center.x;

                    return lhs.id < rhs.id;
                });
        }
        else if (command == UI_AlignmentCommand::DistributeVertical)
        {
            sort(objectRects.begin(), objectRects.end(), [](const ObjectRect& lhs, const ObjectRect& rhs)
                {
                    if (lhs.center.y != rhs.center.y)
                        return lhs.center.y < rhs.center.y;

                    return lhs.id < rhs.id;
                });
        }

        Begin_EditTransaction();

        bool changed = false;
        for (size_t index = 0; index < objectRects.size(); ++index)
        {
            const ObjectRect& rect = objectRects[index];
            UI_Object* object = _document.Find_Object(rect.id);
            if (nullptr == object)
                continue;

            ImVec2 nextWorldPosition = rect.worldPosition;
            switch (command)
            {
            case UI_AlignmentCommand::Left:
                nextWorldPosition.x = selectionMin.x;
                break;
            case UI_AlignmentCommand::Right:
                nextWorldPosition.x = selectionMax.x - rect.size.x;
                break;
            case UI_AlignmentCommand::Top:
                nextWorldPosition.y = selectionMin.y;
                break;
            case UI_AlignmentCommand::Bottom:
                nextWorldPosition.y = selectionMax.y - rect.size.y;
                break;
            case UI_AlignmentCommand::CenterX:
                nextWorldPosition.x = selectionCenter.x - rect.size.x * 0.5f;
                break;
            case UI_AlignmentCommand::CenterY:
                nextWorldPosition.y = selectionCenter.y - rect.size.y * 0.5f;
                break;
            case UI_AlignmentCommand::DistributeHorizontal:
            {
                const float firstCenter = objectRects.front().center.x;
                const float lastCenter = objectRects.back().center.x;
                const float step = (lastCenter - firstCenter) / static_cast<float>(objectRects.size() - 1);
                nextWorldPosition.x = firstCenter + step * static_cast<float>(index) - rect.size.x * 0.5f;
                break;
            }
            case UI_AlignmentCommand::DistributeVertical:
            {
                const float firstCenter = objectRects.front().center.y;
                const float lastCenter = objectRects.back().center.y;
                const float step = (lastCenter - firstCenter) / static_cast<float>(objectRects.size() - 1);
                nextWorldPosition.y = firstCenter + step * static_cast<float>(index) - rect.size.y * 0.5f;
                break;
            }
            default:
                break;
            }

            if (fabsf(nextWorldPosition.x - rect.worldPosition.x) <= 0.001f &&
                fabsf(nextWorldPosition.y - rect.worldPosition.y) <= 0.001f)
            {
                continue;
            }

            if (_document.Set_WorldPosition(rect.id, nextWorldPosition))
                changed = true;
        }

        if (changed)
            Mark_Dirty();

        Commit_EditTransactionIfChanged();
        return changed;
    }

    bool UI_Editor_Manager::Undo()
    {
        if (!_historyService.Undo(_document, _selectedObjectId, _selectedObjectIds))
            return false;

        Prune_InvalidSelection();
        _animationPreviewService.Stop();
        Clear_RuntimeCaches();
        Resolve_LoadedAssetReferences();
        return true;
    }

    bool UI_Editor_Manager::Redo()
    {
        if (!_historyService.Redo(_document, _selectedObjectId, _selectedObjectIds))
            return false;

        Prune_InvalidSelection();
        _animationPreviewService.Stop();
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
        _historyService.Begin_EditTransaction(_document, _selectedObjectId, _selectedObjectIds);
    }

    void UI_Editor_Manager::Commit_EditTransactionIfChanged()
    {
        _historyService.Commit_EditTransactionIfChanged(_document, _selectedObjectId, _selectedObjectIds);
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
        _historyService.Push_UndoSnapshot(_document, _selectedObjectId, _selectedObjectIds);
    }

    void UI_Editor_Manager::Clear_History()
    {
        _historyService.Clear_History();
    }

    void UI_Editor_Manager::Play_AnimationPreview(ObjectId objectId)
    {
        _animationPreviewService.Play(objectId);
    }

    void UI_Editor_Manager::Play_AnimationAssetPreview(const UI_AnimationAsset& asset, int32 startFrame)
    {
        _animationPreviewService.Play_KeyframeAsset(asset, startFrame);
    }

    void UI_Editor_Manager::Preview_AnimationAsset(const UI_AnimationAsset& asset, int32 frame)
    {
        _animationPreviewService.Preview_KeyframeAsset(asset, frame);
    }

    void UI_Editor_Manager::Pause_AnimationPreview()
    {
        _animationPreviewService.Pause();
    }

    void UI_Editor_Manager::Stop_AnimationPreview()
    {
        _animationPreviewService.Stop();
    }

    void UI_Editor_Manager::Restart_AnimationPreview(ObjectId objectId)
    {
        _animationPreviewService.Restart(objectId);
    }

    bool UI_Editor_Manager::Is_AnimationPreviewActive() const
    {
        return _animationPreviewService.Is_Active();
    }

    bool UI_Editor_Manager::Is_AnimationPreviewPlaying() const
    {
        return _animationPreviewService.Is_Playing();
    }

    float UI_Editor_Manager::Get_AnimationPreviewTime() const
    {
        return _animationPreviewService.Get_Time();
    }

    int32 UI_Editor_Manager::Get_AnimationPreviewFrame() const
    {
        return _animationPreviewService.Get_CurrentFrame();
    }

    bool UI_Editor_Manager::Sample_AnimationPreview(const UI_Object& object, UI_AnimationPreviewSample& outSample) const
    {
        return _animationPreviewService.Sample_Object(object, outSample);
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
