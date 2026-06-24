#include "AnimationNotifyEditor.h"

#include "AnimNotify.h"
#include "AnimNotifyState.h"
#include "AnimNotify_Parser.h"
#include "AnimSequencerAdapter.h"
#include "Editor_Macro.h"
#include "EditorInstance.h"
#include "GameInstance.h"
#include "Inspector.h"
#include "../Public/ImSequencer.h"
#include "ModelCom.h"
#include "Notification_Manager.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cctype>

namespace
{
    bool Contains_CaseInsensitive(const string& text, const char* filter)
    {
        if (filter == nullptr || filter[0] == '\0')
            return true;

        string source = text;
        string needle = filter;

        auto toLower = [](unsigned char value)
        {
            return static_cast<char>(std::tolower(value));
        };

        std::transform(source.begin(), source.end(), source.begin(), toLower);
        std::transform(needle.begin(), needle.end(), needle.begin(), toLower);

        return source.find(needle) != string::npos;
    }
}

NS_BEGIN(MainEditor)

AnimationNotifyEditor::AnimationNotifyEditor(Animation_View& view)
    : _view(view)
{
}

#define _sourceMode _view._sourceMode
#define _asset _view._asset
#define _modelGuid _view._modelGuid
#define _availableAnimations _view._availableAnimations
#define _currentAnimationName _view._currentAnimationName
#define _isDirty _view._isDirty
#define _isFocused _view._isFocused
#define _selectedNotifyId _view._selectedNotifyId
#define _selectedStateId _view._selectedStateId
#define _selectedNotifyTrackIndex _view._selectedNotifyTrackIndex
#define _selectedNotifyStateTrackIndex _view._selectedNotifyStateTrackIndex
#define _selectedNotifyIds _view._selectedNotifyIds
#define _selectedStateIds _view._selectedStateIds
#define _sequencer _view._sequencer
#define _sequencerCurrentFrame _view._sequencerCurrentFrame
#define _sequencerExpanded _view._sequencerExpanded
#define _sequencerSelectedRow _view._sequencerSelectedRow
#define _sequencerFirstFrame _view._sequencerFirstFrame
#define _sequencerVisibleFrameCount _view._sequencerVisibleFrameCount
#define _sequencerMarkingState _view._sequencerMarkingState
#define _sequencerMarkStartFrame _view._sequencerMarkStartFrame
#define _sequencerMarkEndFrame _view._sequencerMarkEndFrame
#define _requestedCreateStateStartFrame _view._requestedCreateStateStartFrame
#define _requestedCreateStateEndFrame _view._requestedCreateStateEndFrame
#define _openCreateNotifyPopup _view._openCreateNotifyPopup
#define _openCreateNotifyStatePopup _view._openCreateNotifyStatePopup
#define _openSequencerContextPopup _view._openSequencerContextPopup
#define _createNotifyFilter _view._createNotifyFilter
#define _createNotifyStateFilter _view._createNotifyStateFilter
#define _requestedCreateNotifyFrame _view._requestedCreateNotifyFrame
#define _sequencerContextFrame _view._sequencerContextFrame
#define _sequencerContextHasTrack _view._sequencerContextHasTrack
#define _sequencerContextIsStateTrack _view._sequencerContextIsStateTrack
#define _sequencerContextTrackIndex _view._sequencerContextTrackIndex
#define _newNotifyTrackName _view._newNotifyTrackName
#define _newNotifyStateTrackName _view._newNotifyStateTrackName
#define _notifyClipboardKind _view._notifyClipboardKind
#define _copiedNotifies _view._copiedNotifies
#define _copiedNotifyStates _view._copiedNotifyStates
#define _copiedNotifyState _view._copiedNotifyState
#define _notifyUndoStack _view._notifyUndoStack
#define _notifyRedoStack _view._notifyRedoStack
#define _pendingNotifyUndoSnapshot _view._pendingNotifyUndoSnapshot
#define _hasPendingNotifyUndoSnapshot _view._hasPendingNotifyUndoSnapshot

#define Get_CurrentAnimation() _view.Get_CurrentAnimation()
#define Get_CurrentAnimationFps() _view.Get_CurrentAnimationFps()
#define Get_CurrentAnimationFrameMax() _view.Get_CurrentAnimationFrameMax()
#define Get_CurrentAnimationLengthSec() _view.Get_CurrentAnimationLengthSec()
#define Get_PlaybackModel() _view.Get_PlaybackModel()
#define Get_PlaySectionStartSec(animationData) _view.Get_PlaySectionStartSec(animationData)
#define Get_PlaySectionEndSec(animationData) _view.Get_PlaySectionEndSec(animationData)
#define Update_PreviewRootControllerStates(timeDelta) _view.Update_PreviewRootControllerStates(timeDelta)
#define Reset_PreviewAnimSpeedStates() _view.Reset_PreviewAnimSpeedStates()
#define Reset_PreviewPlayEffectStates() _view.Reset_PreviewPlayEffectStates()
#define Reset_PreviewSLDodgeMoveStates() _view.Reset_PreviewSLDodgeMoveStates()
#define Reset_PreviewRootControllerStates() _view.Reset_PreviewRootControllerStates()
#define Reset_PreviewSourceHistoryEffect() _view.Reset_PreviewSourceHistoryEffect()
#define Reset_PreviewTrailEffect() _view.Reset_PreviewTrailEffect()
#define Reset_PreviewWeaponRotateStates() _view.Reset_PreviewWeaponRotateStates()
#define Reset_PreviewWeaponEchoStates() _view.Reset_PreviewWeaponEchoStates()
#define Find_AnimationEntryByAnyName Animation_View::Find_AnimationEntryByAnyName
#define Normalize_DefaultTrackName _view.Normalize_DefaultTrackName
#define Remove_NotifyTrackEntries Animation_View::Remove_NotifyTrackEntries
#define Remove_NotifyStateTrackEntries Animation_View::Remove_NotifyStateTrackEntries

