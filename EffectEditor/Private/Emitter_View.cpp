#include "Emitter_View.h"

#include "Action_Command.h"
#include "Editor_Context.h"
#include "EffectEditorInstance.h"
#include "Helper_ImGui.h"

NS_BEGIN(EffectEditor)

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

        Draw_EmitterBoard();
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
    emitter.modules.push_back(Make_InitialSizeModule());
    emitter.modules.push_back(Make_InitialVelocityModule());
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
    module.data = RequiredModuleData{};
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
    module.summary = L"생성 위치와 퍼짐 속도";
    module.enabled = true;
    module.movable = true;
    module.removable = true;
    module.data = InitialVelocityModuleData{};
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
    module.enabled = false;
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
    module.displayName = L"서브 이미지 인덱스";
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
    case AuthoringModuleType::InitialSize:
    case AuthoringModuleType::InitialVelocity:
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
    case AuthoringModuleType::InitialSize:
        module = Make_InitialSizeModule();
        break;
    case AuthoringModuleType::InitialVelocity:
        module = Make_InitialVelocityModule();
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
    if (emitterIndex >= _emitters.size() || _emitters[emitterIndex].typeData == AuthoringEmitterTypeData::None)
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
    const size_t clampedIndex = min(insertIndex, _emitters.size());
    _emitters.insert(_emitters.begin() + static_cast<ptrdiff_t>(clampedIndex), Make_DefaultSpriteEmitter());
    Select_Emitter(clampedIndex);
    MarkDirty();
}

void Emitter_View::Duplicate_Emitter(size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
        return;

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

void Emitter_View::Set_GpuSpriteTypeData(size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
        return;

    AuthoringEmitter& emitter = _emitters[emitterIndex];
    if (emitter.typeData == AuthoringEmitterTypeData::GpuSprite)
        return;

    emitter.typeData = AuthoringEmitterTypeData::GpuSprite;
    emitter.previewDirty = true;
    Select_TypeData(emitterIndex);
    MarkDirty();
}

void Emitter_View::Reset_TypeData(size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
        return;

    AuthoringEmitter& emitter = _emitters[emitterIndex];
    if (emitter.typeData == AuthoringEmitterTypeData::None)
        return;

    switch (emitter.typeData)
    {
    case AuthoringEmitterTypeData::GpuSprite:
        emitter.gpuSpriteTypeData = GpuSpriteTypeData{};
        break;
    case AuthoringEmitterTypeData::None:
    default:
        return;
    }

    emitter.previewDirty = true;
    Select_TypeData(emitterIndex);
    MarkDirty();
}

void Emitter_View::Delete_TypeData(size_t emitterIndex)
{
    if (emitterIndex >= _emitters.size())
        return;

    AuthoringEmitter& emitter = _emitters[emitterIndex];
    if (emitter.typeData == AuthoringEmitterTypeData::None)
        return;

    emitter.typeData = AuthoringEmitterTypeData::None;
    emitter.gpuSpriteTypeData = GpuSpriteTypeData{};
    emitter.previewDirty = true;
    Select_Emitter(emitterIndex);
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
    const bool isSelectedModule = isSelectedEmitter && !_selectedTypeData && _selectedModuleIndex.has_value();
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
        RequiredModuleData data{};
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

    case AuthoringModuleType::InitialSize:
        module.data = InitialSizeModuleData{};
        break;

    case AuthoringModuleType::InitialVelocity:
        module.data = InitialVelocityModuleData{};
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

    const AuthoringEmitter& emitter = _emitters[_selectedEmitterIndex.value()];
    if (_selectedTypeData && emitter.typeData == AuthoringEmitterTypeData::None)
        _selectedTypeData = false;

    const auto& modules = _emitters[_selectedEmitterIndex.value()].modules;
    if (_selectedModuleIndex.has_value() && _selectedModuleIndex.value() >= modules.size())
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
    if (_selectedTypeData && emitter.typeData != AuthoringEmitterTypeData::None)
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

    // TypeData/module compatibility 경고는 후속 단계에서 이 경로에 추가한다.
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
        ImGui::BeginDisabled();
        ImGui::MenuItem("새 애님 트레일 데이터");
        ImGui::MenuItem("새 빔 데이터");
        ImGui::EndDisabled();

        const bool canSetGpuSprite = _emitters[emitterIndex].typeData != AuthoringEmitterTypeData::GpuSprite;
        if (!canSetGpuSprite)
            ImGui::BeginDisabled();

        if (ImGui::MenuItem("새 GPU 스프라이트"))
        {
            Execute_AuthoringEdit(
                "Set GPU Sprite TypeData",
                [this, emitterIndex]
                {
                    Set_GpuSpriteTypeData(emitterIndex);
                }
            );
        }

        if (!canSetGpuSprite)
            ImGui::EndDisabled();

        ImGui::BeginDisabled();
        ImGui::MenuItem("새 메시 데이터");
        ImGui::MenuItem("새 리본 데이터");
        ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("모듈"))
    {
        // TODO : + 모듈 추가 버튼 -> 팝업
        ImGui::EndMenu();
    }
}

