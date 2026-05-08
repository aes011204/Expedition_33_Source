#pragma once

#include "Editor_Window.h"
#include "AnimNotify_Types.h"

NS_BEGIN(Engine)
class ModelCom;
NS_END

NS_BEGIN(MainEditor)

class AnimSequencerAdapter;

class Animation_View final : public Editor_Window
{
    friend class AnimSequencerAdapter;

public:
    enum class SourceMode : uint8
    {
        None,
        AssetOnly,
        LiveModel
    };

public:
    Animation_View();
    ~Animation_View() override = default;

public:
    void Update(float timeDelta) override;
    void Render() override;
    bool CanSave() const override;
    void Save() override;

public:
    // Content Browser에서 .model / .anim 기준으로 열 때
    void Open_FromAsset(const wstring& modelPath, const string& modelGuid, const string& preferredAnimationName = "");

    // Prefab / Scene의 실제 ModelCom를 대상으로 열 때
    void Open_FromModelComponent(const Shared<ModelCom>& modelCom, const string& preferredAnimationName = "");

public:
    const string& Get_ModelGuid() const { return _modelGuid; }
    const string& Get_CurrentAnimationName() const { return _currentAnimationName; }

    int32 Get_CurrentAnimationFps() const;
    float Get_CurrentAnimationLengthSec() const;

    void MarkDirty() { _isDirty = true; }

private:
    void Refresh_Source();
    void Refresh_AssetAnimationList();
    void Refresh_LiveAnimationList();

    void Load_NotifyAsset();
    void Save_NotifyAsset();

    void Draw_Header();
    void Draw_AnimationSelector();
    void Draw_PlaybackToolbar();
    void Draw_Sequencer();
    void Draw_SelectedDetail();

    void Add_Notify_AtCurrentTime();
    void Add_State_AtCurrentTime();
    void Delete_Selected();

    AnimNotifyData* Get_CurrentAnimation();
    const AnimNotifyData* Get_CurrentAnimation() const;

    AnimNotifyEventEntry* Find_SelectedNotify();
    AnimNotifyStateEntry* Find_SelectedState();

private:
    SourceMode _sourceMode = SourceMode::None;

    wstring _sourceModelPath{};
    Shared<ModelCom> _liveModelCom{};

    string _modelGuid{};
    string _currentAnimationName{};
    vector<string> _availableAnimationNames{};

    AnimNotifyAsset _asset{};
    bool _isDirty = false;

    int32 _selectedNotifyId = -1;
    int32 _selectedStateId = -1;

    int32 _sequencerCurrentFrame = 0;
    bool _sequencerExpanded = true;
    int32 _sequencerSelectedRow = -1;
    int32 _sequencerFirstFrame = 0;

    Unique<AnimSequencerAdapter> _sequencer{};

public:
    static Shared<Animation_View> Create();
};

NS_END