void AnimationNotifyEditor::Draw_Sequencer()
{
    if (Get_CurrentAnimation() == nullptr)
    {
        ImGui::TextDisabled("No animation selected.");
        return;
    }

    int sequenceOptions =
        ImSequencer::SEQUENCER_EDIT_STARTEND |
        ImSequencer::SEQUENCER_CHANGE_FRAME;

    const int32 fullFrameMin = 0;
    const int32 fullFrameMax = max(1, Get_CurrentAnimationFrameMax());
    const int32 fullFrameRange = max(1, fullFrameMax - fullFrameMin);
    if (_sequencerVisibleFrameCount <= 0 || _sequencerVisibleFrameCount > fullFrameRange)
        _sequencerVisibleFrameCount = fullFrameRange;
    _sequencerFirstFrame = std::clamp(_sequencerFirstFrame, fullFrameMin, max(fullFrameMin, fullFrameMax - _sequencerVisibleFrameCount));

    const int32 sequencerItemCount = _sequencer != nullptr ? max(1, _sequencer->GetItemCount()) : 1;
    const float sequencerHeight = max(120.f, 20.f + 30.f * static_cast<float>(sequencerItemCount) + 10.f);
    ImVec2 sequencerRectMin = ImGui::GetCursorScreenPos();
    ImRect sequencerItemRect(sequencerRectMin, sequencerRectMin);
    bool sequencerHovered = false;
    bool sequencerFrameChanged = false;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    if (ImGui::BeginChild("AnimationSequencerFixedHeight", ImVec2(0.f, sequencerHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        sequencerRectMin = ImGui::GetCursorScreenPos();
        sequencerFrameChanged = ImSequencer::Sequencer(
            _sequencer.get(),
            &_sequencerCurrentFrame,
            &_sequencerExpanded,
            &_sequencerSelectedRow,
            &_sequencerFirstFrame,
            sequenceOptions,
            _sequencerFirstFrame,
            _sequencerFirstFrame + _sequencerVisibleFrameCount);

        sequencerItemRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        sequencerHovered = ImGui::IsItemHovered();

        const ImGuiIO& sequencerIO = ImGui::GetIO();
        if (sequencerHovered && sequencerIO.KeyCtrl && sequencerIO.MouseWheel != 0.f)
        {
            const float timelineMinX = sequencerItemRect.Min.x + 200.f;
            const float timelineMaxX = sequencerItemRect.Max.x;
            const float timelineWidth = max(1.f, timelineMaxX - timelineMinX);
            const float mouseRatio = std::clamp((sequencerIO.MousePos.x - timelineMinX) / timelineWidth, 0.f, 1.f);
            const int32 oldVisibleFrameCount = std::clamp(_sequencerVisibleFrameCount, 1, fullFrameRange);
            const int32 mouseFrame = _sequencerFirstFrame + static_cast<int32>(std::round(mouseRatio * static_cast<float>(oldVisibleFrameCount)));
            const float zoomScale = sequencerIO.MouseWheel > 0.f ? 0.82f : 1.22f;
            const int32 minVisibleFrameCount = min(fullFrameRange, 8);
            const int32 newVisibleFrameCount = std::clamp(
                static_cast<int32>(std::round(static_cast<float>(oldVisibleFrameCount) * zoomScale)),
                minVisibleFrameCount,
                fullFrameRange);

            const float pivotRatio = static_cast<float>(mouseFrame - _sequencerFirstFrame) / static_cast<float>(max(1, oldVisibleFrameCount));
            const int32 requestedFirstFrame = mouseFrame - static_cast<int32>(std::round(pivotRatio * static_cast<float>(newVisibleFrameCount)));
            _sequencerVisibleFrameCount = newVisibleFrameCount;
            _sequencerFirstFrame = std::clamp(requestedFirstFrame, fullFrameMin, max(fullFrameMin, fullFrameMax - _sequencerVisibleFrameCount));
        }
        else if (sequencerHovered && sequencerIO.KeyShift && !sequencerIO.KeyCtrl && sequencerIO.MouseWheel != 0.f)
        {
            const int32 scrollMax = max(fullFrameMin, fullFrameMax - _sequencerVisibleFrameCount);
            const int32 scrollStep = max(1, static_cast<int32>(std::round(static_cast<float>(_sequencerVisibleFrameCount) * 0.1f)));
            const int32 direction = sequencerIO.MouseWheel > 0.f ? -1 : 1;
            _sequencerFirstFrame = std::clamp(_sequencerFirstFrame + direction * scrollStep, fullFrameMin, scrollMax);
        }

        if (const AnimNotifyData* animationData = Get_CurrentAnimation())
        {
            const ImRect sequencerRect = sequencerItemRect;
            const float timelineMinX = sequencerRect.Min.x + 200.f;
            const float timelineMaxX = sequencerRect.Max.x;
            const float timelineWidth = max(1.f, timelineMaxX - timelineMinX);
            const int32 visibleFrameMin = _sequencerFirstFrame;
            const int32 visibleFrameMax = std::clamp(_sequencerFirstFrame + _sequencerVisibleFrameCount, visibleFrameMin + 1, fullFrameMax);
            const int32 visibleFrameRange = max(1, visibleFrameMax - visibleFrameMin);
            const int32 fps = max(1, Get_CurrentAnimationFps());

            const int32 startFrame = std::clamp(
                static_cast<int32>(std::round(Get_PlaySectionStartSec(animationData) * static_cast<float>(fps))),
                0,
                fullFrameMax);
            const int32 endFrame = std::clamp(
                static_cast<int32>(std::round(Get_PlaySectionEndSec(animationData) * static_cast<float>(fps))),
                0,
                fullFrameMax);

            auto frameToX = [&](int32 frame)
            {
                return timelineMinX + timelineWidth * (static_cast<float>(frame - visibleFrameMin) / static_cast<float>(visibleFrameRange));
            };

            const float startX = frameToX(startFrame);
            const float endX = frameToX(endFrame);
            const float clampedStartX = std::clamp(startX, timelineMinX, timelineMaxX);
            const float clampedEndX = std::clamp(endX, timelineMinX, timelineMaxX);

            const float markerY = sequencerRect.Min.y + 5.f;
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImU32 sectionOutside = IM_COL32(255, 72, 72, 24);
            const ImU32 sectionLine = IM_COL32(255, 82, 72, 210);
            const ImU32 sectionGuide = IM_COL32(255, 82, 72, 95);

            if (startFrame > visibleFrameMin)
            {
                drawList->AddRectFilled(
                    ImVec2(timelineMinX, sequencerRect.Min.y),
                    ImVec2(clampedStartX, sequencerRect.Max.y),
                    sectionOutside);
            }

            if (endFrame < visibleFrameMax)
            {
                drawList->AddRectFilled(
                    ImVec2(clampedEndX, sequencerRect.Min.y),
                    ImVec2(timelineMaxX, sequencerRect.Max.y),
                    sectionOutside);
            }

            const int32 sectionOverlapStart = max(startFrame, visibleFrameMin);
            const int32 sectionOverlapEnd = min(endFrame, visibleFrameMax);
            if (sectionOverlapEnd > sectionOverlapStart)
            {
                drawList->AddLine(
                    ImVec2(frameToX(sectionOverlapStart), markerY),
                    ImVec2(frameToX(sectionOverlapEnd), markerY),
                    sectionLine,
                    2.f);
            }

            if (startFrame >= visibleFrameMin && startFrame <= visibleFrameMax)
            {
                drawList->AddLine(
                    ImVec2(startX, sequencerRect.Min.y),
                    ImVec2(startX, sequencerRect.Max.y),
                    sectionGuide,
                    1.f);
            }

            if (endFrame >= visibleFrameMin && endFrame <= visibleFrameMax)
            {
                drawList->AddLine(
                    ImVec2(endX, sequencerRect.Min.y),
                    ImVec2(endX, sequencerRect.Max.y),
                    sectionGuide,
                    1.f);
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();

    if (sequencerFrameChanged)
    {
        Shared<ModelCom> playbackModel = Get_PlaybackModel();
        if (playbackModel != nullptr)
        {
            const float currentTimeSec =
                static_cast<float>(_sequencerCurrentFrame) /
                static_cast<float>(max(1, Get_CurrentAnimationFps()));

            playbackModel->Set_AnimationPaused(true);
            playbackModel->Set_CurrentAnimationTimeSec(currentTimeSec);
            Update_PreviewRootControllerStates(0.f);
            playbackModel->Play_Animation(0.f); // 시퀀서 프레임 점프에서도 preview state notify를 같이 갱신한다.
        }
    }

    const int32 scrollMaxFrame = max(fullFrameMin, fullFrameMax - _sequencerVisibleFrameCount);
    const float scrollbarOffsetX = 200.f;
    const float scrollbarHeight = 16.f;
    const float scrollbarWidth = max(1.f, sequencerItemRect.Max.x - (sequencerItemRect.Min.x + scrollbarOffsetX));
    const bool scrollbarEnabled = _sequencerVisibleFrameCount < fullFrameRange && scrollMaxFrame > fullFrameMin;

    ImGui::Dummy(ImVec2(scrollbarOffsetX, scrollbarHeight));
    ImGui::SameLine(0.f, 0.f);
    ImGui::InvisibleButton("##AnimationSequencerHorizontalScroll", ImVec2(scrollbarWidth, scrollbarHeight));

    const ImRect scrollbarRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    const bool scrollbarHovered = ImGui::IsItemHovered();
    const bool scrollbarActive = ImGui::IsItemActive();
    const ImGuiIO& scrollbarIO = ImGui::GetIO();
    const float visibleRatio = std::clamp(static_cast<float>(_sequencerVisibleFrameCount) / static_cast<float>(fullFrameRange), 0.f, 1.f);
    const float thumbWidth = std::clamp(scrollbarWidth * visibleRatio, min(34.f, scrollbarWidth), scrollbarWidth);
    const float thumbTravel = max(1.f, scrollbarWidth - thumbWidth);

    if (scrollbarEnabled && scrollbarActive)
    {
        const float dragRatio = std::clamp((scrollbarIO.MousePos.x - scrollbarRect.Min.x - thumbWidth * 0.5f) / thumbTravel, 0.f, 1.f);
        const int32 requestedFirstFrame = fullFrameMin + static_cast<int32>(std::round(dragRatio * static_cast<float>(scrollMaxFrame - fullFrameMin)));
        _sequencerFirstFrame = std::clamp(requestedFirstFrame, fullFrameMin, scrollMaxFrame);
    }

    const float scrollRatio = scrollMaxFrame > fullFrameMin
        ? std::clamp(static_cast<float>(_sequencerFirstFrame - fullFrameMin) / static_cast<float>(scrollMaxFrame - fullFrameMin), 0.f, 1.f)
        : 0.f;
    const float thumbMinX = scrollbarRect.Min.x + thumbTravel * scrollRatio;
    const ImRect thumbRect(
        ImVec2(thumbMinX, scrollbarRect.Min.y + 2.f),
        ImVec2(thumbMinX + thumbWidth, scrollbarRect.Max.y - 2.f));

    ImDrawList* scrollbarDrawList = ImGui::GetWindowDrawList();
    scrollbarDrawList->AddRectFilled(
        scrollbarRect.Min,
        scrollbarRect.Max,
        scrollbarEnabled ? IM_COL32(45, 47, 53, 255) : IM_COL32(38, 38, 42, 160),
        4.f);
    scrollbarDrawList->AddRect(
        scrollbarRect.Min,
        scrollbarRect.Max,
        IM_COL32(80, 82, 90, 180),
        4.f);
    scrollbarDrawList->AddRectFilled(
        thumbRect.Min,
        thumbRect.Max,
        !scrollbarEnabled ? IM_COL32(72, 72, 78, 160) :
            (scrollbarActive ? IM_COL32(118, 166, 255, 255) :
                (scrollbarHovered ? IM_COL32(92, 138, 224, 255) : IM_COL32(72, 112, 190, 255))),
        4.f);

    if (sequencerHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        const ImRect sequencerRect = sequencerItemRect;
        bool isStateTrack = false;
        int32 trackIndex = -1;
        const ImVec2 mousePos = ImGui::GetIO().MousePos;
        const bool hasTrackContext = Try_GetSequencerTrackContext(sequencerRectMin, mousePos, isStateTrack, trackIndex);
        const float normalized = std::clamp(
            (mousePos.x - (sequencerRect.Min.x + 200.f)) / max(1.f, sequencerRect.Max.x - (sequencerRect.Min.x + 200.f)),
            0.f,
            1.f);
        const int32 visibleFrameMin = _sequencerFirstFrame;
        const int32 visibleFrameMax = std::clamp(_sequencerFirstFrame + _sequencerVisibleFrameCount, visibleFrameMin + 1, fullFrameMax);
        const int32 clickedFrame = visibleFrameMin + static_cast<int32>(std::round(normalized * static_cast<float>(visibleFrameMax - visibleFrameMin)));
        Begin_SequencerContextMenu(clickedFrame, hasTrackContext, isStateTrack, trackIndex);
    }

    if (!_sequencerMarkingState &&
        _sequencerMarkStartFrame >= 0 &&
        _sequencerMarkEndFrame >= 0 &&
        _sequencerMarkStartFrame != _sequencerMarkEndFrame)
    {
        _requestedCreateStateStartFrame = min(_sequencerMarkStartFrame, _sequencerMarkEndFrame);
        _requestedCreateStateEndFrame = max(_sequencerMarkStartFrame, _sequencerMarkEndFrame);
        _openCreateNotifyStatePopup = true;
        _sequencerMarkStartFrame = -1;
        _sequencerMarkEndFrame = -1;
    }
}


void AnimationNotifyEditor::Draw_EventListPanel()
{
    if (Get_CurrentAnimation() == nullptr)
    {
        ImGui::TextDisabled("선택된 애니메이션 없음");
        return;
    }

    const float availableHeight = ImGui::GetContentRegionAvail().y;
    const float topHeight = max(120.f, availableHeight * 0.5f - 6.f);

    if (ImGui::BeginChild("NotifyListContainer", ImVec2(0.f, topHeight), false))
        Draw_NotifyList();
    ImGui::EndChild();

    ImGui::Spacing();

    if (ImGui::BeginChild("StateListContainer", ImVec2(0.f, 0.f), false))
        Draw_StateList();
    ImGui::EndChild();
}

void AnimationNotifyEditor::Draw_NotifyList()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    const int32 notifyCount = animationData ? static_cast<int32>(animationData->notifies.size()) : 0;

    ImGui::Text("Notifies (%d)", notifyCount);
    ImGui::Separator();

    if (animationData == nullptr)
    {
        ImGui::TextDisabled("No notify data.");
        return;
    }

    const int32 fps = max(1, Get_CurrentAnimationFps());
    for (int32 trackIndex = 0; trackIndex < static_cast<int32>(animationData->notifyTracks.size()); ++trackIndex)
    {
        const string trackLabel = animationData->notifyTracks[trackIndex].name.empty()
            ? "Notify Track " + to_string(trackIndex)
            : animationData->notifyTracks[trackIndex].name;

        if (!ImGui::TreeNode((trackLabel + "##NotifyTrackTree" + to_string(trackIndex)).c_str()))
            continue;

        if (ImGui::Selectable(("Select Track##NotifyTrackSelect" + to_string(trackIndex)).c_str(), _selectedNotifyTrackIndex == trackIndex))
        {
            _selectedNotifyTrackIndex = trackIndex;
            Clear_SelectedNotifySelection();
        }

        for (const auto& entry : animationData->notifies)
        {
            if (!entry.notify || entry.trackIndex != trackIndex)
                continue;

            const int32 frame = static_cast<int32>(std::round(entry.timeSec * fps));
            const string label = entry.notify->Get_TypeName() + " (" + to_string(frame) + ")";
            const bool isSelected = Is_NotifySelected(entry.id);
            if (ImGui::Selectable((label + "##NotifyEntry" + to_string(entry.id)).c_str(), isSelected))
            {
                const bool ctrlPressed = ImGui::GetIO().KeyCtrl;

                if (ctrlPressed)
                {
                    if (Is_NotifySelected(entry.id))
                    {
                        _selectedNotifyIds.erase(entry.id);
                        if (_selectedNotifyId == entry.id)
                            _selectedNotifyId = _selectedNotifyIds.empty() ? -1 : *_selectedNotifyIds.begin();
                    }
                    else
                    {
                        _selectedNotifyIds.insert(entry.id);
                        _selectedNotifyId = entry.id;
                    }
                }
                else
                {
                    _selectedNotifyIds.clear();
                    _selectedNotifyIds.insert(entry.id);
                    _selectedNotifyId = entry.id;
                }

                Clear_SelectedStateSelection();
                _selectedNotifyTrackIndex = trackIndex;
            }
        }

        ImGui::TreePop();
    }
}

void AnimationNotifyEditor::Draw_StateList()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    const int32 stateCount = animationData ? static_cast<int32>(animationData->notifyStates.size()) : 0;

    ImGui::Text("Notify States (%d)", stateCount);
    ImGui::Separator();

    if (animationData == nullptr)
    {
        ImGui::TextDisabled("No notify state data.");
        return;
    }

    const int32 fps = max(1, Get_CurrentAnimationFps());
    for (int32 trackIndex = 0; trackIndex < static_cast<int32>(animationData->notifyStateTracks.size()); ++trackIndex)
    {
        const string trackLabel = animationData->notifyStateTracks[trackIndex].name.empty()
            ? "Notify State Track " + to_string(trackIndex)
            : animationData->notifyStateTracks[trackIndex].name;

        if (!ImGui::TreeNode((trackLabel + "##NotifyStateTrackTree" + to_string(trackIndex)).c_str()))
            continue;

        if (ImGui::Selectable(("Select Track##NotifyStateTrackSelect" + to_string(trackIndex)).c_str(), _selectedNotifyStateTrackIndex == trackIndex))
        {
            _selectedNotifyStateTrackIndex = trackIndex;
            Clear_SelectedNotifySelection();
            Clear_SelectedStateSelection();
        }

        for (const auto& entry : animationData->notifyStates)
        {
            if (!entry.notifyState || entry.trackIndex != trackIndex)
                continue;

            const int32 startFrame = static_cast<int32>(std::round(entry.startSec * fps));
            const int32 endFrame = static_cast<int32>(std::round((entry.startSec + entry.durationSec) * fps));
            const string label = entry.notifyState->Get_TypeName() + " (" + to_string(startFrame) + "~" + to_string(endFrame) + ")";
            const bool isSelected = Is_StateSelected(entry.id);
            if (ImGui::Selectable((label + "##NotifyStateEntry" + to_string(entry.id)).c_str(), isSelected))
            {
                const bool ctrlPressed = ImGui::GetIO().KeyCtrl;

                if (ctrlPressed)
                {
                    if (Is_StateSelected(entry.id))
                    {
                        _selectedStateIds.erase(entry.id);
                        if (_selectedStateId == entry.id)
                            _selectedStateId = _selectedStateIds.empty() ? -1 : *_selectedStateIds.begin();
                    }
                    else
                    {
                        _selectedStateIds.insert(entry.id);
                        _selectedStateId = entry.id;
                    }
                }
                else
                {
                    _selectedStateIds.clear();
                    _selectedStateIds.insert(entry.id);
                    _selectedStateId = entry.id;
                }

                Clear_SelectedNotifySelection();
                _selectedNotifyStateTrackIndex = trackIndex;
            }
        }

        ImGui::TreePop();
    }
}

void AnimationNotifyEditor::Draw_TrackSection()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    if (!ImGui::BeginChild("AnimationTrackSectionPanel", ImVec2(0.f, 0.f), false))
    {
        ImGui::EndChild();
        return;
    }

    ImGui::SeparatorText("트랙 생성 / 관리");
    ImGui::TextDisabled("노티파이 트랙");
    char notifyTrackBuffer[128] = {};
    strcpy_s(notifyTrackBuffer, _newNotifyTrackName.c_str());
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##NotifyTrackInput", "새 노티파이 트랙 이름", notifyTrackBuffer, static_cast<size_t>(std::size(notifyTrackBuffer))))
        _newNotifyTrackName = notifyTrackBuffer;

    ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(56, 72, 48, 255));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(74, 92, 62, 255));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(88, 108, 72, 255));
    for (int32 i = 0; i < static_cast<int32>(animationData->notifyTracks.size()); ++i)
    {
        const bool selected = (_selectedNotifyTrackIndex == i);
        const string displayName = Normalize_DefaultTrackName(animationData->notifyTracks[i].name);
        if (ImGui::Selectable((displayName + "##NotifyTrack" + to_string(i)).c_str(), selected))
        {
            _selectedNotifyTrackIndex = i;
            Clear_SelectedNotifySelection();
        }
    }
    ImGui::PopStyleColor(3);

    if (ImGui::Button("노티파이 트랙 생성", ImVec2(-FLT_MIN, 0.f)))
        Add_NotifyTrack(_newNotifyTrackName);

    if (ImGui::Button("노티파이 트랙 삭제", ImVec2(-FLT_MIN, 0.f)))
        Delete_SelectedNotifyTrack();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("노티파이 스테이트 트랙");
    char notifyStateTrackBuffer[128] = {};
    strcpy_s(notifyStateTrackBuffer, _newNotifyStateTrackName.c_str());
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##NotifyStateTrackInput", "새 노티파이 스테이트 트랙 이름", notifyStateTrackBuffer, static_cast<size_t>(std::size(notifyStateTrackBuffer))))
        _newNotifyStateTrackName = notifyStateTrackBuffer;

    ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(66, 56, 38, 255));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(90, 74, 48, 255));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(108, 88, 56, 255));
    for (int32 i = 0; i < static_cast<int32>(animationData->notifyStateTracks.size()); ++i)
    {
        const bool selected = (_selectedNotifyStateTrackIndex == i);
        const string displayName = Normalize_DefaultTrackName(animationData->notifyStateTracks[i].name);
        if (ImGui::Selectable((displayName + "##NotifyStateTrack" + to_string(i)).c_str(), selected))
        {
            _selectedNotifyStateTrackIndex = i;
            Clear_SelectedNotifySelection();
            Clear_SelectedStateSelection();
        }
    }
    ImGui::PopStyleColor(3);

    if (ImGui::Button("노티파이 스테이트 트랙 생성", ImVec2(-FLT_MIN, 0.f)))
        Add_NotifyStateTrack(_newNotifyStateTrackName);

    if (ImGui::Button("노티파이 스테이트 트랙 삭제", ImVec2(-FLT_MIN, 0.f)))
        Delete_SelectedNotifyStateTrack();

    ImGui::EndChild();
}

