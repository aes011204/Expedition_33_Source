#include "Emitter_View.h"

#include "Action_Command.h"
#include "Editor_Context.h"
#include "EffectEditorInstance.h"
#include "GameInstance.h"
#include "Helper_ImGui.h"
#include "Helper_ReadPreset.h"
#include "Helper_String.h"

NS_BEGIN(EffectEditor)

namespace Internal
{
    constexpr const wchar_t* kDefaultEffectMaterialFileName = L"M_DefaultMaterial.effectmaterial.json";
    constexpr const char* kDefaultEffectMaterialName = "M_DefaultMaterial";
    constexpr const char* kDefaultEffectMaterialTextureId = "Texture_Effect_DefaultTexture";

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
        if (materialPath.empty() || !ReadPreset::Read(materialPath, data.material))
        {
            data.material.sourcePresetName = kDefaultEffectMaterialName;
            data.material.mainTextureId = kDefaultEffectMaterialTextureId;
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

        if (_modulePickerEmitterIndex.has_value() && _modulePickerEmitterIndex.value() >= _emitters.size())
            Close_ModulePicker();

        Draw_EmitterBoard();
        Draw_ModulePickerPopup();
    }

    Set_Open(isOpen);
    ImGui::End();
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
        emitter.textureId = requiredData->material.mainTextureId;
        emitter.resourceSummary = requiredData->material.mainTextureId;
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

AuthoringModule Emitter_View::Make_RequiredModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::Required;
    module.key = "Required";
    module.displayName = L"필수";
    module.summary = L"텍스처, 머티리얼";
    module.enabled = true;
    module.movable = false;
    module.removable = false;
    module.data = Internal::Make_DefaultRequiredModuleData();
    return module;
}

