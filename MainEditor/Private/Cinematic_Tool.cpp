#include "Cinematic_Tool.h"
#include "Cinematic_Player.h"
#include "Cinematic_Utils.h"
#include "FreeCamera.h"
#include "GameInstance.h"
#include <algorithm>
#include "CinematicSequencerAdapter.h"
#include "Cinematic_SequenceIO.h"
#include "EditorInstance.h"

namespace
{
void Draw_SectionHeader(const char* title)
{
    ImGui::Spacing();
    ImGui::TextDisabled("%s", title);
    ImGui::Separator();
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

    if (_cinematicPlayer->Is_CinematicPlaying())
    {
        const int playbackFrame = _cinematicPlayer->Get_CurrentFrame();
        if (_currentFrame != playbackFrame)
        {
            _currentFrame = playbackFrame;
            _isPreviewDirty = true;
        }
    }


    if(_isPreviewDirty)
    {
        Apply_CurrentFramePose();
        _isPreviewDirty = false;
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
            ImGui::TextDisabled("Double click : edit selected key");
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

        Render_SelectedKeyframePopup();
        Render_InsertKeyframePopup();
    }

    Set_Open(isOpen);
    ImGui::End();
}

void Cinematic_Tool::Render_RightPanel()
{
    Render_PreviewState();
    Render_SequenceSettings();
    Render_TrackList();
    Render_KeyframeList();
    Render_LoadSequencePopup();
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
    int sequencerSelectedTrackIndex = _selectedTrackIndex;
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
        _requestOpenInsertKeyPopup = true;

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

    if (ImGui::Button("Add Track"))
    {
        CINEMATIC_TRACK track{};
        track.name = "Camera Track " + std::to_string(_sequence.cinematicTracks.size());
        _sequence.cinematicTracks.push_back(track);
        _selectedTrackIndex = static_cast<int>(_sequence.cinematicTracks.size()) - 1;
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
        return;

    char trackNameBuffer[128]{};
    strncpy_s(trackNameBuffer, selectedTrack->name.c_str(), _TRUNCATE);
    if (ImGui::InputText("track name", trackNameBuffer, static_cast<size_t>(_countof(trackNameBuffer))))
        selectedTrack->name = trackNameBuffer;

    if (ImGui::Checkbox("track enabled", &selectedTrack->enabled))
        _isPreviewDirty = true;
}

void Cinematic_Tool::Render_KeyframeList()
{
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

    const char* interpolationLabels[] = { "Step", "Linear" };
    int interpolationIndex = static_cast<int>(keyframe.interpolation);
    if (ImGui::Combo("interpolation", &interpolationIndex, interpolationLabels, IM_ARRAYSIZE(interpolationLabels)))
    {
        keyframe.interpolation = static_cast<InterpolationMode>(interpolationIndex);
        _isPreviewDirty = true;
    }
}

void Cinematic_Tool::Render_SelectedKeyframePopup()
{
    if (_requestOpenSelectedKeyPopup)
    {
        ImGui::OpenPopup("Edit Keyframe");
        _requestOpenSelectedKeyPopup = false;
    }
    
    if (!ImGui::BeginPopup("Edit Keyframe"))
        return;
    
    CINEMATIC_KEYFRAME* keyframe = Get_SelectedKeyframe();
    if (keyframe == nullptr)
    {
        ImGui::TextDisabled("No keyframe selected.");
    }
    else
    {
        ImGui::TextDisabled("Track %d | Key %d", _selectedTrackIndex, _selectedKeyIndex);
        ImGui::Separator();
        Render_SelectedKeyframeEditor(*keyframe);
    }
    
    if (ImGui::Button("Close"))
        ImGui::CloseCurrentPopup();
    
    ImGui::EndPopup();
}

void Cinematic_Tool::Render_InsertKeyframePopup()
{
    if (_requestOpenInsertKeyPopup)
    {
        ImGui::OpenPopup("Insert Keyframe");
        _requestOpenInsertKeyPopup = false;
    }

    if (!ImGui::BeginPopup("Insert Keyframe"))
        return;

    ImGui::TextDisabled("Track %d", _contextMenuTrackIndex);
    ImGui::TextDisabled("Frame %d", _contextMenuFrame);
    ImGui::Separator();

    if (ImGui::Button("Insert Keyframe Here"))
    {
        Insert_KeyframeAtFrame(_contextMenuTrackIndex, _contextMenuFrame);
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
            auto freeCamera = Find_FreeCamera();

            if(nullptr == freeCamera)
                return;

            _cinematicPlayer->Ready_CinematicPlayer(&_sequence, &_sequence.cinematicTracks[_selectedTrackIndex], freeCamera, true);
        }
        else
        {
            _cinematicPlayer->Stop();  
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
    if (_sequence.cinematicTracks.empty())
    {
        _selectedTrackIndex = 0;
        _selectedKeyIndex = -1;
        return;
    }

    _selectedTrackIndex = std::clamp(_selectedTrackIndex, 0, static_cast<int>(_sequence.cinematicTracks.size()) - 1);

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

void Cinematic_Tool::Capture_KeyframeFromFreeCamera()
{
    Shared<FreeCamera> freeCamera = Find_FreeCamera();
    if (freeCamera == nullptr)
        return;

    CINEMATIC_TRACK* selectedTrack = Get_SelectedTrack();
    if (selectedTrack == nullptr)
        return;

    CINEMATIC_KEYFRAME keyframe{};
    keyframe.frame = _currentFrame;
    keyframe.camPose.position = freeCamera->Get_Transform()->Get_Position();
    keyframe.camPose.rotation = freeCamera->Get_Transform()->Get_RotationEuler();
    keyframe.camPose.fovY = freeCamera->Get_FovY();
    keyframe.camPose.nearPlane = freeCamera->Get_Near();
    keyframe.camPose.farPlane = freeCamera->Get_Far();

    selectedTrack->keyframes.push_back(keyframe);
    Sort_SelectedTrackKeyframes();

    for (int i = 0; i < static_cast<int>(selectedTrack->keyframes.size()); ++i)
    {
        if (selectedTrack->keyframes[i].frame == _currentFrame)
            _selectedKeyIndex = i;
    }

    _isPreviewDirty = true;
}

void Cinematic_Tool::Insert_KeyframeAtFrame(int trackIndex, int frame)
{
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

        Shared<FreeCamera> freeCamera = Find_FreeCamera();
        if (freeCamera != nullptr)
        {
            keyframe.camPose.position = freeCamera->Get_Transform()->Get_Position();
            keyframe.camPose.rotation = freeCamera->Get_Transform()->Get_RotationEuler();
            keyframe.camPose.fovY = freeCamera->Get_FovY();
            keyframe.camPose.nearPlane = freeCamera->Get_Near();
            keyframe.camPose.farPlane = freeCamera->Get_Far();
        }

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
    Shared<FreeCamera> freeCamera = Find_FreeCamera();
    if(nullptr == freeCamera)
        return;

    CINEMATIC_TRACK* track = Get_SelectedTrack();
    if(nullptr == track)
        return;

    CINEMATIC_KEYFRAME* leftKey = nullptr;
    CINEMATIC_KEYFRAME* rightKey = nullptr;

    CAMERAPOSE camPose = Evaluate_CinematicPose(track, _currentFrame, leftKey, rightKey);

    freeCamera->Get_Transform()->Set_WorldPosition(camPose.position);
    freeCamera->Get_Transform()->Set_RotationEuler(camPose.rotation);
    freeCamera->Set_FovY(camPose.fovY);
    freeCamera->Set_Near(camPose.nearPlane);
    freeCamera->Set_Far(camPose.farPlane);
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

Shared<FreeCamera> Cinematic_Tool::Find_FreeCamera() const
{
    const int cameraCount = GAME->Get_CameraCount();
    for (int i = 0; i < cameraCount; ++i)
    {
        Shared<Camera> camera = GAME->Get_Camera(i);
        if (camera == nullptr || camera->Get_ObjectType() != ObjectType::FreeCamera)
            continue;

        Shared<Client::FreeCamera> freeCamera = dynamic_pointer_cast<FreeCamera>(camera);
        if (freeCamera != nullptr)
            return freeCamera;
    }

    return nullptr;
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