void AnimationNotifyEditor::Add_NotifyTrack(const string& trackName)
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

    AnimNotifyTrackDesc trackDesc{};
    trackDesc.name = trackName.empty()
        ? "Notify"
        : trackName;

    animationData->notifyTracks.push_back(trackDesc);
    _selectedNotifyTrackIndex = static_cast<int32>(animationData->notifyTracks.size()) - 1;
    _isDirty = true;
    Commit_NotifyUndoSnapshot(undoSnapshot);
}

void AnimationNotifyEditor::Add_NotifyStateTrack(const string& trackName)
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

    AnimNotifyTrackDesc trackDesc{};
    trackDesc.name = trackName.empty()
        ? "Notify State"
        : trackName;

    animationData->notifyStateTracks.push_back(trackDesc);
    _selectedNotifyStateTrackIndex = static_cast<int32>(animationData->notifyStateTracks.size()) - 1;
    _isDirty = true;
    Commit_NotifyUndoSnapshot(undoSnapshot);
}

void AnimationNotifyEditor::Add_Notify_AtFrame(int32 frame, const string& typeName)
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    Shared<AnimNotify> notifyInstance = GAME->Create_AnimNotifyPrototype(typeName);
    if (notifyInstance == nullptr)
        return;

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

    if (animationData->notifyTracks.empty())
        animationData->notifyTracks.push_back({ "Notify" });

    const int32 fps = max(1, Get_CurrentAnimationFps());
    const int32 maxNotifyFrame = max(0, Get_CurrentAnimationFrameMax() - 1);
    const int32 clampedFrame = std::clamp(frame, 0, maxNotifyFrame);

    AnimNotifyEventEntry entry{};
    const int32 createdId = animationData->nextNotifyId++;
    entry.id = createdId;
    entry.timeSec = static_cast<float>(clampedFrame) / static_cast<float>(fps);
    entry.trackIndex = std::clamp(_sequencerContextHasTrack && !_sequencerContextIsStateTrack ? _sequencerContextTrackIndex : _selectedNotifyTrackIndex, 0, static_cast<int32>(animationData->notifyTracks.size()) - 1);
    entry.notify = notifyInstance;

    animationData->notifies.push_back(std::move(entry));
    AnimNotify_Parser::Sort_And_Clamp(*animationData, Get_CurrentAnimationLengthSec());
    _selectedNotifyIds.clear();
    _selectedNotifyIds.insert(createdId);
    _selectedNotifyId = createdId;
    _selectedNotifyTrackIndex = std::clamp(_sequencerContextHasTrack && !_sequencerContextIsStateTrack ? _sequencerContextTrackIndex : _selectedNotifyTrackIndex, 0, static_cast<int32>(animationData->notifyTracks.size()) - 1);
    _isDirty = true;
    Commit_NotifyUndoSnapshot(undoSnapshot);
}

