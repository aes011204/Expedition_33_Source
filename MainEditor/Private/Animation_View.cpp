#include "Animation_View.h"
#include "AnimSequencerAdapter.h"
#include "AnimNotify_Parser.h"
#include "EditorInstance.h"
#include "ModelCom.h"
#include "GameInstance.h"
#include <imgui.h>
#include <ImSequencer.h>

static float Resolve_AnimNotifyDataLengthSec(const AnimNotifyData* animationData)
{
    if (animationData == nullptr)
        return 5.f;

    float maxTimeSec = 0.f;

    for (const auto& entry : animationData->notifies)
        maxTimeSec = max(maxTimeSec, entry.timeSec);

    for (const auto& entry : animationData->notifyStates)
        maxTimeSec = max(maxTimeSec, entry.startSec + entry.durationSec);

    return max(5.f, maxTimeSec);
}

Animation_View::Animation_View()
    : Editor_Window(L"Animation")
{
    _sequencer = make_unique<AnimSequencerAdapter>(this);

    Set_Open(false);
}

void Animation_View::Update(float timeDelta)
{
    Editor_Window::Update(timeDelta);

    if (_sourceMode != SourceMode::LiveModel || _liveModelCom == nullptr)
         return;

    if (!_liveModelCom->Is_AnimationPaused())
        _sequencerCurrentFrame = static_cast<int32>(std::round(_liveModelCom->Get_CurrentAnimationTimeSec() * static_cast<float>(Get_CurrentAnimationFps())));
}

void Animation_View::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string windowName = String::ToString(Get_WindowName());

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        Draw_Header();
        ImGui::Separator();

        Draw_AnimationSelector();
        ImGui::Separator();

        Draw_PlaybackToolbar();
        ImGui::Separator();

        Draw_Sequencer();
        ImGui::Separator();

        Draw_SelectedDetail();
    }

    Set_Open(isOpen);
    ImGui::End();
}

bool Animation_View::CanSave() const
{
    return !_modelGuid.empty() && !_currentAnimationName.empty();
}

void Animation_View::Save()
{
    Save_NotifyAsset();
}

void Animation_View::Open_FromAsset(const wstring& modelPath, const string& modelGuid, const string& preferredAnimationName)
{
    _sourceMode = SourceMode::AssetOnly;
    _sourceModelPath = modelPath;
    _liveModelCom.reset();

    _modelGuid = modelGuid;
    _currentAnimationName = preferredAnimationName;
    _selectedNotifyId = -1;
    _selectedStateId = -1;
    _isDirty = false;

    Refresh_Source();
    Load_NotifyAsset();

    Set_Open(true);
}

void Animation_View::Open_FromModelComponent(const Shared<ModelCom>& modelCom, const string& preferredAnimationName)
{
    _sourceMode = SourceMode::LiveModel;
    _liveModelCom = modelCom;
    _sourceModelPath = modelCom ? modelCom->Get_SourceModelPath() : wstring{};

    _modelGuid = _sourceModelPath.empty() ? string{} : GAME->Ensure_AssetGUID(_sourceModelPath, "Model");
    _currentAnimationName = preferredAnimationName;
    _selectedNotifyId = -1;
    _selectedStateId = -1;
    _isDirty = false;

    Refresh_Source();
    Load_NotifyAsset();

    Set_Open(true);
}

int32 Animation_View::Get_CurrentAnimationFps() const
{
    if (const AnimNotifyData* animationData = Get_CurrentAnimation())
        return max(1, animationData->displayFps);

    return 60;
}

float Animation_View::Get_CurrentAnimationLengthSec() const
{
    if (_liveModelCom != nullptr)
        return _liveModelCom->Get_CurrentAnimationDurationSec();

    return Resolve_AnimNotifyDataLengthSec(Get_CurrentAnimation());
}

void Animation_View::Refresh_Source()
{
    _availableAnimationNames.clear();

    if (_sourceMode == SourceMode::LiveModel)
        Refresh_LiveAnimationList();
    else
        Refresh_AssetAnimationList();

    if (_currentAnimationName.empty() && !_availableAnimationNames.empty())
        _currentAnimationName = _availableAnimationNames.front();
}

