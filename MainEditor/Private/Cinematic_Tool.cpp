#include "Cinematic_Tool.h"
#include "BattleCameraClipIO.h"
#include "BattleClipSequencerAdapter.h"
#include "Cinematic_Player.h"
#include "Cinematic_Utils.h"
#include "FreeCamera.h"
#include "BattleTurnCamera.h"
#include "BattleController.h"
#include "ClientInstance.h"
#include "Character.h"
#include "GameInstance.h"
#include "ModelCom.h"
#include "PartObject.h"
#include <algorithm>
#include <cmath>
#include "CinematicSequencerAdapter.h"
#include "Cinematic_SequenceIO.h"
#include "EditorInstance.h"
#include "magic_enum/magic_enum.hpp"
#include "BattleAction.h"
#include "CinematicCamera.h"

namespace
{
constexpr float kBattleClipPreviewFps = 30.f;

int Get_BattleClipFrameMax(const BattleCameraClip& clip)
{
    return std::max(0, static_cast<int>(std::lround(clip.duration * kBattleClipPreviewFps)));
}

void Draw_SectionHeader(const char* title)
{
    ImGui::Spacing();
    ImGui::TextDisabled("%s", title);
    ImGui::Separator();
}

    template <typename TEnum>
void Select_PresetEnum(const char* label, TEnum& currentValue, const float width = 180.f)
{

    ImGui::SetNextItemWidth(width);

    const char* currentLabel = magic_enum::enum_name(currentValue).data();
    if (!ImGui::BeginCombo(label, currentLabel))
        return;

    for (TEnum value : magic_enum::enum_values<TEnum>())
    {
        const bool isSelected = (currentValue == value);
        if (ImGui::Selectable(magic_enum::enum_name(value).data(), isSelected))
            currentValue = value;

        if (isSelected)
            ImGui::SetItemDefaultFocus();
    }

    ImGui::EndCombo();
}

}

Cinematic_Tool::Cinematic_Tool()
    : Editor_Window{ L"Cinematic Tool" }
{
    Ensure_DefaultTrack();
}

HRESULT Cinematic_Tool::Initialize()
{
    //툴에서도 플레이어 생성
    _cinematicPlayer = Cinematic_Player::Create();

    return S_OK;
}

void Cinematic_Tool::Update(float timeDelta)
{
    _cinematicPlayer->Update(timeDelta);

    bool wasPlaying = _wasCinematicPlaying;
    bool isPlaying = _cinematicPlayer->Is_CinematicPlaying();

    if (isPlaying)
    {
        const int playbackFrame = _cinematicPlayer->Get_CurrentFrame();
        if (_currentFrame != playbackFrame)
        {
            _currentFrame = playbackFrame;
            _isPreviewDirty = true;
        }
    }

    if (_isBattleClipPreviewDirty)
    {
        Apply_CurrentBattleClipPreview();
        _isBattleClipPreviewDirty = false;

        return;
    }

    if(_isPreviewDirty && !_hasPendingInsertRequest)
    {
        Apply_CurrentFramePose();
        _isPreviewDirty = false;
    }

    _wasCinematicPlaying = isPlaying;

    if( wasPlaying&& !isPlaying)
    {
        CLIENT->Restore_PreviousCamera();
        _isPreviewDirty = true;
    }


}

