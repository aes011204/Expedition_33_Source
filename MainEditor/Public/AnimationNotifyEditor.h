#pragma once

#include "Animation_View.h"

NS_BEGIN(MainEditor)

class AnimationNotifyEditor final
{
public:
    using SourceMode = Animation_View::SourceMode;
    using AnimationEntry = Animation_View::AnimationEntry;
    using NotifyClipboardKind = Animation_View::NotifyClipboardKind;
    using CopiedNotifyEntry = Animation_View::CopiedNotifyEntry;
    using CopiedNotifyStateEntry = Animation_View::CopiedNotifyStateEntry;
    using NotifyUndoSnapshot = Animation_View::NotifyUndoSnapshot;

public:
    explicit AnimationNotifyEditor(Animation_View& view);
    ~AnimationNotifyEditor() = default;

public:
    void Draw_Sequencer();
    void Draw_EventListPanel();
    void Draw_NotifyList();
    void Draw_StateList();
    void Draw_TrackSection();
    void Draw_CreateNotifyPopup();
    void Draw_CreateNotifyStatePopup();
    void Draw_SequencerContextMenu();

    void Add_NotifyTrack(const string& trackName);
    void Add_NotifyStateTrack(const string& trackName);
    void Add_Notify_AtFrame(int32 frame, const string& typeName);
    void Add_State_ByFrameRange(int32 startFrame, int32 endFrame, const string& typeName);
    void Delete_SelectedNotify();
    void Delete_SelectedState();
    void Delete_SelectedNotifyTrack();
    void Delete_SelectedNotifyStateTrack();

    NotifyUndoSnapshot Capture_NotifyUndoSnapshot() const;
    void Restore_NotifyUndoSnapshot(const NotifyUndoSnapshot& snapshot);
    void Commit_NotifyUndoSnapshot(const NotifyUndoSnapshot& snapshot);
    void Begin_PendingNotifyUndo();
    void Commit_PendingNotifyUndo();
    void Clear_NotifyUndoHistory();
    void Undo_NotifyEdit();
    void Redo_NotifyEdit();

    void Handle_ClipboardShortcuts();
    void Move_SelectedNotifiesByFrame(int32 frameDelta);
    void Move_SelectedStateByFrame(int32 frameDelta);
    void Copy_SelectedNotify();
    void Copy_SelectedState();
    void Paste_CopiedNotify();
    void Paste_CopiedState();

    void Begin_SequencerContextMenu(int32 frame, bool hasTrackContext, bool isStateTrack, int32 trackIndex);
    bool Try_GetSequencerTrackContext(const ImVec2& sequencerRectMin, const ImVec2& mousePos, bool& outIsStateTrack, int32& outTrackIndex) const;

    AnimNotifyEventEntry* Find_SelectedNotify();
    AnimNotifyStateEntry* Find_SelectedState();
    void Clear_SelectedNotifySelection();
    void Clear_SelectedStateSelection();
    bool Is_NotifySelected(int32 notifyId) const;
    bool Is_StateSelected(int32 stateId) const;

private:
    Animation_View& _view;
};

NS_END