void Emitter_View::Draw_TypeDataContextMenu(size_t emitterIndex)
{
    if (ImGui::MenuItem("모듈 값 초기화"))
    {
        Execute_AuthoringEdit(
            "Reset TypeData",
            [this, emitterIndex]
            {
                Reset_TypeData(emitterIndex);
            }
        );
    }

    if (ImGui::MenuItem("모듈 삭제"))
    {
        Execute_AuthoringEdit(
            "Delete TypeData",
            [this, emitterIndex]
            {
                Delete_TypeData(emitterIndex);
            }
        );
    }
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
        _selectedEmitterIndex.value() == emitterIndex &&
        !_selectedTypeData;
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
    Draw_TypeDataRow(emitterIndex, emitter);
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

void Emitter_View::Draw_TypeDataRow(size_t emitterIndex, const AuthoringEmitter& emitter)
{
    constexpr float rowPaddingX = 10.f;

    if (emitter.typeData != AuthoringEmitterTypeData::GpuSprite)
    {
        ImGui::InvisibleButton("##EmitterTypeDataSlot", ImVec2(kEmitterColumnWidth, kEmitterTypeDataReserveHeight));
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            Select_Emitter(emitterIndex);
        if (ImGui::BeginPopupContextItem("##EmitterTypeDataPopup"))
        {
            Draw_EmitterContextMenu(emitterIndex);
            ImGui::EndPopup();
        }
        return;
    }

    const bool isSelected =
        _selectedEmitterIndex.has_value() &&
        _selectedEmitterIndex.value() == emitterIndex &&
        _selectedTypeData;
    const ImVec4 rowColor = isSelected ? To_ImVec4(kEmitterAccentColor) : To_ImVec4(kEmitterTypeDataRowColor);
    const ImVec4 textColor = isSelected ? To_ImVec4(kEmitterSelectedTextColor) : ImGui::GetStyleColorVec4(ImGuiCol_Text);

    const ImVec2 rowCursor = ImGui::GetCursorPos();
    ImGui::PushStyleColor(ImGuiCol_Button, rowColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rowColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, rowColor);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    ImGui::Button("##EmitterTypeDataRow", ImVec2(kEmitterColumnWidth, kEmitterTypeDataReserveHeight));
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
        Select_TypeData(emitterIndex);
    ImGui::PopStyleColor(4);

    if (ImGui::BeginPopupContextItem("##EmitterTypeDataRowPopup"))
    {
        Draw_TypeDataContextMenu(emitterIndex);
        ImGui::EndPopup();
    }

    const ImVec2 afterRowCursor = ImGui::GetCursorPos();
    const float textOffsetY = (kEmitterTypeDataReserveHeight - ImGui::GetTextLineHeight()) * 0.5f;
    ImGui::SetCursorPos(ImVec2(rowCursor.x + rowPaddingX, rowCursor.y + textOffsetY));
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    ImGui::TextUnformatted("GPU 스프라이트");
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
