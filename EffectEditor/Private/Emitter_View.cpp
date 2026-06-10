#include "Emitter_View.h"

#include "Action_Command.h"
#include "CurveEditor_View.h"
#include "Editor_Context.h"
#include "EffectAuthoringJsonSerializer.h"
#include "EffectAuthoringModuleMetadata.h"
#include "EffectEditorInstance.h"
#include "EffectMaterialPresetReader.h"
#include "GameInstance.h"
#include "Helper_EffectAuthoring.h"
#include "Helper_ImGui.h"
#include "Helper_String.h"
#include "Notification_Manager.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    constexpr auto kDefaultEffectMaterialFileName = L"M_DefaultMaterial.effectmaterial.json";
    constexpr auto kDefaultEffectMaterialName = "M_DefaultMaterial";
    constexpr auto kDefaultEffectMaterialTexturePath = "Effects/Textures/Shared/DefaultTexture.dds";
    constexpr auto kEffectAuthoringSaveDefaultFolder = L"../../../Client/Bin/Resources/Effects/Assets";
    constexpr auto kEffectAuthoringSaveDefaultFileName = L"NewEffect.effect.json";
    constexpr auto kEffectAuthoringExtension = L".effect.json";
    constexpr ImU32 kCurveEditorFocusBarColor = IM_COL32(80, 170, 205, 255);

    fs::path Normalize_EffectSaveDialogPath(const fs::path& filePath)
    {
        try
        {
            if (fs::exists(filePath))
                return fs::weakly_canonical(filePath);
        }
        catch (...)
        {}

        try
        {
            return fs::absolute(filePath).lexically_normal();
        }
        catch (...)
        {}

        return filePath.lexically_normal();
    }

    fs::path Ensure_EffectAuthoringExtension(const fs::path& filePath)
    {
        const string lowerPath = String::ToLowerCopy(filePath.generic_string());
        if (lowerPath.ends_with(".effect.json"))
            return filePath;

        return fs::path(filePath.wstring() + kEffectAuthoringExtension);
    }

    bool Try_PickEffectSaveFileDialog(fs::path& outFilePath)
    {
        outFilePath.clear();

        const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool shouldUninit = SUCCEEDED(initHr);

        IFileSaveDialog* dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(hr) || dialog == nullptr)
        {
            if (shouldUninit)
                CoUninitialize();
            return false;
        }

        dialog->SetTitle(L"Save Effect");

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_OVERWRITEPROMPT);

        const fs::path defaultFolder = Normalize_EffectSaveDialogPath(kEffectAuthoringSaveDefaultFolder);
        if (!defaultFolder.empty() && fs::exists(defaultFolder))
        {
            IShellItem* folderItem = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(defaultFolder.wstring().c_str(), nullptr, IID_PPV_ARGS(&folderItem))) && folderItem != nullptr)
            {
                dialog->SetFolder(folderItem);
                folderItem->Release();
            }
        }

        const COMDLG_FILTERSPEC filters[]
        {
            { L"Effect Authoring", L"*.effect.json" },
            { L"All", L"*.*" },
        };
        dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
        dialog->SetFileTypeIndex(1);
        dialog->SetDefaultExtension(L"effect.json");
        dialog->SetFileName(kEffectAuthoringSaveDefaultFileName);

        const HWND owner = EDITOR != nullptr ? EDITOR->Get_WindowHandle() : nullptr;
        hr = dialog->Show(owner);
        if (SUCCEEDED(hr))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr)
            {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != nullptr)
                {
                    outFilePath = Ensure_EffectAuthoringExtension(fs::path(path));
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }

        dialog->Release();

        if (shouldUninit)
            CoUninitialize();

        return !outFilePath.empty();
    }

    bool Try_PickEffectOpenFileDialog(fs::path& outFilePath, const wchar_t* title = L"Open Effect")
    {
        outFilePath.clear();

        const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const bool shouldUninit = SUCCEEDED(initHr);

        IFileOpenDialog* dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(hr) || dialog == nullptr)
        {
            if (shouldUninit)
                CoUninitialize();
            return false;
        }

        dialog->SetTitle(title);

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST);

        const fs::path defaultFolder = Normalize_EffectSaveDialogPath(kEffectAuthoringSaveDefaultFolder);
        if (!defaultFolder.empty() && fs::exists(defaultFolder))
        {
            IShellItem* folderItem = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(defaultFolder.wstring().c_str(), nullptr, IID_PPV_ARGS(&folderItem))) && folderItem != nullptr)
            {
                dialog->SetFolder(folderItem);
                folderItem->Release();
            }
        }

        const COMDLG_FILTERSPEC filters[]
        {
            { L"Effect Authoring", L"*.effect.json" },
            { L"All", L"*.*" },
        };
        dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
        dialog->SetFileTypeIndex(1);

        const HWND owner = EDITOR != nullptr ? EDITOR->Get_WindowHandle() : nullptr;
        hr = dialog->Show(owner);
        if (SUCCEEDED(hr))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr)
            {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != nullptr)
                {
                    outFilePath = fs::path(path);
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }

        dialog->Release();

        if (shouldUninit)
            CoUninitialize();

        return !outFilePath.empty();
    }

    bool Confirm_DiscardDirtyAuthoringDocument()
    {
        const HWND owner = EDITOR != nullptr ? EDITOR->Get_WindowHandle() : nullptr;
        const int result = MessageBoxW(
            owner,
            L"저장되지 않은 변경 사항이 사라질 수 있습니다.\n계속 열겠습니까?",
            L"Open Effect",
            MB_OKCANCEL | MB_ICONWARNING
        );

        return result == IDOK;
    }

    fs::path Resolve_DefaultEffectMaterialPath()
    {
        if (GAME == nullptr)
            return {};

        const vector<AssetMeta> materialAssets = GAME->Get_AssetsByType("EffectMaterial");
        for (const AssetMeta& assetMeta : materialAssets)
        {
            if (assetMeta.fullPath.empty())
                continue;

            if (fs::path(assetMeta.fullPath).filename() == kDefaultEffectMaterialFileName)
                return assetMeta.fullPath;
        }

        const wstring assetRoot = GAME->Get_AssetRoot();
        if (assetRoot.empty())
            return {};

        const fs::path candidatePath =
            fs::path(assetRoot) / L"Effects" / L"Materials" / kDefaultEffectMaterialFileName;
        if (!fs::exists(candidatePath))
            return {};

        const string materialGuid = GAME->Ensure_AssetGUID(candidatePath.wstring(), "EffectMaterial");
        if (!materialGuid.empty())
        {
            const wstring resolvedPath = GAME->Resolve_AssetPath(materialGuid);
            if (!resolvedPath.empty())
                return resolvedPath;
        }

        return candidatePath;
    }

    RequiredModuleData Make_DefaultRequiredModuleData()
    {
        RequiredModuleData data{};
        const fs::path materialPath = Resolve_DefaultEffectMaterialPath();
        if (materialPath.empty() || !EffectMaterialPresetReader::Read(materialPath, data.material))
        {
            data.material.sourcePresetName = kDefaultEffectMaterialName;
            data.material.mainTexturePath = kDefaultEffectMaterialTexturePath;
            data.material.mainTextureGuid = GAME != nullptr
                                            ? GAME->Ensure_AssetGUID(
                                                (fs::path(GAME->Get_AssetRoot()) / String::ToWString(kDefaultEffectMaterialTexturePath)).wstring(),
                                                "Texture"
                                            )
                                            : string{};
        }

        return data;
    }
}

Emitter_View::Emitter_View()
    : Editor_Window{ L"Emitter", ICON_FA_FIRE }
{
}

HRESULT Emitter_View::Initialize()
{
    CHECK_FAILED(Editor_Window::Initialize(), E_FAIL);

    _particleSystemData = {};
    _emitters.clear();
    _emitters.push_back(Make_DefaultSpriteEmitter());
    _selectedEmitterIndex = 0;
    _selectedTypeData = false;
    _selectedModuleIndex.reset();

    if (EDITOR != nullptr && EDITOR->Get_EditorContext() != nullptr)
        EDITOR->Get_EditorContext()->Set_SelectEffectEmitter(_emitters.front().id);

    return S_OK;
}

void Emitter_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string& windowName = Get_ImGuiWindowName();
    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        Handle_Shortcuts();

        if (_modulePickerEmitterIndex.has_value() && _modulePickerEmitterIndex.value() >= _emitters.size())
            Close_ModulePicker();

        Draw_EmitterBoard();
        Draw_ModulePickerPopup();
    }

    Set_Open(isOpen);
    ImGui::End();
}

bool Emitter_View::CanSave() const
{
    return true;
}

void Emitter_View::Save()
{
    fs::path savePath{};
    if (!Internal::Try_PickEffectSaveFileDialog(savePath))
        return;

    ofstream file(savePath);
    if (!file.is_open())
    {
        if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
        {
            EDITOR->Get_Notification()->Add_Notification_With_Type(
                NotifyType::Warning,
                "Effect 저장 실패: {}",
                String::ToString(savePath.filename().wstring())
            );
        }
        return;
    }

    const json documentJson = Build_AuthoringDocumentJson();
    file << documentJson.dump(4);
    file.close();
    if (!file.good())
    {
        if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
        {
            EDITOR->Get_Notification()->Add_Notification_With_Type(
                NotifyType::Warning,
                "Effect 저장 실패: {}",
                String::ToString(savePath.filename().wstring())
            );
        }
        return;
    }

    const string effectGuid = GAME != nullptr ? GAME->Ensure_AssetGUID(savePath.wstring(), "Effect") : string{};
    _currentDocumentPath = Internal::Normalize_EffectSaveDialogPath(savePath);

    ClearDirty();

    if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
    {
        if (!effectGuid.empty())
        {
            EDITOR->Get_Notification()->Add_Notification_With_Type(
                NotifyType::Success,
                "Effect 저장 완료: {}",
                String::ToString(savePath.filename().wstring())
            );
        }
        else
        {
            EDITOR->Get_Notification()->Add_Notification_With_Type(
                NotifyType::Warning,
                "Effect 저장 완료, meta 생성 실패: {}",
                String::ToString(savePath.filename().wstring())
            );
        }
    }
}

bool Emitter_View::CanOpen() const
{
    return true;
}

void Emitter_View::Open()
{
    if (Is_Dirty() && !Internal::Confirm_DiscardDirtyAuthoringDocument())
        return;

    fs::path openPath{};
    if (!Internal::Try_PickEffectOpenFileDialog(openPath))
        return;

    ifstream file(openPath);
    if (!file.is_open())
    {
        if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
        {
            EDITOR->Get_Notification()->Add_Notification_With_Type(
                NotifyType::Warning,
                "Effect 열기 실패: {}",
                String::ToString(openPath.filename().wstring())
            );
        }
        return;
    }

    json documentJson{};
    try
    {
        file >> documentJson;
    }
    catch (const json::exception&)
    {
        if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
        {
            EDITOR->Get_Notification()->Add_Notification_With_Type(
                NotifyType::Warning,
                "Effect JSON 해석 실패: {}",
                String::ToString(openPath.filename().wstring())
            );
        }
        return;
    }

    if (!Restore_AuthoringDocumentJson(documentJson))
    {
        if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
        {
            EDITOR->Get_Notification()->Add_Notification_With_Type(
                NotifyType::Warning,
                "Effect 복원 실패: {}",
                String::ToString(openPath.filename().wstring())
            );
        }
        return;
    }

    _currentDocumentPath = Internal::Normalize_EffectSaveDialogPath(openPath);
    ClearDirty();

    if (EDITOR != nullptr)
        EDITOR->Request_RestartPreview();

    if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
    {
        EDITOR->Get_Notification()->Add_Notification_With_Type(
            NotifyType::Success,
            "Effect 열기 완료: {}",
            String::ToString(openPath.filename().wstring())
        );
    }
}

bool Emitter_View::CanAdd() const
{
    return true;
}