void Cinematic_Tool::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string windowName = String::ToString(Get_WindowName());

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

        if (ImGui::BeginTabBar("##CinematicToolTabs"))
        {
            if (ImGui::BeginTabItem("Cinematic"))
            {
                Render_CinematicTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Battle Clip"))
            {
                Render_BattleClipTab();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

    Set_Open(isOpen);
    ImGui::End();
}

void Cinematic_Tool::Render_CinematicTab()
{
    Clear_BattleClipPreviewState();
    Ensure_DefaultTrack();
    Clamp_Selection();

    if (ImGui::BeginTable("##CinematicToolLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Timeline", ImGuiTableColumnFlags_WidthStretch, 2.2f);
        ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthStretch, 1.0f);

        ImGui::TableNextColumn();
        ImGui::BeginChild("##SequencerPanel", ImVec2(0.f, 0.f), true);
        Draw_SectionHeader("Timeline");
        ImGui::TextDisabled("Left click : select / drag keyframe");
        ImGui::TextDisabled("Double click : expand selected key editor");
        ImGui::TextDisabled("Right click row : insert keyframe at clicked frame");
        ImGui::Spacing();
        Render_ImSequencer();
        ImGui::EndChild();

        ImGui::TableNextColumn();
        ImGui::BeginChild("##CinematicRightPanel", ImVec2(0.f, 0.f), false);
        Render_RightPanel();
        ImGui::EndChild();

        ImGui::EndTable();
    }

    Render_InsertKeyframePopup();
}

void Cinematic_Tool::Render_BattleClipTab()
{
    const string selectedSkillId = Get_SelectedBattleClipSkillId();

    if (!_hasLoadedBattleClip ||
        _loadedBattleClipMode != _selectedBattleClipMode ||
        _loadedBattleClipSkillId != selectedSkillId)
    {
        BattleCameraClipIO::LoadBattleCameraClipFromFile(_selectedBattleClipMode, _editingBattleClip, selectedSkillId);
        _loadedBattleClipMode = _selectedBattleClipMode;
        _loadedBattleClipSkillId = selectedSkillId;
        _hasLoadedBattleClip = true;
        _isBattleClipPreviewDirty = true;
    }

    Render_BattleClipHeader();
    ImGui::Separator();

    if (ImGui::BeginTable("##BattleClipLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Timeline", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthStretch, 1.1f);

        ImGui::TableNextColumn();
        ImGui::BeginChild("##BattleClipSequencerPanel", ImVec2(0.f, 0.f), true);
        Draw_SectionHeader("Battle Clip Timeline");
        ImGui::TextDisabled("Left click : select / drag keyframe");
        ImGui::TextDisabled("Right click row : insert keyframe at clicked time");
        ImGui::TextDisabled("Right click key : remove selected keyframe");
        ImGui::Spacing();
        Render_BattleClipSequencer();
        ImGui::EndChild();

        ImGui::TableNextColumn();
        ImGui::BeginChild("##BattleClipRightPanel", ImVec2(0.f, 0.f), false);
        ImGui::SetNextItemWidth(160.f);
        if (ImGui::DragFloat("Duration", &_editingBattleClip.duration, 0.01f, 0.f, 30.f))
        {
            _editingBattleClip.duration = std::max(0.f, _editingBattleClip.duration);
            _battleClipCurrentFrame = std::clamp(_battleClipCurrentFrame, 0, Get_BattleClipFrameMax(_editingBattleClip));
            _isBattleClipPreviewDirty = true;
        }
        else
        {
            _editingBattleClip.duration = std::max(0.f, _editingBattleClip.duration);
        }

        ImGui::TextDisabled("Current Frame : %d", _battleClipCurrentFrame);
        ImGui::TextDisabled("Current Time : %.3f s", static_cast<float>(_battleClipCurrentFrame) / kBattleClipPreviewFps);

        Render_SelectedBattleClipKeyframe();
        Render_BattleClipTrackEditor("Distance Offset", _editingBattleClip.distanceOffset);
        Render_BattleClipTrackEditor("FOV Offset", _editingBattleClip.fovOffset);
        Render_BattleClipTrackEditor("Shake Amplitude", _editingBattleClip.shakeAmplitude);
        Render_BattleClipTrackEditor("Side Offset", _editingBattleClip.sideOffset);
        Render_BattleClipTrackEditor("Yaw Offset", _editingBattleClip.yawOffset);
        ImGui::EndChild();

        ImGui::EndTable();
    }

    Render_BattleClipInsertKeyframePopup();
    Render_BattleClipRemoveKeyframePopup();
}

void Cinematic_Tool::Render_BattleClipHeader()
{
    Ensure_BattleClipSkillListLoaded();

    const BattleCameraMode previousMode = _selectedBattleClipMode;
    Select_PresetEnum("Battle Mode", _selectedBattleClipMode, 100.f);

    if (previousMode != _selectedBattleClipMode)
    {
        _hasLoadedBattleClip = false;
        _battleClipSelectedTrackIndex = 0;
        _battleClipSelectedKeyIndex = -1;
        _battleClipCurrentFrame = 0;
        _isBattleClipPreviewDirty = true;
    }

    ImGui::SameLine();
    if (ImGui::Checkbox("Use Skill Override", &_useBattleClipSkillOverride))
    {
        _hasLoadedBattleClip = false;
        _battleClipCurrentFrame = 0;
        _isBattleClipPreviewDirty = true;
    }

    if (_useBattleClipSkillOverride)
    {
        const char* currentSkillLabel = "None";

        if (_selectedBattleClipSkillIndex >= 0 &&
            _selectedBattleClipSkillIndex < static_cast<int>(_battleClipSkillIds.size()))
        {
            currentSkillLabel = _battleClipSkillIds[_selectedBattleClipSkillIndex].c_str();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(280.f);

        if (ImGui::BeginCombo("Skill", currentSkillLabel))
        {
            for (int i = 0; i < static_cast<int>(_battleClipSkillIds.size()); ++i)
            {
                const bool isSelected = (_selectedBattleClipSkillIndex == i);

                if (ImGui::Selectable(_battleClipSkillIds[i].c_str(), isSelected))
                {
                    _selectedBattleClipSkillIndex = i;
                    _hasLoadedBattleClip = false;
                    _battleClipCurrentFrame = 0;
                    _isBattleClipPreviewDirty = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }
    }

    const string selectedSkillId = Get_SelectedBattleClipSkillId();

    ImGui::SameLine();
    if (ImGui::Button("Load Clip"))
    {
        BattleCameraClipIO::LoadBattleCameraClipFromFile(_selectedBattleClipMode, _editingBattleClip, selectedSkillId);
        _loadedBattleClipMode = _selectedBattleClipMode;
        _loadedBattleClipSkillId = selectedSkillId;
        _hasLoadedBattleClip = true;
        _battleClipSelectedTrackIndex = 0;
        _battleClipSelectedKeyIndex = -1;
        _battleClipCurrentFrame = 0;
        _isBattleClipPreviewDirty = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Save Clip"))
        BattleCameraClipIO::SaveBattleCameraClipToFile(_selectedBattleClipMode, _editingBattleClip, selectedSkillId);

    ImGui::SameLine();
    if (ImGui::Button("Preview Clip"))
        _isBattleClipPreviewDirty = true;

    ImGui::SameLine();
    if (ImGui::Button("Go To Start"))
    {
        _battleClipCurrentFrame = 0;
        _isBattleClipPreviewDirty = true;
    }
}

void Cinematic_Tool::Ensure_BattleClipSkillListLoaded()
{
    if (!_battleClipSkillIds.empty())
        return;

    _battleClipSkillIds = CLIENT->Get_AllSkillIds();

    if (!_battleClipSkillIds.empty() && _selectedBattleClipSkillIndex < 0)
        _selectedBattleClipSkillIndex = 0;
}

string Cinematic_Tool::Get_SelectedBattleClipSkillId() const
{
    if (!_useBattleClipSkillOverride)
        return "";

    if (_selectedBattleClipSkillIndex < 0 ||
        _selectedBattleClipSkillIndex >= static_cast<int>(_battleClipSkillIds.size()))
    {
        return "";
    }

    return _battleClipSkillIds[_selectedBattleClipSkillIndex];
}

Shared<Character> Cinematic_Tool::Resolve_BattleClipPreviewSubject() const
{
    auto battleController = CLIENT->Get_BattleController();
    if (!battleController)
        return nullptr;

    if (_selectedBattleClipMode == BattleCameraMode::ENEMYFOCUS ||
        _selectedBattleClipMode == BattleCameraMode::ENEMYATTACK)
    {
        if (const auto enemies = battleController->Get_AliveEnemies(); !enemies.empty() && enemies[0].actor != nullptr)
            return enemies[0].actor;
    }

    if (_selectedBattleClipMode == BattleCameraMode::PLAYERFOCUS ||
        _selectedBattleClipMode == BattleCameraMode::PLAYERATTACK)
    {
        //원래는 battleController->Get_TurnAction().target.actor
        if(const auto hero = battleController->Get_TurnAction().GetInitTarget().actor)   
            return hero;
        //if (const auto heroes = battleController->Get_AliveHeroes(); !heroes.empty() && heroes[0].actor != nullptr)
        //    return heroes[0].actor;
    }

    if (const auto heroes = battleController->Get_AliveHeroes(); !heroes.empty() && heroes[0].actor != nullptr)
        return heroes[0].actor;

    if (const auto enemies = battleController->Get_AliveEnemies(); !enemies.empty() && enemies[0].actor != nullptr)
        return enemies[0].actor;

    return nullptr;
}

Shared<Character> Cinematic_Tool::Resolve_BattleClipPreviewOpponent() const
{
    auto battleController = CLIENT->Get_BattleController();
    if (!battleController)
        return nullptr;

    if (_selectedBattleClipMode == BattleCameraMode::ENEMYATTACK)
    {
        if (auto target = battleController->Get_TurnAction().GetInitTarget().actor)
            return target;

        if (const auto heroes = battleController->Get_AliveHeroes(); !heroes.empty() && heroes[0].actor != nullptr)
            return heroes[0].actor;
    }

    if (_selectedBattleClipMode == BattleCameraMode::PLAYERATTACK)
    {
        if (const BattleChar* preview = battleController->Get_TargetPreview())
        {
            if (preview->actor != nullptr)
                return preview->actor;
        }

        if (const auto enemies = battleController->Get_AliveEnemies(); !enemies.empty() && enemies[0].actor != nullptr)
            return enemies[0].actor;
    }

    if (const auto enemies = battleController->Get_AliveEnemies(); !enemies.empty() && enemies[0].actor != nullptr)
        return enemies[0].actor;

    if (const auto heroes = battleController->Get_AliveHeroes(); !heroes.empty() && heroes[0].actor != nullptr)
        return heroes[0].actor;

    return nullptr;
}

Shared<ModelCom> Cinematic_Tool::Resolve_BattleClipPreviewModel() const
{
    if (Shared<Character> subject = Resolve_BattleClipPreviewSubject())
    {
        PartObject* body = subject->Get_PartObject(L"Part_Body");
        if (body != nullptr)
        {
            if (Shared<ModelCom> model = body->Get_Component<ModelCom>())
                return model;
        }
    }

    auto battleController = CLIENT->Get_BattleController();
    if (!battleController)
        return nullptr;

    const Client::BattleAction& action = battleController->Get_TurnAction();
    if (action.Get_CurChar().ModelCom != nullptr)
        return action.Get_CurChar().ModelCom;

    return nullptr;
}

void Cinematic_Tool::Apply_BattleClipPreviewAnimation(float previewTimeSec) const
{
    Shared<ModelCom> model = Resolve_BattleClipPreviewModel();
    if (model == nullptr)
        return;

    auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr)
        return;

    const Client::BattleAction& action = battleController->Get_TurnAction();
    const Client::Skill_Data& skillData = action.GetSkillData();
    if ((_selectedBattleClipMode == BattleCameraMode::PLAYERATTACK ||
         _selectedBattleClipMode == BattleCameraMode::ENEMYATTACK) &&
        !skillData.AnimPhases.empty())
    {
        const uint32 phaseIndex = std::min<uint32>(action.GetCurAnimIndex(), static_cast<uint32>(skillData.AnimPhases.size() - 1));
        const string& animationKey = skillData.AnimPhases[phaseIndex].animationKey;
        const char* currentAnimationName = model->Get_AnimationName(model->Get_CurrentAnimationIndex());

        if (!animationKey.empty() &&
            (currentAnimationName == nullptr || animationKey != currentAnimationName))
        {
            model->Set_Animation(animationKey, false, 0.f);
        }
    }

    model->Set_AnimationPaused(true);
    model->Set_CurrentAnimationTimeSec(previewTimeSec);
    model->Refresh_CurrentAnimationPose();
}

void Cinematic_Tool::Clip_BattleClipHotInput()
{
    CameraKeyframe* key = Get_SelectedBattleClipKeyframe();

    if(key == nullptr)
        return;

    if(!_isFocused||!_isHovered)
        return;

}

void Cinematic_Tool::Clear_BattleClipPreviewState() const
{
    if (Shared<Client::BattleTurnCamera> battleTurnCamera = Get_BattleTurnCamera())
        battleTurnCamera->Set_ClipScrubPreviewEnabled(false);

    if (Shared<ModelCom> model = Resolve_BattleClipPreviewModel())
        model->Set_AnimationPaused(false);
}

void Cinematic_Tool::Render_BattleClipTrackEditor(const char* label, CameraTrack& track)
{
    if (!ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::PushID(label);
    bool needsSort = false;
    float selectionTime = -1.f;

    const string addKeyId = string("Add Key##") + label;
    if (ImGui::Button(addKeyId.c_str()))
    {
        CameraKeyframe keyframe {};
        if (!track.keyframes.empty())
            keyframe.time = track.keyframes.back().time;

        track.keyframes.push_back(keyframe);
        Sort_BattleClipTrackKeyframes(track);
        _isBattleClipPreviewDirty = true;
    }

    ImGui::Separator();

    for (int i = 0; i < static_cast<int>(track.keyframes.size()); ++i)
    {
        CameraKeyframe& keyframe = track.keyframes[i];

        ImGui::PushID(i);
        ImGui::TextDisabled("Key %d", i);
        ImGui::SameLine();

        if (ImGui::Button("Remove"))
        {
            track.keyframes.erase(track.keyframes.begin() + i);
            if (&track == Get_SelectedBattleClipTrack())
                _battleClipSelectedKeyIndex = std::clamp(_battleClipSelectedKeyIndex, -1, static_cast<int>(track.keyframes.size()) - 1);
            _isBattleClipPreviewDirty = true;
            ImGui::PopID();
            break;
        }

        ImGui::SetNextItemWidth(140.f);
        if (ImGui::DragFloat("Time", &keyframe.time, 0.01f, 0.f, _editingBattleClip.duration))
        {
            keyframe.time = std::clamp(keyframe.time, 0.f, _editingBattleClip.duration);
            needsSort = true;
            _isBattleClipPreviewDirty = true;
            if (&track == Get_SelectedBattleClipTrack() && _battleClipSelectedKeyIndex == i)
                selectionTime = keyframe.time;
        }

        ImGui::SetNextItemWidth(140.f);
        if (ImGui::DragFloat("Value", &keyframe.value, 0.01f))
            _isBattleClipPreviewDirty = true;
        ImGui::Separator();
        ImGui::PopID();
    }

    if (needsSort)
    {
        Sort_BattleClipTrackKeyframes(track);

        if (&track == Get_SelectedBattleClipTrack() && selectionTime >= 0.f)
        {
            for (int i = 0; i < static_cast<int>(track.keyframes.size()); ++i)
            {
                if (fabsf(track.keyframes[i].time - selectionTime) <= 0.0001f)
                {
                    _battleClipSelectedKeyIndex = i;
                    break;
                }
            }
        }
    }

    ImGui::PopID();
}

void Cinematic_Tool::Render_BattleClipSequencer()
{
    bool consumedKeyClick = false;
    bool requestOpenInsertPopup = false;
    bool requestRemoveKeyPopup = false;
    int sequencerSelectedTrackIndex = _battleClipSelectedTrackIndex;
    const int previousFrame = _battleClipCurrentFrame;
    const int previousTrackIndex = _battleClipSelectedTrackIndex;
    const int previousKeyIndex = _battleClipSelectedKeyIndex;

    BattleClipSequencerAdapter adapter(
        &_editingBattleClip,
        &_battleClipSelectedTrackIndex,
        &_battleClipSelectedKeyIndex,
        &_battleClipCurrentFrame,
        &consumedKeyClick,
        &requestOpenInsertPopup,
        &requestRemoveKeyPopup,
        &_battleClipContextTrackIndex,
        &_battleClipContextFrame,
        nullptr,
        false,
        nullptr);

    static bool expanded = true;
    static int firstFrame = 0;
    ImSequencer::Sequencer(&adapter, &_battleClipCurrentFrame, &expanded, &sequencerSelectedTrackIndex, &firstFrame, ImSequencer::SEQUENCER_CHANGE_FRAME);

    if (!consumedKeyClick)
        _battleClipSelectedTrackIndex = sequencerSelectedTrackIndex;

    if (requestOpenInsertPopup)
        _requestOpenBattleClipInsertPopup = true;

    if (requestRemoveKeyPopup)
        _requestRemoveBattleClipKeyPopup = true;

    if (previousFrame != _battleClipCurrentFrame ||
        previousTrackIndex != _battleClipSelectedTrackIndex ||
        previousKeyIndex != _battleClipSelectedKeyIndex)
    {
        _isBattleClipPreviewDirty = true;
    }
}

void Cinematic_Tool::Render_SelectedBattleClipKeyframe()
{
    CameraKeyframe* keyframe = Get_SelectedBattleClipKeyframe();
    if (keyframe == nullptr)
        return;

    Draw_SectionHeader("Selected Battle Key");
    ImGui::TextDisabled("Track %d | Key %d", _battleClipSelectedTrackIndex, _battleClipSelectedKeyIndex);

    ImGui::SetNextItemWidth(140.f);
    if (ImGui::DragFloat("Selected Time", &keyframe->time, 0.01f, 0.f, _editingBattleClip.duration))
    {
        keyframe->time = std::clamp(keyframe->time, 0.f, _editingBattleClip.duration);
        if (CameraTrack* track = Get_SelectedBattleClipTrack())
        {
            const float selectionTime = keyframe->time;
            Sort_BattleClipTrackKeyframes(*track);
            for (int i = 0; i < static_cast<int>(track->keyframes.size()); ++i)
            {
                if (fabsf(track->keyframes[i].time - selectionTime) <= 0.0001f)
                {
                    _battleClipSelectedKeyIndex = i;
                    break;
                }
            }
        }

        _isBattleClipPreviewDirty = true;
    }

    ImGui::SetNextItemWidth(140.f);
    if (ImGui::DragFloat("Selected Value", &keyframe->value, 0.01f))
        _isBattleClipPreviewDirty = true;

    ImGui::SetNextItemWidth(160.f);
    const char* interpolationLabel = magic_enum::enum_name(keyframe->interpolation).data();
    if (ImGui::BeginCombo("Interpolation", interpolationLabel))
    {
        for (InterpolationMode mode : magic_enum::enum_values<InterpolationMode>())
        {
            const bool isSelected = keyframe->interpolation == mode;
            if (ImGui::Selectable(magic_enum::enum_name(mode).data(), isSelected))
            {
                keyframe->interpolation = mode;
                _isBattleClipPreviewDirty = true;
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }
}

void Cinematic_Tool::Render_BattleClipInsertKeyframePopup()
{
    if (_requestOpenBattleClipInsertPopup)
    {
        ImGui::OpenPopup("Insert Battle Clip Keyframe");
        _requestOpenBattleClipInsertPopup = false;
    }

    if (!ImGui::BeginPopup("Insert Battle Clip Keyframe"))
        return;

    ImGui::TextDisabled("Track %d", _battleClipContextTrackIndex);
    ImGui::TextDisabled("Frame %d", _battleClipContextFrame);
    ImGui::Separator();

    if (ImGui::Button("Insert Keyframe"))
    {
        Insert_BattleClipKeyAtFrame(_battleClipContextTrackIndex, _battleClipContextFrame);
        _isBattleClipPreviewDirty = true;
        ImGui::CloseCurrentPopup();
    }

    if (ImGui::Button("Cancel"))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void Cinematic_Tool::Render_BattleClipRemoveKeyframePopup()
{
    if (_requestRemoveBattleClipKeyPopup)
    {
        ImGui::OpenPopup("Remove Battle Clip Keyframe");
        _requestRemoveBattleClipKeyPopup = false;
    }

    if (!ImGui::BeginPopup("Remove Battle Clip Keyframe"))
        return;

    if (ImGui::Button("Remove Selected Key"))
    {
        CameraTrack* track = Get_SelectedBattleClipTrack();
        if (track != nullptr &&
            _battleClipSelectedKeyIndex >= 0 &&
            _battleClipSelectedKeyIndex < static_cast<int>(track->keyframes.size()))
        {
            track->keyframes.erase(track->keyframes.begin() + _battleClipSelectedKeyIndex);
            _battleClipSelectedKeyIndex = std::clamp(_battleClipSelectedKeyIndex, -1, static_cast<int>(track->keyframes.size()) - 1);
            _isBattleClipPreviewDirty = true;
        }

        ImGui::CloseCurrentPopup();
    }

    if (ImGui::Button("Cancel"))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void Cinematic_Tool::Render_RightPanel()
{
    Render_PreviewState();
    Render_SelectedKeyframePopup();
    Render_SequenceSettings();
    Render_TrackList();
    Render_KeyframeList();
    Render_LoadSequencePopup();
    Render_RemoveKeyframePopup();

}

void Cinematic_Tool::Render_SequenceSettings()
{
    Draw_SectionHeader("Sequence");

    char nameBuffer[128]{};
    strncpy_s(nameBuffer, _sequence.name.c_str(), _TRUNCATE);
    if (ImGui::InputText("name", nameBuffer, static_cast<size_t>(_countof(nameBuffer))))
        _sequence.name = nameBuffer;

    ImGui::SetNextItemWidth(100.f);
    if (ImGui::DragInt("fps", &_sequence.fps, 1.f, 1, 240))
    {
        _sequence.fps = std::clamp(_sequence.fps, 1, 240);
        _isPreviewDirty = true;
    }

    ImGui::SetNextItemWidth(100.f);
    if (ImGui::DragInt("start frame", &_sequence.startFrame, 1.f, 0, _sequence.endFrame))
    {
        _sequence.startFrame = std::clamp(_sequence.startFrame, 0, _sequence.endFrame);
        _currentFrame = std::clamp(_currentFrame, _sequence.startFrame, _sequence.endFrame);
        _isPreviewDirty = true;
    }

    ImGui::SetNextItemWidth(100.f);
    if (ImGui::DragInt("end frame", &_sequence.endFrame, 1.f, _sequence.startFrame, 100000))
    {
        _sequence.endFrame = std::max(_sequence.endFrame, _sequence.startFrame);
        _currentFrame = std::clamp(_currentFrame, _sequence.startFrame, _sequence.endFrame);
        _isPreviewDirty = true;
    }

    if(ImGui::Button("Save Sequence"))
    {
        Cinematic_SequenceIO::SaveSequenceToFile(_sequence,   "../../../Client/Bin/Resources/Data/json/Cinematics/" + _sequence.name + ".json");
    }

    ImGui::SameLine();
    if(ImGui::Button("Load Sequence"))
    {
        Refresh_SequenceFileList();
        _selectedSequenceFileIndex = -1;
        ImGui::OpenPopup("Load Cinematic Sequence");
    }

    if (ImGui::Button("Clear All Keys"))
    {
        Clear_AllSequenceKeyframes();
    }

    ImGui::SameLine();
    if (ImGui::Button("New Empty Sequence"))
    {
        Reset_ToNewSequence();
    }
}

void Cinematic_Tool::Render_LoadSequencePopup()
{
    if (!ImGui::BeginPopupModal("Load Cinematic Sequence", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    if (_sequenceFilePaths.empty())
    {
        ImGui::TextDisabled("No cinematic sequence files found.");
    }
    else if (ImGui::BeginListBox("##CinematicSequenceFiles", ImVec2(420.f, 240.f)))
    {
        for (int i = 0; i < static_cast<int>(_sequenceFilePaths.size()); ++i)
        {
            const string label = _sequenceFilePaths[i].filename().string();
            const bool isSelected = (_selectedSequenceFileIndex == i);

            if (ImGui::Selectable(label.c_str(), isSelected))
                _selectedSequenceFileIndex = i;

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            {
                _selectedSequenceFileIndex = i;
                Load_SelectedSequenceFile();
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndListBox();
    }

    ImGui::Separator();

    if (_selectedSequenceFileIndex >= 0 &&
        _selectedSequenceFileIndex < static_cast<int>(_sequenceFilePaths.size()))
    {
        ImGui::TextDisabled("Selected : %s", _sequenceFilePaths[_selectedSequenceFileIndex].filename().string().c_str());
    }
    else
    {
        ImGui::TextDisabled("No file selected.");
    }

    if (_selectedSequenceFileIndex < 0)
        ImGui::BeginDisabled();

    if (ImGui::Button("Load", ImVec2(120.f, 0.f)))
    {
        Load_SelectedSequenceFile();
        ImGui::CloseCurrentPopup();
    }

    if (_selectedSequenceFileIndex < 0)
        ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.f, 0.f)))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void Cinematic_Tool::Render_ImSequencer()
{
    bool consumedKeyClick = false;
    bool requestOpenKeyPopup = false;
    bool requestOpenInsertPopup = false;
    bool requestRemoveKeyPopup = false;
    int  sequencerSelectedTrackIndex = _selectedTrackIndex;
    const int previousFrame = _currentFrame;
    const int previousTrackIndex = _selectedTrackIndex;
    const int previousKeyIndex = _selectedKeyIndex;

    CinematicSequencerAdapter adapter(
        &_sequence,
        &_selectedTrackIndex,
        &_selectedKeyIndex,
        &_currentFrame,
        &consumedKeyClick,
        &requestOpenKeyPopup,
        &requestOpenInsertPopup,
        &requestRemoveKeyPopup, 
        &_contextMenuTrackIndex,
        &_contextMenuFrame);

    static bool expanded = true;
    static int firstFrame = 0;

    ImSequencer::Sequencer(&adapter, &_currentFrame, &expanded, &sequencerSelectedTrackIndex, &firstFrame, ImSequencer::SEQUENCER_CHANGE_FRAME);

    if (!consumedKeyClick)
        _selectedTrackIndex = sequencerSelectedTrackIndex;

    if (requestOpenKeyPopup)
        _requestOpenSelectedKeyPopup = true;

    if (requestOpenInsertPopup)
    {
        _hasPendingInsertRequest = true;
        _pendingInsertTrackIndex = _contextMenuTrackIndex;
        _pendingInsertFrame = _contextMenuFrame;
        _hasPendingInsertPose = Try_CaptureActiveFreeCameraPose(_pendingInsertPose);
        _requestOpenInsertKeyPopup = true;
    }

    if(requestRemoveKeyPopup)
        _requestRemoveKeyPopup = true;

    if (previousFrame != _currentFrame ||
        previousTrackIndex != _selectedTrackIndex ||
        previousKeyIndex != _selectedKeyIndex)
    {
        _isPreviewDirty = true;
    }
}

void Cinematic_Tool::Render_TrackList()
{

    Draw_SectionHeader("Cinematic Tracks");

    if (ImGui::Button("Add World Track"))
    {
        CINEMATIC_TRACK track{};
        track.name = "World Track " + std::to_string(_sequence.cinematicTracks.size());
        _sequence.cinematicTracks.push_back(track);
        _selectedTrackIndex = static_cast<int>(_sequence.cinematicTracks.size()) - 1;
        _selectedKeyIndex = -1;
        _isPreviewDirty = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("Add Interaction Track"))
    {
        INTERACTION_TRACK track{};
        track.name = "Interaction Track " + std::to_string(_sequence.interactionTracks.size());
        _sequence.interactionTracks.push_back(track);
        _selectedTrackIndex = static_cast<int>(_sequence.interactionTracks.size()) - 1;
        _selectedKeyIndex = -1;
        _isPreviewDirty = true;

    }
    ImGui::SameLine();
    const bool isRemoveTrackDisabled = (_sequence.cinematicTracks.size() <= 1);
    if (isRemoveTrackDisabled)
        ImGui::BeginDisabled();

    if (ImGui::Button("Remove Track"))
    {
        if (_selectedTrackIndex >= 0 &&
            _selectedTrackIndex < static_cast<int>(_sequence.cinematicTracks.size()))
        {
            _sequence.cinematicTracks.erase(_sequence.cinematicTracks.begin() + _selectedTrackIndex);
            _selectedTrackIndex = std::clamp(_selectedTrackIndex, 0, static_cast<int>(_sequence.cinematicTracks.size()) - 1);
            _selectedKeyIndex = -1;
            _isPreviewDirty = true;
        }
    }

    if (isRemoveTrackDisabled)
        ImGui::EndDisabled();

    if (ImGui::BeginListBox("##CameraTrackList", ImVec2(-FLT_MIN, 120.f)))
    {
        for (int i = 0; i < static_cast<int>(_sequence.cinematicTracks.size()); ++i)
        {
            CINEMATIC_TRACK& track = _sequence.cinematicTracks[i];
            const bool isSelected = (_selectedTrackIndex == i);
            const string label = track.name.empty() ? ("Track " + std::to_string(i)) : track.name;

            if (ImGui::Selectable(label.c_str(), isSelected))
            {
                _selectedTrackIndex = i;
                _selectedKeyIndex = -1;
                _isPreviewDirty = true;
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndListBox();
    }

    CINEMATIC_TRACK* selectedTrack = Get_SelectedTrack();
    if (selectedTrack == nullptr)
    {
        if (Is_SelectedModifierTrack())
            ImGui::TextDisabled("Modifier track is selected in the timeline.");
        return;
    }

    char trackNameBuffer[128]{};
    strncpy_s(trackNameBuffer, selectedTrack->name.c_str(), _TRUNCATE);
    if (ImGui::InputText("track name", trackNameBuffer, static_cast<size_t>(_countof(trackNameBuffer))))
        selectedTrack->name = trackNameBuffer;

    if (ImGui::Checkbox("track enabled", &selectedTrack->enabled))
        _isPreviewDirty = true;
}

void Cinematic_Tool::Render_KeyframeList()
{
    if (Is_SelectedModifierTrack())
    {
        CameraTrack* selectedModifierTrack = Get_SelectedModifierTrack();
        if (selectedModifierTrack == nullptr)
        {
            ImGui::TextDisabled("No modifier track selected.");
            return;
        }

        Draw_SectionHeader("Modifier Keyframes");

        const bool isRemoveKeyDisabled =
            (_selectedKeyIndex < 0 || _selectedKeyIndex >= static_cast<int>(selectedModifierTrack->keyframes.size()));
        if (isRemoveKeyDisabled)
            ImGui::BeginDisabled();

        if (ImGui::Button("Remove Key"))
        {
            selectedModifierTrack->keyframes.erase(selectedModifierTrack->keyframes.begin() + _selectedKeyIndex);
            _selectedKeyIndex = std::clamp(_selectedKeyIndex, -1, static_cast<int>(selectedModifierTrack->keyframes.size()) - 1);
            _isPreviewDirty = true;
        }

        if (isRemoveKeyDisabled)
            ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Sort Keys"))
        {
            Sort_SelectedModifierTrackKeyframes();
            _isPreviewDirty = true;
        }

        if (selectedModifierTrack->keyframes.empty())
        {
            ImGui::TextDisabled("No modifier keys yet. Right click the timeline row to insert one.");
            return;
        }

        if (ImGui::BeginListBox("##ModifierKeyframeList", ImVec2(-FLT_MIN, 160.f)))
        {
            for (int i = 0; i < static_cast<int>(selectedModifierTrack->keyframes.size()); ++i)
            {
                const CameraKeyframe& keyframe = selectedModifierTrack->keyframes[i];
                char label[128]{};
                sprintf_s(label, "Time %.3f | Value %.3f", keyframe.time, keyframe.value);

                const bool isSelected = (_selectedKeyIndex == i);
                if (ImGui::Selectable(label, isSelected))
                {
                    _selectedKeyIndex = i;
                    _currentFrame = std::clamp(
                        _sequence.startFrame + static_cast<int>(std::lround(keyframe.time * static_cast<float>(std::max(_sequence.fps, 1)))),
                        _sequence.startFrame,
                        _sequence.endFrame);
                    _isPreviewDirty = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndListBox();
        }

        return;
    }

    CINEMATIC_TRACK* selectedTrack = Get_SelectedTrack();
    if (selectedTrack == nullptr)
    {
        ImGui::TextDisabled("No track selected.");
        return;
    }

    Draw_SectionHeader("Keyframes");

    if (ImGui::Button("Capture From FreeCamera"))
    {
        Capture_KeyframeFromFreeCamera();
        _isPreviewDirty = true;
    }

    ImGui::SameLine();
    const bool isRemoveKeyDisabled =
        (_selectedKeyIndex < 0 || _selectedKeyIndex >= static_cast<int>(selectedTrack->keyframes.size()));
    if (isRemoveKeyDisabled)
        ImGui::BeginDisabled();

    if (ImGui::Button("Remove Key"))
    {
        selectedTrack->keyframes.erase(selectedTrack->keyframes.begin() + _selectedKeyIndex);
        _selectedKeyIndex = std::clamp(_selectedKeyIndex, -1, static_cast<int>(selectedTrack->keyframes.size()) - 1);
        _isPreviewDirty = true;
    }

    if (isRemoveKeyDisabled)
        ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Sort Keys"))
    {
        Sort_SelectedTrackKeyframes();
        _isPreviewDirty = true;
    }

    if (selectedTrack->keyframes.empty())
    {
        ImGui::TextDisabled("No keyframes yet. Capture one from the current FreeCamera pose.");
        return;
    }

    if (ImGui::BeginListBox("##CameraKeyframeList", ImVec2(-FLT_MIN, 160.f)))
    {
        for (int i = 0; i < static_cast<int>(selectedTrack->keyframes.size()); ++i)
        {
            const CINEMATIC_KEYFRAME& keyframe = selectedTrack->keyframes[i];
            char label[128]{};
            sprintf_s(label, "Frame %d | Pos(%.1f, %.1f, %.1f)",
                keyframe.frame,
                keyframe.camPose.position.x,
                keyframe.camPose.position.y,
                keyframe.camPose.position.z);

            const bool isSelected = (_selectedKeyIndex == i);

            if (ImGui::Selectable(label, isSelected))
            {
                _selectedKeyIndex = i;
                _currentFrame = keyframe.frame;
                _isPreviewDirty = true;
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndListBox();
    }
}

void Cinematic_Tool::Render_SelectedKeyframe()
{
    if (CameraKeyframe* modifierKeyframe = Get_SelectedModifierKeyframe())
    {
        Draw_SectionHeader("Modifier Keyframe");
        Render_SelectedModifierKeyframeEditor(*modifierKeyframe);
        return;
    }

    CINEMATIC_KEYFRAME* keyframe = Get_SelectedKeyframe();
    if (keyframe == nullptr)
    {
        ImGui::TextDisabled("No keyframe selected.");
        return;
    }

    Draw_SectionHeader("Keyframe");
    Render_SelectedKeyframeEditor(*keyframe);
}

void Cinematic_Tool::Render_SelectedKeyframeEditor(CINEMATIC_KEYFRAME& keyframe)
{
    if (ImGui::DragInt("frame", &keyframe.frame, 1.f, _sequence.startFrame, _sequence.endFrame))
    {
        keyframe.frame = std::clamp(keyframe.frame, _sequence.startFrame, _sequence.endFrame);
        _isPreviewDirty = true;
    }

    if(ImGui::DragFloat3("position", &keyframe.camPose.position.x, 0.1f))
        _isPreviewDirty = true;

    if(ImGui::DragFloat3("rotation", &keyframe.camPose.rotation.x, 0.1f))
        _isPreviewDirty = true;
    

    if(ImGui::DragFloat("fovY", &keyframe.camPose.fovY, 0.01f, 0.1f, 3.13f))
        _isPreviewDirty = true;

    if (ImGui::DragFloat("near", &keyframe.camPose.nearPlane, 0.01f, 0.01f, keyframe.camPose.farPlane - 0.01f))
    {
        _isPreviewDirty = true;
        keyframe.camPose.nearPlane = std::clamp(keyframe.camPose.nearPlane, 0.01f, keyframe.camPose.farPlane - 0.01f);
    }

    if (ImGui::DragFloat("far", &keyframe.camPose.farPlane, 0.1f, keyframe.camPose.nearPlane + 0.01f, 100000.f))
    {
        _isPreviewDirty = true;
        keyframe.camPose.farPlane = std::max(keyframe.camPose.farPlane, keyframe.camPose.nearPlane + 0.01f);
    }

   // const char* interpolationLabels[] = { "Step", "Linear" };
   // int interpolationIndex = static_cast<int>(keyframe.interpolation);
   // if (ImGui::Combo("interpolation", &interpolationIndex, interpolationLabels, IM_ARRAYSIZE(interpolationLabels)))
   // {
   //     keyframe.interpolation = static_cast<InterpolationMode>(interpolationIndex);
   //     _isPreviewDirty = true;
   // }
   //

    const char* interpolationLabel = magic_enum::enum_name(keyframe.interpolation).data();
    
    if(ImGui::BeginCombo("Interpolation", interpolationLabel))
    {
        for(InterpolationMode mode : magic_enum::enum_values<InterpolationMode>())
        {
            const bool isSelected = (mode == keyframe.interpolation);

            if(ImGui::Selectable(magic_enum::enum_name(mode).data(), isSelected))
            {
               keyframe.interpolation = mode; 
            }

            if(isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    if(ImGui::Button("Overwrite Keyframe With Current Camera"))
    {
        ImGui::OpenPopup("Confirm Overwrite Keyframe");
    }

    if (ImGui::BeginPopupModal("Confirm Overwrite Keyframe", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Overwrite the selected keyframe with the current FreeCamera pose?");
        ImGui::Separator();

        if (ImGui::Button("Yes", ImVec2(120.f, 0.f)))
        {
            Shared<FreeCamera> freeCamera = Get_ActiveFreeCamera();
            CINEMATIC_TRACK* selectedTrack = Get_SelectedTrack();

            if (freeCamera != nullptr &&
                selectedTrack != nullptr &&
                _selectedKeyIndex >= 0 &&
                _selectedKeyIndex < static_cast<int>(selectedTrack->keyframes.size()))
            {
                auto& keyframe = selectedTrack->keyframes[_selectedKeyIndex];
                keyframe.camPose.farPlane = freeCamera->Get_Far();
                keyframe.camPose.nearPlane = freeCamera->Get_Near();
                keyframe.camPose.fovY = freeCamera->Get_FovY();
                keyframe.camPose.position = freeCamera->Get_Transform()->Get_WorldPosition();
                keyframe.camPose.rotation = freeCamera->Get_Transform()->Get_RotationEuler();

                _isPreviewDirty = true;
            }

            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120.f, 0.f)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

}

void Cinematic_Tool::Render_SelectedModifierKeyframeEditor(CameraKeyframe& keyframe)
{
    const float maxTime = Evaluate_CinematicSequenceTime(_sequence, _sequence.endFrame);
    bool needsSort = false;

    if (ImGui::DragFloat("time", &keyframe.time, 0.01f, 0.f, maxTime))
    {
        keyframe.time = std::clamp(keyframe.time, 0.f, maxTime);
        needsSort = true;
        _isPreviewDirty = true;
    }

    if (ImGui::DragFloat("value", &keyframe.value, 0.01f))
        _isPreviewDirty = true;

    const char* interpolationLabel = magic_enum::enum_name(keyframe.interpolation).data();
    if (ImGui::BeginCombo("Interpolation", interpolationLabel))
    {
        for (InterpolationMode mode : magic_enum::enum_values<InterpolationMode>())
        {
            const bool isSelected = (mode == keyframe.interpolation);
            if (ImGui::Selectable(magic_enum::enum_name(mode).data(), isSelected))
            {
                keyframe.interpolation = mode;
                _isPreviewDirty = true;
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    if (needsSort)
    {
        Sort_SelectedModifierTrackKeyframes();
        if (CameraKeyframe* selectedModifierKeyframe = Get_SelectedModifierKeyframe())
        {
            _currentFrame = std::clamp(
                _sequence.startFrame + static_cast<int>(std::lround(selectedModifierKeyframe->time * static_cast<float>(std::max(_sequence.fps, 1)))),
                _sequence.startFrame,
                _sequence.endFrame);
        }
    }
}

void Cinematic_Tool::Render_SelectedKeyframePopup()
{
    if (CameraKeyframe* modifierKeyframe = Get_SelectedModifierKeyframe())
    {
        const bool shouldExpandEditor = _requestOpenSelectedKeyPopup;
        if (shouldExpandEditor)
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
            _requestOpenSelectedKeyPopup = false;
        }

        if (!ImGui::CollapsingHeader("Selected Modifier Keyframe", ImGuiTreeNodeFlags_None))
            return;

        if (shouldExpandEditor)
            ImGui::SetScrollHereY();

        ImGui::TextDisabled("Track %d | Key %d", _selectedTrackIndex, _selectedKeyIndex);
        ImGui::Separator();
        Render_SelectedModifierKeyframeEditor(*modifierKeyframe);
        return;
    }

    CINEMATIC_KEYFRAME* keyframe = Get_SelectedKeyframe();

    if (keyframe == nullptr)
    {
        _requestOpenSelectedKeyPopup = false;
        return;
    }

    const bool shouldExpandEditor = _requestOpenSelectedKeyPopup;
    if (shouldExpandEditor)
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        _requestOpenSelectedKeyPopup = false;
    }

    if (!ImGui::CollapsingHeader("Selected Keyframe", ImGuiTreeNodeFlags_None))
        return;

    if (shouldExpandEditor)
        ImGui::SetScrollHereY();

    ImGui::TextDisabled("Track %d | Key %d", _selectedTrackIndex, _selectedKeyIndex);
    ImGui::Separator();
    Render_SelectedKeyframeEditor(*keyframe);
}

void Cinematic_Tool::Render_InsertKeyframePopup()
{
    if (_requestOpenInsertKeyPopup)
    {
        ImGui::OpenPopup("Insert Keyframe");
        _requestOpenInsertKeyPopup = false;
    }

    if (!ImGui::BeginPopup("Insert Keyframe"))
    {
        if (_hasPendingInsertRequest && !ImGui::IsPopupOpen("Insert Keyframe"))
            Clear_PendingInsertRequest();
        return;
    }

    ImGui::TextDisabled("Track %d", _pendingInsertTrackIndex);
    ImGui::TextDisabled("Frame %d", _pendingInsertFrame);
    ImGui::Separator();

    const bool isModifierInsert = _pendingInsertTrackIndex >= static_cast<int>(_sequence.cinematicTracks.size());

    if (!isModifierInsert && !_hasPendingInsertPose)
        ImGui::TextDisabled("Active FreeCamera is required to insert from the current camera pose.");

    const bool canInsertFromCamera =
        _hasPendingInsertRequest &&
        (isModifierInsert || _hasPendingInsertPose) &&
        _pendingInsertTrackIndex >= 0;

    if (!canInsertFromCamera)
        ImGui::BeginDisabled();

    if (ImGui::Button("Insert Keyframe Here"))
    {
        Insert_KeyframeAtFrame(_pendingInsertTrackIndex, _pendingInsertFrame);
        Clear_PendingInsertRequest();
        ImGui::CloseCurrentPopup();
    }

    if (!canInsertFromCamera)
        ImGui::EndDisabled();

    if (ImGui::Button("Cancel"))
    {
        Clear_PendingInsertRequest();
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void Cinematic_Tool::Render_RemoveKeyframePopup()
{
    if(_requestRemoveKeyPopup)
    {
        ImGui::OpenPopup("Remove Keyframe");
        _requestRemoveKeyPopup = false;
    }

    if(!ImGui::BeginPopup("Remove Keyframe"))
        return;

    if (CameraKeyframe* modifierKeyframe = Get_SelectedModifierKeyframe())
    {
        ImGui::TextDisabled("Modifier Track %d | Key %d", _selectedTrackIndex, _selectedKeyIndex);
        ImGui::Separator();
        Render_SelectedModifierKeyframeEditor(*modifierKeyframe);
        ImGui::Separator();

        if (ImGui::Button("Remove Keyframe"))
        {
            if (CameraTrack* selectedModifierTrack = Get_SelectedModifierTrack())
            {
                selectedModifierTrack->keyframes.erase(selectedModifierTrack->keyframes.begin() + _selectedKeyIndex);
                _selectedKeyIndex = std::clamp(_selectedKeyIndex, -1, static_cast<int>(selectedModifierTrack->keyframes.size()) - 1);
                _isPreviewDirty = true;
            }

            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
        return;
    }

    if (ImGui::Button("Remove Keyframe Here"))
    {
        CINEMATIC_TRACK* selectedTrack = Get_SelectedTrack();

        selectedTrack->keyframes.erase(selectedTrack->keyframes.begin()+_selectedKeyIndex);
        _selectedKeyIndex = std::clamp(_selectedKeyIndex, -1, static_cast<int>(selectedTrack->keyframes.size()));
        _isPreviewDirty = true;

        ImGui::CloseCurrentPopup();
    }

    if (ImGui::Button("Cancel"))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void Cinematic_Tool::Render_PreviewState()
{
    Draw_SectionHeader("Preview");

    ImGui::SetNextItemWidth(120.f);
    if (ImGui::DragInt("current frame", &_currentFrame, 1.f, _sequence.startFrame, _sequence.endFrame))
    {
        _currentFrame = std::clamp(_currentFrame, _sequence.startFrame, _sequence.endFrame);
        _isPreviewDirty = true;
    }

    if (ImGui::Button(_cinematicPlayer->Is_CinematicPlaying() ? "Stop Preview" : "Play Preview"))
    {
        if(!_cinematicPlayer->Is_CinematicPlaying())
        {
            CINEMATIC_TRACK* previewTrack = Get_SelectedTrack();
            if (previewTrack == nullptr)
            {
                if (_sequence.cinematicTracks.empty())
                    return;

                previewTrack = &_sequence.cinematicTracks[0];
            }

            CLIENT->Request_Cinematic();

            auto camera = GAME->Get_CameraByType(ObjectType::CinematicCamera);

            auto cinematicCamera = dynamic_pointer_cast<CinematicCamera>(camera);

            if(nullptr == cinematicCamera)
                return;

            _cinematicPlayer->Ready_CinematicPlayer(&_sequence, previewTrack, cinematicCamera, true);
        }
        else
        {
            _cinematicPlayer->Stop();  
            CLIENT ->Restore_PreviousCamera();
        }

        _isPreviewDirty = true;
    }

    ImGui::SameLine();
    ImGui::TextDisabled("%s", _cinematicPlayer->Is_CinematicPlaying() ? "Playing" : "Stopped");

    CINEMATIC_TRACK* selectedTrack = Get_SelectedTrack();
    CINEMATIC_KEYFRAME* leftKey = nullptr;
    CINEMATIC_KEYFRAME* rightKey = nullptr;

    Find_SurroundingKeyFrames(selectedTrack, _currentFrame,leftKey, rightKey);

    const int keyCount = selectedTrack ? static_cast<int>(selectedTrack->keyframes.size()) : 0;

    ImGui::TextDisabled("Selected Track : %d", _selectedTrackIndex);
    ImGui::TextDisabled("Selected Key : %d", _selectedKeyIndex);
    ImGui::TextDisabled("Keys In Track : %d", keyCount);
    ImGui::TextDisabled("Left Frame : %d", leftKey ? leftKey->frame : -1);
    ImGui::TextDisabled("Right Frame : %d", rightKey ? rightKey->frame : -1);
    ImGui::TextDisabled("Current Playing Frame : %d", _cinematicPlayer->Get_CurrentFrame());

    if(leftKey&&rightKey)
    {
        if(leftKey->frame == rightKey->frame)
            return;
        float t = static_cast<float>(_currentFrame - leftKey->frame) / static_cast<float>(rightKey->frame - leftKey->frame);
        ImGui::TextDisabled("t : %f", t);
    }


}

void Cinematic_Tool::Ensure_DefaultTrack()
{
    if (_sequence.cinematicTracks.empty())
        _sequence.cinematicTracks.push_back(CINEMATIC_TRACK{});
}

void Cinematic_Tool::Clamp_Selection()
{
    const int cinematicTrackCount = static_cast<int>(_sequence.cinematicTracks.size());
    const int totalTrackCount = cinematicTrackCount + CinematicSequencerAdapter::kModifierTrackCount;

    if (cinematicTrackCount == 0)
    {
        _selectedTrackIndex = 0;
        _selectedKeyIndex = -1;
        return;
    }

    _selectedTrackIndex = std::clamp(_selectedTrackIndex, 0, std::max(totalTrackCount - 1, 0));

    if (Is_SelectedModifierTrack())
    {
        CameraTrack* selectedModifierTrack = Get_SelectedModifierTrack();
        if (selectedModifierTrack == nullptr || selectedModifierTrack->keyframes.empty())
        {
            _selectedKeyIndex = -1;
            return;
        }

        _selectedKeyIndex = std::clamp(_selectedKeyIndex, 0, static_cast<int>(selectedModifierTrack->keyframes.size()) - 1);
        return;
    }

    CINEMATIC_TRACK* selectedTrack = Get_SelectedTrack();
    if (selectedTrack == nullptr || selectedTrack->keyframes.empty())
    {
        _selectedKeyIndex = -1;
        return;
    }

    _selectedKeyIndex = std::clamp(_selectedKeyIndex, 0, static_cast<int>(selectedTrack->keyframes.size()) - 1);
}

void Cinematic_Tool::Sort_SelectedTrackKeyframes()
{
    CINEMATIC_TRACK* selectedTrack = Get_SelectedTrack();
    if (selectedTrack == nullptr)
        return;

    const int selectedFrame = (_selectedKeyIndex >= 0 &&
        _selectedKeyIndex < static_cast<int>(selectedTrack->keyframes.size()))
        ? selectedTrack->keyframes[_selectedKeyIndex].frame
        : -1;

    std::sort(selectedTrack->keyframes.begin(), selectedTrack->keyframes.end(),
        [](const CINEMATIC_KEYFRAME& lhs, const CINEMATIC_KEYFRAME& rhs)
        {
            return lhs.frame < rhs.frame;
        });

    if (selectedTrack->keyframes.empty())
    {
        _selectedKeyIndex = -1;
        return;
    }

    if (selectedFrame < 0)
    {
        _selectedKeyIndex = 0;
        return;
    }

    for (int i = 0; i < static_cast<int>(selectedTrack->keyframes.size()); ++i)
    {
        if (selectedTrack->keyframes[i].frame == selectedFrame)
        {
            _selectedKeyIndex = i;
            return;
        }
    }

    _selectedKeyIndex = 0;
}

void Cinematic_Tool::Sort_BattleClipTrackKeyframes(CameraTrack& track)
{
    std::sort(track.keyframes.begin(), track.keyframes.end(),
        [](const CameraKeyframe& lhs, const CameraKeyframe& rhs)
        {
            return lhs.time < rhs.time;
        });
}

void Cinematic_Tool::Insert_BattleClipKeyAtFrame(int trackIndex, int frame)
{
    CameraTrack* track = nullptr;

    switch (trackIndex)
    {
    case 0:
        track = &_editingBattleClip.distanceOffset;
        break;
    case 1:
        track = &_editingBattleClip.fovOffset;
        break;
    case 2:
        track = &_editingBattleClip.shakeAmplitude;
        break;
    case 3:
        track = &_editingBattleClip.sideOffset;
        break;
    case 4:
        track = &_editingBattleClip.pivotOffset;
        break;
    case 5:
        track = &_editingBattleClip.heightOffset;
        break;
    case 6:
        track = &_editingBattleClip.aimOffset;
        break;
    case 7:
        track = &_editingBattleClip.lookHeightOffset;
        break;
    case 8:
        track = &_editingBattleClip.yawOffset;
        break;
    default:
        return;
    }

    const float keyTime = std::clamp(static_cast<float>(frame) / 30.f, 0.f, _editingBattleClip.duration);

    for (int i = 0; i < static_cast<int>(track->keyframes.size()); ++i)
    {
        if (fabsf(track->keyframes[i].time - keyTime) <= 0.0001f)
        {
            _battleClipSelectedTrackIndex = trackIndex;
            _battleClipSelectedKeyIndex = i;
            _battleClipCurrentFrame = frame;
            return;
        }
    }

    CameraKeyframe keyframe {};
    keyframe.time = keyTime;
    track->keyframes.push_back(keyframe);
    Sort_BattleClipTrackKeyframes(*track);

    _battleClipSelectedTrackIndex = trackIndex;
    _battleClipCurrentFrame = frame;

    for (int i = 0; i < static_cast<int>(track->keyframes.size()); ++i)
    {
        if (fabsf(track->keyframes[i].time - keyTime) <= 0.0001f)
        {
            _battleClipSelectedKeyIndex = i;
            break;
        }
    }
}

void Cinematic_Tool::Capture_KeyframeFromFreeCamera()
{
    CAMERAPOSE camPose{};
    if (!Try_CaptureActiveFreeCameraPose(camPose))
        return;

    CINEMATIC_TRACK* selectedTrack = Get_SelectedTrack();
    if (selectedTrack == nullptr)
        return;

    CINEMATIC_KEYFRAME keyframe{};
    keyframe.frame = _currentFrame;
    keyframe.camPose = camPose;

    selectedTrack->keyframes.push_back(keyframe);
    Sort_SelectedTrackKeyframes();

    for (int i = 0; i < static_cast<int>(selectedTrack->keyframes.size()); ++i)
    {
        if (selectedTrack->keyframes[i].frame == _currentFrame)
            _selectedKeyIndex = i;
    }

    _isPreviewDirty = true;
}

void Cinematic_Tool::Sort_SelectedModifierTrackKeyframes()
{
    CameraTrack* selectedTrack = Get_SelectedModifierTrack();
    if (selectedTrack == nullptr)
        return;

    const float selectedTime = (_selectedKeyIndex >= 0 &&
        _selectedKeyIndex < static_cast<int>(selectedTrack->keyframes.size()))
        ? selectedTrack->keyframes[_selectedKeyIndex].time
        : -1.f;

    std::sort(selectedTrack->keyframes.begin(), selectedTrack->keyframes.end(),
        [](const CameraKeyframe& lhs, const CameraKeyframe& rhs)
        {
            return lhs.time < rhs.time;
        });

    if (selectedTrack->keyframes.empty())
    {
        _selectedKeyIndex = -1;
        return;
    }

    if (selectedTime < 0.f)
    {
        _selectedKeyIndex = 0;
        return;
    }

    for (int i = 0; i < static_cast<int>(selectedTrack->keyframes.size()); ++i)
    {
        if (fabsf(selectedTrack->keyframes[i].time - selectedTime) <= 0.0001f)
        {
            _selectedKeyIndex = i;
            return;
        }
    }

    _selectedKeyIndex = 0;
}

void Cinematic_Tool::Insert_KeyframeAtFrame(int trackIndex, int frame)
{
    if (trackIndex >= static_cast<int>(_sequence.cinematicTracks.size()))
    {
        CameraTrack* modifierTrack = nullptr;

        const int modifierTrackIndex = trackIndex - static_cast<int>(_sequence.cinematicTracks.size());
        switch (modifierTrackIndex)
        {
        case 0: modifierTrack = &_sequence.modifierClip.shakeAmplitude; break;
        case 1: modifierTrack = &_sequence.modifierClip.shakeFrequencyX; break;
        case 2: modifierTrack = &_sequence.modifierClip.shakeFrequencyY; break;
        case 3: modifierTrack = &_sequence.modifierClip.shakeDecayRateX; break;
        case 4: modifierTrack = &_sequence.modifierClip.shakeDecayRate; break;
        case 5: modifierTrack = &_sequence.modifierClip.shakeVerticalRatio; break;
        case 6: modifierTrack = &_sequence.modifierClip.shakeStrengthScale; break;
        default: return;
        }

        const float keyTime = std::max(static_cast<float>(frame - _sequence.startFrame) / static_cast<float>(std::max(_sequence.fps, 1)), 0.f);
        for (int i = 0; i < static_cast<int>(modifierTrack->keyframes.size()); ++i)
        {
            if (fabsf(modifierTrack->keyframes[i].time - keyTime) <= 0.0001f)
            {
                _selectedTrackIndex = trackIndex;
                _selectedKeyIndex = i;
                _currentFrame = frame;
                _requestOpenSelectedKeyPopup = true;
                _isPreviewDirty = true;
                return;
            }
        }

        CameraKeyframe keyframe{};
        keyframe.time = keyTime;
        modifierTrack->keyframes.push_back(keyframe);
        _selectedTrackIndex = trackIndex;
        _currentFrame = frame;
        Sort_SelectedModifierTrackKeyframes();

        for (int i = 0; i < static_cast<int>(modifierTrack->keyframes.size()); ++i)
        {
            if (fabsf(modifierTrack->keyframes[i].time - keyTime) <= 0.0001f)
            {
                _selectedKeyIndex = i;
                break;
            }
        }

        _requestOpenSelectedKeyPopup = true;
        _isPreviewDirty = true;
        return;
    }

    if (trackIndex < 0 || trackIndex >= static_cast<int>(_sequence.cinematicTracks.size()))
        return;

    CINEMATIC_TRACK& track = _sequence.cinematicTracks[trackIndex];
    frame = std::clamp(frame, _sequence.startFrame, _sequence.endFrame);

    for (int i = 0; i < static_cast<int>(track.keyframes.size()); ++i)
    {
        if (track.keyframes[i].frame == frame)
        {
            _selectedTrackIndex = trackIndex;
            _selectedKeyIndex = i;
            _currentFrame = frame;
            _requestOpenSelectedKeyPopup = true;
            _isPreviewDirty = true;
            return;
        }
    }

    CINEMATIC_KEYFRAME keyframe{};
    keyframe.frame = frame;
    if (!_hasPendingInsertPose ||
        _pendingInsertTrackIndex != trackIndex ||
        _pendingInsertFrame != frame)
    {
        return;
    }

    keyframe.camPose = _pendingInsertPose;

    track.keyframes.push_back(keyframe);
    _selectedTrackIndex = trackIndex;
    _currentFrame = frame;
    Sort_SelectedTrackKeyframes();

    for (int i = 0; i < static_cast<int>(track.keyframes.size()); ++i)
    {
        if (track.keyframes[i].frame == frame)
        {
            _selectedKeyIndex = i;
            break;
        }
    }

    _requestOpenSelectedKeyPopup = true;
    _isPreviewDirty = true;
}

void Cinematic_Tool::Clear_AllSequenceKeyframes()
{
    _cinematicPlayer->Stop();
    Clear_PendingInsertRequest();

    for (auto& track : _sequence.cinematicTracks)
        track.keyframes.clear();
    _sequence.modifierClip = {};

    for (auto& track : _sequence.interactionTracks)
        track.keyframes.clear();

    _selectedKeyIndex = -1;
    _currentFrame = std::clamp(_currentFrame, _sequence.startFrame, _sequence.endFrame);
    _lastPreviewFrame = -1;
    _lastPreviewTrackIndex = -1;
    _lastPreviewKeyIndex = -1;
    _isPreviewDirty = true;
}

void Cinematic_Tool::Reset_ToNewSequence()
{
    _cinematicPlayer->Stop();
    Clear_PendingInsertRequest();

    _sequence = {};
    Ensure_DefaultTrack();
    _selectedTrackIndex = 0;
    _selectedKeyIndex = -1;
    _selectedSequenceFileIndex = -1;
    _currentFrame = std::clamp(_sequence.startFrame, _sequence.startFrame, _sequence.endFrame);
    _lastPreviewFrame = -1;
    _lastPreviewTrackIndex = -1;
    _lastPreviewKeyIndex = -1;
    _isPreviewDirty = true;
}

void Cinematic_Tool::Refresh_SequenceFileList()
{
    _sequenceFilePaths.clear();

    const fs::path sequenceFolder = "../../../Client/Bin/Resources/Data/json/Cinematics";
    if (!fs::exists(sequenceFolder))
        return;

    for (const auto& entry : fs::directory_iterator(sequenceFolder))
    {
        if (!entry.is_regular_file())
            continue;

        if (entry.path().extension() != ".json")
            continue;

        _sequenceFilePaths.push_back(entry.path());
    }

    std::sort(_sequenceFilePaths.begin(), _sequenceFilePaths.end());
}

void Cinematic_Tool::Load_SelectedSequenceFile()
{
    if (_selectedSequenceFileIndex < 0 ||
        _selectedSequenceFileIndex >= static_cast<int>(_sequenceFilePaths.size()))
    {
        return;
    }

    CINEMATIC_SEQUENCE loadedSequence{};
    const string filePath = _sequenceFilePaths[_selectedSequenceFileIndex].string();
    if (!Cinematic_SequenceIO::LoadSequenceFromFile(filePath, loadedSequence))
        return;

    _cinematicPlayer->Stop();
    Clear_PendingInsertRequest();
    _sequence = std::move(loadedSequence);

    Ensure_DefaultTrack();
    _currentFrame = std::clamp(_sequence.startFrame, _sequence.startFrame, _sequence.endFrame);
    _selectedTrackIndex = 0;
    _selectedKeyIndex = -1;
    Clamp_Selection();
    _lastPreviewFrame = -1;
    _lastPreviewTrackIndex = -1;
    _lastPreviewKeyIndex = -1;
    _isPreviewDirty = true;
}

void Cinematic_Tool::Apply_CurrentFramePose()
{
    Shared<FreeCamera> freeCamera = Get_ActiveFreeCamera();
    if(nullptr == freeCamera)
        return;

    CINEMATIC_TRACK* track = Get_SelectedTrack();
    if(nullptr == track)
        return;

    CINEMATIC_KEYFRAME* leftKey = nullptr;
    CINEMATIC_KEYFRAME* rightKey = nullptr;

    CAMERAPOSE camPose = Evaluate_CinematicPose(track, _currentFrame, leftKey, rightKey);
    const Client::CameraModifierSample modifierSample = Evaluate_CinematicModifierSample(_sequence, _currentFrame);
    const float sequenceTime = Evaluate_CinematicSequenceTime(_sequence, _currentFrame);

    freeCamera->Get_Transform()->Set_WorldPosition(camPose.position);
    freeCamera->Get_Transform()->Set_RotationEuler(camPose.rotation);
    freeCamera->Set_FovY(camPose.fovY);
    freeCamera->Set_Near(camPose.nearPlane);
    freeCamera->Set_Far(camPose.farPlane);

    freeCamera->Update_TransformMatrices();

    const Vec3 shakeOffset = Sample_CameraModifierShakeOffset(
        modifierSample,
        sequenceTime,
        freeCamera->Get_Transform()->Get_WorldRight(),
        freeCamera->Get_Transform()->Get_WorldUp());

    freeCamera->Get_Transform()->Set_WorldPosition(camPose.position + shakeOffset);
    freeCamera->Update_TransformMatrices();
    GAME->UpdateFrustum(*GAME->Get_Transform(D3DTS::View), *GAME->Get_Transform(D3DTS::Proj));
}

void Cinematic_Tool::Apply_CurrentBattleClipPreview()
{
    Shared<Client::BattleTurnCamera> battleTurnCamera = Get_BattleTurnCamera();
    if (battleTurnCamera == nullptr)
        return;

    BattleTurnCamera::BattleShotRequest request {};
    request.mode = _selectedBattleClipMode;
    request.subject = Resolve_BattleClipPreviewSubject();
    request.opponent = Resolve_BattleClipPreviewOpponent();

    if (request.subject == nullptr)
        return;

    _battleClipCurrentFrame = std::clamp(_battleClipCurrentFrame, 0, Get_BattleClipFrameMax(_editingBattleClip));
    const float previewTimeSec = static_cast<float>(_battleClipCurrentFrame) / kBattleClipPreviewFps;

    Apply_BattleClipPreviewAnimation(previewTimeSec);
    battleTurnCamera->Set_Clip(_selectedBattleClipMode, _editingBattleClip);
    battleTurnCamera->Set_IsClipPreview(true);
    battleTurnCamera->Preview_ClipAtTime(request, previewTimeSec);
    GAME->Change_ActiveCamera(battleTurnCamera);
}

CINEMATIC_TRACK* Cinematic_Tool::Get_SelectedTrack()
{
    if (_selectedTrackIndex < 0 || _selectedTrackIndex >= static_cast<int>(_sequence.cinematicTracks.size()))
        return nullptr;

    return &_sequence.cinematicTracks[_selectedTrackIndex];
}

CINEMATIC_KEYFRAME* Cinematic_Tool::Get_SelectedKeyframe()
{
    CINEMATIC_TRACK* selectedTrack = Get_SelectedTrack();
    if (selectedTrack == nullptr)
        return nullptr;

    if (_selectedKeyIndex < 0 || _selectedKeyIndex >= static_cast<int>(selectedTrack->keyframes.size()))
        return nullptr;

    return &selectedTrack->keyframes[_selectedKeyIndex];
}

bool Cinematic_Tool::Is_SelectedModifierTrack() const
{
    return _selectedTrackIndex >= static_cast<int>(_sequence.cinematicTracks.size()) &&
        _selectedTrackIndex < static_cast<int>(_sequence.cinematicTracks.size()) + CinematicSequencerAdapter::kModifierTrackCount;
}

int Cinematic_Tool::Get_SelectedModifierTrackIndex() const
{
    if (!Is_SelectedModifierTrack())
        return -1;

    return _selectedTrackIndex - static_cast<int>(_sequence.cinematicTracks.size());
}

CameraTrack* Cinematic_Tool::Get_SelectedModifierTrack()
{
    switch (Get_SelectedModifierTrackIndex())
    {
    case 0: return &_sequence.modifierClip.shakeAmplitude;
    case 1: return &_sequence.modifierClip.shakeFrequencyX;
    case 2: return &_sequence.modifierClip.shakeFrequencyY;
    case 3: return &_sequence.modifierClip.shakeDecayRateX;
    case 4: return &_sequence.modifierClip.shakeDecayRate;
    case 5: return &_sequence.modifierClip.shakeVerticalRatio;
    case 6: return &_sequence.modifierClip.shakeStrengthScale;
    default: return nullptr;
    }
}

CameraKeyframe* Cinematic_Tool::Get_SelectedModifierKeyframe()
{
    CameraTrack* selectedTrack = Get_SelectedModifierTrack();
    if (selectedTrack == nullptr)
        return nullptr;

    if (_selectedKeyIndex < 0 || _selectedKeyIndex >= static_cast<int>(selectedTrack->keyframes.size()))
        return nullptr;

    return &selectedTrack->keyframes[_selectedKeyIndex];
}

Shared<Client::BattleTurnCamera> Cinematic_Tool::Get_BattleTurnCamera() const
{
    Shared<Camera> camera = GAME->Get_CameraByType(ObjectType::BattleTurnCamera);
    if (camera == nullptr)
        return nullptr;

    return dynamic_pointer_cast<Client::BattleTurnCamera>(camera);
}

CameraTrack* Cinematic_Tool::Get_SelectedBattleClipTrack()
{
    switch (_battleClipSelectedTrackIndex)
    {
    case 0:
        return &_editingBattleClip.distanceOffset;
    case 1:
        return &_editingBattleClip.fovOffset;
    case 2:
        return &_editingBattleClip.shakeAmplitude;
    case 3:
        return &_editingBattleClip.sideOffset;
    case 4:
        return &_editingBattleClip.pivotOffset;
    case 5:
        return &_editingBattleClip.heightOffset;
    case 6:
        return &_editingBattleClip.aimOffset;
    case 7:
        return &_editingBattleClip.lookHeightOffset;
    case 8:
        return &_editingBattleClip.yawOffset;
    default:
        return nullptr;
    }
}

CameraKeyframe* Cinematic_Tool::Get_SelectedBattleClipKeyframe()
{
    CameraTrack* track = Get_SelectedBattleClipTrack();
    if (track == nullptr)
        return nullptr;

    if (_battleClipSelectedKeyIndex < 0 || _battleClipSelectedKeyIndex >= static_cast<int>(track->keyframes.size()))
        return nullptr;

    return &track->keyframes[_battleClipSelectedKeyIndex];
}

void Cinematic_Tool::Clear_PendingInsertRequest()
{
    _hasPendingInsertRequest = false;
    _hasPendingInsertPose = false;
    _pendingInsertTrackIndex = -1;
    _pendingInsertFrame = 0;
    _pendingInsertPose = {};
    _isPreviewDirty = true;
}

Shared<FreeCamera> Cinematic_Tool::Get_ActiveFreeCamera() const
{
    Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
    if (activeCamera == nullptr || activeCamera->Get_ObjectType() != ObjectType::FreeCamera)
        return nullptr;

    return dynamic_pointer_cast<FreeCamera>(activeCamera);
}

bool Cinematic_Tool::Try_CaptureActiveFreeCameraPose(CAMERAPOSE& outPose) const
{
    Shared<FreeCamera> freeCamera = Get_ActiveFreeCamera();
    if (freeCamera == nullptr)
        return false;

    outPose.position = freeCamera->Get_Transform()->Get_WorldPosition();
    outPose.rotation = freeCamera->Get_Transform()->Get_RotationEuler();
    outPose.fovY = freeCamera->Get_FovY();
    outPose.nearPlane = freeCamera->Get_Near();
    outPose.farPlane = freeCamera->Get_Far();
    return true;
}

Shared<Cinematic_Tool> Cinematic_Tool::Create()
{
    Shared<Cinematic_Tool> instance =  make_shared<Cinematic_Tool>();

    if(FAILED(instance->Initialize()))
    {
        instance = nullptr; 
    }
    return instance;
}