AuthoringModule Emitter_View::Make_SpawnModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::Spawn;
    module.key = "Spawn";
    module.displayName = L"스폰";
    module.summary = L"생성량 / 루프 기준";
    module.enabled = true;
    module.movable = false;
    module.removable = false;
    module.data = SpawnModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_LifetimeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::Lifetime;
    module.key = "Lifetime";
    module.displayName = L"수명";
    module.summary = L"파티클 수명 범위";
    module.enabled = true;
    module.movable = true;
    module.removable = true;
    module.data = LifetimeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialLocationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialLocation;
    module.key = "Initial Location";
    module.displayName = L"초기 위치";
    module.summary = L"생성 시점 위치 분포";
    module.enabled = true;
    module.movable = true;
    module.removable = true;
    module.data = InitialLocationModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_SphereLocationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::SphereLocation;
    module.key = "Sphere Location";
    module.displayName = L"구 위치";
    module.summary = L"구 표면/부피 생성 위치";
    module.enabled = true;
    module.movable = true;
    module.removable = true;
    module.data = SphereLocationModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialSizeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialSize;
    module.key = "Initial Size";
    module.displayName = L"초기 크기";
    module.summary = L"생성 시점 크기";
    module.enabled = true;
    module.movable = true;
    module.removable = true;
    module.data = InitialSizeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialVelocityModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialVelocity;
    module.key = "Initial Velocity";
    module.displayName = L"초기 속도";
    module.summary = L"생성 시점 벡터 속도";
    module.enabled = true;
    module.movable = true;
    module.removable = true;
    module.data = InitialVelocityModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialRadialVelocityModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialRadialVelocity;
    module.key = "Initial Radial Velocity";
    module.displayName = L"초기 방사성 속도";
    module.summary = L"생성 위치 기준 방사성 속도";
    module.enabled = true;
    module.movable = true;
    module.removable = true;
    module.data = InitialRadialVelocityModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_AccelerationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::Acceleration;
    module.key = "Acceleration";
    module.displayName = L"가속";
    module.summary = L"시간에 따라 더해지는 속도";
    module.enabled = true;
    module.movable = true;
    module.removable = true;
    module.data = AccelerationModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_DragModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::Drag;
    module.key = "Drag";
    module.displayName = L"드래그";
    module.summary = L"속도 감쇠";
    module.enabled = true;
    module.movable = true;
    module.removable = true;
    module.data = DragModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_VelocityOverLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::VelocityOverLife;
    module.key = "Velocity Over Life";
    module.displayName = L"수명에 따른 속도";
    module.summary = L"수명 진행률 기준 속도 배율";
    module.enabled = true;
    module.movable = true;
    module.removable = true;
    module.data = VelocityOverLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialRotationModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialRotation;
    module.key = "Initial Rotation";
    module.displayName = L"초기 회전";
    module.summary = L"생성 시점 회전각";
    module.enabled = true;
    module.movable = true;
    module.removable = true;
    module.data = InitialRotationModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_RotationOverLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::RotationOverLife;
    module.key = "Rotation Over Life";
    module.displayName = L"수명에 따른 회전";
    module.summary = L"수명 진행률 기준 추가 회전";
    module.enabled = true;
    module.movable = true;
    module.removable = true;
    module.data = RotationOverLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialRotationRateModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialRotationRate;
    module.key = "Initial Rotation Rate";
    module.displayName = L"초기 회전 속도";
    module.summary = L"초당 회전 속도";
    module.enabled = true;
    module.movable = true;
    module.removable = true;
    module.data = InitialRotationRateModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_RotationRateScaleByLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::RotationRateScaleByLife;
    module.key = "Rotation Rate Scale By Life";
    module.displayName = L"수명에 따른 회전 속도";
    module.summary = L"수명 진행률 기준 회전 속도 배율";
    module.enabled = true;
    module.movable = true;
    module.removable = true;
    module.data = RotationRateScaleByLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_InitialColorModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::InitialColor;
    module.key = "Initial Color";
    module.displayName = L"초기 컬러";
    module.summary = L"생성 시점 색상 / 알파";
    module.enabled = true;
    module.movable = true;
    module.removable = true;
    module.data = InitialColorModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_ColorOverLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::ColorOverLife;
    module.key = "Color Over Life";
    module.displayName = L"수명에 따른 색상";
    module.summary = L"수명 진행률 기준 색 변화";
    module.enabled = true;
    module.movable = true;
    module.removable = true;
    module.data = ColorOverLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_SubUVFrameOverLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::SubUVFrameOverLife;
    module.key = "SubUV Frame Over Life";
    module.displayName = L"서브UV 프레임 선택/재생";
    module.summary = L"수명 진행률 기준 frame 선택";
    module.enabled = true;
    module.movable = true;
    module.removable = true;
    module.data = SubUVFrameOverLifeModuleData{};
    return module;
}