void Emitter_View::Add()
{
    fs::path addPath{};
    if (!Internal::Try_PickEffectOpenFileDialog(addPath, L"Add Effect Emitters"))
        return;

    ifstream file(addPath);
    if (!file.is_open())
    {
        if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
        {
            EDITOR->Get_Notification()->Add_Notification_With_Type(
                NotifyType::Warning,
                "Effect Add 실패: {}",
                String::ToString(addPath.filename().wstring())
            );
        }
        return;
    }

    json documentJson{};
    try
    {
        file >> documentJson;
    }
    catch (const json::exception&)
    {
        if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
        {
            EDITOR->Get_Notification()->Add_Notification_With_Type(
                NotifyType::Warning,
                "Effect JSON 해석 실패: {}",
                String::ToString(addPath.filename().wstring())
            );
        }
        return;
    }

    EffectAuthoringDocument document{};
    if (!EffectAuthoringJsonSerializer::From_Json(documentJson, document))
    {
        if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
        {
            EDITOR->Get_Notification()->Add_Notification_With_Type(
                NotifyType::Warning,
                "Effect Add 실패: {}",
                String::ToString(addPath.filename().wstring())
            );
        }
        return;
    }

    if (document.emitters.empty())
    {
        if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
        {
            EDITOR->Get_Notification()->Add_Notification_With_Type(
                NotifyType::Warning,
                "Effect Add 실패: import 가능한 emitter가 없습니다."
            );
        }
        return;
    }

    for (AuthoringEmitter& emitter : document.emitters)
    {
        for (AuthoringModule& module : emitter.modules)
            Apply_AuthoringModuleMetadata(module);
    }

    const size_t importedCount = document.emitters.size();
    Execute_AuthoringEdit(
        "Add Emitters",
        [this, sourceEmitters = document.emitters]
        {
            Append_ImportedEmitters(sourceEmitters);
        }
    );

    if (EDITOR != nullptr && EDITOR->Get_Notification() != nullptr)
    {
        EDITOR->Get_Notification()->Add_Notification_With_Type(
            NotifyType::Success,
            "Effect Add 완료: {} ({} emitters)",
            String::ToString(addPath.filename().wstring()),
            importedCount
        );
    }
}

AuthoringEmitter* Emitter_View::Find_Emitter(uint32 emitterId)
{
    for (AuthoringEmitter& emitter : _emitters)
    {
        if (emitter.id == emitterId)
            return &emitter;
    }

    return nullptr;
}

AuthoringModule* Emitter_View::Find_Module(uint32 emitterId, uint32 moduleId)
{
    AuthoringEmitter* emitter = Find_Emitter(emitterId);
    if (nullptr == emitter)
        return nullptr;

    for (AuthoringModule& module : emitter->modules)
    {
        if (module.id == moduleId)
            return &module;
    }

    return nullptr;
}

AuthoringEmitter* Emitter_View::Get_SelectedOrFirstEmitter()
{
    Clamp_Selection();

    if (_selectedEmitterIndex.has_value())
        return &_emitters[_selectedEmitterIndex.value()];

    if (!_emitters.empty())
        return &_emitters.front();

    return nullptr;
}

void Emitter_View::Clear_PreviewDirty(uint32 emitterId)
{
    AuthoringEmitter* emitter = Find_Emitter(emitterId);
    if (nullptr == emitter)
        return;

    emitter->previewDirty = false;
}

bool Emitter_View::Has_PreviewDirtyEmitter() const
{
    for (const AuthoringEmitter& emitter : _emitters)
    {
        if (emitter.previewDirty)
            return true;
    }

    return false;
}

Emitter_View::AuthoringSnapshot Emitter_View::Capture_AuthoringSnapshot() const
{
    AuthoringSnapshot snapshot{};
    snapshot.particleSystemData = _particleSystemData;
    snapshot.emitters = _emitters;
    snapshot.selectedEmitterIndex = _selectedEmitterIndex;
    snapshot.selectedTypeData = _selectedTypeData;
    snapshot.selectedModuleIndex = _selectedModuleIndex;
    snapshot.nextAuthoringId = _nextAuthoringId;
    return snapshot;
}

void Emitter_View::Restore_AuthoringSnapshot(const AuthoringSnapshot& snapshot)
{
    Close_ModulePicker();
    _particleSystemData = snapshot.particleSystemData;
    _emitters = snapshot.emitters;
    _selectedEmitterIndex = snapshot.selectedEmitterIndex;
    _selectedTypeData = snapshot.selectedTypeData;
    _selectedModuleIndex = snapshot.selectedModuleIndex;
    _nextAuthoringId = snapshot.nextAuthoringId;

    Sync_AllEmitterMirrors();
    Mark_AllPreviewDirty();
    Clamp_Selection();
    Sync_SelectionContext();
    MarkDirty();
}

void Emitter_View::Execute_AuthoringSnapshotCommand(
    const AuthoringSnapshot& beforeSnapshot,
    const AuthoringSnapshot& afterSnapshot,
    const string& description)
{
    if (EDITOR == nullptr)
        return;

    EDITOR->Execute_Command(
        Action_Command::Create(
            [this, beforeSnapshot]
            {
                Restore_AuthoringSnapshot(beforeSnapshot);
            },
            [this, afterSnapshot]
            {
                Restore_AuthoringSnapshot(afterSnapshot);
            },
            description
        )
    );
}

json Emitter_View::Build_AuthoringDocumentJson() const
{
    EffectAuthoringDocument document{};
    document.particleSystemData = _particleSystemData;
    document.emitters = _emitters;
    return EffectAuthoringJsonSerializer::To_Json(document);
}

bool Emitter_View::Restore_AuthoringDocumentJson(const json& root)
{
    EffectAuthoringDocument document{};
    if (!EffectAuthoringJsonSerializer::From_Json(root, document))
        return false;

    for (AuthoringEmitter& emitter : document.emitters)
    {
        for (AuthoringModule& module : emitter.modules)
            Apply_AuthoringModuleMetadata(module);
    }

    const uint32 nextAuthoringId = EffectAuthoringJsonSerializer::Find_NextAuthoringId(document);

    Close_ModulePicker();
    _particleSystemData = document.particleSystemData;
    _emitters = move(document.emitters);
    _selectedEmitterIndex = _emitters.empty() ? optional<size_t>{} : optional<size_t>{ 0 };
    _selectedTypeData = false;
    _selectedModuleIndex.reset();
    _nextAuthoringId = nextAuthoringId;

    Sync_AllEmitterMirrors();
    Mark_AllPreviewDirty();
    Clamp_Selection();
    Sync_SelectionContext();
    if (EDITOR != nullptr)
    {
        if (const Shared<CurveEditor_View> curveEditor = dynamic_pointer_cast<CurveEditor_View>(EDITOR->Get_Window(L"Curve Editor")))
            curveEditor->Clear_PinnedTracks();
    }
    MarkDirty();
    return true;
}

uint32 Emitter_View::Issue_AuthoringId()
{
    return _nextAuthoringId++;
}

AuthoringEmitter Emitter_View::Make_DefaultSpriteEmitter()
{
    AuthoringEmitter emitter{};
    emitter.id = Issue_AuthoringId();
    emitter.name = "Particle Emitter";
    emitter.enabled = true;
    emitter.previewDirty = true;

    AuthoringModule requiredModule = Make_RequiredModule();
    if (const auto* requiredData = get_if<RequiredModuleData>(&requiredModule.data))
    {
        emitter.rendererType = requiredData->rendererType;
        emitter.textureId = requiredData->material.mainTexturePath;
        emitter.resourceSummary = requiredData->material.mainTexturePath;
    }

    emitter.modules.push_back(move(requiredModule));
    emitter.modules.push_back(Make_SpawnModule());
    emitter.modules.push_back(Make_LifetimeModule());
    emitter.modules.push_back(Make_InitialLocationModule());
    emitter.modules.push_back(Make_InitialSizeModule());
    emitter.modules.push_back(Make_InitialVelocityModule());
    emitter.modules.push_back(Make_InitialRadialVelocityModule());
    emitter.modules.push_back(Make_InitialColorModule());
    emitter.modules.push_back(Make_ColorOverLifeModule());

    return emitter;
}

AuthoringEmitter Emitter_View::Clone_Emitter(const AuthoringEmitter& sourceEmitter)
{
    AuthoringEmitter clonedEmitter = sourceEmitter;
    clonedEmitter.id = Issue_AuthoringId();
    clonedEmitter.previewDirty = true;

    for (AuthoringModule& module : clonedEmitter.modules)
        module.id = Issue_AuthoringId();

    return clonedEmitter;
}

AuthoringModule Emitter_View::Clone_Module(const AuthoringModule& sourceModule)
{
    AuthoringModule clonedModule = sourceModule;
    clonedModule.id = Issue_AuthoringId();
    Apply_AuthoringModuleMetadata(clonedModule);
    return clonedModule;
}

AuthoringModule Emitter_View::Make_DefaultModuleByType(AuthoringModuleType type)
{
    switch (type)
    {
    case AuthoringModuleType::Required:
        return Make_RequiredModule();
    case AuthoringModuleType::Spawn:
        return Make_SpawnModule();
    case AuthoringModuleType::Lifetime:
        return Make_LifetimeModule();
    case AuthoringModuleType::InitialLocation:
        return Make_InitialLocationModule();
    case AuthoringModuleType::SphereLocation:
        return Make_SphereLocationModule();
    case AuthoringModuleType::PlaneRadialLocation:
        return Make_PlaneRadialLocationModule();
    case AuthoringModuleType::InitialSize:
        return Make_InitialSizeModule();
    case AuthoringModuleType::InitialMeshSize:
        return Make_InitialMeshSizeModule();
    case AuthoringModuleType::InitialVelocity:
        return Make_InitialVelocityModule();
    case AuthoringModuleType::InitialRadialVelocity:
        return Make_InitialRadialVelocityModule();
    case AuthoringModuleType::VelocityCone:
        return Make_VelocityConeModule();
    case AuthoringModuleType::Acceleration:
        return Make_AccelerationModule();
    case AuthoringModuleType::Drag:
        return Make_DragModule();
    case AuthoringModuleType::VelocityOverLife:
        return Make_VelocityOverLifeModule();
    case AuthoringModuleType::OrbitOverLife:
        return Make_OrbitOverLifeModule();
    case AuthoringModuleType::InitialRotation:
        return Make_InitialRotationModule();
    case AuthoringModuleType::SphereRadialOrientation:
        return Make_SphereRadialOrientationModule();
    case AuthoringModuleType::PlaneRadialOrientation:
        return Make_PlaneRadialOrientationModule();
    case AuthoringModuleType::RotationOverLife:
        return Make_RotationOverLifeModule();
    case AuthoringModuleType::InitialRotationRate:
        return Make_InitialRotationRateModule();
    case AuthoringModuleType::RotationRateScaleByLife:
        return Make_RotationRateScaleByLifeModule();
    case AuthoringModuleType::InitialMeshRotation:
        return Make_InitialMeshRotationModule();
    case AuthoringModuleType::MeshRotationOverLife:
        return Make_MeshRotationOverLifeModule();
    case AuthoringModuleType::InitialMeshRotationRate:
        return Make_InitialMeshRotationRateModule();
    case AuthoringModuleType::MeshRotationRateScaleByLife:
        return Make_MeshRotationRateScaleByLifeModule();
    case AuthoringModuleType::InitialColor:
        return Make_InitialColorModule();
    case AuthoringModuleType::ColorOverLife:
        return Make_ColorOverLifeModule();
    case AuthoringModuleType::SubUVFrameOverLife:
        return Make_SubUVFrameOverLifeModule();
    case AuthoringModuleType::SizeByLife:
        return Make_SizeByLifeModule();
    case AuthoringModuleType::MeshSizeByLife:
        return Make_MeshSizeByLifeModule();
    case AuthoringModuleType::SpawnPerUnit:
        return Make_SpawnPerUnitModule();
    case AuthoringModuleType::MaterialScalarModulation:
        return Make_MaterialScalarModulationModule();
    }

    return Make_RequiredModule();
}

AuthoringModule Emitter_View::Make_RequiredModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::Required;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = Internal::Make_DefaultRequiredModuleData();
    return module;
}