void Animation_View::Refresh_AssetAnimationList()
{
    if (_sourceModelPath.empty())
        return;

    const fs::path modelDir = fs::path(_sourceModelPath).parent_path();
    if (!fs::exists(modelDir))
        return;

    for (const auto& entry : fs::directory_iterator(modelDir))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".anim")
            continue;

        _availableAnimationNames.push_back(entry.path().stem().string());
    }

    ::sort(_availableAnimationNames.begin(), _availableAnimationNames.end());
    _availableAnimationNames.erase(
        ::unique(_availableAnimationNames.begin(), _availableAnimationNames.end()),
        _availableAnimationNames.end());
}

void Animation_View::Refresh_LiveAnimationList()
{
    if (_liveModelCom == nullptr)
        return;

    const uint32 numAnimations = static_cast<uint32>(_liveModelCom->Get_NumAnimations());
    for (uint32 i = 0; i < numAnimations; ++i)
    {
        if (const char* animationName = _liveModelCom->Get_AnimationName(i))
            _availableAnimationNames.push_back(animationName);
    }
}

void Animation_View::Load_NotifyAsset()
{
    _asset = {};
    _asset.modelGuid = _modelGuid;

    const fs::path filePath = AnimNotify_Parser::Get_ModelNotifyFilePath(_modelGuid);
    AnimNotify_Parser::Load_FromFile(filePath.wstring(), _asset);

    if (_asset.modelGuid.empty())
        _asset.modelGuid = _modelGuid;

    if (!_currentAnimationName.empty())
        AnimNotify_Parser::Get_OrAddAnimation(_asset, _currentAnimationName);
}

void Animation_View::Save_NotifyAsset()
{
     if (_modelGuid.empty())
        return;

    if (AnimNotifyData* animationData = Get_CurrentAnimation())
        AnimNotify_Parser::Sort_And_Clamp(*animationData, Get_CurrentAnimationLengthSec());

    const fs::path filePath = AnimNotify_Parser::Get_ModelNotifyFilePath(_modelGuid);
    if (AnimNotify_Parser::Save_ToFile(filePath.wstring(), _asset))
        _isDirty = false;
}

void Animation_View::Draw_Header()
{
    ImGui::Text("Model GUID");
    ImGui::SameLine(120.f);
    ImGui::TextWrapped("%s", _modelGuid.empty() ? "(none)" : _modelGuid.c_str());

    ImGui::Text("Source");
    ImGui::SameLine(120.f);

    const char* sourceLabel = "None";
    if (_sourceMode == SourceMode::AssetOnly)
        sourceLabel = "Asset";
    else if (_sourceMode == SourceMode::LiveModel)
        sourceLabel = "LiveModel";

    ImGui::TextUnformatted(sourceLabel);

    ImGui::Text("Animation");
    ImGui::SameLine(120.f);
    ImGui::TextWrapped("%s", _currentAnimationName.empty() ? "(none)" : _currentAnimationName.c_str());
}