void AnimationNotifyEditor::Add_State_ByFrameRange(int32 startFrame, int32 endFrame, const string& typeName)
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    Shared<AnimNotifyState> notifyStateInstance = GAME->Create_AnimNotifyStatePrototype(typeName);
    if (notifyStateInstance == nullptr)
        return;

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

    if (animationData->notifyStateTracks.empty())
        animationData->notifyStateTracks.push_back({ "Notify State" });

    const int32 fps = max(1, Get_CurrentAnimationFps());
    const int32 minFrame = min(startFrame, endFrame);
    const int32 maxFrame = max(startFrame, endFrame);

    AnimNotifyStateEntry entry{};
    const int32 createdId = animationData->nextStateId++;
    entry.id = createdId;
    entry.startSec = static_cast<float>(minFrame) / static_cast<float>(fps);
    entry.durationSec = static_cast<float>(max(1, maxFrame - minFrame)) / static_cast<float>(fps);
    entry.trackIndex = std::clamp(_sequencerContextHasTrack && _sequencerContextIsStateTrack ? _sequencerContextTrackIndex : _selectedNotifyStateTrackIndex, 0, static_cast<int32>(animationData->notifyStateTracks.size()) - 1);
    entry.notifyState = notifyStateInstance;

    animationData->notifyStates.push_back(std::move(entry));
    AnimNotify_Parser::Sort_And_Clamp(*animationData, Get_CurrentAnimationLengthSec());
    Clear_SelectedNotifySelection();
    _selectedStateIds.clear();
    _selectedStateIds.insert(createdId);
    _selectedStateId = createdId;
    _selectedNotifyStateTrackIndex = std::clamp(_sequencerContextHasTrack && _sequencerContextIsStateTrack ? _sequencerContextTrackIndex : _selectedNotifyStateTrackIndex, 0, static_cast<int32>(animationData->notifyStateTracks.size()) - 1);
    _isDirty = true;
    Commit_NotifyUndoSnapshot(undoSnapshot);
}

void AnimationNotifyEditor::Delete_SelectedNotify()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr || _selectedNotifyIds.empty())
        return;

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

    auto it = std::remove_if(
        animationData->notifies.begin(),
        animationData->notifies.end(),
        [this](const AnimNotifyEventEntry& entry)
        {
            return Is_NotifySelected(entry.id);
        });

    if (it != animationData->notifies.end())
    {
        animationData->notifies.erase(it, animationData->notifies.end());
        Clear_SelectedNotifySelection();
        _isDirty = true;
        Commit_NotifyUndoSnapshot(undoSnapshot);
    }
}

void AnimationNotifyEditor::Delete_SelectedState()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr || _selectedStateIds.empty())
        return;

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

    auto it = std::remove_if(
        animationData->notifyStates.begin(),
        animationData->notifyStates.end(),
        [this](const AnimNotifyStateEntry& entry)
        {
            return Is_StateSelected(entry.id);
        });

    if (it != animationData->notifyStates.end())
    {
        animationData->notifyStates.erase(it, animationData->notifyStates.end());
        Clear_SelectedStateSelection();
        _isDirty = true;
        Commit_NotifyUndoSnapshot(undoSnapshot);
    }
}