AuthoringModule Emitter_View::Make_SpawnModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::Spawn;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = SpawnModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_LifetimeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::Lifetime;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = LifetimeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialLocationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialLocation;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = InitialLocationModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_SphereLocationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::SphereLocation;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = SphereLocationModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_PlaneRadialLocationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::PlaneRadialLocation;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = PlaneRadialLocationModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialSizeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialSize;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = InitialSizeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialMeshSizeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialMeshSize;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = InitialMeshSizeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialVelocityModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialVelocity;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = InitialVelocityModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialRadialVelocityModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialRadialVelocity;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = InitialRadialVelocityModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_VelocityConeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::VelocityCone;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = VelocityConeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_AccelerationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::Acceleration;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = AccelerationModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_DragModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::Drag;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = DragModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_VelocityOverLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::VelocityOverLife;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = VelocityOverLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_OrbitOverLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::OrbitOverLife;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = OrbitOverLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialRotationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialRotation;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = InitialRotationModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_SphereRadialOrientationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::SphereRadialOrientation;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = SphereRadialOrientationModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_PlaneRadialOrientationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::PlaneRadialOrientation;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = PlaneRadialOrientationModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_RotationOverLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::RotationOverLife;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = RotationOverLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialRotationRateModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialRotationRate;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = InitialRotationRateModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_RotationRateScaleByLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::RotationRateScaleByLife;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = RotationRateScaleByLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialMeshRotationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialMeshRotation;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = InitialMeshRotationModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_MeshRotationOverLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::MeshRotationOverLife;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = MeshRotationOverLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialMeshRotationRateModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialMeshRotationRate;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = InitialMeshRotationRateModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_MeshRotationRateScaleByLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::MeshRotationRateScaleByLife;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = MeshRotationRateScaleByLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialColorModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialColor;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = InitialColorModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_ColorOverLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::ColorOverLife;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = ColorOverLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_SubUVFrameOverLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::SubUVFrameOverLife;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = SubUVFrameOverLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_SizeByLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::SizeByLife;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = SizeByLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_MeshSizeByLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::MeshSizeByLife;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = MeshSizeByLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_SpawnPerUnitModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::SpawnPerUnit;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = SpawnPerUnitModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_MaterialScalarModulationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::MaterialScalarModulation;
    module.enabled = true;
    Apply_AuthoringModuleMetadata(module);
    module.data = MaterialScalarModulationModuleData{};
    return module;
}

bool Emitter_View::Has_ModuleType(const AuthoringEmitter& emitter, AuthoringModuleType type) const
{
    for (const AuthoringModule& module : emitter.modules)
    {
        if (module.type == type)
            return true;
    }

    return false;
}

bool Emitter_View::Is_ModuleCompatibleWithEmitter(const AuthoringEmitter& emitter, AuthoringModuleType type) const
{
    if (emitter.typeData.kind == AuthoringTypeDataKind::Ribbon)
    {
        switch (type)
        {
        case AuthoringModuleType::Required:
        case AuthoringModuleType::Lifetime:
        case AuthoringModuleType::InitialColor:
        case AuthoringModuleType::ColorOverLife:
        case AuthoringModuleType::SubUVFrameOverLife:
        case AuthoringModuleType::SizeByLife:
        case AuthoringModuleType::MaterialScalarModulation:
            return true;
        default:
            return false;
        }
    }

    if (emitter.typeData.kind == AuthoringTypeDataKind::Mesh)
    {
        switch (type)
        {
        case AuthoringModuleType::InitialSize:
        case AuthoringModuleType::InitialRotation:
        case AuthoringModuleType::RotationOverLife:
        case AuthoringModuleType::InitialRotationRate:
        case AuthoringModuleType::RotationRateScaleByLife:
        case AuthoringModuleType::SizeByLife:
            return false;
        default:
            break;
        }
    }

    switch (type)
    {
    case AuthoringModuleType::InitialMeshSize:
    case AuthoringModuleType::InitialMeshRotation:
    case AuthoringModuleType::MeshRotationOverLife:
    case AuthoringModuleType::InitialMeshRotationRate:
    case AuthoringModuleType::MeshRotationRateScaleByLife:
    case AuthoringModuleType::MeshSizeByLife:
    case AuthoringModuleType::SphereRadialOrientation:
        return emitter.typeData.kind == AuthoringTypeDataKind::Mesh;
    default:
        break;
    }

    const AuthoringModuleMetadata& metadata = Get_AuthoringModuleMetadata(type);
    if (emitter.typeData.kind == AuthoringTypeDataKind::Trail)
        return metadata.supportsTrail;

    return metadata.supportsSprite;
}

bool Emitter_View::Can_AddModuleType(const AuthoringEmitter& emitter, AuthoringModuleType type) const
{
    switch (type)
    {
    case AuthoringModuleType::Required:
    case AuthoringModuleType::Spawn:
        return false;

    case AuthoringModuleType::Lifetime:
    case AuthoringModuleType::InitialLocation:
    case AuthoringModuleType::SphereLocation:
    case AuthoringModuleType::PlaneRadialLocation:
    case AuthoringModuleType::InitialSize:
    case AuthoringModuleType::InitialMeshSize:
    case AuthoringModuleType::InitialVelocity:
    case AuthoringModuleType::InitialRadialVelocity:
    case AuthoringModuleType::VelocityCone:
    case AuthoringModuleType::Acceleration:
    case AuthoringModuleType::Drag:
    case AuthoringModuleType::VelocityOverLife:
    case AuthoringModuleType::OrbitOverLife:
    case AuthoringModuleType::InitialRotation:
    case AuthoringModuleType::SphereRadialOrientation:
    case AuthoringModuleType::PlaneRadialOrientation:
    case AuthoringModuleType::RotationOverLife:
    case AuthoringModuleType::InitialRotationRate:
    case AuthoringModuleType::RotationRateScaleByLife:
    case AuthoringModuleType::InitialMeshRotation:
    case AuthoringModuleType::MeshRotationOverLife:
    case AuthoringModuleType::InitialMeshRotationRate:
    case AuthoringModuleType::MeshRotationRateScaleByLife:
    case AuthoringModuleType::InitialColor:
    case AuthoringModuleType::ColorOverLife:
    case AuthoringModuleType::SubUVFrameOverLife:
    case AuthoringModuleType::SizeByLife:
    case AuthoringModuleType::MeshSizeByLife:
    case AuthoringModuleType::SpawnPerUnit:
    case AuthoringModuleType::MaterialScalarModulation:
        return Is_ModuleCompatibleWithEmitter(emitter, type) && !Has_ModuleType(emitter, type);

    default:
        return false;
    }
}

void Emitter_View::Add_Module(size_t emitterIndex, AuthoringModuleType type)
{
    if (emitterIndex >= _emitters.size())
        return;

    AuthoringEmitter& emitter = _emitters[emitterIndex];
    if (!Can_AddModuleType(emitter, type))
        return;

    AuthoringModule module{};
    switch (type)
    {
    case AuthoringModuleType::Lifetime:
        module = Make_LifetimeModule();
        break;
    case AuthoringModuleType::InitialLocation:
        module = Make_InitialLocationModule();
        break;
    case AuthoringModuleType::SphereLocation:
        module = Make_SphereLocationModule();
        break;
    case AuthoringModuleType::PlaneRadialLocation:
        module = Make_PlaneRadialLocationModule();
        break;
    case AuthoringModuleType::InitialSize:
        module = Make_InitialSizeModule();
        break;
    case AuthoringModuleType::InitialMeshSize:
        module = Make_InitialMeshSizeModule();
        break;
    case AuthoringModuleType::InitialVelocity:
        module = Make_InitialVelocityModule();
        break;
    case AuthoringModuleType::InitialRadialVelocity:
        module = Make_InitialRadialVelocityModule();
        break;
    case AuthoringModuleType::VelocityCone:
        module = Make_VelocityConeModule();
        break;
    case AuthoringModuleType::Acceleration:
        module = Make_AccelerationModule();
        break;
    case AuthoringModuleType::Drag:
        module = Make_DragModule();
        break;
    case AuthoringModuleType::VelocityOverLife:
        module = Make_VelocityOverLifeModule();
        break;
    case AuthoringModuleType::OrbitOverLife:
        module = Make_OrbitOverLifeModule();
        break;
    case AuthoringModuleType::InitialRotation:
        module = Make_InitialRotationModule();
        break;
    case AuthoringModuleType::SphereRadialOrientation:
        module = Make_SphereRadialOrientationModule();
        break;
    case AuthoringModuleType::PlaneRadialOrientation:
        module = Make_PlaneRadialOrientationModule();
        break;
    case AuthoringModuleType::RotationOverLife:
        module = Make_RotationOverLifeModule();
        break;
    case AuthoringModuleType::InitialRotationRate:
        module = Make_InitialRotationRateModule();
        break;
    case AuthoringModuleType::RotationRateScaleByLife:
        module = Make_RotationRateScaleByLifeModule();
        break;
    case AuthoringModuleType::InitialMeshRotation:
        module = Make_InitialMeshRotationModule();
        break;
    case AuthoringModuleType::MeshRotationOverLife:
        module = Make_MeshRotationOverLifeModule();
        break;
    case AuthoringModuleType::InitialMeshRotationRate:
        module = Make_InitialMeshRotationRateModule();
        break;
    case AuthoringModuleType::MeshRotationRateScaleByLife:
        module = Make_MeshRotationRateScaleByLifeModule();
        break;
    case AuthoringModuleType::InitialColor:
        module = Make_InitialColorModule();
        break;
    case AuthoringModuleType::ColorOverLife:
        module = Make_ColorOverLifeModule();
        break;
    case AuthoringModuleType::SubUVFrameOverLife:
        module = Make_SubUVFrameOverLifeModule();
        break;
    case AuthoringModuleType::SizeByLife:
        module = Make_SizeByLifeModule();
        break;
    case AuthoringModuleType::MeshSizeByLife:
        module = Make_MeshSizeByLifeModule();
        break;
    case AuthoringModuleType::SpawnPerUnit:
        module = Make_SpawnPerUnitModule();
        break;
    case AuthoringModuleType::MaterialScalarModulation:
        module = Make_MaterialScalarModulationModule();
        break;
    default:
        return;
    }

    emitter.modules.push_back(std::move(module));
    emitter.previewDirty = true;
    _selectedEmitterIndex = emitterIndex;
    _selectedTypeData = false;
    _selectedModuleIndex = emitter.modules.size() - 1;
    Sync_SelectionContext();
    MarkDirty();
}

void Emitter_View::Copy_EmitterToClipboard(size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
        return;

    _clipboardKind = AuthoringClipboardKind::Emitter;
    _clipboardEmitter = _emitters[emitterIndex];
    _clipboardModule.reset();
}

void Emitter_View::Copy_ModuleToClipboard(size_t emitterIndex, size_t moduleIndex)
{
    if (emitterIndex >= _emitters.size() || moduleIndex >= _emitters[emitterIndex].modules.size())
        return;

    _clipboardKind = AuthoringClipboardKind::Module;
    _clipboardEmitter.reset();
    _clipboardModule = _emitters[emitterIndex].modules[moduleIndex];
}

bool Emitter_View::Can_PasteEmitterAfter(size_t emitterIndex, string* disabledReason) const
{
    const auto set_reason =
        [&disabledReason](const char* message)
    {
        if (disabledReason != nullptr)
            *disabledReason = message;
    };

    if (emitterIndex >= _emitters.size())
    {
        set_reason("대상 이미터가 없습니다.");
        return false;
    }

    if (_clipboardKind != AuthoringClipboardKind::Emitter || !_clipboardEmitter.has_value())
    {
        set_reason("복사된 이미터가 없습니다.");
        return false;
    }

    set_reason("");
    return true;
}

bool Emitter_View::Can_PasteEmitterToEnd(string* disabledReason) const
{
    const auto set_reason =
        [&disabledReason](const char* message)
    {
        if (disabledReason != nullptr)
            *disabledReason = message;
    };

    if (_clipboardKind != AuthoringClipboardKind::Emitter || !_clipboardEmitter.has_value())
    {
        set_reason("복사된 이미터가 없습니다.");
        return false;
    }

    set_reason("");
    return true;
}

