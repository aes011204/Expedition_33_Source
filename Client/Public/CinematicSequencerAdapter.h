#pragma once
#include <ImSequencer.h>
#include "Cinematic_Types.h"

class CinematicSequencerAdapter : public ImSequencer::SequenceInterface
{
public:
    static constexpr int kModifierTrackCount = 7;
    static constexpr int kEventTrackCount = 1;

public:
    CinematicSequencerAdapter(
        CINEMATIC_SEQUENCE* sequence,
        int* selectedTrackIndex,
        int* selectedKeyIndex,
        int* selectedEventIndex,
        int* selectedActorTransformTrackIndex,
        int* selectedActorTransformKeyIndex,
        int* currentFrame,
        bool* consumedKeyClick,
        bool* requestOpenKeyPopup,
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
    CINEMATIC_SEQUENCE* _sequence = nullptr;
    int* _selectedTrackIndex = nullptr;
    int* _selectedKeyIndex = nullptr;
    int* _selectedEventIndex = nullptr;
    int* _selectedActorTransformTrackIndex = nullptr;
    int* _selectedActorTransformKeyIndex = nullptr;
    int* _currentFrame = nullptr;
    bool* _consumedKeyClick = nullptr;
    bool* _requestOpenKeyPopup = nullptr;
    bool* _requestOpenInsertPopup = nullptr;
    bool* _requestRemoveKeyPopup = nullptr;
    int* _contextTrackIndex = nullptr;
    int* _contextFrame = nullptr;
};

