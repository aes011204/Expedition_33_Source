#pragma once

#include "AnimNotify_Types.h"
#include "ImSequencer.h"

NS_BEGIN(MainEditor)

class Animation_View;

class AnimSequencerAdapter final : public ImSequencer::SequenceInterface
{
public:
    explicit AnimSequencerAdapter(Animation_View* view);
    ~AnimSequencerAdapter() override = default;

public:
    int GetFrameMin() const override;
    int GetFrameMax() const override;
    int GetViewFrameMin() const;
    int GetViewFrameMax() const;
    int GetItemCount() const override;
    const char* GetItemLabel(int index) const override;
    void Get(int index, int** start, int** end, int* type, unsigned int* color) override;
    void CustomDraw(int index, ImDrawList* drawList, const ImRect& rc, const ImRect& legendRect,
                    const ImRect& clippingRect, const ImRect& legendClippingRect) override;
    void BeginEdit(int index) override;
    void EndEdit() override;

private:
    enum class EStateDragMode : uint8
    {
        None,
        Move,
        ResizeStart,
        ResizeEnd
    };

    struct TrackColors
    {
        static constexpr unsigned int NotifyTrack = 0x2223A85A;
        static constexpr unsigned int StateTrack = 0x22FFA34D;

        static constexpr unsigned int NotifyFill = 0xFF33CC55;
        static constexpr unsigned int NotifyHover = 0xFF55EE77;
        static constexpr unsigned int NotifyBorder = 0xFF146B2D;

        static constexpr unsigned int StateFill = 0xFFFFA34D;
        static constexpr unsigned int StateHover = 0xFFFFBE79;
        static constexpr unsigned int StateBorder = 0xFF8A4F1B;

        static constexpr unsigned int SelectedBorder = 0xFF00FFFF;
        static constexpr unsigned int Text = 0xFFFFFFFF;
        static constexpr unsigned int StateHandle = 0xFF885522;
        static constexpr unsigned int StateHandleHover = 0xFFC5793D;
    };

    struct NotifyDragEntry
    {
        int32 id = -1;
        int32 startFrame = 0;
        int32 startTrackIndex = 0;
    };

    static int32 TimeSec_ToFrame(float timeSec, int32 fps);
    static float Frame_ToTimeSec(int32 frame, int32 fps);
    bool Resolve_TrackRow(int32 rowIndex, bool& outIsStateTrack, int32& outTrackIndex) const;

    void Draw_NotifyRow(int32 trackIndex, ImDrawList* drawList, const ImRect& rc);
    void Draw_StateRow(int32 trackIndex, ImDrawList* drawList, const ImRect& rc);
    void Handle_StateMarkGesture(int32 trackIndex, const ImRect& rc);

private:
    Animation_View* _view = nullptr;

    mutable vector<int> _rowStart{};
    mutable vector<int> _rowEnd{};
    mutable vector<string> _rowLabels{};

private:
    int32 _draggingNotifyId = -1;
    int32 _notifyDragStartFrame = 0;
    int32 _notifyDragStartTrackIndex = 0;
    vector<NotifyDragEntry> _notifyDragEntries{};
    int32 _draggingStateId = -1;
    float _stateDragOffsetX = 0.f;
    EStateDragMode _stateDragMode = EStateDragMode::None;
};

NS_END