bool Emitter_View::Can_PasteModuleInto(size_t emitterIndex, string* disabledReason) const
{
    const auto set_reason =
        [&disabledReason](const char* message)
    {
        if (disabledReason != nullptr)
            *disabledReason = message;
    };

    if (emitterIndex >= _emitters.size())
    {
        set_reason("대상 이미터가 없습니다.");
        return false;
    }

    if (_clipboardKind != AuthoringClipboardKind::Module || !_clipboardModule.has_value())
    {
        set_reason("복사된 모듈이 없습니다.");
        return false;
    }

    const AuthoringModule& clipboardModule = _clipboardModule.value();
    if (clipboardModule.type == AuthoringModuleType::Required ||
        clipboardModule.type == AuthoringModuleType::Spawn)
    {
        set_reason("이 모듈은 새로 붙여넣을 수 없습니다.");
        return false;
    }

    const AuthoringEmitter& emitter = _emitters[emitterIndex];
    if (!Can_AddModuleType(emitter, clipboardModule.type))
    {
        if (Has_ModuleType(emitter, clipboardModule.type))
            set_reason("같은 타입 모듈이 이미 있습니다.");
        else if (!Is_ModuleCompatibleWithEmitter(emitter, clipboardModule.type))
            set_reason("현재 TypeData와 호환되지 않습니다.");
        else
            set_reason("이 모듈은 여기에 붙여넣을 수 없습니다.");
        return false;
    }

    set_reason("");
    return true;
}

bool Emitter_View::Can_PasteModuleValues(size_t emitterIndex, size_t moduleIndex, string* disabledReason) const
{
    const auto set_reason =
        [&disabledReason](const char* message)
    {
        if (disabledReason != nullptr)
            *disabledReason = message;
    };

    if (emitterIndex >= _emitters.size() || moduleIndex >= _emitters[emitterIndex].modules.size())
    {
        set_reason("대상 모듈이 없습니다.");
        return false;
    }

    if (_clipboardKind != AuthoringClipboardKind::Module || !_clipboardModule.has_value())
    {
        set_reason("복사된 모듈이 없습니다.");
        return false;
    }

    const AuthoringModule& clipboardModule = _clipboardModule.value();
    const AuthoringModule& targetModule = _emitters[emitterIndex].modules[moduleIndex];
    if (clipboardModule.type != targetModule.type)
    {
        set_reason("같은 타입 모듈에서만 값 붙여넣기가 가능합니다.");
        return false;
    }

    if (targetModule.type == AuthoringModuleType::Required ||
        targetModule.type == AuthoringModuleType::Spawn)
    {
        set_reason("이 모듈 값 붙여넣기는 이번 v1 범위에서 지원하지 않습니다.");
        return false;
    }

    set_reason("");
    return true;
}

void Emitter_View::Paste_EmitterAfter(size_t emitterIndex)
{
    if (!Can_PasteEmitterAfter(emitterIndex))
        return;

    Close_ModulePicker();
    const size_t insertIndex = emitterIndex + 1;
    _emitters.insert(
        _emitters.begin() + static_cast<ptrdiff_t>(insertIndex),
        Clone_Emitter(_clipboardEmitter.value())
    );
    Select_Emitter(insertIndex);
    MarkDirty();
}

void Emitter_View::Paste_EmitterToEnd()
{
    if (!Can_PasteEmitterToEnd())
        return;

    Close_ModulePicker();
    const size_t insertIndex = _emitters.size();
    _emitters.push_back(Clone_Emitter(_clipboardEmitter.value()));
    Select_Emitter(insertIndex);
    MarkDirty();
}

void Emitter_View::Paste_ModuleInto(size_t emitterIndex)
{
    if (!Can_PasteModuleInto(emitterIndex))
        return;

    AuthoringEmitter& emitter = _emitters[emitterIndex];
    emitter.modules.push_back(Clone_Module(_clipboardModule.value()));
    emitter.previewDirty = true;
    _selectedEmitterIndex = emitterIndex;
    _selectedTypeData = false;
    _selectedModuleIndex = emitter.modules.size() - 1;
    Sync_SelectionContext();
    MarkDirty();
}

void Emitter_View::Paste_ModuleValues(size_t emitterIndex, size_t moduleIndex)
{
    if (!Can_PasteModuleValues(emitterIndex, moduleIndex))
        return;

    AuthoringEmitter& emitter = _emitters[emitterIndex];
    AuthoringModule& targetModule = emitter.modules[moduleIndex];
    targetModule.data = _clipboardModule.value().data;
    emitter.previewDirty = true;
    _selectedEmitterIndex = emitterIndex;
    _selectedTypeData = false;
    _selectedModuleIndex = moduleIndex;
    Sync_SelectionContext();
    MarkDirty();
}

void Emitter_View::Notify_ClipboardStatus(const string& message, bool warning) const
{
    if (message.empty() || EDITOR == nullptr || EDITOR->Get_Notification() == nullptr)
        return;

    EDITOR->Get_Notification()->Add_Notification_With_Type(
        warning ? NotifyType::Warning : NotifyType::Info,
        "{}",
        message
    );
}

void Emitter_View::Handle_Shortcuts()
{
    if (!_isFocused)
        return;

    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput)
        return;

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false))
    {
        if (_selectedEmitterIndex.has_value() && !_selectedTypeData && _selectedModuleIndex.has_value())
        {
            Copy_ModuleToClipboard(_selectedEmitterIndex.value(), _selectedModuleIndex.value());
            Notify_ClipboardStatus("모듈을 클립보드에 복사했습니다.");
        }
        else if (_selectedEmitterIndex.has_value() && !_selectedTypeData)
        {
            Copy_EmitterToClipboard(_selectedEmitterIndex.value());
            Notify_ClipboardStatus("이미터를 클립보드에 복사했습니다.");
        }
        else
            Notify_ClipboardStatus("이미터 또는 모듈을 선택한 상태에서 복사할 수 있습니다.", true);

        return;
    }

    if (!io.KeyCtrl || !ImGui::IsKeyPressed(ImGuiKey_V, false))
        return;

    if (!_selectedEmitterIndex.has_value() || _selectedTypeData)
    {
        Notify_ClipboardStatus("이미터 헤더 또는 모듈을 선택한 상태에서 붙여넣을 수 있습니다.", true);
        return;
    }

    const size_t emitterIndex = _selectedEmitterIndex.value();
    if (_clipboardKind == AuthoringClipboardKind::Emitter)
    {
        if (_selectedModuleIndex.has_value())
        {
            Notify_ClipboardStatus("이미터 붙여넣기는 이미터 헤더 선택 상태에서만 지원합니다.", true);
            return;
        }

        string disabledReason{};
        if (!Can_PasteEmitterAfter(emitterIndex, &disabledReason))
        {
            Notify_ClipboardStatus(disabledReason, true);
            return;
        }

        Execute_AuthoringEdit(
            "Paste Emitter",
            [this, emitterIndex]
            {
                Paste_EmitterAfter(emitterIndex);
            }
        );
        return;
    }

    if (_clipboardKind == AuthoringClipboardKind::Module)
    {
        if (_selectedModuleIndex.has_value())
        {
            const size_t moduleIndex = _selectedModuleIndex.value();
            string valuePasteReason{};
            const bool canPasteValues = Can_PasteModuleValues(emitterIndex, moduleIndex, &valuePasteReason);
            if (canPasteValues)
            {
                Execute_AuthoringEdit(
                    "Paste Module Values",
                    [this, emitterIndex, moduleIndex]
                    {
                        Paste_ModuleValues(emitterIndex, moduleIndex);
                    }
                );
                return;
            }

            string modulePasteReason{};
            const bool canPasteModule = Can_PasteModuleInto(emitterIndex, &modulePasteReason);
            if (canPasteModule)
            {
                Execute_AuthoringEdit(
                    "Paste Module",
                    [this, emitterIndex]
                    {
                        Paste_ModuleInto(emitterIndex);
                    }
                );
                return;
            }

            const AuthoringModule* targetModule =
                moduleIndex < _emitters[emitterIndex].modules.size()
                ? &_emitters[emitterIndex].modules[moduleIndex]
                : nullptr;
            const AuthoringModuleType clipboardType =
                _clipboardModule.has_value()
                ? _clipboardModule->type
                : AuthoringModuleType::Required;
            const bool prefersValuePasteReason =
                targetModule != nullptr &&
                _clipboardModule.has_value() &&
                targetModule->type == clipboardType;
            Notify_ClipboardStatus(
                prefersValuePasteReason || modulePasteReason.empty()
                ? valuePasteReason
                : modulePasteReason,
                true
            );
            return;
        }

        string disabledReason{};
        if (!Can_PasteModuleInto(emitterIndex, &disabledReason))
        {
            Notify_ClipboardStatus(disabledReason, true);
            return;
        }

        Execute_AuthoringEdit(
            "Paste Module",
            [this, emitterIndex]
            {
                Paste_ModuleInto(emitterIndex);
            }
        );
        return;
    }

    Notify_ClipboardStatus("복사된 이미터 또는 모듈이 없습니다.", true);
}

void Emitter_View::Process_PendingClipboardAction()
{
    if (_pendingClipboardAction == PendingClipboardAction::None ||
        !_pendingClipboardEmitterIndex.has_value())
        return;

    const PendingClipboardAction action = _pendingClipboardAction;
    const size_t emitterIndex = _pendingClipboardEmitterIndex.value();
    const optional<size_t> moduleIndex = _pendingClipboardModuleIndex;

    switch (action)
    {
    case PendingClipboardAction::PasteEmitterAfter:
        Execute_AuthoringEdit(
            "Paste Emitter",
            [this, emitterIndex]
            {
                Paste_EmitterAfter(emitterIndex);
            }
        );
        break;

    case PendingClipboardAction::PasteEmitterToEnd:
        Execute_AuthoringEdit(
            "Paste Emitter",
            [this]
            {
                Paste_EmitterToEnd();
            }
        );
        break;

    case PendingClipboardAction::PasteModule:
        Execute_AuthoringEdit(
            "Paste Module",
            [this, emitterIndex]
            {
                Paste_ModuleInto(emitterIndex);
            }
        );
        break;

    case PendingClipboardAction::PasteModuleValues:
        if (moduleIndex.has_value())
        {
            Execute_AuthoringEdit(
                "Paste Module Values",
                [this, emitterIndex, moduleIndex]
                {
                    Paste_ModuleValues(emitterIndex, moduleIndex.value());
                }
            );
        }
        break;

    case PendingClipboardAction::None:
    default:
        break;
    }

    _pendingClipboardAction = PendingClipboardAction::None;
    _pendingClipboardEmitterIndex.reset();
    _pendingClipboardModuleIndex.reset();
}

void Emitter_View::Queue_ClipboardAction(
    PendingClipboardAction action,
    size_t emitterIndex,
    optional<size_t> moduleIndex)
{
    _pendingClipboardAction = action;
    _pendingClipboardEmitterIndex = emitterIndex;
    _pendingClipboardModuleIndex = moduleIndex;
}

void Emitter_View::Clear_Selection()
{
    _selectedEmitterIndex.reset();
    _selectedTypeData = false;
    _selectedModuleIndex.reset();

    if (EDITOR != nullptr && EDITOR->Get_EditorContext() != nullptr)
        EDITOR->Get_EditorContext()->Clear_EffectSelection();
}

void Emitter_View::Clamp_Selection()
{
    if (_emitters.empty())
    {
        _selectedEmitterIndex.reset();
        _selectedTypeData = false;
        _selectedModuleIndex.reset();
        return;
    }

    if (!_selectedEmitterIndex.has_value())
    {
        _selectedTypeData = false;
        _selectedModuleIndex.reset();
        return;
    }

    _selectedEmitterIndex = min(_selectedEmitterIndex.value(), _emitters.size() - 1);

    const auto& modules = _emitters[_selectedEmitterIndex.value()].modules;
    if (_selectedModuleIndex.has_value() && _selectedModuleIndex.value() >= modules.size())
        _selectedModuleIndex.reset();

    if (_selectedTypeData)
        _selectedModuleIndex.reset();
}