void AnimationNotifyEditor::Delete_SelectedNotifyTrack()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    if (_selectedNotifyTrackIndex < 0 || _selectedNotifyTrackIndex >= static_cast<int32>(animationData->notifyTracks.size()))
        return;

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

    Remove_NotifyTrackEntries(*animationData, _selectedNotifyTrackIndex);
    animationData->notifyTracks.erase(animationData->notifyTracks.begin() + _selectedNotifyTrackIndex);
    _selectedNotifyTrackIndex = animationData->notifyTracks.empty() ? 0 : min(_selectedNotifyTrackIndex, static_cast<int32>(animationData->notifyTracks.size()) - 1);
    Clear_SelectedNotifySelection();
    _isDirty = true;
    Commit_NotifyUndoSnapshot(undoSnapshot);
}

void AnimationNotifyEditor::Delete_SelectedNotifyStateTrack()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return;

    if (_selectedNotifyStateTrackIndex < 0 || _selectedNotifyStateTrackIndex >= static_cast<int32>(animationData->notifyStateTracks.size()))
        return;

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

    Remove_NotifyStateTrackEntries(*animationData, _selectedNotifyStateTrackIndex);
    animationData->notifyStateTracks.erase(animationData->notifyStateTracks.begin() + _selectedNotifyStateTrackIndex);
    _selectedNotifyStateTrackIndex = animationData->notifyStateTracks.empty() ? 0 : min(_selectedNotifyStateTrackIndex, static_cast<int32>(animationData->notifyStateTracks.size()) - 1);
    Clear_SelectedStateSelection();
    _isDirty = true;
    Commit_NotifyUndoSnapshot(undoSnapshot);
}

Animation_View::NotifyUndoSnapshot AnimationNotifyEditor::Capture_NotifyUndoSnapshot() const
{
    NotifyUndoSnapshot snapshot{};
    snapshot.assetJson = AnimNotify_Parser::To_Json(_asset);
    snapshot.currentAnimationName = _currentAnimationName;
    snapshot.selectedNotifyId = _selectedNotifyId;
    snapshot.selectedStateId = _selectedStateId;
    snapshot.selectedNotifyTrackIndex = _selectedNotifyTrackIndex;
    snapshot.selectedNotifyStateTrackIndex = _selectedNotifyStateTrackIndex;
    snapshot.sequencerCurrentFrame = _sequencerCurrentFrame;
    snapshot.selectedNotifyIds.assign(_selectedNotifyIds.begin(), _selectedNotifyIds.end());
    snapshot.selectedStateIds.assign(_selectedStateIds.begin(), _selectedStateIds.end());
    return snapshot;
}

void AnimationNotifyEditor::Restore_NotifyUndoSnapshot(const NotifyUndoSnapshot& snapshot)
{
    AnimNotifyAsset restoredAsset{};
    if (!AnimNotify_Parser::From_Json(snapshot.assetJson, restoredAsset))
        return;

    _asset = std::move(restoredAsset);
    if (_asset.modelGuid.empty())
        _asset.modelGuid = _modelGuid;

    _currentAnimationName = snapshot.currentAnimationName;
    if (!_currentAnimationName.empty())
    {
        if (const AnimationEntry* entry = Find_AnimationEntryByAnyName(_availableAnimations, _currentAnimationName))
            _currentAnimationName = entry->displayName;
    }

    _selectedNotifyId = snapshot.selectedNotifyId;
    _selectedStateId = snapshot.selectedStateId;
    _selectedNotifyTrackIndex = snapshot.selectedNotifyTrackIndex;
    _selectedNotifyStateTrackIndex = snapshot.selectedNotifyStateTrackIndex;
    _sequencerCurrentFrame = snapshot.sequencerCurrentFrame;

    _selectedNotifyIds.clear();
    for (const int32 notifyId : snapshot.selectedNotifyIds)
        _selectedNotifyIds.insert(notifyId);

    _selectedStateIds.clear();
    for (const int32 stateId : snapshot.selectedStateIds)
        _selectedStateIds.insert(stateId);

    if (Find_SelectedNotify() == nullptr)
        Clear_SelectedNotifySelection();

    if (Find_SelectedState() == nullptr)
        Clear_SelectedStateSelection();

    if (AnimNotifyData* animationData = Get_CurrentAnimation())
    {
        _selectedNotifyTrackIndex = animationData->notifyTracks.empty()
            ? 0
            : std::clamp(_selectedNotifyTrackIndex, 0, static_cast<int32>(animationData->notifyTracks.size()) - 1);
        _selectedNotifyStateTrackIndex = animationData->notifyStateTracks.empty()
            ? 0
            : std::clamp(_selectedNotifyStateTrackIndex, 0, static_cast<int32>(animationData->notifyStateTracks.size()) - 1);
    }

    Reset_PreviewAnimSpeedStates();
    Reset_PreviewPlayEffectStates();
    Reset_PreviewSLDodgeMoveStates();
    Reset_PreviewRootControllerStates();
    Reset_PreviewSourceHistoryEffect();
    Reset_PreviewTrailEffect();
    Reset_PreviewWeaponRotateStates();
    Reset_PreviewWeaponEchoStates();

    _isDirty = true;
}

void AnimationNotifyEditor::Commit_NotifyUndoSnapshot(const NotifyUndoSnapshot& snapshot)
{
    const NotifyUndoSnapshot currentSnapshot = Capture_NotifyUndoSnapshot();
    if (snapshot.assetJson == currentSnapshot.assetJson)
        return;

    static constexpr size_t MaxUndoCount = 64;
    _notifyUndoStack.push_back(snapshot);
    if (_notifyUndoStack.size() > MaxUndoCount)
        _notifyUndoStack.erase(_notifyUndoStack.begin());

    _notifyRedoStack.clear();
}

void AnimationNotifyEditor::Begin_PendingNotifyUndo()
{
    if (_hasPendingNotifyUndoSnapshot)
        return;

    _pendingNotifyUndoSnapshot = Capture_NotifyUndoSnapshot();
    _hasPendingNotifyUndoSnapshot = true;
}

void AnimationNotifyEditor::Commit_PendingNotifyUndo()
{
    if (!_hasPendingNotifyUndoSnapshot)
        return;

    Commit_NotifyUndoSnapshot(_pendingNotifyUndoSnapshot);
    _pendingNotifyUndoSnapshot = {};
    _hasPendingNotifyUndoSnapshot = false;
}

void AnimationNotifyEditor::Clear_NotifyUndoHistory()
{
    _notifyUndoStack.clear();
    _notifyRedoStack.clear();
    _pendingNotifyUndoSnapshot = {};
    _hasPendingNotifyUndoSnapshot = false;
}

void AnimationNotifyEditor::Undo_NotifyEdit()
{
    if (_notifyUndoStack.empty())
        return;

    NotifyUndoSnapshot currentSnapshot = Capture_NotifyUndoSnapshot();
    NotifyUndoSnapshot undoSnapshot = std::move(_notifyUndoStack.back());
    _notifyUndoStack.pop_back();
    _notifyRedoStack.push_back(std::move(currentSnapshot));

    Restore_NotifyUndoSnapshot(undoSnapshot);
}

void AnimationNotifyEditor::Redo_NotifyEdit()
{
    if (_notifyRedoStack.empty())
        return;

    NotifyUndoSnapshot currentSnapshot = Capture_NotifyUndoSnapshot();
    NotifyUndoSnapshot redoSnapshot = std::move(_notifyRedoStack.back());
    _notifyRedoStack.pop_back();
    _notifyUndoStack.push_back(std::move(currentSnapshot));

    Restore_NotifyUndoSnapshot(redoSnapshot);
}

void AnimationNotifyEditor::Handle_ClipboardShortcuts()
{
    if (!_isFocused)
        return;

    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput)
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
    {
        if (!_selectedNotifyIds.empty())
            Delete_SelectedNotify();
        else if (!_selectedStateIds.empty())
            Delete_SelectedState();
    }

    if (!io.KeyCtrl)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false))
        {
            if (!_selectedNotifyIds.empty())
                Move_SelectedNotifiesByFrame(-1);
            else if (!_selectedStateIds.empty())
                Move_SelectedStateByFrame(-1);
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false))
        {
            if (!_selectedNotifyIds.empty())
                Move_SelectedNotifiesByFrame(1);
            else if (!_selectedStateIds.empty())
                Move_SelectedStateByFrame(1);
        }
    }

    if (!io.KeyCtrl)
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
    {
        Undo_NotifyEdit();
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
    {
        Redo_NotifyEdit();
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_C, false))
    {
        if (!_selectedNotifyIds.empty())
            Copy_SelectedNotify();
        else if (!_selectedStateIds.empty())
            Copy_SelectedState();
    }

    if (ImGui::IsKeyPressed(ImGuiKey_V, false))
    {
        if (_notifyClipboardKind == NotifyClipboardKind::Notify)
            Paste_CopiedNotify();
        else if (_notifyClipboardKind == NotifyClipboardKind::State)
            Paste_CopiedState();
    }
}

