#pragma once

#include "Editor_Window.h"
#include "BattleCameraClipTypes.h"
#include "CameraPresetTypes.h"

struct ImVec2;

NS_BEGIN(Engine)
class GameObject;
class ModelCom;
class RenderTarget;
NS_END

NS_BEGIN(Client)
class SoulLikeCamera;
class SkyBoxFlow;
class SkyCloudCard;
NS_END

NS_BEGIN(MainEditor)

class Editor_Camera_Free;

class SoulLikeCamera_Viewer final : public Editor_Window
{
public:
    SoulLikeCamera_Viewer();
    ~SoulLikeCamera_Viewer() override = default;

public:
    void Update(float timeDelta) override;
    void Pre_Render() override;
    void Render() override;
    bool CanSave() const override { return _presetDirty || _clipDirty; }
    void Save() override;

public:
    enum class CameraProfileType : uint8
    {
        Base,
        PlayerSkill,
        MonsterAttack,
        Parry,
        PlayerHit,
        JumpCounter,
    };

private:
    void Refresh_SkillList();
    void Refresh_PlayerAnimationList();
    void Refresh_MonsterAnimationList();

    void Ensure_PreviewRenderTarget(uint32 width, uint32 height);
    void Ensure_PreviewCamera();
    void Ensure_PreviewActors();
    void Ensure_PreviewSkyBackdrop();
    void Clear_PreviewActors();

    void Render_Header();
    void Render_LeftPanel();
    void Render_PreviewCanvas();
    void Render_PresetPanel();
    void Render_TimelinePanel();
    void Render_ClipTimelinePanel();
    void Update_PreviewCameraControls(const ImVec2& previewMin, const ImVec2& previewMax);
    void Render_ClipSequencer();
    void Render_SelectedClipKeyEditor();
    void Handle_ClipShortcuts();

    void Load_CurrentPreset(bool loadBaseOnly = false);
    void Save_CurrentPreset();
    string Build_CurrentPresetKey() const;
    void Load_CurrentClip();
    void Save_CurrentClip();
    void Reset_CurrentClip();
    string Build_CurrentClipKey() const;
    void Render_PresetBrowser();
    void Refresh_PresetBrowser();
    void Load_BrowserPreset(const string& presetKey);
    void Apply_BrowserPresetContext(const string& presetKey);

    Shared<Client::SoulLikeCamera> Resolve_RuntimeSoulLikeCamera() const;
    void Capture_RuntimePreset();
    bool Capture_RuntimeAnimationPose();
    void Apply_RuntimePreset();
    void Reset_CurrentPresetFromBase();
    bool Load_CurrentPresetFile(TargetCameraPreset& outPreset) const;
    bool Current_PresetFileExists() const;
    void Toggle_SavedComparison();
    void Save_CopyPreset();
    void Set_PreviewPose(float monsterDistance, float monsterSideOffset);
    bool Sync_CurrentRuntimeSkill();
    bool Is_CurrentProfileRuntimeConnected() const;
    void Push_UndoSnapshot(const TargetCameraPreset& preset);
    void Undo_Preset();
    void Redo_Preset();
    int Get_ClipFrameMax() const;
    float Get_ClipTimeFromFrame(int frame) const;
    int Get_ClipFrameFromTime(float timeSec) const;
    void Sync_ClipFrameFromPreviewTime();
    void Sync_PreviewTimeFromClipFrame();
    CameraTrack* Get_ClipTrack(int trackIndex);
    const CameraTrack* Get_ClipTrack(int trackIndex) const;
    float Get_CurrentClipTrackValue(int trackIndex) const;
    void Sort_ClipTrack(CameraTrack& track);
    void Insert_ClipKeyAtFrame(int trackIndex, int frame);
    void Insert_CurrentPoseKeysAtFrame(int frame);
    bool Update_CurrentPoseKeysAtFrame(int frame);
    void Remove_SelectedClipKey();
    bool Has_ClipTrackKeys(const CameraTrack& track, bool enabled) const;