void Emitter_View::Select_Emitter(size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
    {
        Clear_Selection();
        return;
    }

    _selectedEmitterIndex = emitterIndex;
    _selectedTypeData = false;
    _selectedModuleIndex.reset();

    if (EDITOR != nullptr && EDITOR->Get_EditorContext() != nullptr)
        EDITOR->Get_EditorContext()->Set_SelectEffectEmitter(_emitters[emitterIndex].id);
}

void Emitter_View::Select_Module(size_t emitterIndex, size_t moduleIndex)
{
    if (emitterIndex >= _emitters.size() || moduleIndex >= _emitters[emitterIndex].modules.size())
    {
        Clear_Selection();
        return;
    }

    _selectedEmitterIndex = emitterIndex;
    _selectedTypeData = false;
    _selectedModuleIndex = moduleIndex;

    if (EDITOR != nullptr && EDITOR->Get_EditorContext() != nullptr)
        EDITOR->Get_EditorContext()->Set_SelectEffectModule(_emitters[emitterIndex].id, _emitters[emitterIndex].modules[moduleIndex].id);
}

void Emitter_View::Select_TypeData(size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
    {
        Clear_Selection();
        return;
    }

    _selectedEmitterIndex = emitterIndex;
    _selectedTypeData = true;
    _selectedModuleIndex.reset();

    if (EDITOR != nullptr && EDITOR->Get_EditorContext() != nullptr)
        EDITOR->Get_EditorContext()->Set_SelectEffectTypeData(_emitters[emitterIndex].id);
}

void Emitter_View::Sync_SelectionContext()
{
    if (EDITOR == nullptr || EDITOR->Get_EditorContext() == nullptr)
        return;

    if (!_selectedEmitterIndex.has_value() || _selectedEmitterIndex.value() >= _emitters.size())
    {
        EDITOR->Get_EditorContext()->Clear_EffectSelection();
        return;
    }

    const AuthoringEmitter& emitter = _emitters[_selectedEmitterIndex.value()];

    if (_selectedTypeData)
    {
        EDITOR->Get_EditorContext()->Set_SelectEffectTypeData(emitter.id);
        return;
    }

    if (_selectedModuleIndex.has_value() && _selectedModuleIndex.value() < emitter.modules.size())
    {
        EDITOR->Get_EditorContext()->Set_SelectEffectModule(
            emitter.id,
            emitter.modules[_selectedModuleIndex.value()].id
        );
        return;
    }

    EDITOR->Get_EditorContext()->Set_SelectEffectEmitter(emitter.id);
}

void Emitter_View::Delete_Emitter(size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
        return;

    Close_ModulePicker();
    _emitters.erase(_emitters.begin() + static_cast<ptrdiff_t>(emitterIndex));
    Clamp_Selection();

    if (_emitters.empty())
        Clear_Selection();
    else
    {
        const size_t nextIndex = min(emitterIndex, _emitters.size() - 1);
        Select_Emitter(nextIndex);
    }

    MarkDirty();
}

void Emitter_View::Duplicate_Emitter(size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
        return;

    Close_ModulePicker();
    const size_t insertIndex = emitterIndex + 1;
    _emitters.insert(
        _emitters.begin() + static_cast<ptrdiff_t>(insertIndex),
        Clone_Emitter(_emitters[emitterIndex])
    );
    Select_Emitter(insertIndex);
    MarkDirty();
}

void Emitter_View::Append_ImportedEmitters(const vector<AuthoringEmitter>& sourceEmitters)
{
    if (sourceEmitters.empty())
        return;

    Close_ModulePicker();
    const size_t firstImportedIndex = _emitters.size();
    _emitters.reserve(_emitters.size() + sourceEmitters.size());

    for (const AuthoringEmitter& sourceEmitter : sourceEmitters)
        _emitters.push_back(Clone_Emitter(sourceEmitter));

    Select_Emitter(firstImportedIndex);
    MarkDirty();
}

void Emitter_View::Insert_NewEmitter(size_t insertIndex)
{
    Close_ModulePicker();
    const size_t clampedIndex = min(insertIndex, _emitters.size());
    _emitters.insert(_emitters.begin() + static_cast<ptrdiff_t>(clampedIndex), Make_DefaultSpriteEmitter());
    Select_Emitter(clampedIndex);
    MarkDirty();
}

void Emitter_View::Process_PendingEmitterAction()
{
    if (_pendingEmitterAction == PendingEmitterAction::None || !_pendingEmitterActionIndex.has_value())
        return;

    const size_t emitterIndex = _pendingEmitterActionIndex.value();
    switch (_pendingEmitterAction)
    {
    case PendingEmitterAction::Duplicate:
        Execute_AuthoringEdit(
            "Duplicate Emitter",
            [this, emitterIndex]
            {
                Duplicate_Emitter(emitterIndex);
            }
        );
        break;
    case PendingEmitterAction::Delete:
        Execute_AuthoringEdit(
            "Delete Emitter",
            [this, emitterIndex]
            {
                Delete_Emitter(emitterIndex);
            }
        );
        break;
    case PendingEmitterAction::InsertBefore:
        Execute_AuthoringEdit(
            "Insert Emitter",
            [this, emitterIndex]
            {
                Insert_NewEmitter(emitterIndex);
            }
        );
        break;
    case PendingEmitterAction::InsertAfter:
        Execute_AuthoringEdit(
            "Insert Emitter",
            [this, emitterIndex]
            {
                Insert_NewEmitter(emitterIndex + 1);
            }
        );
        break;
    case PendingEmitterAction::None:
    default:
        break;
    }

    _pendingEmitterAction = PendingEmitterAction::None;
    _pendingEmitterActionIndex.reset();
}

void Emitter_View::Queue_EmitterAction(PendingEmitterAction action, size_t emitterIndex)
{
    _pendingEmitterAction = action;
    _pendingEmitterActionIndex = emitterIndex;
}

void Emitter_View::Delete_Module(size_t emitterIndex, size_t moduleIndex)
{
    if (emitterIndex >= _emitters.size())
        return;

    AuthoringEmitter& emitter = _emitters[emitterIndex];
    if (moduleIndex >= emitter.modules.size())
        return;

    const AuthoringModule& module = emitter.modules[moduleIndex];
    if (!module.removable)
        return;

    const bool isSelectedEmitter = _selectedEmitterIndex.has_value() && _selectedEmitterIndex.value() == emitterIndex;
    const bool isSelectedModule = isSelectedEmitter && _selectedModuleIndex.has_value();
    const bool isDeletingSelectedModule = isSelectedModule && _selectedModuleIndex.value() == moduleIndex;

    emitter.modules.erase(emitter.modules.begin() + static_cast<ptrdiff_t>(moduleIndex));

    if (isDeletingSelectedModule)
        Select_Emitter(emitterIndex);
    else if (isSelectedModule && _selectedModuleIndex.value() > moduleIndex)
        _selectedModuleIndex = _selectedModuleIndex.value() - 1;

    emitter.previewDirty = true;
    MarkDirty();
}

void Emitter_View::Process_PendingModuleAction()
{
    if (_pendingModuleAction == PendingModuleAction::None ||
        !_pendingModuleEmitterIndex.has_value() ||
        !_pendingModuleIndex.has_value())
        return;

    const size_t emitterIndex = _pendingModuleEmitterIndex.value();
    const size_t moduleIndex = _pendingModuleIndex.value();
    switch (_pendingModuleAction)
    {
    case PendingModuleAction::Delete:
        Execute_AuthoringEdit(
            "Delete Module",
            [this, emitterIndex, moduleIndex]
            {
                Delete_Module(emitterIndex, moduleIndex);
            }
        );
        break;
    case PendingModuleAction::ResetData:
        Execute_AuthoringEdit(
            "Reset Module Data",
            [this, emitterIndex, moduleIndex]
            {
                Reset_ModuleData(emitterIndex, moduleIndex);
            }
        );
        break;
    case PendingModuleAction::None:
    default:
        break;
    }

    _pendingModuleAction = PendingModuleAction::None;
    _pendingModuleEmitterIndex.reset();
    _pendingModuleIndex.reset();
}

void Emitter_View::Queue_ModuleAction(PendingModuleAction action, size_t emitterIndex, size_t moduleIndex)
{
    _pendingModuleAction = action;
    _pendingModuleEmitterIndex = emitterIndex;
    _pendingModuleIndex = moduleIndex;
}

void Emitter_View::Reset_ModuleData(size_t emitterIndex, size_t moduleIndex)
{
    if (emitterIndex >= _emitters.size())
        return;

    AuthoringEmitter& emitter = _emitters[emitterIndex];
    if (moduleIndex >= emitter.modules.size())
        return;

    AuthoringModule& module = emitter.modules[moduleIndex];
    switch (module.type)
    {
    case AuthoringModuleType::Required:
    {
        RequiredModuleData data = Internal::Make_DefaultRequiredModuleData();
        module.data = data;
        emitter.textureId = data.material.mainTexturePath;
        emitter.resourceSummary = data.material.mainTexturePath;
        break;
    }

    case AuthoringModuleType::Spawn:
        module.data = SpawnModuleData{};
        break;

    case AuthoringModuleType::Lifetime:
        module.data = LifetimeModuleData{};
        break;

    case AuthoringModuleType::InitialLocation:
        module.data = InitialLocationModuleData{};
        break;

    case AuthoringModuleType::SphereLocation:
        module.data = SphereLocationModuleData{};
        break;

    case AuthoringModuleType::PlaneRadialLocation:
        module.data = PlaneRadialLocationModuleData{};
        break;

    case AuthoringModuleType::InitialSize:
        module.data = InitialSizeModuleData{};
        break;

    case AuthoringModuleType::InitialMeshSize:
        module.data = InitialMeshSizeModuleData{};
        break;

    case AuthoringModuleType::InitialVelocity:
        module.data = InitialVelocityModuleData{};
        break;

    case AuthoringModuleType::InitialRadialVelocity:
        module.data = InitialRadialVelocityModuleData{};
        break;

    case AuthoringModuleType::VelocityCone:
        module.data = VelocityConeModuleData{};
        break;

    case AuthoringModuleType::Acceleration:
        module.data = AccelerationModuleData{};
        break;

    case AuthoringModuleType::Drag:
        module.data = DragModuleData{};
        break;

    case AuthoringModuleType::VelocityOverLife:
        module.data = VelocityOverLifeModuleData{};
        break;

    case AuthoringModuleType::OrbitOverLife:
        module.data = OrbitOverLifeModuleData{};
        break;

    case AuthoringModuleType::InitialRotation:
        module.data = InitialRotationModuleData{};
        break;

    case AuthoringModuleType::SphereRadialOrientation:
        module.data = SphereRadialOrientationModuleData{};
        break;

    case AuthoringModuleType::PlaneRadialOrientation:
        module.data = PlaneRadialOrientationModuleData{};
        break;

    case AuthoringModuleType::RotationOverLife:
        module.data = RotationOverLifeModuleData{};
        break;

    case AuthoringModuleType::InitialRotationRate:
        module.data = InitialRotationRateModuleData{};
        break;

    case AuthoringModuleType::RotationRateScaleByLife:
        module.data = RotationRateScaleByLifeModuleData{};
        break;

    case AuthoringModuleType::InitialMeshRotation:
        module.data = InitialMeshRotationModuleData{};
        break;

    case AuthoringModuleType::MeshRotationOverLife:
        module.data = MeshRotationOverLifeModuleData{};
        break;

    case AuthoringModuleType::InitialMeshRotationRate:
        module.data = InitialMeshRotationRateModuleData{};
        break;

    case AuthoringModuleType::MeshRotationRateScaleByLife:
        module.data = MeshRotationRateScaleByLifeModuleData{};
        break;

    case AuthoringModuleType::InitialColor:
        module.data = InitialColorModuleData{};
        break;

    case AuthoringModuleType::ColorOverLife:
        module.data = ColorOverLifeModuleData{};
        break;

    case AuthoringModuleType::SubUVFrameOverLife:
        module.data = SubUVFrameOverLifeModuleData{};
        break;

    case AuthoringModuleType::SizeByLife:
        module.data = SizeByLifeModuleData{};
        break;
    case AuthoringModuleType::MeshSizeByLife:
        module.data = MeshSizeByLifeModuleData{};
        break;
    case AuthoringModuleType::SpawnPerUnit:
        module.data = SpawnPerUnitModuleData{};
        break;
    case AuthoringModuleType::MaterialScalarModulation:
        module.data = MaterialScalarModulationModuleData{};
        break;

    default:
        return;
    }

    Sync_EmitterMirror(emitter);
    emitter.previewDirty = true;
    MarkDirty();
}