void AnimationNotifyEditor::Move_SelectedNotifiesByFrame(int32 frameDelta)
{
    if (frameDelta == 0)
        return;

    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr || _selectedNotifyIds.empty())
        return;

    const int32 fps = max(1, Get_CurrentAnimationFps());
    const int32 maxNotifyFrame = max(0, Get_CurrentAnimationFrameMax() - 1);

    vector<pair<AnimNotifyEventEntry*, int32>> selectedEntries{};
    selectedEntries.reserve(_selectedNotifyIds.size());

    int32 minFrame = maxNotifyFrame;
    int32 maxFrame = 0;
    for (AnimNotifyEventEntry& entry : animationData->notifies)
    {
        if (!Is_NotifySelected(entry.id))
            continue;

        const int32 frame = std::clamp(
            static_cast<int32>(std::round(entry.timeSec * static_cast<float>(fps))),
            0,
            maxNotifyFrame);

        selectedEntries.emplace_back(&entry, frame);
        minFrame = min(minFrame, frame);
        maxFrame = max(maxFrame, frame);
    }

    if (selectedEntries.empty())
        return;

    const int32 clampedDelta = std::clamp(frameDelta, -minFrame, maxNotifyFrame - maxFrame);
    if (clampedDelta == 0)
        return;

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

    float primaryTimeSec = -1.f;
    for (const auto& [entry, frame] : selectedEntries)
    {
        const int32 movedFrame = frame + clampedDelta;
        entry->timeSec = static_cast<float>(movedFrame) / static_cast<float>(fps);

        if (entry->id == _selectedNotifyId)
            primaryTimeSec = entry->timeSec;
    }

    if (primaryTimeSec < 0.f)
        primaryTimeSec = selectedEntries.front().first->timeSec;

    AnimNotify_Parser::Sort_And_Clamp(*animationData, Get_CurrentAnimationLengthSec());

    if (primaryTimeSec >= 0.f)
    {
        _sequencerCurrentFrame = std::clamp(
            static_cast<int32>(std::round(primaryTimeSec * static_cast<float>(fps))),
            0,
            maxNotifyFrame);

        Shared<ModelCom> playbackModel = Get_PlaybackModel();
        if (_sourceMode == SourceMode::LiveModel && playbackModel != nullptr)
        {
            playbackModel->Set_AnimationPaused(true);
            playbackModel->Set_CurrentAnimationTimeSec(primaryTimeSec);
            playbackModel->Play_Animation(0.f);
        }
    }

    _isDirty = true;
    Commit_NotifyUndoSnapshot(undoSnapshot);
}

void AnimationNotifyEditor::Move_SelectedStateByFrame(int32 frameDelta)
{
    if (frameDelta == 0)
        return;

    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr || _selectedStateIds.empty())
        return;

    const int32 fps = max(1, Get_CurrentAnimationFps());
    const int32 maxFrame = max(0, Get_CurrentAnimationFrameMax());

    struct SelectedStateMoveEntry
    {
        AnimNotifyStateEntry* state = nullptr;
        int32 startFrame = 0;
    };

    vector<SelectedStateMoveEntry> selectedEntries{};
    selectedEntries.reserve(_selectedStateIds.size());

    int32 minStartFrame = maxFrame;
    int32 maxEndFrame = 0;
    for (AnimNotifyStateEntry& entry : animationData->notifyStates)
    {
        if (!Is_StateSelected(entry.id))
            continue;

        const int32 startFrame = std::clamp(
            static_cast<int32>(std::round(entry.startSec * static_cast<float>(fps))),
            0,
            maxFrame);
        const int32 durationFrames = max(
            1,
            static_cast<int32>(std::round(entry.durationSec * static_cast<float>(fps))));
        const int32 endFrame = min(maxFrame, startFrame + durationFrames);

        selectedEntries.push_back({ &entry, startFrame });
        minStartFrame = min(minStartFrame, startFrame);
        maxEndFrame = max(maxEndFrame, endFrame);
    }

    if (selectedEntries.empty())
        return;

    const int32 clampedDelta = std::clamp(frameDelta, -minStartFrame, maxFrame - maxEndFrame);
    if (clampedDelta == 0)
        return;

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

    int32 primaryStartFrame = -1;
    for (const SelectedStateMoveEntry& selectedEntry : selectedEntries)
    {
        const int32 movedStartFrame = selectedEntry.startFrame + clampedDelta;
        selectedEntry.state->startSec = static_cast<float>(movedStartFrame) / static_cast<float>(fps);

        if (selectedEntry.state->id == _selectedStateId)
            primaryStartFrame = movedStartFrame;
    }

    if (primaryStartFrame < 0)
        primaryStartFrame = selectedEntries.front().startFrame + clampedDelta;

    AnimNotify_Parser::Sort_And_Clamp(*animationData, Get_CurrentAnimationLengthSec());

    _sequencerCurrentFrame = primaryStartFrame;

    Shared<ModelCom> playbackModel = Get_PlaybackModel();
    if (_sourceMode == SourceMode::LiveModel && playbackModel != nullptr)
    {
        const float currentTimeSec = static_cast<float>(_sequencerCurrentFrame) / static_cast<float>(fps);
        playbackModel->Set_AnimationPaused(true);
        playbackModel->Set_CurrentAnimationTimeSec(currentTimeSec);
        playbackModel->Play_Animation(0.f);
    }

    _isDirty = true;
    Commit_NotifyUndoSnapshot(undoSnapshot);
}

void AnimationNotifyEditor::Copy_SelectedNotify()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr || _selectedNotifyIds.empty())
        return;

    _notifyClipboardKind = NotifyClipboardKind::Notify;
    _copiedNotifies.clear();
    _copiedNotifyStates.clear();
    _copiedNotifyState = {};

    for (const AnimNotifyEventEntry& entry : animationData->notifies)
    {
        if (!Is_NotifySelected(entry.id) || entry.notify == nullptr)
            continue;

        CopiedNotifyEntry copiedEntry{};
        copiedEntry.typeName = entry.notify->Get_TypeName();
        copiedEntry.payload = entry.notify->To_Json();
        copiedEntry.timeSec = entry.timeSec;
        copiedEntry.trackIndex = entry.trackIndex;
        _copiedNotifies.emplace_back(std::move(copiedEntry));
    }

    if (_copiedNotifies.empty())
        return;

    std::sort(_copiedNotifies.begin(), _copiedNotifies.end(),
        [](const CopiedNotifyEntry& a, const CopiedNotifyEntry& b)
        {
            if (a.timeSec != b.timeSec)
                return a.timeSec < b.timeSec;
            return a.trackIndex < b.trackIndex;
        });

    NOTIFY("노티파이 복사: {}개", static_cast<int32>(_copiedNotifies.size()));
}

void AnimationNotifyEditor::Copy_SelectedState()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr || _selectedStateIds.empty())
        return;

    _notifyClipboardKind = NotifyClipboardKind::State;
    _copiedNotifies.clear();
    _copiedNotifyStates.clear();
    _copiedNotifyState = {};

    for (const AnimNotifyStateEntry& entry : animationData->notifyStates)
    {
        if (!Is_StateSelected(entry.id) || entry.notifyState == nullptr)
            continue;

        CopiedNotifyStateEntry copiedEntry{};
        copiedEntry.typeName = entry.notifyState->Get_TypeName();
        copiedEntry.payload = entry.notifyState->To_Json();
        copiedEntry.startSec = entry.startSec;
        copiedEntry.durationSec = entry.durationSec;
        copiedEntry.trackIndex = entry.trackIndex;
        _copiedNotifyStates.emplace_back(std::move(copiedEntry));
    }

    if (_copiedNotifyStates.empty())
        return;

    std::sort(_copiedNotifyStates.begin(), _copiedNotifyStates.end(),
        [](const CopiedNotifyStateEntry& a, const CopiedNotifyStateEntry& b)
        {
            if (a.startSec != b.startSec)
                return a.startSec < b.startSec;
            return a.trackIndex < b.trackIndex;
        });

    _copiedNotifyState = _copiedNotifyStates.front();

    NOTIFY("노티파이 스테이트 복사: {}개", static_cast<int32>(_copiedNotifyStates.size()));
}