    void Apply_PreviewAnimation(float timeDelta);
    void Apply_SoulLikeCameraPreviewPose();
    void Position_PreviewActors() const;
    void Render_PreviewScene(float timeDelta);
    float Resolve_CurrentAnimationDurationSec() const;
    int Resolve_CurrentAnimationFps() const;

    Shared<ModelCom> Resolve_PlayerModel() const;
    Shared<ModelCom> Resolve_MonsterModel() const;
    static Shared<ModelCom> Resolve_PrimaryModel(const Shared<GameObject>& object);

private:
    CameraProfileType _profileType = CameraProfileType::Base;

    string _playerPrefab = "SL_Maelle";
    string _monsterPrefab = "SL_Dualliste";

    vector<string> _skillKeys{};
    int _selectedSkillIndex = -1;

    vector<string> _playerAnimationNames{};
    int _selectedPlayerAnimationIndex = -1;

    vector<string> _monsterAnimationNames{};
    int _selectedMonsterAnimationIndex = -1;

    TargetCameraPreset _editingPreset{};
    bool _presetDirty = false;
    string _loadedPresetKey{};
    BattleCameraClip _editingClip{ 1.f };
    bool _clipDirty = false;
    int _clipCurrentFrame = 0;
    int _clipSelectedTrackIndex = 0;
    int _clipSelectedKeyIndex = -1;
    int _clipContextTrackIndex = 0;
    int _clipContextFrame = 0;
    string _loadedClipKey{};
    bool _clipDistanceEnabled = true;
    bool _clipFovEnabled = true;
    bool _clipShakeEnabled = true;
    bool _clipSideEnabled = true;
    bool _clipPivotEnabled = true;
    bool _clipHeightEnabled = true;
    bool _clipAimEnabled = true;
    bool _clipLookHeightEnabled = true;
    bool _clipYawEnabled = true;
    bool _clipOrbitYawEnabled = false;
    bool _clipOrbitRadiusEnabled = false;
    bool _clipOrbitHeightEnabled = false;
    bool _clipOrbitLookHeightEnabled = false;
    bool _clipShakeFrequencyXEnabled = false;
    bool _clipShakeFrequencyYEnabled = false;
    bool _clipShakeDecayRateXEnabled = false;
    bool _clipShakeDecayRateEnabled = false;
    bool _clipShakeVerticalRatioEnabled = false;
    bool _clipShakeStrengthScaleEnabled = false;
    string _statusMessage{};
    bool _runtimeFollowEnabled = false;
    bool _autoSyncRuntimeSkill = false;
    bool _viewingSavedPreset = false;
    TargetCameraPreset _compareEditPreset{};
    bool _comparePresetDirty = false;
    string _copyPresetKey{};
    vector<TargetCameraPreset> _undoStack{};
    vector<TargetCameraPreset> _redoStack{};

    vector<string> _presetBrowserKeys{};
    string _presetBrowserSearch{};
    int _selectedPresetBrowserIndex = -1;
    bool _presetBrowserDirty = true;

    float _previewTimeSec = 0.f;
    bool _timelinePlaying = false;
    float _previewMonsterDistance = 4.f;
    float _previewMonsterSideOffset = 0.f;

    uint32 _previewWidth = 1;
    uint32 _previewHeight = 1;
    float _topSectionHeight = 420.f;

    bool _previewActorsDirty = true;
    bool _previewObjectsBegunPlay = false;
    bool _playerAnimationListDirty = true;
    bool _monsterAnimationListDirty = true;
    bool _previewCameraControlActive = false;
    TargetCameraPreset _previewCameraControlStartPreset{};

    Shared<RenderTarget> _previewRenderTarget{};
    Shared<Editor_Camera_Free> _previewCamera{};
    Shared<GameObject> _previewPlayer{};
    Shared<GameObject> _previewMonster{};
    Shared<Client::SkyBoxFlow> _previewSkyBox{};
    vector<Shared<Client::SkyCloudCard>> _previewSkyCloudCards{};

public:
    static Shared<SoulLikeCamera_Viewer> Create();
};

NS_END