void Emitter_View::Apply_TypeDataAction(PendingTypeDataAction action, size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
        return;

    AuthoringEmitter& emitter = _emitters[emitterIndex];
    switch (action)
    {
    case PendingTypeDataAction::AddTrail:
    {
        emitter.typeData.kind = AuthoringTypeDataKind::Trail;
        emitter.typeData.payload = TrailTypeData{};
        break;
    }
    case PendingTypeDataAction::AddMesh:
    {
        emitter.typeData.kind = AuthoringTypeDataKind::Mesh;
        emitter.typeData.payload = MeshTypeData{};
        break;
    }
    case PendingTypeDataAction::AddRibbon:
    {
        emitter.typeData.kind = AuthoringTypeDataKind::Ribbon;
        emitter.typeData.payload = RibbonTypeData{};
        break;
    }
    case PendingTypeDataAction::AddBeam:
    {
        emitter.typeData.kind = AuthoringTypeDataKind::Beam;
        emitter.typeData.payload = BeamTypeData{};
        break;
    }
    case PendingTypeDataAction::ResetData:
    {
        if (emitter.typeData.kind == AuthoringTypeDataKind::Mesh)
            emitter.typeData.payload = MeshTypeData{};
        else if (emitter.typeData.kind == AuthoringTypeDataKind::Ribbon)
            emitter.typeData.payload = RibbonTypeData{};
        else if (emitter.typeData.kind == AuthoringTypeDataKind::Beam)
            emitter.typeData.payload = BeamTypeData{};
        else
        {
            emitter.typeData.kind = AuthoringTypeDataKind::Trail;
            emitter.typeData.payload = TrailTypeData{};
        }
        break;
    }
    case PendingTypeDataAction::Remove:
    {
        emitter.typeData.kind = AuthoringTypeDataKind::None;
        emitter.typeData.payload = EmptyTypeData{};
        break;
    }
    case PendingTypeDataAction::None:
    default:
        return;
    }

    Sync_EmitterMirror(emitter);
    emitter.previewDirty = true;
    MarkDirty();
}

void Emitter_View::Process_PendingTypeDataAction()
{
    if (_pendingTypeDataAction == PendingTypeDataAction::None ||
        !_pendingTypeDataEmitterIndex.has_value())
        return;

    const PendingTypeDataAction action = _pendingTypeDataAction;
    const size_t emitterIndex = _pendingTypeDataEmitterIndex.value();
    const char* description = "Edit TypeData";
    switch (action)
    {
    case PendingTypeDataAction::AddTrail:
        description = "Add Trail TypeData";
        break;

    case PendingTypeDataAction::AddMesh:
        description = "Add Mesh TypeData";
        break;

    case PendingTypeDataAction::AddRibbon:
        description = "Add Ribbon TypeData";
        break;

    case PendingTypeDataAction::AddBeam:
        description = "Add Beam TypeData";
        break;

    case PendingTypeDataAction::ResetData:
        description = "Reset TypeData";
        break;

    case PendingTypeDataAction::Remove:
        description = "Remove TypeData";
        break;

    case PendingTypeDataAction::None:
    default:
        break;
    }

    Execute_AuthoringEdit(
        description,
        [this, action, emitterIndex]
        {
            Apply_TypeDataAction(action, emitterIndex);
        }
    );

    _pendingTypeDataAction = PendingTypeDataAction::None;
    _pendingTypeDataEmitterIndex.reset();
}

void Emitter_View::Queue_TypeDataAction(PendingTypeDataAction action, size_t emitterIndex)
{
    _pendingTypeDataAction = action;
    _pendingTypeDataEmitterIndex = emitterIndex;
}

void Emitter_View::Mark_AllPreviewDirty()
{
    for (AuthoringEmitter& emitter : _emitters)
        emitter.previewDirty = true;
}

void Emitter_View::Sync_EmitterMirror(AuthoringEmitter& emitter)
{
    emitter.rendererType = Authoring::Resolve_RendererType(emitter.typeData);

    for (const AuthoringModule& module : emitter.modules)
    {
        if (module.type != AuthoringModuleType::Required)
            continue;

        const RequiredModuleData* requiredData = get_if<RequiredModuleData>(&module.data);
        if (requiredData == nullptr)
            return;

        emitter.textureId = requiredData->material.mainTexturePath;
        emitter.resourceSummary = requiredData->material.mainTexturePath;
        return;
    }
}

void Emitter_View::Sync_AllEmitterMirrors()
{
    for (AuthoringEmitter& emitter : _emitters)
        Sync_EmitterMirror(emitter);
}

vector<Emitter_View::ModulePickerEntry> Emitter_View::Build_ModulePickerEntries(const AuthoringEmitter& emitter) const
{
    const auto makeEntry = [&](
        AuthoringModuleType moduleType,
        bool implemented,
        bool canAdd,
        string disabledReason)
    {
        ModulePickerEntry entry{};
        entry.moduleType = moduleType;
        entry.displayName = Get_AuthoringModuleMetadata(moduleType).displayName;
        entry.disabledReason = std::move(disabledReason);
        entry.implemented = implemented;
        entry.compatible = Is_ModuleCompatibleWithEmitter(emitter, moduleType);
        entry.canAdd = canAdd;

        if (!entry.compatible)
            entry.stateText = disabledReason == "이미 장착됨" ? "현재 TypeData와 비호환" : disabledReason;
        else if (canAdd)
            entry.stateText = "추가 가능";
        else if (implemented)
            entry.stateText = "이미 장착됨";
        else
            entry.stateText = "미구현";

        return entry;
    };

    vector<ModulePickerEntry> entries{};
    entries.push_back(
        makeEntry(
            AuthoringModuleType::Lifetime,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::Lifetime),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialSize,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialSize),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialMeshSize,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialMeshSize),
            "Mesh TypeData 전용"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialLocation,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialLocation),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::SphereLocation,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::SphereLocation),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::PlaneRadialLocation,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::PlaneRadialLocation),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialVelocity,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialVelocity),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialRadialVelocity,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialRadialVelocity),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::VelocityCone,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::VelocityCone),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::Acceleration,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::Acceleration),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::Drag,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::Drag),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::VelocityOverLife,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::VelocityOverLife),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::OrbitOverLife,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::OrbitOverLife),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialRotation,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialRotation),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::SphereRadialOrientation,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::SphereRadialOrientation),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::PlaneRadialOrientation,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::PlaneRadialOrientation),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::RotationOverLife,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::RotationOverLife),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialRotationRate,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialRotationRate),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::RotationRateScaleByLife,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::RotationRateScaleByLife),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialMeshRotation,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialMeshRotation),
            "Mesh TypeData 전용"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::MeshRotationOverLife,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::MeshRotationOverLife),
            "Mesh TypeData 전용"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialMeshRotationRate,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialMeshRotationRate),
            "Mesh TypeData 전용"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::MeshRotationRateScaleByLife,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::MeshRotationRateScaleByLife),
            "Mesh TypeData 전용"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialColor,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialColor),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::ColorOverLife,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::ColorOverLife),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::SubUVFrameOverLife,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::SubUVFrameOverLife),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::SizeByLife,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::SizeByLife),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::MeshSizeByLife,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::MeshSizeByLife),
            "Mesh TypeData 전용"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::SpawnPerUnit,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::SpawnPerUnit),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::MaterialScalarModulation,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::MaterialScalarModulation),
            "이미 장착됨"
        )
    );

    ranges::sort(
        entries,
        [](const ModulePickerEntry& lhs, const ModulePickerEntry& rhs)
        {
            return String::ToString(lhs.displayName) < String::ToString(rhs.displayName);
        }
    );

    return entries;
}

void Emitter_View::Close_ModulePicker()
{
    _modulePickerEmitterIndex.reset();
    _modulePickerPendingOpen = false;
    _modulePickerSearchBuffer[0] = '\0';
}

void Emitter_View::Draw_ModulePickerPopup()
{
    constexpr const char* modalPopupId = "새 모듈##ModuleAddPopup";

    if (_modulePickerPendingOpen)
    {
        ImGui::OpenPopup(modalPopupId);
        _modulePickerPendingOpen = false;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(kModulePickerModalWidth, kModulePickerModalHeight), ImGuiCond_Appearing);

    const bool isPopupOpen = ImGui::IsPopupOpen(modalPopupId);
    bool isModalOpen = true;
    if (!ImGui::BeginPopupModal(modalPopupId, &isModalOpen))
    {
        if (!isPopupOpen)
            Close_ModulePicker();
        return;
    }

    if (!_modulePickerEmitterIndex.has_value() || _modulePickerEmitterIndex.value() >= _emitters.size())
    {
        ImGui::EndPopup();
        Close_ModulePicker();
        return;
    }

    const AuthoringEmitter& emitter = _emitters[_modulePickerEmitterIndex.value()];
    const string searchLower = String::ToLowerCopy(string(_modulePickerSearchBuffer));
    const vector<ModulePickerEntry> entries = Build_ModulePickerEntries(emitter);

    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputTextWithHint("##ModulePickerSearch", "모듈 검색", _modulePickerSearchBuffer, IM_ARRAYSIZE(_modulePickerSearchBuffer));
    ImGui::Spacing();

    if (ImGui::BeginChild("##ModulePickerList", ImVec2(0.f, 0.f), true))
    {
        if (ImGui::BeginTable("##ModulePickerTable", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Module", ImGuiTableColumnFlags_WidthStretch, kModulePickerNameColumnStretch);
            ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthStretch, kModulePickerStateColumnStretch);

            for (const ModulePickerEntry& entry : entries)
            {
                const string displayName = String::ToString(entry.displayName);
                if (!searchLower.empty() &&
                    String::ToLowerCopy(displayName).find(searchLower) == string::npos)
                    continue;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                if (!entry.canAdd)
                    ImGui::BeginDisabled();

                const bool useUnsupportedPickerColor = !entry.compatible && entry.canAdd;
                if (useUnsupportedPickerColor)
                    ImGui::PushStyleColor(ImGuiCol_Text, To_ImVec4(kEmitterUnsupportedModuleTextColor));

                const bool picked = ImGui::Selectable(displayName.c_str(), false, ImGuiSelectableFlags_SpanAllColumns);
                const bool isModuleNameHovered = ImGui::IsItemHovered();
                if (useUnsupportedPickerColor)
                    ImGui::PopStyleColor();

                if (isModuleNameHovered && !entry.compatible)
                    ImGui::SetTooltip("현재 emitter 타입에서는 이 모듈이 preview에 반영되지 않습니다.");

                if (!entry.canAdd)
                    ImGui::EndDisabled();

                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("%s", entry.canAdd ? entry.stateText.c_str() : entry.disabledReason.c_str());

                if (!picked || !entry.canAdd || !entry.moduleType.has_value())
                    continue;

                const size_t emitterIndex = _modulePickerEmitterIndex.value();
                const AuthoringModuleType moduleType = entry.moduleType.value();
                Execute_AuthoringEdit(
                    "Add Module",
                    [this, emitterIndex, moduleType]
                    {
                        Add_Module(emitterIndex, moduleType);
                    }
                );

                ImGui::CloseCurrentPopup();
                Close_ModulePicker();
                break;
            }

            ImGui::EndTable();
        }

        ImGui::EndChild();
    }

    ImGui::EndPopup();

    if (!isModalOpen)
    {
        ImGui::CloseCurrentPopup();
        Close_ModulePicker();
    }
}

void Emitter_View::Open_ModulePicker(size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
        return;

    _modulePickerEmitterIndex = emitterIndex;
    _modulePickerPendingOpen = true;
    _modulePickerSearchBuffer[0] = '\0';
}