void AnimationNotifyEditor::Paste_CopiedNotify()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr || _copiedNotifies.empty())
        return;

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

    if (animationData->notifyTracks.empty())
        animationData->notifyTracks.push_back({ "Notify" });

    const int32 fps = max(1, Get_CurrentAnimationFps());
    const float baseTimeSec = _copiedNotifies.front().timeSec;
    const float animationLengthSec = Get_CurrentAnimationLengthSec();

    float maxRelativeTimeSec = 0.f;
    int32 baseTrackIndex = _copiedNotifies.front().trackIndex;
    int32 maxRelativeTrackIndex = 0;
    for (const CopiedNotifyEntry& copiedEntry : _copiedNotifies)
    {
        maxRelativeTimeSec = max(maxRelativeTimeSec, copiedEntry.timeSec - baseTimeSec);
        baseTrackIndex = min(baseTrackIndex, copiedEntry.trackIndex);
    }

    for (const CopiedNotifyEntry& copiedEntry : _copiedNotifies)
        maxRelativeTrackIndex = max(maxRelativeTrackIndex, copiedEntry.trackIndex - baseTrackIndex);

    const float requestedPasteTimeSec = static_cast<float>(_sequencerCurrentFrame) / static_cast<float>(fps);
    const float maxPasteTimeSec = max(0.f, animationLengthSec - maxRelativeTimeSec);
    const float pasteBaseTimeSec = std::clamp(requestedPasteTimeSec, 0.f, maxPasteTimeSec);

    const int32 notifyTrackCount = static_cast<int32>(animationData->notifyTracks.size());
    const int32 maxPasteTrackIndex = max(0, notifyTrackCount - 1 - maxRelativeTrackIndex);
    const int32 pasteBaseTrackIndex = std::clamp(_selectedNotifyTrackIndex, 0, maxPasteTrackIndex);

    _selectedNotifyIds.clear();
    int32 lastCreatedId = -1;
    int32 lastTrackIndex = _selectedNotifyTrackIndex;

    for (const CopiedNotifyEntry& copiedEntry : _copiedNotifies)
    {
        Shared<AnimNotify> notifyInstance = GAME->Create_AnimNotifyPrototype(copiedEntry.typeName);
        if (notifyInstance == nullptr)
            continue;

        notifyInstance->From_Json(copiedEntry.payload);

        AnimNotifyEventEntry entry{};
        const int32 createdId = animationData->nextNotifyId++;
        const float relativeTimeSec = copiedEntry.timeSec - baseTimeSec;
        const int32 relativeTrackIndex = copiedEntry.trackIndex - baseTrackIndex;
        entry.id = createdId;
        entry.timeSec = std::clamp(pasteBaseTimeSec + relativeTimeSec, 0.f, animationLengthSec);
        entry.trackIndex = std::clamp(pasteBaseTrackIndex + relativeTrackIndex, 0, notifyTrackCount - 1);
        entry.notify = notifyInstance;

        animationData->notifies.push_back(std::move(entry));
        _selectedNotifyIds.insert(createdId);
        lastCreatedId = createdId;
        lastTrackIndex = entry.trackIndex;
    }

    AnimNotify_Parser::Sort_And_Clamp(*animationData, Get_CurrentAnimationLengthSec());

    _selectedNotifyId = lastCreatedId;
    Clear_SelectedStateSelection();
    _selectedNotifyTrackIndex = lastTrackIndex;
    _isDirty = true;

    if (!_selectedNotifyIds.empty())
        NOTIFY("노티파이 붙여넣기: {}개", static_cast<int32>(_selectedNotifyIds.size()));

    Commit_NotifyUndoSnapshot(undoSnapshot);
}

void AnimationNotifyEditor::Paste_CopiedState()
{
    AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr || _copiedNotifyStates.empty())
        return;

    const NotifyUndoSnapshot undoSnapshot = Capture_NotifyUndoSnapshot();

    if (animationData->notifyStateTracks.empty())
        animationData->notifyStateTracks.push_back({ "Notify State" });

    const int32 fps = max(1, Get_CurrentAnimationFps());
    const float animationLengthSec = Get_CurrentAnimationLengthSec();
    const float baseStartSec = _copiedNotifyStates.front().startSec;

    float maxRelativeEndSec = 0.f;
    int32 baseTrackIndex = _copiedNotifyStates.front().trackIndex;
    int32 maxRelativeTrackIndex = 0;
    for (const CopiedNotifyStateEntry& copiedEntry : _copiedNotifyStates)
    {
        maxRelativeEndSec = max(maxRelativeEndSec, copiedEntry.startSec - baseStartSec + copiedEntry.durationSec);
        baseTrackIndex = min(baseTrackIndex, copiedEntry.trackIndex);
    }

    for (const CopiedNotifyStateEntry& copiedEntry : _copiedNotifyStates)
        maxRelativeTrackIndex = max(maxRelativeTrackIndex, copiedEntry.trackIndex - baseTrackIndex);

    const float requestedPasteStartSec = static_cast<float>(_sequencerCurrentFrame) / static_cast<float>(fps);
    const float maxPasteStartSec = max(0.f, animationLengthSec - maxRelativeEndSec);
    const float pasteBaseStartSec = std::clamp(requestedPasteStartSec, 0.f, maxPasteStartSec);

    const int32 stateTrackCount = static_cast<int32>(animationData->notifyStateTracks.size());
    const int32 maxPasteTrackIndex = max(0, stateTrackCount - 1 - maxRelativeTrackIndex);
    const int32 pasteBaseTrackIndex = std::clamp(_selectedNotifyStateTrackIndex, 0, maxPasteTrackIndex);

    Clear_SelectedNotifySelection();
    _selectedStateIds.clear();
    _selectedStateId = -1;

    int32 lastCreatedId = -1;
    int32 lastTrackIndex = _selectedNotifyStateTrackIndex;
    for (const CopiedNotifyStateEntry& copiedEntry : _copiedNotifyStates)
    {
        Shared<AnimNotifyState> notifyStateInstance = GAME->Create_AnimNotifyStatePrototype(copiedEntry.typeName);
        if (notifyStateInstance == nullptr)
            continue;

        notifyStateInstance->From_Json(copiedEntry.payload);

        AnimNotifyStateEntry entry{};
        const int32 createdId = animationData->nextStateId++;
        const float relativeStartSec = copiedEntry.startSec - baseStartSec;
        const int32 relativeTrackIndex = copiedEntry.trackIndex - baseTrackIndex;
        entry.id = createdId;
        entry.startSec = std::clamp(pasteBaseStartSec + relativeStartSec, 0.f, animationLengthSec);
        entry.durationSec = max(copiedEntry.durationSec, 1.f / static_cast<float>(fps));
        entry.trackIndex = std::clamp(pasteBaseTrackIndex + relativeTrackIndex, 0, stateTrackCount - 1);
        entry.notifyState = notifyStateInstance;

        animationData->notifyStates.push_back(std::move(entry));
        _selectedStateIds.insert(createdId);
        lastCreatedId = createdId;
        lastTrackIndex = entry.trackIndex;
    }

    if (_selectedStateIds.empty())
        return;

    AnimNotify_Parser::Sort_And_Clamp(*animationData, Get_CurrentAnimationLengthSec());

    _selectedStateId = lastCreatedId;
    _selectedNotifyStateTrackIndex = lastTrackIndex;
    _isDirty = true;

    if (!_selectedStateIds.empty())
        NOTIFY("노티파이 스테이트 붙여넣기: {}개", static_cast<int32>(_selectedStateIds.size()));

    Commit_NotifyUndoSnapshot(undoSnapshot);
}

void AnimationNotifyEditor::Begin_SequencerContextMenu(int32 frame, bool hasTrackContext, bool isStateTrack, int32 trackIndex)
{
    _sequencerContextFrame = frame;
    _sequencerContextHasTrack = hasTrackContext;
    _sequencerContextIsStateTrack = isStateTrack;
    _sequencerContextTrackIndex = trackIndex;
    _openSequencerContextPopup = true;
}

bool AnimationNotifyEditor::Try_GetSequencerTrackContext(const ImVec2& sequencerRectMin, const ImVec2& mousePos, bool& outIsStateTrack, int32& outTrackIndex) const
{
    const AnimNotifyData* animationData = Get_CurrentAnimation();
    if (animationData == nullptr)
        return false;

    constexpr float sequencerHeaderHeight = 20.f;
    constexpr float rowHeight = 30.f;

    const float localY = mousePos.y - (sequencerRectMin.y + sequencerHeaderHeight);
    if (localY < 0.f)
        return false;

    const int32 rowIndex = static_cast<int32>(localY / rowHeight);
    const int32 notifyTrackCount = max(1, static_cast<int32>(animationData->notifyTracks.size()));
    const int32 stateHeaderRow = 1 + notifyTrackCount;
    const int32 stateTrackCount = max(1, static_cast<int32>(animationData->notifyStateTracks.size()));

    if (rowIndex > 0 && rowIndex < stateHeaderRow)
    {
        outIsStateTrack = false;
        outTrackIndex = std::clamp(rowIndex - 1, 0, notifyTrackCount - 1);
        return true;
    }

    if (rowIndex > stateHeaderRow && rowIndex <= stateHeaderRow + stateTrackCount)
    {
        outIsStateTrack = true;
        outTrackIndex = std::clamp(rowIndex - stateHeaderRow - 1, 0, stateTrackCount - 1);
        return true;
    }

    return false;
}