AuthoringModule Emitter_View::Make_SizeByLifeModule()
{
    AuthoringModule module{};
    module.id = Issue_AuthoringId();
    module.type = AuthoringModuleType::SizeByLife;
    module.key = "Size By Life";
    module.displayName = L"수명에 따른 크기";
    module.summary = L"수명 진행률 기준 크기 배율";
    module.enabled = true;
    module.movable = true;
    module.removable = true;
    module.data = SizeByLifeModuleData{};
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
    case AuthoringModuleType::InitialSize:
    case AuthoringModuleType::InitialVelocity:
    case AuthoringModuleType::InitialRadialVelocity:
    case AuthoringModuleType::Acceleration:
    case AuthoringModuleType::Drag:
    case AuthoringModuleType::VelocityOverLife:
    case AuthoringModuleType::InitialRotation:
    case AuthoringModuleType::RotationOverLife:
    case AuthoringModuleType::InitialRotationRate:
    case AuthoringModuleType::RotationRateScaleByLife:
    case AuthoringModuleType::InitialColor:
    case AuthoringModuleType::ColorOverLife:
    case AuthoringModuleType::SubUVFrameOverLife:
    case AuthoringModuleType::SizeByLife:
        return !Has_ModuleType(emitter, type);

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
    case AuthoringModuleType::InitialSize:
        module = Make_InitialSizeModule();
        break;
    case AuthoringModuleType::InitialVelocity:
        module = Make_InitialVelocityModule();
        break;
    case AuthoringModuleType::InitialRadialVelocity:
        module = Make_InitialRadialVelocityModule();
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
    case AuthoringModuleType::InitialRotation:
        module = Make_InitialRotationModule();
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

void Emitter_View::Clear_Selection()
{
    _selectedEmitterIndex.reset();
    _selectedTypeData = false;
    _selectedModuleIndex.reset();

    if (EDITOR != nullptr && EDITOR->Get_EditorContext() != nullptr)
        EDITOR->Get_EditorContext()->Clear_EffectSelection();
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

void Emitter_View::Insert_NewEmitter(size_t insertIndex)
{
    Close_ModulePicker();
    const size_t clampedIndex = min(insertIndex, _emitters.size());
    _emitters.insert(_emitters.begin() + static_cast<ptrdiff_t>(clampedIndex), Make_DefaultSpriteEmitter());
    Select_Emitter(clampedIndex);
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

void Emitter_View::Queue_EmitterAction(PendingEmitterAction action, size_t emitterIndex)
{
    _pendingEmitterAction = action;
    _pendingEmitterActionIndex = emitterIndex;
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
        emitter.rendererType = data.rendererType;
        emitter.textureId = data.material.mainTextureId;
        emitter.resourceSummary = data.material.mainTextureId;
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

    case AuthoringModuleType::InitialSize:
        module.data = InitialSizeModuleData{};
        break;

    case AuthoringModuleType::InitialVelocity:
        module.data = InitialVelocityModuleData{};
        break;

    case AuthoringModuleType::InitialRadialVelocity:
        module.data = InitialRadialVelocityModuleData{};
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

    case AuthoringModuleType::InitialRotation:
        module.data = InitialRotationModuleData{};
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

    default:
        return;
    }

    emitter.previewDirty = true;
    MarkDirty();
}

void Emitter_View::Queue_ModuleAction(PendingModuleAction action, size_t emitterIndex, size_t moduleIndex)
{
    _pendingModuleAction = action;
    _pendingModuleEmitterIndex = emitterIndex;
    _pendingModuleIndex = moduleIndex;
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

vector<Emitter_View::ModulePickerEntry> Emitter_View::Build_ModulePickerEntries(const AuthoringEmitter& emitter) const
{
    const auto makeEntry = [&](
        optional<AuthoringModuleType> moduleType,
        const wchar_t* displayName,
        bool implemented,
        bool compatible,
        bool canAdd,
        string disabledReason)
    {
        ModulePickerEntry entry{};
        entry.moduleType = moduleType;
        entry.displayName = displayName;
        entry.disabledReason = std::move(disabledReason);
        entry.implemented = implemented;
        entry.compatible = compatible;
        entry.canAdd = canAdd;

        if (canAdd)
            entry.stateText = "추가 가능";
        else if (implemented && !compatible)
            entry.stateText = "현재 emitter와 비호환";
        else if (implemented)
            entry.stateText = "이미 장착됨";
        else if (!compatible)
            entry.stateText = "현재 emitter와 비호환";
        else
            entry.stateText = "미구현";

        return entry;
    };

    vector<ModulePickerEntry> entries{};
    entries.push_back(
        makeEntry(
            AuthoringModuleType::Lifetime,
            L"수명",
            true,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::Lifetime),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialSize,
            L"초기 크기",
            true,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialSize),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialLocation,
            L"초기 위치",
            true,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialLocation),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::SphereLocation,
            L"구 위치",
            true,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::SphereLocation),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialVelocity,
            L"초기 속도",
            true,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialVelocity),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialRadialVelocity,
            L"초기 방사성 속도",
            true,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialRadialVelocity),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::Acceleration,
            L"가속",
            true,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::Acceleration),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::Drag,
            L"드래그",
            true,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::Drag),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::VelocityOverLife,
            L"수명에 따른 속도",
            true,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::VelocityOverLife),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialRotation,
            L"초기 회전",
            true,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialRotation),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::RotationOverLife,
            L"수명에 따른 회전",
            true,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::RotationOverLife),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialRotationRate,
            L"초기 회전 속도",
            true,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialRotationRate),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::RotationRateScaleByLife,
            L"수명에 따른 회전 속도",
            true,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::RotationRateScaleByLife),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::InitialColor,
            L"초기 컬러",
            true,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::InitialColor),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::ColorOverLife,
            L"수명에 따른 색상",
            true,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::ColorOverLife),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::SubUVFrameOverLife,
            L"서브UV 프레임 선택/재생",
            true,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::SubUVFrameOverLife),
            "이미 장착됨"
        )
    );
    entries.push_back(
        makeEntry(
            AuthoringModuleType::SizeByLife,
            L"수명에 따른 크기",
            true,
            true,
            Can_AddModuleType(emitter, AuthoringModuleType::SizeByLife),
            "이미 장착됨"
        )
    );

    std::sort(
        entries.begin(),
        entries.end(),
        [](const ModulePickerEntry& lhs, const ModulePickerEntry& rhs)
        {
            return String::ToString(lhs.displayName) < String::ToString(rhs.displayName);
        }
    );

    return entries;
}

