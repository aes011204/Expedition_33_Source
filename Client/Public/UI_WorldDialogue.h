#pragma once
#include "UI_RuntimePanelBase.h"

NS_BEGIN(Engine)
class UI_Image;
class UI_Text;
NS_END

NS_BEGIN(Client)

class UI_WorldDialogue final : public UI_RuntimePanelBase
{
public:
    struct LineDesc
    {
        wstring text{};
        wstring portraitTexturePath{};
    };

public:
    HRESULT Initialize(uint32 levelIndex) override;
    void    Update(float timeDelta) override;
    void    Close() override;

    void    Push_Line(const wstring& text, const wstring& portraitTexturePath = L"");
    void    Push_Line(const LineDesc& line);
    void    Clear_Lines();

private:
    struct SlotElements
    {
        Shared<UI_Base> portraitPanel{};
        Shared<UI_Image> portraitImage{};
        Shared<UI_Text> shortText{};
        Shared<UI_Text> longText{};
    };

    struct RuntimeLine
    {
        uint64 id = 0;
        wstring text{};
        wstring portraitTexturePath{};
    };

    struct SlotAnim
    {
        float fromY = 0.f;
        float targetY = 0.f;
    };

private:
    HRESULT Load_UI(uint32 levelIndex);
    HRESULT Bind_UI();

    void    Apply_Layout(bool immediate);
    void    Apply_AnimatedTransforms(float ratio);
    void    Show_Slot(size_t slotIndex, const RuntimeLine& line);
    void    Hide_Slot(size_t slotIndex);
    bool    Is_LongText(const wstring& text) const;
    float   Find_PreviousLineY(uint64 lineId) const;
    float   Get_CurrentTopLineY() const;
    float   Get_CurrentBgHeight() const;

private:
    static constexpr size_t kMaxVisibleLines = 3;

    Shared<UI_Base> _topLinePanel{};
    Shared<UI_Base> _bg{};
    array<SlotElements, kMaxVisibleLines> _slots{};

    deque<RuntimeLine> _lines{};
    vector<pair<uint64, float>> _previousLinePositions{};

    array<SlotAnim, kMaxVisibleLines> _slotAnims{};
    float _topLineFromY = 0.f;
    float _topLineTargetY = 0.f;
    float _bgFromHeight = 0.f;
    float _bgTargetHeight = 0.f;
    float _layoutAnimElapsed = 0.f;
    bool  _isLayoutAnimating = false;
    uint64 _nextLineId = 1;

public:
    static Unique<UI_WorldDialogue> Create(uint32 levelIndex);
    void Free() override;
};

NS_END
