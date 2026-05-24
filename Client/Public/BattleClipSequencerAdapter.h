#pragma once

#include <ImSequencer.h>
#include "BattleCameraClipTypes.h"

class BattleClipSequencerAdapter : public ImSequencer::SequenceInterface
{
public:
    BattleClipSequencerAdapter(
        BattleCameraClip* clip,
        int* selectedTrackIndex,
        int* selectedKeyIndex,
        int* currentFrame,
        bool* consumedKeyClick,
        bool* requestOpenInsertPopup,
        bool* requestRemoveKeyPopup,
        int* contextTrackIndex,
        int* contextFrame);

public:
    int GetFrameMin() const override;
    int GetFrameMax() const override;
    int GetItemCount() const override;
    const char* GetItemLabel(int index) const override;
    void Get(int index, int** start, int** end, int* type, unsigned int* color) override;
    size_t GetCustomHeight(int index) override;
    void CustomDraw(int index, ImDrawList* draw_list, const ImRect& rc, const ImRect& legendRect, const ImRect& clippingRect, const ImRect& legendClippingRect) override;
    void CustomDrawCompact(int index, ImDrawList* draw_list, const ImRect& rc, const ImRect& clippingRect) override;

private:
    BattleCameraClip* GetClip() const { return _clip; }

private:
    BattleCameraClip* _clip = nullptr;
    int* _selectedTrackIndex = nullptr;
    int* _selectedKeyIndex = nullptr;
    int* _currentFrame = nullptr;
    bool* _consumedKeyClick = nullptr;
    bool* _requestOpenInsertPopup = nullptr;
    bool* _requestRemoveKeyPopup = nullptr;
    int* _contextTrackIndex = nullptr;
    int* _contextFrame = nullptr;
    mutable int _frameMin = 0;
    mutable int _frameMax = 1;
};