void AnimationNotifyEditor::Draw_CreateNotifyPopup()
{
    const bool requestSearchFocus = _openCreateNotifyPopup;

    if (_openCreateNotifyPopup)
    {
        _openCreateNotifyPopup = false;
        _createNotifyFilter[0] = '\0';
        ImGui::OpenPopup("CreateNotifyPopup");
    }

    if (!ImGui::BeginPopup("CreateNotifyPopup"))
        return;

    ImGui::SetNextItemWidth(280.f);
    if (requestSearchFocus)
        ImGui::SetKeyboardFocusHere();
    ImGui::InputTextWithHint("##NotifySearch", "Search notify...", _createNotifyFilter, IM_ARRAYSIZE(_createNotifyFilter));
    ImGui::Separator();

    bool hasAny = false;
    bool hasVisible = false;
    GAME->Visit_AnimNotifyTypes([&](const TypeRegistry::AnimNotifyTypeInfo& info)
    {
        hasAny = true;

        if (!Contains_CaseInsensitive(info.className, _createNotifyFilter))
            return;

        hasVisible = true;
        if (ImGui::Selectable(info.className.c_str()))
        {
            Add_Notify_AtFrame(_requestedCreateNotifyFrame, info.className);
            ImGui::CloseCurrentPopup();
        }
    });

    if (!hasAny)
        ImGui::TextDisabled("No AnimNotify types registered.");
    else if (!hasVisible)
        ImGui::TextDisabled("No matching AnimNotify types.");

    ImGui::EndPopup();
}

void AnimationNotifyEditor::Draw_CreateNotifyStatePopup()
{
    const bool requestSearchFocus = _openCreateNotifyStatePopup;

    if (_openCreateNotifyStatePopup)
    {
        _openCreateNotifyStatePopup = false;
        _createNotifyStateFilter[0] = '\0';
        ImGui::OpenPopup("CreateNotifyStatePopup");
    }

    if (!ImGui::BeginPopup("CreateNotifyStatePopup"))
        return;

    ImGui::SetNextItemWidth(280.f);
    if (requestSearchFocus)
        ImGui::SetKeyboardFocusHere();
    ImGui::InputTextWithHint("##NotifyStateSearch", "Search notify state...", _createNotifyStateFilter, IM_ARRAYSIZE(_createNotifyStateFilter));
    ImGui::Separator();

    bool hasAny = false;
    bool hasVisible = false;
    GAME->Visit_AnimNotifyStateTypes([&](const TypeRegistry::AnimNotifyStateTypeInfo& info)
    {
        hasAny = true;

        if (!Contains_CaseInsensitive(info.className, _createNotifyStateFilter))
            return;

        hasVisible = true;
        if (ImGui::Selectable(info.className.c_str()))
        {
            Add_State_ByFrameRange(_requestedCreateStateStartFrame, _requestedCreateStateEndFrame, info.className);
            ImGui::CloseCurrentPopup();
        }
    });

    if (!hasAny)
        ImGui::TextDisabled("No AnimNotifyState types registered.");
    else if (!hasVisible)
        ImGui::TextDisabled("No matching AnimNotifyState types.");

    ImGui::EndPopup();
}

void AnimationNotifyEditor::Draw_SequencerContextMenu()
{
    if (_openSequencerContextPopup)
    {
        _openSequencerContextPopup = false;
        ImGui::OpenPopup("AnimationSequencerContextMenu");
    }

    if (!ImGui::BeginPopup("AnimationSequencerContextMenu"))
        return;

    ImGui::TextDisabled("Frame: %d", _sequencerContextFrame);
    ImGui::Separator();

    const bool canAddNotify = _sequencerContextHasTrack ? !_sequencerContextIsStateTrack : true;
    const bool canAddState = _sequencerContextHasTrack ? _sequencerContextIsStateTrack : true;

    if (ImGui::MenuItem("Notify Add", nullptr, false, canAddNotify))
    {
        _requestedCreateNotifyFrame = _sequencerContextFrame;
        _openCreateNotifyPopup = true;
        ImGui::CloseCurrentPopup();
    }

    if (ImGui::MenuItem("Notify State Add", nullptr, false, canAddState))
    {
        _requestedCreateStateStartFrame = _sequencerContextFrame;
        _requestedCreateStateEndFrame = min(Get_CurrentAnimationFrameMax(), _sequencerContextFrame + 12);
        _openCreateNotifyStatePopup = true;
        ImGui::CloseCurrentPopup();
    }

    if (ImGui::BeginMenu("Track Add"))
    {
        if (ImGui::MenuItem("Notify"))
        {
            Add_NotifyTrack(_newNotifyTrackName);
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::MenuItem("Notify State"))
        {
            Add_NotifyStateTrack(_newNotifyStateTrackName);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndMenu();
    }

    ImGui::EndPopup();
}



AnimNotifyEventEntry* AnimationNotifyEditor::Find_SelectedNotify()
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

AnimNotifyStateEntry* AnimationNotifyEditor::Find_SelectedState()
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

void AnimationNotifyEditor::Clear_SelectedNotifySelection()
{
    _selectedNotifyId = -1;
    _selectedNotifyIds.clear();
}

void AnimationNotifyEditor::Clear_SelectedStateSelection()
{
    _selectedStateId = -1;
    _selectedStateIds.clear();
}

bool AnimationNotifyEditor::Is_NotifySelected(int32 notifyId) const
{
    return _selectedNotifyIds.find(notifyId) != _selectedNotifyIds.end();
}

bool AnimationNotifyEditor::Is_StateSelected(int32 stateId) const
{
    return _selectedStateIds.find(stateId) != _selectedStateIds.end();
}


#undef _sourceMode
#undef _asset
#undef _modelGuid
#undef _availableAnimations
#undef _currentAnimationName
#undef _isDirty
#undef _selectedNotifyId
#undef _selectedStateId
#undef _selectedNotifyTrackIndex
#undef _selectedNotifyStateTrackIndex
#undef _selectedNotifyIds
#undef _selectedStateIds
#undef _sequencer
#undef _sequencerCurrentFrame
#undef _sequencerExpanded
#undef _sequencerSelectedRow
#undef _sequencerFirstFrame
#undef _sequencerVisibleFrameCount
#undef _sequencerMarkingState
#undef _sequencerMarkStartFrame
#undef _sequencerMarkEndFrame
#undef _requestedCreateStateStartFrame
#undef _requestedCreateStateEndFrame
#undef _openCreateNotifyPopup
#undef _openCreateNotifyStatePopup
#undef _openSequencerContextPopup
#undef _createNotifyFilter
#undef _createNotifyStateFilter
#undef _requestedCreateNotifyFrame
#undef _sequencerContextFrame
#undef _sequencerContextHasTrack
#undef _sequencerContextIsStateTrack
#undef _sequencerContextTrackIndex
#undef _newNotifyTrackName
#undef _newNotifyStateTrackName
#undef _notifyClipboardKind
#undef _copiedNotifies
#undef _copiedNotifyStates
#undef _copiedNotifyState
#undef _notifyUndoStack
#undef _notifyRedoStack
#undef _pendingNotifyUndoSnapshot
#undef _hasPendingNotifyUndoSnapshot
#undef Get_CurrentAnimation
#undef Get_CurrentAnimationFps
#undef Get_CurrentAnimationFrameMax
#undef Get_CurrentAnimationLengthSec
#undef Get_PlaybackModel
#undef Get_PlaySectionStartSec
#undef Get_PlaySectionEndSec
#undef Update_PreviewRootControllerStates
#undef Reset_PreviewAnimSpeedStates
#undef Reset_PreviewPlayEffectStates
#undef Reset_PreviewSLDodgeMoveStates
#undef Reset_PreviewRootControllerStates
#undef Reset_PreviewSourceHistoryEffect
#undef Reset_PreviewTrailEffect
#undef Reset_PreviewWeaponRotateStates
#undef Reset_PreviewWeaponEchoStates
#undef Find_AnimationEntryByAnyName
#undef Remove_NotifyTrackEntries
#undef Remove_NotifyStateTrackEntries

NS_END