void Emitter_View::Execute_AuthoringEdit(const string& description, const function<void()>& edit)
{
    if (!edit)
        return;

    const AuthoringSnapshot beforeSnapshot = Capture_AuthoringSnapshot();
    edit();
    const AuthoringSnapshot afterSnapshot = Capture_AuthoringSnapshot();
    Execute_AuthoringSnapshotCommand(beforeSnapshot, afterSnapshot, description);
}

string Emitter_View::Build_ModuleWarningTooltip(const AuthoringEmitter& emitter, const AuthoringModule& module) const
{
    if (!emitter.enabled)
        return "비활성화된 이미터입니다.";

    if (!module.enabled && module.type != AuthoringModuleType::Required)
        return "비활성화된 모듈입니다.";

    if (!Is_ModuleCompatibleWithEmitter(emitter, module.type))
        return "현재 emitter 타입에서는 이 모듈이 preview에 반영되지 않습니다.";

    return {};
}

void Emitter_View::Draw_EmitterContextMenu(size_t emitterIndex)
{
    string emitterPasteReason{};
    const bool canPasteEmitter = Can_PasteEmitterAfter(emitterIndex, &emitterPasteReason);

    if (ImGui::BeginMenu("이미터"))
    {
        if (ImGui::MenuItem("이미터 복사"))
            Copy_EmitterToClipboard(emitterIndex);

        if (!canPasteEmitter)
            ImGui::BeginDisabled();
        if (ImGui::MenuItem("뒤에 이미터 붙여넣기"))
            Queue_ClipboardAction(PendingClipboardAction::PasteEmitterAfter, emitterIndex);
        if (!canPasteEmitter)
            ImGui::EndDisabled();
        if (!canPasteEmitter && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !emitterPasteReason.empty())
            ImGui::SetTooltip("%s", emitterPasteReason.c_str());

        if (ImGui::MenuItem("이미터 복제"))
            Queue_EmitterAction(PendingEmitterAction::Duplicate, emitterIndex);

        if (ImGui::MenuItem("이미터 삭제"))
            Queue_EmitterAction(PendingEmitterAction::Delete, emitterIndex);

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("파티클 시스템"))
    {
        if (ImGui::MenuItem("앞에 새 이미터 추가"))
            Queue_EmitterAction(PendingEmitterAction::InsertBefore, emitterIndex);

        if (ImGui::MenuItem("뒤에 새 이미터 추가"))
            Queue_EmitterAction(PendingEmitterAction::InsertAfter, emitterIndex);

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("타입 데이터"))
    {
        Draw_TypeDataContextMenu(emitterIndex);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("모듈"))
    {
        if (ImGui::MenuItem("새 모듈"))
            Open_ModulePicker(emitterIndex);

        ImGui::EndMenu();
    }
}

void Emitter_View::Draw_TypeDataContextMenu(size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
        return;

    const AuthoringEmitter& emitter = _emitters[emitterIndex];
    if (emitter.typeData.kind == AuthoringTypeDataKind::None)
    {
        if (ImGui::MenuItem("Trail TypeData 추가"))
            Queue_TypeDataAction(PendingTypeDataAction::AddTrail, emitterIndex);
        if (ImGui::MenuItem("Mesh TypeData 추가"))
            Queue_TypeDataAction(PendingTypeDataAction::AddMesh, emitterIndex);
        if (ImGui::MenuItem("Ribbon TypeData 추가"))
            Queue_TypeDataAction(PendingTypeDataAction::AddRibbon, emitterIndex);
        if (ImGui::MenuItem("Beam TypeData 추가"))
            Queue_TypeDataAction(PendingTypeDataAction::AddBeam, emitterIndex);
        return;
    }

    if (ImGui::MenuItem("타입 데이터 값 초기화"))
        Queue_TypeDataAction(PendingTypeDataAction::ResetData, emitterIndex);

    if (ImGui::MenuItem("타입 데이터 제거"))
        Queue_TypeDataAction(PendingTypeDataAction::Remove, emitterIndex);
}

void Emitter_View::Draw_ModuleContextMenu(size_t emitterIndex, size_t moduleIndex, const AuthoringModule& module)
{
    string modulePasteReason{};
    const bool canPasteModule = Can_PasteModuleInto(emitterIndex, &modulePasteReason);
    string valuePasteReason{};
    const bool canPasteValues = Can_PasteModuleValues(emitterIndex, moduleIndex, &valuePasteReason);

    if (ImGui::MenuItem("모듈 복사"))
        Copy_ModuleToClipboard(emitterIndex, moduleIndex);

    if (!canPasteModule)
        ImGui::BeginDisabled();
    if (ImGui::MenuItem("모듈 붙여넣기"))
        Queue_ClipboardAction(PendingClipboardAction::PasteModule, emitterIndex);
    if (!canPasteModule)
        ImGui::EndDisabled();
    if (!canPasteModule && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !modulePasteReason.empty())
        ImGui::SetTooltip("%s", modulePasteReason.c_str());

    if (!canPasteValues)
        ImGui::BeginDisabled();
    if (ImGui::MenuItem("값 붙여넣기"))
        Queue_ClipboardAction(PendingClipboardAction::PasteModuleValues, emitterIndex, moduleIndex);
    if (!canPasteValues)
        ImGui::EndDisabled();
    if (!canPasteValues && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !valuePasteReason.empty())
        ImGui::SetTooltip("%s", valuePasteReason.c_str());

    ImGui::Separator();

    if (ImGui::MenuItem("모듈 값 초기화"))
        Queue_ModuleAction(PendingModuleAction::ResetData, emitterIndex, moduleIndex);

    if (!module.removable)
        ImGui::BeginDisabled();

    if (ImGui::MenuItem("모듈 삭제"))
        Queue_ModuleAction(PendingModuleAction::Delete, emitterIndex, moduleIndex);

    if (!module.removable)
        ImGui::EndDisabled();
}

void Emitter_View::Draw_EmitterBoard()
{
    if (ImGui::BeginChild("##EmitterBoard", ImVec2(0.f, 0.f), true, ImGuiWindowFlags_HorizontalScrollbar))
    {
        const ImVec2 boardMin = ImGui::GetWindowPos();
        const ImVec2 boardMax{
            boardMin.x + ImGui::GetWindowWidth(),
            boardMin.y + ImGui::GetWindowHeight()
        };
        const float columnHeight = max(1.f, ImGui::GetContentRegionAvail().y);

        for (size_t emitterIndex = 0; emitterIndex < _emitters.size(); ++emitterIndex)
        {
            ImGui::PushID(static_cast<int>(emitterIndex));
            Draw_EmitterColumn(emitterIndex, _emitters[emitterIndex], columnHeight);
            ImGui::PopID();

            if (emitterIndex + 1 < _emitters.size())
                ImGui::SameLine(0.f, 0.f);
        }

        if (!_emitters.empty())
            ImGui::SameLine(0.f, 0.f);

        ImGui::Dummy(ImVec2(kEmitterRightBlankWidth, columnHeight));
        const bool isRightGutterHovered = ImGui::IsItemHovered();
        const float blankRegionLeft = ImGui::GetItemRectMax().x;

        const ImVec2 mousePos = ImGui::GetMousePos();
        const bool isTrailingBlankHovered =
            ImGui::IsWindowHovered() &&
            !ImGui::IsAnyItemHovered() &&
            mousePos.x > blankRegionLeft &&
            mousePos.x < boardMax.x &&
            mousePos.y >= boardMin.y &&
            mousePos.y < boardMax.y;
        const bool isRightBlankHovered = isRightGutterHovered || isTrailingBlankHovered;

        if (isRightBlankHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            Clear_Selection();
        else if (isRightBlankHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            Clear_Selection();
            ImGui::OpenPopup("##EmitterBoardBlankPopup");
        }

        ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_Appearing);
        if (ImGui::BeginPopup("##EmitterBoardBlankPopup"))
        {
            string blankPasteReason{};
            const bool canPasteEmitterToEnd = Can_PasteEmitterToEnd(&blankPasteReason);

            if (ImGui::Selectable("새 이미터"))
            {
                Execute_AuthoringEdit(
                    "Insert Emitter",
                    [this]
                    {
                        Insert_NewEmitter(_emitters.size());
                    }
                );
            }

            if (!canPasteEmitterToEnd)
                ImGui::BeginDisabled();
            if (ImGui::MenuItem("이미터 붙여넣기"))
                Queue_ClipboardAction(PendingClipboardAction::PasteEmitterToEnd, _emitters.size());
            if (!canPasteEmitterToEnd)
                ImGui::EndDisabled();
            if (!canPasteEmitterToEnd && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !blankPasteReason.empty())
                ImGui::SetTooltip("%s", blankPasteReason.c_str());

            ImGui::EndPopup();
        }

        Process_PendingEmitterAction();
        Process_PendingTypeDataAction();
        Process_PendingModuleAction();
        Process_PendingClipboardAction();
    }
    ImGui::EndChild();
}

void Emitter_View::Draw_EmitterColumn(size_t emitterIndex, AuthoringEmitter& emitter, float columnHeight)
{
    constexpr float headerPaddingX = 10.f;
    constexpr float headerPaddingY = 12.f;

    const bool isSelected =
        _selectedEmitterIndex.has_value() &&
        _selectedEmitterIndex.value() == emitterIndex;
    const ImVec4 accentColor = To_ImVec4(kEmitterAccentColor);
    const ImVec4 headerColor = isSelected ? accentColor : To_ImVec4(kEmitterHeaderColor);
    const ImVec4 textColor = isSelected ? To_ImVec4(kEmitterSelectedTextColor) : ImGui::GetStyleColorVec4(ImGuiCol_Text);

    ImGui::BeginGroup();
    const ImVec2 columnMin = ImGui::GetCursorScreenPos();
    const ImVec2 columnMax{
        columnMin.x + kEmitterColumnWidth,
        columnMin.y + columnHeight
    };
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        columnMin,
        columnMax,
        ImGui::ColorConvertFloat4ToU32(To_ImVec4(kEmitterColumnBackgroundColor))
    );
    drawList->AddRectFilled(
        ImVec2(columnMax.x - kEmitterDividerWidth, columnMin.y),
        columnMax,
        ImGui::ColorConvertFloat4ToU32(To_ImVec4(kEmitterDividerColor))
    );

    const ImVec2 headerCursor = ImGui::GetCursorPos();
    ImGui::PushStyleColor(ImGuiCol_Button, headerColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, headerColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, headerColor);
    ImGui::SetNextItemAllowOverlap();
    if (ImGui::Button("##EmitterHeader", ImVec2(kEmitterColumnWidth, kEmitterHeaderHeight)))
        Select_Emitter(emitterIndex);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        Select_Emitter(emitterIndex);
    if (ImGui::BeginPopupContextItem("##EmitterHeaderPopup"))
    {
        Draw_EmitterContextMenu(emitterIndex);
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(3);

    const ImVec2 moduleStackCursor = ImVec2(headerCursor.x, headerCursor.y + kEmitterHeaderHeight);
    ImGui::SetCursorPos(ImVec2(headerCursor.x + headerPaddingX, headerCursor.y + headerPaddingY));
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    ImGui::PushTextWrapPos(headerCursor.x + kEmitterColumnWidth - headerPaddingX);
    ImGui::TextUnformatted(emitter.name.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();

    ImGui::SetCursorPos(
        ImVec2(
            headerCursor.x + headerPaddingX,
            headerCursor.y + kEmitterHeaderHeight - ImGui::GetFrameHeight() - headerPaddingY
        )
    );
    ImGui::PushID("EmitterEnabled");
    bool nextEmitterEnabled = emitter.enabled;
    if (ImGui::Checkbox("##Enabled", &nextEmitterEnabled))
    {
        const AuthoringSnapshot beforeEmitterEnabledSnapshot = Capture_AuthoringSnapshot();
        emitter.enabled = nextEmitterEnabled;
        emitter.previewDirty = true;
        MarkDirty();
        const AuthoringSnapshot afterSnapshot = Capture_AuthoringSnapshot();
        Execute_AuthoringSnapshotCommand(beforeEmitterEnabledSnapshot, afterSnapshot, "Toggle Emitter Enabled");
    }
    ImGui::PopID();

    ImGui::SetCursorPos(moduleStackCursor);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.f));
    Draw_TypeDataRow(emitterIndex);
    for (size_t moduleIndex = 0; moduleIndex < emitter.modules.size(); ++moduleIndex)
    {
        ImGui::PushID(static_cast<int>(moduleIndex));
        Draw_ModuleRow(emitterIndex, moduleIndex, emitter.modules[moduleIndex]);
        ImGui::PopID();
    }
    ImGui::PopStyleVar();

    const float reservedTailHeight = columnMax.y - ImGui::GetCursorScreenPos().y;
    if (reservedTailHeight > 0.f)
    {
        ImGui::InvisibleButton("##EmitterReservedTail", ImVec2(kEmitterColumnWidth, reservedTailHeight));
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            Select_Emitter(emitterIndex);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            Select_Emitter(emitterIndex);
        if (ImGui::BeginPopupContextItem("##EmitterReservedTailPopup"))
        {
            Draw_EmitterContextMenu(emitterIndex);
            ImGui::EndPopup();
        }
    }

    ImGui::EndGroup();
}