void Animation_View::Draw_AnimationSelector()
{
    const char* previewName = _currentAnimationName.empty() ? "(select animation)" : _currentAnimationName.c_str();

    ImGui::SetNextItemWidth(280.f);
    if (ImGui::BeginCombo("Animation", previewName))
    {
        for (const string& animationName : _availableAnimationNames)
        {
            const bool isSelected = (_currentAnimationName == animationName);

            if (ImGui::Selectable(animationName.c_str(), isSelected))
            {
                _currentAnimationName = animationName;
                AnimNotify_Parser::Get_OrAddAnimation(_asset, _currentAnimationName);

                _selectedNotifyId = -1;
                _selectedStateId = -1;
                _sequencerCurrentFrame = 0;
                _sequencerSelectedRow = -1;

                if (_sourceMode == SourceMode::LiveModel && _liveModelCom != nullptr)
                {
                    const uint32 numAnimations = static_cast<uint32>(_liveModelCom->Get_NumAnimations());
                    for (uint32 i = 0; i < numAnimations; ++i)
                    {
                        const char* currentName = _liveModelCom->Get_AnimationName(i);
                        if (currentName && animationName == currentName)
                        {
                            _liveModelCom->Set_Animation(i, true);
                            _liveModelCom->Set_AnimationPaused(true);
                            _liveModelCom->Set_CurrentAnimationTimeSec(0.f);
                            _liveModelCom->Refresh_CurrentAnimationPose();
                            break;
                        }
                    }
                }
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("FPS: %d", Get_CurrentAnimationFps());
}

void Animation_View::Draw_PlaybackToolbar()
{
    const bool isLive = (_sourceMode == SourceMode::LiveModel && _liveModelCom != nullptr);
    const float animationLengthSec = Get_CurrentAnimationLengthSec();
    const float sliderMaxTimeSec = max(0.01f, animationLengthSec);

    if (isLive)
    {
        if (ImGui::Button(_liveModelCom->Is_AnimationPaused() ? "Play" : "Pause"))
            _liveModelCom->Set_AnimationPaused(!_liveModelCom->Is_AnimationPaused());

        ImGui::SameLine();

        if (ImGui::Button("Stop"))
        {
            _liveModelCom->Set_AnimationPaused(true);
            _liveModelCom->Set_CurrentAnimationTimeSec(0.f);
            _liveModelCom->Refresh_CurrentAnimationPose();
            _sequencerCurrentFrame = 0;
        }

        ImGui::SameLine();

        float currentTimeSec = _liveModelCom->Get_CurrentAnimationTimeSec();
        ImGui::SetNextItemWidth(260.f);
        if (ImGui::SliderFloat("##AnimScrub", &currentTimeSec, 0.f, sliderMaxTimeSec, "%.3f s"))
        {
            _liveModelCom->Set_AnimationPaused(true);
            _liveModelCom->Set_CurrentAnimationTimeSec(currentTimeSec);
            _liveModelCom->Refresh_CurrentAnimationPose();
            _sequencerCurrentFrame = static_cast<int32>(std::round(currentTimeSec * static_cast<float>(Get_CurrentAnimationFps())));
        }
    }
    else
    {
        ImGui::BeginDisabled();
        ImGui::Button("Play");
        ImGui::SameLine();
        ImGui::Button("Stop");
        ImGui::SameLine();

        float dummyTimeSec = 0.f;
        ImGui::SetNextItemWidth(260.f);
        ImGui::SliderFloat("##AnimScrub", &dummyTimeSec, 0.f, sliderMaxTimeSec, "%.3f s");
        ImGui::EndDisabled();
    }

    ImGui::SameLine();

    if (ImGui::Button("Save"))
        Save_NotifyAsset();

    ImGui::SameLine();

    if (ImGui::Button("Add Notify"))
        Add_Notify_AtCurrentTime();

    ImGui::SameLine();

    if (ImGui::Button("Add NotifyState"))
        Add_State_AtCurrentTime();

    ImGui::SameLine();

    if (ImGui::Button("Delete Selected"))
        Delete_Selected();
}

void Animation_View::Draw_Sequencer()
{
    if (Get_CurrentAnimation() == nullptr)
    {
        ImGui::TextDisabled("No animation selected.");
        return;
    }

    int sequenceOptions =
        ImSequencer::SEQUENCER_EDIT_STARTEND |
        ImSequencer::SEQUENCER_CHANGE_FRAME;

    if (ImSequencer::Sequencer(
        _sequencer.get(),
        &_sequencerCurrentFrame,
        &_sequencerExpanded,
        &_sequencerSelectedRow,
        &_sequencerFirstFrame,
        sequenceOptions))
    {
        if (_sourceMode == SourceMode::LiveModel && _liveModelCom != nullptr)
        {
            const float currentTimeSec =
                static_cast<float>(_sequencerCurrentFrame) /
                static_cast<float>(max(1, Get_CurrentAnimationFps()));

            _liveModelCom->Set_AnimationPaused(true);
            _liveModelCom->Set_CurrentAnimationTimeSec(currentTimeSec);
            _liveModelCom->Refresh_CurrentAnimationPose();
        }
    }
}

void Animation_View::Draw_SelectedDetail()
{
    const float detailMaxTimeSec = max(0.01f, Get_CurrentAnimationLengthSec());

    if (AnimNotifyEventEntry* notify = Find_SelectedNotify())
    {
        char nameBuffer[128]{};
        strcpy_s(nameBuffer, notify->name.c_str());

        ImGui::Text("Selected Notify");

        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
        {
            notify->name = nameBuffer;
            _isDirty = true;
        }

        if (ImGui::DragFloat("Time", &notify->timeSec, 0.01f, 0.f, detailMaxTimeSec))
            _isDirty = true;

        return;
    }

    if (AnimNotifyStateEntry* notifyState = Find_SelectedState())
    {
        char nameBuffer[128]{};
        strcpy_s(nameBuffer, notifyState->name.c_str());

        ImGui::Text("Selected NotifyState");

        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
        {
            notifyState->name = nameBuffer;
            _isDirty = true;
        }

        if (ImGui::DragFloat("Start", &notifyState->startSec, 0.01f, 0.f, detailMaxTimeSec))
            _isDirty = true;

        if (ImGui::DragFloat("Duration", &notifyState->durationSec, 0.01f, 0.f, detailMaxTimeSec))
            _isDirty = true;

        return;
    }

    ImGui::TextDisabled("No notify selected.");
}

void Animation_View::Add_Notify_AtCurrentTime()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    const float currentTimeSec =
        (_sourceMode == SourceMode::LiveModel && _liveModelCom != nullptr)
        ? _liveModelCom->Get_CurrentAnimationTimeSec()
        : static_cast<float>(_sequencerCurrentFrame) / static_cast<float>(max(1, Get_CurrentAnimationFps()));

    AnimNotifyEventEntry entry{};
    entry.id = animationData->nextNotifyId++;
    entry.timeSec = currentTimeSec;
    entry.trackIndex = 0;
    entry.name = "NewNotify";

    animationData->notifies.push_back(std::move(entry));
    AnimNotify_Parser::Sort_And_Clamp(*animationData, Get_CurrentAnimationLengthSec());
    _isDirty = true;
}

void Animation_View::Add_State_AtCurrentTime()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    const float currentTimeSec =
        (_sourceMode == SourceMode::LiveModel && _liveModelCom != nullptr)
        ? _liveModelCom->Get_CurrentAnimationTimeSec()
        : static_cast<float>(_sequencerCurrentFrame) / static_cast<float>(max(1, Get_CurrentAnimationFps()));

    AnimNotifyStateEntry entry{};
    entry.id = animationData->nextStateId++;
    entry.startSec = currentTimeSec;
    entry.durationSec = 0.2f;
    entry.trackIndex = 0;
    entry.name = "NewNotifyState";

    animationData->notifyStates.push_back(std::move(entry));
    AnimNotify_Parser::Sort_And_Clamp(*animationData, Get_CurrentAnimationLengthSec());
    _isDirty = true;
}

void Animation_View::Delete_Selected()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    if (_selectedNotifyId != -1)
    {
        auto begin = animationData->notifies.begin();
        auto end = animationData->notifies.end();

        auto it = std::remove_if(
            begin,
            end,
            [this](const AnimNotifyEventEntry& entry)
            {
                return entry.id == _selectedNotifyId;
            });

        if (it != end)
        {
            animationData->notifies.erase(it, end);
            _selectedNotifyId = -1;
            _isDirty = true;
        }

        return;
    }

    if (_selectedStateId != -1)
    {
        auto begin = animationData->notifyStates.begin();
        auto end = animationData->notifyStates.end();

        auto it = std::remove_if(
            begin,
            end,
            [this](const AnimNotifyStateEntry& entry)
            {
                return entry.id == _selectedStateId;
            });

        if (it != end)
        {
            animationData->notifyStates.erase(it, end);
            _selectedStateId = -1;
            _isDirty = true;
        }
    }
}

AnimNotifyData* Animation_View::Get_CurrentAnimation()
{
    if (_currentAnimationName.empty())
        return nullptr;

    return AnimNotify_Parser::Find_Animation(_asset, _currentAnimationName);
}

const AnimNotifyData* Animation_View::Get_CurrentAnimation() const
{
    if (_currentAnimationName.empty())
        return nullptr;

    return AnimNotify_Parser::Find_Animation(_asset, _currentAnimationName);
}

AnimNotifyEventEntry* Animation_View::Find_SelectedNotify()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr || _selectedNotifyId == -1)
        return nullptr;

    for (auto& entry : animationData->notifies)
    {
        if (entry.id == _selectedNotifyId)
            return &entry;
    }

    return nullptr;
}

AnimNotifyStateEntry* Animation_View::Find_SelectedState()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr || _selectedStateId == -1)
        return nullptr;

    for (auto& entry : animationData->notifyStates)
    {
        if (entry.id == _selectedStateId)
            return &entry;
    }

    return nullptr;
}

Shared<Animation_View> Animation_View::Create()
{
    return make_shared<Animation_View>();
}