void Emitter_View::Open_ModulePicker(size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
        return;

    _modulePickerEmitterIndex = emitterIndex;
    _modulePickerPendingOpen = true;
    _modulePickerSearchBuffer[0] = '\0';
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

                const bool picked = ImGui::Selectable(displayName.c_str(), false, ImGuiSelectableFlags_SpanAllColumns);

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

void Emitter_View::Sync_EmitterMirror(AuthoringEmitter& emitter)
{
    for (const AuthoringModule& module : emitter.modules)
    {
        if (module.type != AuthoringModuleType::Required)
            continue;

        const RequiredModuleData* requiredData = get_if<RequiredModuleData>(&module.data);
        if (requiredData == nullptr)
            return;

        emitter.rendererType = requiredData->rendererType;
        emitter.textureId = requiredData->material.mainTextureId;
        emitter.resourceSummary = requiredData->material.mainTextureId;
        return;
    }
}

void Emitter_View::Sync_AllEmitterMirrors()
{
    for (AuthoringEmitter& emitter : _emitters)
        Sync_EmitterMirror(emitter);
}

void Emitter_View::Mark_AllPreviewDirty()
{
    for (AuthoringEmitter& emitter : _emitters)
        emitter.previewDirty = true;
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

    // 모듈 간 compatibility 경고는 후속 단계에서 이 경로에 추가한다.
    return {};
}

void Emitter_View::Draw_EmitterContextMenu(size_t emitterIndex)
{
    if (ImGui::BeginMenu("이미터"))
    {
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
        Draw_TypeDataContextMenu();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("모듈"))
    {
        if (ImGui::MenuItem("새 모듈"))
            Open_ModulePicker(emitterIndex);

        ImGui::EndMenu();
    }
}

void Emitter_View::Draw_TypeDataContextMenu()
{
    ImGui::BeginDisabled();
    ImGui::MenuItem("추가 가능한 타입 데이터 없음");
    ImGui::EndDisabled();
}

void Emitter_View::Draw_ModuleContextMenu(size_t emitterIndex, size_t moduleIndex, const AuthoringModule& module)
{
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
        float lastColumnRight = ImGui::GetCursorScreenPos().x;

        for (size_t emitterIndex = 0; emitterIndex < _emitters.size(); ++emitterIndex)
        {
            ImGui::PushID(static_cast<int>(emitterIndex));
            Draw_EmitterColumn(emitterIndex, _emitters[emitterIndex], columnHeight);
            lastColumnRight = ImGui::GetItemRectMax().x;
            ImGui::PopID();

            if (emitterIndex + 1 < _emitters.size())
                ImGui::SameLine(0.f, 0.f);
        }

        const ImVec2 mousePos = ImGui::GetMousePos();
        const bool isRightBlankHovered =
            ImGui::IsWindowHovered() &&
            !ImGui::IsAnyItemHovered() &&
            mousePos.x > lastColumnRight &&
            mousePos.x < boardMax.x &&
            mousePos.y >= boardMin.y &&
            mousePos.y < boardMax.y;

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

            ImGui::EndPopup();
        }

        Process_PendingEmitterAction();
        Process_PendingModuleAction();
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
    const AuthoringSnapshot beforeEmitterEnabledSnapshot = Capture_AuthoringSnapshot();
    if (ImGui::Checkbox("##Enabled", &emitter.enabled))
    {
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

    const bool isSelected =
        _selectedEmitterIndex.has_value() &&
        _selectedEmitterIndex.value() == emitterIndex &&
        _selectedTypeData;
    const ImVec4 accentColor = To_ImVec4(kEmitterAccentColor);
    const ImVec4 rowColor = isSelected ? accentColor : To_ImVec4(kEmitterTypeDataRowColor);
    const ImVec4 textColor = isSelected
                             ? To_ImVec4(kEmitterSelectedTextColor)
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
        Draw_TypeDataContextMenu();
        ImGui::EndPopup();
    }

    const ImVec2 afterRowCursor = ImGui::GetCursorPos();
    const float textOffsetY = (kEmitterTypeDataRowHeight - ImGui::GetTextLineHeight()) * 0.5f;
    ImGui::SetCursorPos(ImVec2(rowCursor.x + rowPaddingX, rowCursor.y + textOffsetY));
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    ImGui::TextUnformatted("타입 데이터");
    ImGui::SameLine();
    ImGui::TextDisabled("(미장착)");
    ImGui::PopStyleColor();
    ImGui::SetCursorPos(afterRowCursor);
}

