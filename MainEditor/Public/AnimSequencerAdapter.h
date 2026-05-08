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
    int GetItemCount() const override;
    const char* GetItemLabel(int index) const override;
    void Get(int index, int** start, int** end, int* type, unsigned int* color) override;
    void CustomDraw(int index, ImDrawList* drawList, const ImRect& rc, const ImRect& legendRect,
                    const ImRect& clippingRect, const ImRect& legendClippingRect) override;
    void BeginEdit(int index) override;
    void EndEdit() override;

private:
    static int32 TimeSec_ToFrame(float timeSec, int32 fps);
    static float Frame_ToTimeSec(int32 frame, int32 fps);

    void Draw_NotifyRow(ImDrawList* drawList, const ImRect& rc);
    void Draw_StateRow(ImDrawList* drawList, const ImRect& rc);

private:
    Animation_View* _view = nullptr;

    int _rowStart[2]{ 0, 0 };
    int _rowEnd[2]{ 0, 0 };
};

NS_END
