#pragma once

#include "Editor_Window.h"
#include "EffectAuthoring_Types.h"

NS_BEGIN(EffectEditor)

class Emitter_View final : public Editor_Window
{
public:
    struct AuthoringSnapshot
    {
        ParticleSystemAuthoringData particleSystemData{};
        vector<AuthoringEmitter> emitters{};
        optional<size_t> selectedEmitterIndex{};
        bool selectedTypeData{ false };
        optional<size_t> selectedModuleIndex{};
        uint32 nextAuthoringId{ 1 };
    };

public:
    Emitter_View();
    ~Emitter_View() override = default;

public:
    HRESULT Initialize() override;
    void Render() override;

    bool CanSave() const override; // 현재 effect authoring document가 Save 대상임을 알린다.
    void Save() override;          // Save As dialog를 열어 emitter/module authoring json을 저장한다.
    bool CanOpen() const;          // 현재 effect authoring document가 Open 대상으로 교체 가능함을 알린다.
    void Open();                   // Open dialog로 .effect.json을 열어 authoring 상태를 복원한다.
    bool CanAdd() const;           // 다른 effect authoring document의 emitter들을 현재 문서에 추가할 수 있는지 알린다.
    void Add();                    // Add dialog로 .effect.json의 emitter stack만 현재 문서 끝에 추가한다.

public: //## Accessor::Authoring
    ParticleSystemAuthoringData& Get_ParticleSystemData() { return _particleSystemData; }
    const ParticleSystemAuthoringData& Get_ParticleSystemData() const { return _particleSystemData; }
    const vector<AuthoringEmitter>& Get_Emitters() const { return _emitters; }
    const optional<fs::path>& Get_CurrentDocumentPath() const { return _currentDocumentPath; }
    AuthoringEmitter* Find_Emitter(uint32 emitterId);
    AuthoringModule* Find_Module(uint32 emitterId, uint32 moduleId);
    AuthoringEmitter* Get_SelectedOrFirstEmitter();
    void Clear_PreviewDirty(uint32 emitterId);
    bool Has_PreviewDirtyEmitter() const;

public: //## Behavior::AuthoringHistory
    AuthoringSnapshot Capture_AuthoringSnapshot() const;
    void Restore_AuthoringSnapshot(const AuthoringSnapshot& snapshot);
    void Execute_AuthoringSnapshotCommand(
        const AuthoringSnapshot& beforeSnapshot,
        const AuthoringSnapshot& afterSnapshot,
        const string& description);
    json Build_AuthoringDocumentJson() const;
    bool Restore_AuthoringDocumentJson(const json& root);

private: //## Types::PendingAction
    enum class PendingEmitterAction
    {
        None,
        Duplicate,
        Delete,
        InsertBefore,
        InsertAfter
    };

    enum class PendingModuleAction
    {
        None,
        Delete,
        ResetData
    };

    enum class PendingTypeDataAction
    {
        None,
        AddTrail,
        AddMesh,
        AddRibbon,
        AddSourceHistoryRibbon,
        ResetData,
        Remove
    };

private: //## Types::ModulePicker
    struct ModulePickerEntry
    {
        optional<AuthoringModuleType> moduleType{};
        wstring displayName{};
        string stateText{};
        string disabledReason{};
        bool implemented{ false };
        bool compatible{ true };
        bool canAdd{ false };
    };

private: //## Static Values
    inline static const Color kEmitterAccentColor{ 1.0f, 0.55f, 0.20f, 1.0f };
    inline static const Color kEmitterColumnBackgroundColor{ 0.06f, 0.065f, 0.08f, 1.0f };
    inline static const Color kEmitterDividerColor{ 0.015f, 0.018f, 0.025f, 1.0f };
    inline static const Color kEmitterHeaderColor{ 0.36f, 0.36f, 0.36f, 1.0f };
    inline static const Color kEmitterSelectedTextColor{ 0.05f, 0.05f, 0.05f, 1.0f };
    inline static const Color kEmitterCategoryTextColor{ 0.08f, 0.08f, 0.08f, 1.0f };
    inline static const Color kEmitterTypeDataRowColor{ 0.45f, 0.75f, 0.50f, 1.0f };
    inline static const Color kEmitterRequiredRowColor{ 0.9f, 0.35f, 0.35f, 1.0f };
    inline static const Color kEmitterSpawnRowColor{ 0.75f, 0.75f, 0.3f, 1.0f };
    inline static const Color kEmitterCommonModuleRowColor{ 0.18f, 0.19f, 0.24f, 1.0f };
    inline static const Color kEmitterModuleDisabledRowColor{ 0.16f, 0.16f, 0.17f, 1.0f };
    inline static const Color kEmitterUnsupportedModuleTextColor{ 0.9f, 0.25f, 0.25f, 1.0f };