void Emitter_View::Draw_TypeDataRow(size_t emitterIndex)
{
    constexpr float rowPaddingX = 10.f;

    const AuthoringTypeDataKind typeDataKind = _emitters[emitterIndex].typeData.kind;
    const bool hasTypeData = typeDataKind != AuthoringTypeDataKind::None;
    const bool isSelected =
        _selectedEmitterIndex.has_value() &&
        _selectedEmitterIndex.value() == emitterIndex &&
        _selectedTypeData;
    const ImVec4 accentColor = To_ImVec4(kEmitterAccentColor);
    const ImVec4 rowColor = isSelected ? accentColor : To_ImVec4(kEmitterTypeDataRowColor);
    const ImVec4 textColor = isSelected
                             ? To_ImVec4(kEmitterSelectedTextColor)
                             : hasTypeData
                               ? ImGui::GetStyleColorVec4(ImGuiCol_Text)
                               : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);

    const ImVec2 rowCursor = ImGui::GetCursorPos();
    ImGui::PushStyleColor(ImGuiCol_Button, rowColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rowColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, rowColor);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    ImGui::SetNextItemAllowOverlap();
    ImGui::Button("##TypeDataRow", ImVec2(kEmitterColumnWidth, kEmitterTypeDataRowHeight));
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
        Select_TypeData(emitterIndex);
    ImGui::PopStyleColor(4);

    if (ImGui::BeginPopupContextItem("##TypeDataRowPopup"))
    {
        Draw_TypeDataContextMenu(emitterIndex);
        ImGui::EndPopup();
    }

    const ImVec2 afterRowCursor = ImGui::GetCursorPos();
    const float textOffsetY = (kEmitterTypeDataRowHeight - ImGui::GetTextLineHeight()) * 0.5f;
    ImGui::SetCursorPos(ImVec2(rowCursor.x + rowPaddingX, rowCursor.y + textOffsetY));
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    if (hasTypeData)
    {
        switch (typeDataKind)
        {
        case AuthoringTypeDataKind::Trail:
            ImGui::TextUnformatted("트레일 데이터");
            break;

        case AuthoringTypeDataKind::Mesh:
            ImGui::TextUnformatted("메시 데이터");
            break;

        case AuthoringTypeDataKind::Ribbon:
            ImGui::TextUnformatted("리본 데이터");
            break;

        case AuthoringTypeDataKind::Beam:
            ImGui::TextUnformatted("빔 데이터");
            break;

        case AuthoringTypeDataKind::None:
        default:
            ImGui::TextUnformatted("타입 데이터");
            break;
        }
    }
    else
        ImGui::TextUnformatted("타입 데이터 (미장착)");
    ImGui::PopStyleColor();
    ImGui::SetCursorPos(afterRowCursor);
}

void Emitter_View::Draw_ModuleRow(size_t emitterIndex, size_t moduleIndex, AuthoringModule& module)
{
    constexpr float rowHeight = 28.f;
    constexpr float rowPaddingX = 10.f;
    constexpr float moduleCheckboxRightPadding = 34.f;
    constexpr float curveButtonSize = 22.f;
    constexpr float curveButtonGap = 4.f;

    const bool isSelected =
        _selectedEmitterIndex.has_value() &&
        _selectedEmitterIndex.value() == emitterIndex &&
        _selectedModuleIndex.has_value() &&
        _selectedModuleIndex.value() == moduleIndex;
    const bool isRequiredRow = module.type == AuthoringModuleType::Required;
    const bool isSpawnRow = module.type == AuthoringModuleType::Spawn;
    const ImVec4 accentColor = To_ImVec4(kEmitterAccentColor);
    const ImVec4 categoryColor = To_ImVec4(
        isRequiredRow
        ? kEmitterRequiredRowColor
        : isSpawnRow
          ? kEmitterSpawnRowColor
          : kEmitterCommonModuleRowColor
    );
    const ImVec4 disabledColor = To_ImVec4(kEmitterModuleDisabledRowColor);
    const bool usesCategoryColor = isRequiredRow || isSpawnRow;
    const ImVec4 rowColor = isSelected ? accentColor : categoryColor;
    const ImVec4 textColor = isSelected
                             ? To_ImVec4(kEmitterSelectedTextColor)
                             : usesCategoryColor
                               ? To_ImVec4(kEmitterCategoryTextColor)
                               : ImGui::GetStyleColorVec4(ImGuiCol_Text);
    AuthoringEmitter& emitter = _emitters[emitterIndex];
    const bool moduleCompatible = Is_ModuleCompatibleWithEmitter(emitter, module.type);
    const bool canPinCurve = CurveEditor_View::Has_ModuleCurveTargets(emitter, module);
    const bool canConvertCurve = !canPinCurve && CurveEditor_View::Has_ModuleConvertibleCurveTargets(emitter, module);
    const bool canUseCurveButton = canPinCurve || canConvertCurve;
    bool isCurveEditorFocused = false;
    if (canPinCurve && EDITOR != nullptr)
    {
        if (const Shared<CurveEditor_View> curveEditor = dynamic_pointer_cast<CurveEditor_View>(EDITOR->Get_Window(L"Curve Editor")))
            isCurveEditorFocused = curveEditor->Is_ModuleFocused(emitter.id, module.id);
    }
    const string displayNameUtf8 = String::ToString(module.displayName);
    const ImVec4 moduleNameTextColor =
        !isSelected && module.enabled && !moduleCompatible
        ? To_ImVec4(kEmitterUnsupportedModuleTextColor)
        : module.enabled
          ? textColor
          : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);

    const ImVec2 rowCursor = ImGui::GetCursorPos();
    const ImVec4 buttonColor = module.enabled || usesCategoryColor ? rowColor : disabledColor;
    ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, buttonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, buttonColor);
    ImGui::PushStyleColor(ImGuiCol_Text, module.enabled ? textColor : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::PushID(module.key.c_str());
    ImGui::SetNextItemAllowOverlap();
    ImGui::Button("##ModuleRow", ImVec2(kEmitterColumnWidth, rowHeight));
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
        Select_Module(emitterIndex, moduleIndex);

    const bool isRowHovered = ImGui::IsItemHovered();
    if (isCurveEditorFocused)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 rowMin = ImGui::GetItemRectMin();
        const ImVec2 rowMax = ImGui::GetItemRectMax();
        drawList->AddRectFilled(
            rowMin,
            ImVec2{ rowMin.x + 4.f, rowMax.y },
            Internal::kCurveEditorFocusBarColor
        );
    }
    ImGui::PopStyleColor(4);
    if (ImGui::BeginPopupContextItem("##ModuleRowPopup"))
    {
        Draw_ModuleContextMenu(emitterIndex, moduleIndex, module);
        ImGui::EndPopup();
    }
    ImGui::PopID();

    const ImVec2 afterRowCursor = ImGui::GetCursorPos();
    const float textOffsetY = (rowHeight - ImGui::GetTextLineHeight()) * 0.5f;
    ImGui::SetCursorPos(ImVec2(rowCursor.x + rowPaddingX, rowCursor.y + textOffsetY));
    ImGui::PushStyleColor(ImGuiCol_Text, moduleNameTextColor);
    ImGui::TextUnformatted(displayNameUtf8.c_str());
    ImGui::PopStyleColor();

    if (!isRequiredRow)
    {
        const float curveButtonRightPadding = moduleCheckboxRightPadding + curveButtonSize + curveButtonGap;
        ImGui::SetCursorPos(
            ImVec2(
                rowCursor.x + kEmitterColumnWidth - curveButtonRightPadding,
                rowCursor.y + (rowHeight - curveButtonSize) * 0.5f
            )
        );
        ImGui::PushID("PinCurves");
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2{ 0.42f, 0.5f });
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 0.f, 0.f });
        if (!canUseCurveButton)
            ImGui::BeginDisabled();
        if (ImGui::Button(ICON_FA_CHART_LINE, ImVec2(curveButtonSize, curveButtonSize)) && canUseCurveButton)
        {
            if (canPinCurve && EDITOR != nullptr)
            {
                if (const Shared<CurveEditor_View> curveEditor = dynamic_pointer_cast<CurveEditor_View>(EDITOR->Get_Window(L"Curve Editor")))
                    curveEditor->Pin_ModuleTargets(emitter.id, module.id);
            }
            else if (canConvertCurve)
                ImGui::OpenPopup("ConvertCurvesPopup");
        }
        if (!canUseCurveButton)
            ImGui::EndDisabled();
        ImGui::PopStyleVar(2);
        if (ImGui::IsItemHovered(canUseCurveButton ? ImGuiHoveredFlags_None : ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip(
                canPinCurve
                ? "커브 편집"
                : canConvertCurve
                  ? "커브로 전환 후 편집"
                  : "이 모듈에서는 커브 분포가 지원되지 않습니다"
            );
        }
        if (ImGui::BeginPopup("ConvertCurvesPopup"))
        {
            ImGui::TextUnformatted("이 모듈의 분포를 고정값 커브로 전환할까요?");
            ImGui::TextDisabled("전환 후 Curve Editor에서 바로 편집합니다.");
            if (ImGui::Button("전환", ImVec2{ 72.f, 0.f }))
            {
                if (EDITOR != nullptr)
                {
                    if (const Shared<CurveEditor_View> curveEditor = dynamic_pointer_cast<CurveEditor_View>(EDITOR->Get_Window(L"Curve Editor")))
                        curveEditor->Convert_AndPin_ModuleTargets(emitter.id, module.id);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("취소", ImVec2{ 72.f, 0.f }))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::PopID();

        ImGui::SetCursorPos(
            ImVec2(
                rowCursor.x + kEmitterColumnWidth - moduleCheckboxRightPadding,
                rowCursor.y + (rowHeight - ImGui::GetFrameHeight()) * 0.5f
            )
        );
        ImGui::PushID("ModuleEnabled");
        bool nextModuleEnabled = module.enabled;
        if (ImGui::Checkbox("##Enabled", &nextModuleEnabled))
        {
            const AuthoringSnapshot beforeModuleEnabledSnapshot = Capture_AuthoringSnapshot();
            module.enabled = nextModuleEnabled;
            emitter.previewDirty = true;
            MarkDirty();
            const AuthoringSnapshot afterSnapshot = Capture_AuthoringSnapshot();
            Execute_AuthoringSnapshotCommand(beforeModuleEnabledSnapshot, afterSnapshot, "Toggle Module Enabled");
        }
        ImGui::PopID();
    }

    ImGui::SetCursorPos(afterRowCursor);

    if (isRowHovered)
    {
        const bool warningTooltipNeeded =
            !emitter.enabled ||
            (!module.enabled && module.type != AuthoringModuleType::Required) ||
            !moduleCompatible;
        if (warningTooltipNeeded)
        {
            const string warningTooltip = Build_ModuleWarningTooltip(emitter, module);
            if (!warningTooltip.empty())
                ImGui::SetTooltip("%s", warningTooltip.c_str());
        }
    }
}

Shared<Emitter_View> Emitter_View::Create()
{
    return make_shared<Emitter_View>();
}

NS_END
