#include "pch.h"
#include "UI_Editor_Manager.h"

#include "GameInstance.h"
#include "UI_Animation_View.h"
#include "UI_DocumentInternalAccess.h"
#include "UI_Editor_Utils.h"
#include "UI_Hierarchy_View.h"
#include "UI_Inspector.h"
#include "UI_Prefab_Browser.h"
#include "UI_Vfx_View.h"
#include "UI_Viewport_View.h"

namespace UIEditor
{
    namespace
    {
        constexpr const char* kHoverBrushFlowName = "Hover_PaintSlashFlow";
        constexpr const char* kHoverBrushFlowTexturePath = "../Bin/Resources/Textures/UI/Common/Paint/T_Generic_PaintSlash_Text_Variation3.png";

        string Normalize_PathForCompare(const string& path)
        {
            string normalizedPath = String::ToLowerCopy(path);
            replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');
            return normalizedPath;
        }

        bool Starts_With(const string& value, const string& prefix)
        {
            return value.size() >= prefix.size() && equal(prefix.begin(), prefix.end(), value.begin());
        }

        ObjectId Find_GeneratedChildByName(const UI_Document& document, ObjectId parentId, const string& baseName)
        {
            const string generatedPrefix = baseName + "_";
            for (const UI_Object& object : document.Get_Objects())
            {
                if (object.parentId != parentId)
                    continue;

                if (object.name == baseName || Starts_With(object.name, generatedPrefix))
                    return object.id;
            }

            return 0;
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
        Add_Window(UI_Vfx_View::Create(*this));
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

    const UI_Document& UI_Editor_Manager::Get_Document() const
    {
        return _document;
    }

    ObjectId UI_Editor_Manager::Get_SelectedObjectId() const
    {
        return _selectionService.Get_SelectedObjectId();
    }

    void UI_Editor_Manager::Set_SelectedObjectId(ObjectId objectId)
    {
        Set_SingleSelection(objectId);
    }

    void UI_Editor_Manager::Set_SingleSelection(ObjectId objectId)
    {
        UI_SelectionState nextSelection = _selectionService.Make_SingleSelectionState(_document, objectId);
        if (_selectionService.Get_SelectedObjectId() != nextSelection.selectedObjectId)
            _animationPreviewService.Stop();

        _selectionService.Apply_SelectionState(move(nextSelection));
    }

    void UI_Editor_Manager::Set_SelectedObjectIds(const vector<ObjectId>& objectIds, ObjectId primaryObjectId)
    {
        Apply_Selection(objectIds, primaryObjectId);
    }

    const vector<ObjectId>& UI_Editor_Manager::Get_SelectedObjectIds() const
    {
        return _selectionService.Get_SelectedObjectIds();
    }

    vector<ObjectId> UI_Editor_Manager::Get_RootSelectedObjectIds() const
    {
        return _selectionService.Get_RootSelectedObjectIds(_document);
    }

    bool UI_Editor_Manager::Is_ObjectSelected(ObjectId objectId) const
    {
        return _selectionService.Is_ObjectSelected(objectId);
    }

    bool UI_Editor_Manager::Has_SelectedObjects() const
    {
        return _selectionService.Has_SelectedObjects();
    }

    size_t UI_Editor_Manager::Get_SelectedObjectCount() const
    {
        return _selectionService.Get_SelectedObjectCount();
    }

    void UI_Editor_Manager::Add_ToSelection(ObjectId objectId)
    {
        Apply_SelectionState(_selectionService.Make_AddToSelectionState(_document, objectId), true);
    }

    void UI_Editor_Manager::Toggle_Selection(ObjectId objectId)
    {
        Apply_SelectionState(_selectionService.Make_ToggleSelectionState(_document, objectId), true);
    }

    void UI_Editor_Manager::Clear_Selection()
    {
        Apply_SelectionState(_selectionService.Make_ClearSelectionState(), true);
    }

    const UI_Object* UI_Editor_Manager::Get_SelectedObject() const
    {
        return _selectionService.Get_SelectedObject(_document);
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
        const string& maskTexturePath,
        UI_MaskTextureMode maskTextureMode)
    {
        return _previewService.Get_MaskedTexturePreviewSRV(textureGuid, texturePath, maskTextureGuid, maskTexturePath, maskTextureMode);
    }

    ID3D11ShaderResourceView* UI_Editor_Manager::Get_FillRatioPreviewSRV(
        const string& textureGuid,
        const string& texturePath,
        float fillRatio,
        UI_FillOrigin fillOrigin)
    {
        return _previewService.Get_FillRatioPreviewSRV(textureGuid, texturePath, fillRatio, fillOrigin);
    }

    ID3D11ShaderResourceView* UI_Editor_Manager::Get_PatternOverlayPreviewSRV(
        const PatternOverlayPreviewDesc& previewDesc)
    {
        return _previewService.Get_PatternOverlayPreviewSRV(previewDesc);
    }

    ID3D11ShaderResourceView* UI_Editor_Manager::Get_MaterialVfxPreviewSRV(
        const MaterialVfxPreviewDesc& previewDesc)
    {
        return _previewService.Get_MaterialVfxPreviewSRV(previewDesc);
    }

    ID3D11ShaderResourceView* UI_Editor_Manager::Get_ButtonSheenPreviewSRV(
        const ButtonSheenPreviewDesc& previewDesc)
    {
        return _previewService.Get_ButtonSheenPreviewSRV(previewDesc);
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

                if (ImGui::MenuItem("UI VFX"))
                    Create_UIObject(UI_ObjectType::UIVfx);

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
        Refresh_AnimationTargetBindings();
        Clear_History();
        Clear_Dirty();
        return true;
    }

    bool UI_Editor_Manager::New_Document()
    {
        if (!Confirm_SaveIfDirty(L"create a new UI document"))
            return false;

        UI_DocumentInternalAccess::Reset(_document);
        _fileService.Clear_CurrentDocumentPath();
        Clear_Selection();
        _animationPreviewService.Stop();
        Clear_RuntimeCaches();
        Refresh_AnimationTargetBindings();
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
        const bool hasBlockingPopup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup);

        if (!io.WantTextInput && !hasBlockingPopup && ImGui::IsKeyPressed(ImGuiKey_F2, false))
        {
            if (Get_SelectedObjectId() == 0)
                Request_RenameCanvas();
            else
                Request_RenameUIObject(Get_SelectedObjectId());
            return;
        }

        if (io.WantTextInput ||
            ImGui::IsAnyItemActive() ||
            hasBlockingPopup)
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
            const UI_Object* object = _isRenamingCanvas ? nullptr : static_cast<const UI_Document&>(_document).Find_Object(_renameRequestObjectId);
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

        vector<UI_Object>& objects = UI_DocumentInternalAccess::Get_MutableObjects(_document);

        for (UI_Object& object : objects)
        {
            if (object.type == UI_ObjectType::Image)
            {
                auto resolveTextureGuid = [](string& guid, const string& path, const char* label)
                {
                    if (!guid.empty() && nullptr != GAME->Find_AssetByGUID(guid))
                        return;

                    if (path.empty())
                        return;

                    guid.clear();

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
                resolveTextureGuid(object.patternMaskTextureGuid, object.patternMaskTexturePath, "pattern mask");
                resolveTextureGuid(object.materialVfx.noiseTextureGuid, object.materialVfx.noiseTexturePath, "material vfx noise");
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

    void UI_Editor_Manager::Refresh_AnimationTargetBindings()
    {
        const Shared<UI_Editor_Window> animationWindow = Find_Window(L"Animation");
        const Shared<UI_Animation_View> animationView = dynamic_pointer_cast<UI_Animation_View>(animationWindow);
        if (nullptr != animationView)
            animationView->Refresh_TargetBindings();
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

    void UI_Editor_Manager::Apply_Selection(const vector<ObjectId>& objectIds, ObjectId primaryObjectId)
    {
        Apply_SelectionState(_selectionService.Make_SelectionState(_document, objectIds, primaryObjectId), true);
    }

    void UI_Editor_Manager::Apply_SelectionState(UI_SelectionState selectionState, bool commitTransactionOnPrimaryChange)
    {
        if (_selectionService.Get_SelectedObjectId() != selectionState.selectedObjectId)
        {
            if (commitTransactionOnPrimaryChange)
                Commit_EditTransactionIfChanged();

            _animationPreviewService.Stop();
        }

        _selectionService.Apply_SelectionState(move(selectionState));
    }

    void UI_Editor_Manager::Prune_InvalidSelection()
    {
        const UI_SelectionState currentSelection = _selectionService.Make_CurrentSelectionState();
        Apply_Selection(currentSelection.selectedObjectIds, currentSelection.selectedObjectId);
    }

    bool UI_Editor_Manager::Select_ImportedObjectsAndRefresh(const vector<ObjectId>& createdRootObjectIds)
    {
        if (createdRootObjectIds.empty())
            return false;

        Set_SelectedObjectIds(createdRootObjectIds, createdRootObjectIds.back());
        Resolve_LoadedAssetReferences();
        Refresh_AnimationTargetBindings();
        Clear_RuntimeCaches();
        Mark_Dirty();
        return true;
    }

    void UI_Editor_Manager::Restore_HistorySelectionAndRefresh(ObjectId selectedObjectId, const vector<ObjectId>& selectedObjectIds)
    {
        _selectionService.Restore_SelectionState(_document, selectedObjectId, selectedObjectIds);
        Prune_InvalidSelection();
        _animationPreviewService.Stop();
        Clear_RuntimeCaches();
        Resolve_LoadedAssetReferences();
        Refresh_AnimationTargetBindings();
    }

    void UI_Editor_Manager::Create_UIObject(UI_ObjectType objectType)
    {
        Push_UndoSnapshot();

        const ObjectId parentId = _objectCommandService.Resolve_CreateParentId(_document, Get_SelectedObjectId());
        const ObjectId createdObjectId = _objectCommandService.Create_Object(_document, objectType, parentId);
        if (createdObjectId == 0)
            return;

        Set_SingleSelection(createdObjectId);
        Mark_Dirty();
    }

    bool UI_Editor_Manager::Create_PatternOverlayFromImage(ObjectId sourceObjectId)
    {
        const UI_Object* sourceObject = static_cast<const UI_Document&>(_document).Find_Object(sourceObjectId);
        if (nullptr == sourceObject || sourceObject->type != UI_ObjectType::Image)
            return false;

        if (Is_ObjectEffectivelyLocked(sourceObjectId))
            return false;

        const UI_Object source = *sourceObject;
        const int32 overlaySortOrder = source.sortOrder + 1;

        Push_UndoSnapshot();

        const ObjectId overlayObjectId = UI_DocumentInternalAccess::Create_Object(_document, UI_ObjectType::Image, source.parentId);
        UI_Object* overlayObject = UI_DocumentInternalAccess::Find_Object(_document, overlayObjectId);
        if (nullptr == overlayObject)
            return false;

        for (UI_Object& object : UI_DocumentInternalAccess::Get_MutableObjects(_document))
        {
            if (object.id != overlayObjectId && object.sortOrder >= overlaySortOrder)
                ++object.sortOrder;
        }

        UI_Object overlay = source;
        overlay.id = overlayObjectId;
        overlay.name = _document.Generate_UniqueName(source.name.empty() ? "PatternOverlay" : source.name + "_PatternOverlay", overlayObjectId);
        overlay.parentId = source.parentId;
        overlay.imageRenderMode = UI_ImageRenderMode::PatternOverlay;
        overlay.textureGuid.clear();
        overlay.texturePath.clear();
        overlay.patternMaskTextureGuid.clear();
        overlay.patternMaskTexturePath.clear();
        overlay.patternUvOffset = Get_DefaultPatternUvOffset();
        overlay.patternUvScale = Get_DefaultPatternUvScale();
        overlay.color = ImVec4(0.74f, 0.58f, 0.25f, 0.35f);
        overlay.visible = true;
        overlay.locked = false;
        overlay.sortOrder = overlaySortOrder;
        overlay.animationClips.clear();

        *overlayObject = overlay;

        Set_SingleSelection(overlayObjectId);
        Clear_RuntimeCaches();
        Mark_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Create_HoverBrushPreset(ObjectId targetObjectId, bool allowStateAnimationOverwrite)
    {
        UNREFERENCED_PARAMETER(allowStateAnimationOverwrite);

        const UI_Object* targetObject = static_cast<const UI_Document&>(_document).Find_Object(targetObjectId);
        if (nullptr == targetObject || !Is_ContainerObjectType(targetObject->type))
            return false;

        if (Is_ObjectEffectivelyLocked(targetObjectId))
            return false;

        const ObjectId flowObjectId = Find_GeneratedChildByName(_document, targetObjectId, kHoverBrushFlowName);
        if (flowObjectId != 0 && Is_ObjectEffectivelyLocked(flowObjectId))
            return false;

        const string flowName = flowObjectId != 0
            ? static_cast<const UI_Document&>(_document).Find_Object(flowObjectId)->name
            : _document.Generate_UniqueName(kHoverBrushFlowName);

        Push_UndoSnapshot();

        targetObject = static_cast<const UI_Document&>(_document).Find_Object(targetObjectId);
        if (nullptr == targetObject)
            return false;

        const ImVec2 targetSize = ImVec2(max(1.f, targetObject->size.x), max(1.f, targetObject->size.y));
        const ImVec2 targetCenter = ImVec2(targetSize.x * 0.5f, targetSize.y * 0.5f);
        const UI_RenderGroup targetRenderGroup = targetObject->renderGroup;

        int32 minChildSortOrder = numeric_limits<int32>::max();
        for (const UI_Object& object : _document.Get_Objects())
        {
            if (object.parentId != targetObjectId)
                continue;

            minChildSortOrder = min(minChildSortOrder, object.sortOrder);
        }

        if (minChildSortOrder == numeric_limits<int32>::max())
            minChildSortOrder = targetObject->sortOrder;

        const int32 flowSortOrder = minChildSortOrder - 1;

        auto configureHoverBrushImage = [&](ObjectId objectId, const string& objectName, const ImVec2& size, int32 sortOrder) -> ObjectId
        {
            ObjectId resolvedObjectId = objectId;
            if (resolvedObjectId == 0)
                resolvedObjectId = UI_DocumentInternalAccess::Create_Object(_document, UI_ObjectType::Image, targetObjectId);

            UI_Object* object = UI_DocumentInternalAccess::Find_Object(_document, resolvedObjectId);
            if (nullptr == object)
                return 0;

            object->parentId = targetObjectId;
            object->name = objectName;
            object->type = UI_ObjectType::Image;
            object->position = targetCenter;
            object->size = size;
            object->scale = ImVec2(1.f, 1.f);
            object->rotationRadians = 0.f;
            object->pivot = ImVec2(0.5f, 0.5f);
            object->anchorMin = ImVec2(0.f, 0.f);
            object->anchorMax = ImVec2(0.f, 0.f);
            object->color = ImVec4(1.f, 1.f, 1.f, 1.f);
            object->alphaMultiplier = 1.f;
            object->visible = true;
            object->locked = false;
            object->sortOrder = sortOrder;
            object->renderGroup = targetRenderGroup;
            object->textureGuid.clear();
            object->texturePath = kHoverBrushFlowTexturePath;
            object->imageRenderMode = UI_ImageRenderMode::MaterialVfx;
            object->maskTextureGuid.clear();
            object->maskTexturePath.clear();
            object->maskTextureMode = UI_MaskTextureMode::Alpha;
            object->patternMaskTextureGuid.clear();
            object->patternMaskTexturePath.clear();
            object->patternUvOffset = Get_DefaultPatternUvOffset();
            object->patternUvScale = Get_DefaultPatternUvScale();
            object->materialVfx = Get_DefaultMaterialVfxDesc();
            object->materialVfx.mode = UI_MaterialVfxMode::BrushFlow;
            object->materialVfx.color = ImVec4(0.025f, 0.021f, 0.018f, 0.92f);
            object->materialVfx.coreColor = ImVec4(0.42f, 0.18f, 0.055f, 0.58f);
            object->materialVfx.glow = 0.65f;
            object->materialVfx.alpha = 1.f;
            object->materialVfx.coreStrength = 0.9f;
            object->materialVfx.coreWidth = 0.09f;
            object->materialVfx.coreSoftness = 0.1f;
            object->materialVfx.noiseStrength = 0.82f;
            object->materialVfx.uvScroll = ImVec2(0.48f, 0.08f);
            object->materialVfx.edgeWidth = 0.3f;
            object->materialVfx.edgeSoftness = 0.18f;
            object->materialVfx.erosion = 0.13f;
            object->materialVfx.flicker = 0.28f;
            object->materialVfx.distortion = 0.044f;
            object->materialVfx.edgeWobbleSpeed = 2.85f;
            object->buttonSheen = Get_DefaultButtonSheenDesc();
            object->useFillRatio = false;
            object->fillRatio = Get_DefaultFillRatio();
            object->fillOrigin = Get_DefaultFillOrigin();
            object->vfxPath.clear();
            object->vfxPlayOnOpen = true;
            object->vfxLoop = false;
            object->vfxStopOnClose = true;
            object->stateAnimationPath.clear();
            object->animationClips.clear();

            return resolvedObjectId;
        };

        const ImVec2 flowSize = ImVec2(
            max(targetSize.x + 88.f, targetSize.x * 1.26f),
            max(targetSize.y + 28.f, targetSize.y * 1.5f));

        const ObjectId resolvedFlowId = configureHoverBrushImage(
            flowObjectId,
            flowName,
            flowSize,
            flowSortOrder);

        if (resolvedFlowId == 0)
            return false;

        Set_SingleSelection(targetObjectId);
        Resolve_LoadedAssetReferences();
        Refresh_AnimationTargetBindings();
        Clear_RuntimeCaches();
        Mark_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Create_UIObjectsFromPrefab(const UI_PrefabAsset& prefab)
    {
        if (prefab.objects.empty() || prefab.rootObjectIds.empty())
            return false;

        Push_UndoSnapshot();

        const ObjectId parentId = _objectCommandService.Resolve_CreateParentId(_document, Get_SelectedObjectId());
        const vector<ObjectId> createdRootObjectIds = _objectCommandService.Create_ObjectsFromPrefab(
            _document,
            prefab,
            parentId,
            ImVec2(100.f, 100.f));

        return Select_ImportedObjectsAndRefresh(createdRootObjectIds);
    }

    void UI_Editor_Manager::Duplicate_UIObject(ObjectId objectId)
    {
        if (!_objectCommandService.Can_DuplicateObject(_document, objectId))
            return;

        Push_UndoSnapshot();

        const ObjectId duplicatedObjectId = _objectCommandService.Duplicate_Object(_document, objectId, ImVec2(20.f, 20.f));
        if (duplicatedObjectId == 0)
            return;

        Set_SingleSelection(duplicatedObjectId);
        Mark_Dirty();
    }

    void UI_Editor_Manager::Delete_UIObject(ObjectId objectId)
    {
        if (!_objectCommandService.Can_DeleteObject(_document, objectId))
            return;

        Push_UndoSnapshot();

        if (!_objectCommandService.Delete_Object(_document, objectId))
            return;

        Prune_InvalidSelection();

        _animationPreviewService.Stop();
        Refresh_AnimationTargetBindings();
        Mark_Dirty();
    }

    void UI_Editor_Manager::Duplicate_SelectedUIObject()
    {
        if (!Has_SelectedObjects())
            return;

        vector<ObjectId> rootObjectIds = _objectCommandService.Filter_UnlockedObjectIds(_document, Get_RootSelectedObjectIds());

        if (rootObjectIds.empty())
            return;

        Push_UndoSnapshot();

        vector<ObjectId> duplicatedObjectIds = _objectCommandService.Duplicate_Objects(_document, rootObjectIds, ImVec2(20.f, 20.f));

        if (duplicatedObjectIds.empty())
            return;

        Set_SelectedObjectIds(duplicatedObjectIds, duplicatedObjectIds.back());
        Mark_Dirty();
    }

    void UI_Editor_Manager::Delete_SelectedUIObject()
    {
        if (!Has_SelectedObjects())
            return;

        vector<ObjectId> rootObjectIds = _objectCommandService.Filter_UnlockedObjectIds(_document, Get_RootSelectedObjectIds());

        if (rootObjectIds.empty())
            return;

        Push_UndoSnapshot();

        if (!_objectCommandService.Delete_Objects(_document, rootObjectIds))
            return;

        Prune_InvalidSelection();
        _animationPreviewService.Stop();
        Refresh_AnimationTargetBindings();
        Mark_Dirty();
    }

    bool UI_Editor_Manager::Copy_SelectedUIObjects()
    {
        const vector<ObjectId> rootObjectIds = Get_RootSelectedObjectIds();
        return _clipboardService.Copy_FromDocument(_document, rootObjectIds);
    }

    bool UI_Editor_Manager::Paste_UIObjects()
    {
        if (!_clipboardService.Has_CopiedUIObjects())
            return false;

        const ObjectId parentId = _objectCommandService.Resolve_PasteParentId(_document, Get_SelectedObjectId());

        Push_UndoSnapshot();

        vector<ObjectId> createdRootObjectIds;
        _clipboardService.Paste_ToDocument(_document, parentId, createdRootObjectIds);
        return Select_ImportedObjectsAndRefresh(createdRootObjectIds);
    }

    bool UI_Editor_Manager::Has_CopiedUIObjects() const
    {
        return _clipboardService.Has_CopiedUIObjects();
    }

    bool UI_Editor_Manager::Rename_Canvas(const string& newName)
    {
        if (!_objectCommandService.Can_RenameCanvas(newName))
            return false;

        if (_document.Get_CanvasName() == newName)
            return true;

        Push_UndoSnapshot();
        _objectCommandService.Rename_Canvas(_document, newName);
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
        if (!_objectCommandService.Can_RenameObject(_document, objectId, newName))
            return false;

        const UI_Object* object = static_cast<const UI_Document&>(_document).Find_Object(objectId);
        const string uniqueName = _objectCommandService.Resolve_RenameObjectName(_document, objectId, newName);
        if (object->name == uniqueName)
            return true;

        Push_UndoSnapshot();

        _objectCommandService.Rename_Object(_document, objectId, newName);
        Mark_Dirty();
        return true;
    }

    void UI_Editor_Manager::Request_RenameUIObject(ObjectId objectId)
    {
        const UI_Object* object = static_cast<const UI_Document&>(_document).Find_Object(objectId);
        if (nullptr == object || Is_ObjectEffectivelyLocked(objectId))
            return;

        _renameRequestObjectId = objectId;
        _isRenamingCanvas = false;
        strncpy_s(_renameBuffer, object->name.c_str(), _TRUNCATE);
        _openRenamePopup = true;
    }

    bool UI_Editor_Manager::Reparent_UIObject(ObjectId objectId, ObjectId newParentId)
    {
        const UI_Object* object = static_cast<const UI_Document&>(_document).Find_Object(objectId);
        if (nullptr == object || object->parentId == newParentId)
            return false;

        if (!_objectCommandService.Can_ReparentObject(_document, objectId, newParentId))
            return false;

        Push_UndoSnapshot();

        if (!_objectCommandService.Reparent_Object(_document, objectId, newParentId, true))
            return false;

        Set_SingleSelection(objectId);
        Mark_Dirty();
        return true;
    }

    bool UI_Editor_Manager::Can_ReparentUIObject(ObjectId objectId, ObjectId newParentId) const
    {
        return _objectCommandService.Can_ReparentObject(_document, objectId, newParentId);
    }

    bool UI_Editor_Manager::Undo()
    {
        ObjectId selectedObjectId = Get_SelectedObjectId();
        vector<ObjectId> selectedObjectIds = Get_SelectedObjectIds();
        if (!_historyService.Undo(_document, selectedObjectId, selectedObjectIds))
            return false;

        Restore_HistorySelectionAndRefresh(selectedObjectId, selectedObjectIds);
        return true;
    }

    bool UI_Editor_Manager::Redo()
    {
        ObjectId selectedObjectId = Get_SelectedObjectId();
        vector<ObjectId> selectedObjectIds = Get_SelectedObjectIds();
        if (!_historyService.Redo(_document, selectedObjectId, selectedObjectIds))
            return false;

        Restore_HistorySelectionAndRefresh(selectedObjectId, selectedObjectIds);
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
        _historyService.Begin_EditTransaction(_document, Get_SelectedObjectId(), Get_SelectedObjectIds());
    }

    void UI_Editor_Manager::Commit_EditTransactionIfChanged()
    {
        _historyService.Commit_EditTransactionIfChanged(_document, Get_SelectedObjectId(), Get_SelectedObjectIds());
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
        _historyService.Push_UndoSnapshot(_document, Get_SelectedObjectId(), Get_SelectedObjectIds());
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