    static constexpr float kEmitterColumnWidth{ 270.f };
    static constexpr float kEmitterHeaderHeight{ 90.f };
    static constexpr float kEmitterTypeDataRowHeight{ 28.f };
    static constexpr float kEmitterDividerWidth{ 1.f };
    static constexpr float kEmitterRightBlankWidth{ 96.f };

    static constexpr float kModulePickerModalWidth{ 700.f };
    static constexpr float kModulePickerModalHeight{ 460.f };
    static constexpr float kModulePickerNameColumnStretch{ 0.5f };
    static constexpr float kModulePickerStateColumnStretch{ 1.f - kModulePickerNameColumnStretch };

private: //## Data::Authoring
    ParticleSystemAuthoringData _particleSystemData{};
    // 현재 편집 중인 effect의 emitter 목록이다.
    vector<AuthoringEmitter> _emitters{};
    // 현재 authoring 문서가 기존 effect 파일과 연결되어 있으면 그 경로를 보관한다.
    optional<fs::path> _currentDocumentPath{};
    // 저장용 id가 아니라 editor 세션 안에서 emitter/module을 구분하기 위한 임시 id 발급값이다.
    uint32 _nextAuthoringId{ 1 };

private: //## Data::Selection
    // 현재 선택된 emitter index다. 값이 없으면 emitter 선택이 해제된 상태다.
    optional<size_t> _selectedEmitterIndex{};
    // 현재 선택 대상이 emitter-level TypeData 슬롯인지 나타낸다.
    bool _selectedTypeData{ false };
    // 현재 선택된 emitter 내부의 module index다. 값이 없으면 emitter header만 선택된 상태다.
    optional<size_t> _selectedModuleIndex{};

private: //## Data::PendingAction
    PendingEmitterAction _pendingEmitterAction{ PendingEmitterAction::None };
    optional<size_t> _pendingEmitterActionIndex{};
    PendingModuleAction _pendingModuleAction{ PendingModuleAction::None };
    optional<size_t> _pendingModuleEmitterIndex{};
    optional<size_t> _pendingModuleIndex{};
    PendingTypeDataAction _pendingTypeDataAction{ PendingTypeDataAction::None };
    optional<size_t> _pendingTypeDataEmitterIndex{};

private: //## Data::ModulePicker
    optional<size_t> _modulePickerEmitterIndex{};
    bool _modulePickerPendingOpen{ false };
    char _modulePickerSearchBuffer[128]{};

private: //## Helper::AuthoringFactory
    // 새 emitter/module에 부여할 editor-local id를 발급한다.
    uint32 Issue_AuthoringId();
    // Cascade의 기본 더미 emitter 하나를 만든다.
    AuthoringEmitter Make_DefaultSpriteEmitter();
    AuthoringEmitter Clone_Emitter(const AuthoringEmitter& sourceEmitter);
    AuthoringModule Make_DefaultModuleByType(AuthoringModuleType type);
    AuthoringModule Make_RequiredModule();
    AuthoringModule Make_SpawnModule();
    AuthoringModule Make_LifetimeModule();
    AuthoringModule Make_InitialLocationModule();
    AuthoringModule Make_SphereLocationModule();
    AuthoringModule Make_PlaneRadialLocationModule();
    AuthoringModule Make_InitialSizeModule();
    AuthoringModule Make_InitialMeshSizeModule();
    AuthoringModule Make_InitialVelocityModule();
    AuthoringModule Make_InitialRadialVelocityModule();
    AuthoringModule Make_VelocityConeModule();
    AuthoringModule Make_AccelerationModule();
    AuthoringModule Make_DragModule();
    AuthoringModule Make_VelocityOverLifeModule();
    AuthoringModule Make_OrbitOverLifeModule();
    AuthoringModule Make_InitialRotationModule();
    AuthoringModule Make_SphereRadialOrientationModule();
    AuthoringModule Make_PlaneRadialOrientationModule();
    AuthoringModule Make_RotationOverLifeModule();
    AuthoringModule Make_InitialRotationRateModule();
    AuthoringModule Make_RotationRateScaleByLifeModule();
    AuthoringModule Make_InitialMeshRotationModule();
    AuthoringModule Make_MeshRotationOverLifeModule();
    AuthoringModule Make_InitialMeshRotationRateModule();
    AuthoringModule Make_MeshRotationRateScaleByLifeModule();
    AuthoringModule Make_InitialColorModule();
    AuthoringModule Make_ColorOverLifeModule();
    AuthoringModule Make_SubUVFrameOverLifeModule();
    AuthoringModule Make_SizeByLifeModule();
    AuthoringModule Make_MeshSizeByLifeModule();
    AuthoringModule Make_SpawnPerUnitModule();
    AuthoringModule Make_MaterialScalarModulationModule();

private: //## Helper::ModulePolicy
    // 이미터가 특정 모듈 타입을 이미 가지고 있는지 확인한다.
    bool Has_ModuleType(const AuthoringEmitter& emitter, AuthoringModuleType type) const;
    // 현재 emitter 타입이 모듈을 preview에 소비하는지 확인한다.
    bool Is_ModuleCompatibleWithEmitter(const AuthoringEmitter& emitter, AuthoringModuleType type) const;
    // 중복 정책 기준으로 새 모듈 추가 가능 여부를 판단한다.
    bool Can_AddModuleType(const AuthoringEmitter& emitter, AuthoringModuleType type) const;
    // 선택 이미터에 새 optional 모듈을 추가하고 선택 상태를 갱신한다.
    void Add_Module(size_t emitterIndex, AuthoringModuleType type);

private: //## Helper::Selection
    void Clear_Selection();
    void Clamp_Selection(); // 현재 선택 index가 emitter 목록 범위를 벗어나지 않도록 보정한다.
    void Select_Emitter(size_t emitterIndex);
    void Select_Module(size_t emitterIndex, size_t moduleIndex);
    void Select_TypeData(size_t emitterIndex);
    void Sync_SelectionContext();

private: //## Helper::EmitterAction
    void Delete_Emitter(size_t emitterIndex);
    void Duplicate_Emitter(size_t emitterIndex);
    void Append_ImportedEmitters(const vector<AuthoringEmitter>& sourceEmitters);
    void Insert_NewEmitter(size_t insertIndex);
    void Process_PendingEmitterAction();
    void Queue_EmitterAction(PendingEmitterAction action, size_t emitterIndex);

private: //## Helper::ModuleAction
    void Delete_Module(size_t emitterIndex, size_t moduleIndex);
    void Process_PendingModuleAction();
    void Queue_ModuleAction(PendingModuleAction action, size_t emitterIndex, size_t moduleIndex);
    void Reset_ModuleData(size_t emitterIndex, size_t moduleIndex);

private: //## Helper::TypeDataAction
    void Apply_TypeDataAction(PendingTypeDataAction action, size_t emitterIndex);
    void Process_PendingTypeDataAction();
    void Queue_TypeDataAction(PendingTypeDataAction action, size_t emitterIndex);

private: //## Helper::PreviewState
    void Mark_AllPreviewDirty();
    void Sync_EmitterMirror(AuthoringEmitter& emitter);
    void Sync_AllEmitterMirrors();

private: //## Helper::ModulePicker
    vector<ModulePickerEntry> Build_ModulePickerEntries(const AuthoringEmitter& emitter) const;
    void Close_ModulePicker();
    void Draw_ModulePickerPopup();
    void Open_ModulePicker(size_t emitterIndex);

private: //## Helper::AuthoringHistory
    void Execute_AuthoringEdit(const string& description, const function<void()>& edit);

private: //## Helper::Render
    // hover tooltip은 설명 dump가 아니라 경고성 사유만 노출하는 채널로 유지한다.
    string Build_ModuleWarningTooltip(const AuthoringEmitter& emitter, const AuthoringModule& module) const;
    void Draw_EmitterContextMenu(size_t emitterIndex);
    void Draw_TypeDataContextMenu(size_t emitterIndex);
    void Draw_ModuleContextMenu(size_t emitterIndex, size_t moduleIndex, const AuthoringModule& module);

    void Draw_EmitterBoard();
    void Draw_EmitterColumn(size_t emitterIndex, AuthoringEmitter& emitter, float columnHeight);
    void Draw_TypeDataRow(size_t emitterIndex);
    void Draw_ModuleRow(size_t emitterIndex, size_t moduleIndex, AuthoringModule& module);

public:
    static Shared<Emitter_View> Create();
};

NS_END