void Emitter_View::Draw_ModuleRow(size_t emitterIndex, size_t moduleIndex, AuthoringModule& module)
{
    constexpr float rowHeight = 28.f;
    constexpr float rowPaddingX = 10.f;
    constexpr float moduleCheckboxRightPadding = 34.f;

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
    ImGui::PopStyleColor(4);
    if (ImGui::BeginPopupContextItem("##ModuleRowPopup"))
    {
        Draw_ModuleContextMenu(emitterIndex, moduleIndex, module);
        ImGui::EndPopup();
    }
    ImGui::PopID();
    const string warningTooltip = Build_ModuleWarningTooltip(_emitters[emitterIndex], module);

    const ImVec2 afterRowCursor = ImGui::GetCursorPos();
    const float textOffsetY = (rowHeight - ImGui::GetTextLineHeight()) * 0.5f;
    ImGui::SetCursorPos(ImVec2(rowCursor.x + rowPaddingX, rowCursor.y + textOffsetY));
    ImGui::PushStyleColor(ImGuiCol_Text, module.enabled ? textColor : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextUnformatted(String::ToString(module.displayName).c_str());
    ImGui::PopStyleColor();

    if (!isRequiredRow)
    {
        ImGui::SetCursorPos(
            ImVec2(
                rowCursor.x + kEmitterColumnWidth - moduleCheckboxRightPadding,
                rowCursor.y + (rowHeight - ImGui::GetFrameHeight()) * 0.5f
            )
        );
        ImGui::PushID("ModuleEnabled");
        const AuthoringSnapshot beforeModuleEnabledSnapshot = Capture_AuthoringSnapshot();
        if (ImGui::Checkbox("##Enabled", &module.enabled))
        {
            _emitters[emitterIndex].previewDirty = true;
            MarkDirty();
            const AuthoringSnapshot afterSnapshot = Capture_AuthoringSnapshot();
            Execute_AuthoringSnapshotCommand(beforeModuleEnabledSnapshot, afterSnapshot, "Toggle Module Enabled");
        }
        ImGui::PopID();
    }

    ImGui::SetCursorPos(afterRowCursor);

    if (isRowHovered && !warningTooltip.empty())
        ImGui::SetTooltip("%s", warningTooltip.c_str());
}

Shared<Emitter_View> Emitter_View::Create()
{
    return make_shared<Emitter_View>();
}

NS_END
