#include "UI_WorldDialogue.h"

#include "Client_Constants.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

namespace
{
    constexpr wchar_t kRootUIName[] = L"Panel_WorldDialogue";
    constexpr wchar_t kTopLineUIName[] = L"Panel_WorldDialogue_Top_Line";
    constexpr wchar_t kBgUIName[] = L"Image_WorldDialogue_BG";

    constexpr float kAnimationDurationSec = 0.22f;
    constexpr size_t kShortTextMaxLength = 26;

    constexpr array<float, 4> kTopLineYByCount =
    {
        190.f,
        190.f,
        106.75f,
        23.5f,
    };

    constexpr array<float, 4> kBgHeightByCount =
    {
        83.04f,
        83.04f,
        166.742f,
        249.782f,
    };

    constexpr array<float, 3> kSlotY =
    {
        233.f,
        150.f,
        67.f,
    };

    constexpr array<const wchar_t*, 3> kPortraitPanelNames =
    {
        L"Panel_WorldDialogue_Portrait_First",
        L"Panel_WorldDialogue_Portrait_Second",
        L"Panel_WorldDialogue_Portrait_Third",
    };

    constexpr array<const wchar_t*, 3> kPortraitImageNames =
    {
        L"Image_WorldDialogue_Portrait_First_Circle_Portrait",
        L"Image_WorldDialogue_Portrait_Second_Circle_Portrait",
        L"Image_WorldDialogue_Portrait_Third_Circle_Portrait",
    };

    constexpr array<const wchar_t*, 3> kShortTextNames =
    {
        L"Text_WorldDialogue_First_Short_Dialogue",
        L"Text_WorldDialogue_Second_Short_Dialogue",
        L"Text_WorldDialogue_Third_Short_Dialogue",
    };

    constexpr array<const wchar_t*, 3> kLongTextNames =
    {
        L"Text_WorldDialogue_First_Long_Dialogue",
        L"Text_WorldDialogue_Second_Long_Dialogue",
        L"Text_WorldDialogue_Third_Long_Dialogue",
    };

    float SmoothStep(float value)
    {
        const float t = min(max(value, 0.f), 1.f);
        return t * t * (3.f - 2.f * t);
    }

    float Lerp(float from, float to, float ratio)
    {
        return from + (to - from) * ratio;
    }
}

HRESULT UI_WorldDialogue::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Clear_Lines();

    return S_OK;
}

void UI_WorldDialogue::Update(float timeDelta)
{
    if (!_isLayoutAnimating)
        return;

    _layoutAnimElapsed += timeDelta;
    const float ratio = SmoothStep(_layoutAnimElapsed / kAnimationDurationSec);
    Apply_AnimatedTransforms(ratio);

    if (ratio >= 1.f)
        _isLayoutAnimating = false;
}

void UI_WorldDialogue::Close()
{
    Clear_Lines();
}

void UI_WorldDialogue::Push_Line(const wstring& text, const wstring& portraitTexturePath)
{
    LineDesc line{};
    line.text = text;
    line.portraitTexturePath = portraitTexturePath;
    Push_Line(line);
}

void UI_WorldDialogue::Push_Line(const LineDesc& line)
{
    if (line.text.empty())
        return;

    _previousLinePositions.clear();
    for (size_t slotIndex = 0; slotIndex < _slots.size(); ++slotIndex)
    {
        const size_t lineIndexFromBottom = slotIndex;
        if (lineIndexFromBottom >= _lines.size())
            continue;

        const size_t lineIndex = _lines.size() - 1 - lineIndexFromBottom;
        _previousLinePositions.push_back({ _lines[lineIndex].id, kSlotY[slotIndex] });
    }

    RuntimeLine runtimeLine{};
    runtimeLine.id = _nextLineId++;
    runtimeLine.text = line.text;
    runtimeLine.portraitTexturePath = line.portraitTexturePath;

    _lines.push_back(runtimeLine);
    while (_lines.size() > kMaxVisibleLines)
        _lines.pop_front();

    if (!Is_Open())
        UI_RuntimePanelBase::Open();

    Apply_Layout(false);
}

void UI_WorldDialogue::Clear_Lines()
{
    _lines.clear();
    _previousLinePositions.clear();
    _isLayoutAnimating = false;
    _layoutAnimElapsed = 0.f;

    for (size_t slotIndex = 0; slotIndex < _slots.size(); ++slotIndex)
    {
        Hide_Slot(slotIndex);

        if (_slots[slotIndex].portraitPanel && _slots[slotIndex].portraitPanel->Get_Transform())
        {
            _slots[slotIndex].portraitPanel->Get_Transform()->Set_Position(Vec2(
                _slots[slotIndex].portraitPanel->Get_Transform()->Get_Position().x,
                kSlotY[slotIndex]));
            _slots[slotIndex].portraitPanel->Refresh_OpenBaseTransform();
        }
    }

    if (_topLinePanel && _topLinePanel->Get_Transform())
    {
        _topLinePanel->Get_Transform()->Set_Position(Vec2(
            _topLinePanel->Get_Transform()->Get_Position().x,
            kTopLineYByCount[1]));
        _topLinePanel->Refresh_OpenBaseTransform();
        _topLinePanel->Close();
    }

    if (_bg && _bg->Get_Transform())
    {
        _bg->Get_Transform()->Set_Size(Vec2(
            _bg->Get_Transform()->Get_Size().x,
            kBgHeightByCount[1]));
        _bg->Refresh_OpenBaseTransform();
        _bg->Close();
    }

    UI_RuntimePanelBase::Close();
}

HRESULT UI_WorldDialogue::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_WorldDialogue;
    desc.prototypeTagPrefix = L"WorldDialogue";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = UI_Utils::Find_UI(_uiByName, kRootUIName);
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_WorldDialogue::Bind_UI()
{
    _topLinePanel = UI_Utils::Find_UI(_uiByName, kTopLineUIName);
    _bg = UI_Utils::Find_UI(_uiByName, kBgUIName);

    CHECK_NULL(_topLinePanel, E_FAIL);
    CHECK_NULL(_bg, E_FAIL);

    for (size_t slotIndex = 0; slotIndex < _slots.size(); ++slotIndex)
    {
        SlotElements& slot = _slots[slotIndex];
        slot.portraitPanel = UI_Utils::Find_UI(_uiByName, kPortraitPanelNames[slotIndex]);
        slot.portraitImage = UI_Utils::Find_TypedUI<UI_Image>(_uiByName, kPortraitImageNames[slotIndex]);
        slot.shortText = UI_Utils::Find_TypedUI<UI_Text>(_uiByName, kShortTextNames[slotIndex]);
        slot.longText = UI_Utils::Find_TypedUI<UI_Text>(_uiByName, kLongTextNames[slotIndex]);

        CHECK_NULL(slot.portraitPanel, E_FAIL);
        CHECK_NULL(slot.portraitImage, E_FAIL);
        CHECK_NULL(slot.shortText, E_FAIL);
        CHECK_NULL(slot.longText, E_FAIL);

        slot.longText->Set_TextWrap(true);
    }

    return S_OK;
}

void UI_WorldDialogue::Apply_Layout(bool immediate)
{
    const size_t lineCount = min(_lines.size(), kMaxVisibleLines);
    if (lineCount == 0)
        return;

    if (_root && !_root->Is_Open())
        _root->Open();

    if (_topLinePanel && !_topLinePanel->Is_Open())
        _topLinePanel->Open();

    if (_bg && !_bg->Is_Open())
        _bg->Open();

    _topLineFromY = Get_CurrentTopLineY();
    _topLineTargetY = kTopLineYByCount[lineCount];
    _bgFromHeight = Get_CurrentBgHeight();
    _bgTargetHeight = kBgHeightByCount[lineCount];

    for (size_t slotIndex = 0; slotIndex < _slots.size(); ++slotIndex)
    {
        if (slotIndex >= lineCount)
        {
            Hide_Slot(slotIndex);
            _slotAnims[slotIndex] = { kSlotY[slotIndex], kSlotY[slotIndex] };
            continue;
        }

        const size_t lineIndex = _lines.size() - 1 - slotIndex;
        const RuntimeLine& line = _lines[lineIndex];
        Show_Slot(slotIndex, line);

        _slotAnims[slotIndex].fromY = Find_PreviousLineY(line.id);
        _slotAnims[slotIndex].targetY = kSlotY[slotIndex];
    }

    _layoutAnimElapsed = 0.f;
    _isLayoutAnimating = !immediate;
    Apply_AnimatedTransforms(immediate ? 1.f : 0.f);
}

void UI_WorldDialogue::Apply_AnimatedTransforms(float ratio)
{
    if (_topLinePanel && _topLinePanel->Get_Transform())
    {
        UI_Transform* transform = _topLinePanel->Get_Transform();
        transform->Set_Position(Vec2(
            transform->Get_Position().x,
            Lerp(_topLineFromY, _topLineTargetY, ratio)));
    }

    if (_bg && _bg->Get_Transform())
    {
        UI_Transform* transform = _bg->Get_Transform();
        transform->Set_Size(Vec2(
            transform->Get_Size().x,
            Lerp(_bgFromHeight, _bgTargetHeight, ratio)));
    }

    for (size_t slotIndex = 0; slotIndex < _slots.size(); ++slotIndex)
    {
        Shared<UI_Base> panel = _slots[slotIndex].portraitPanel;
        if (panel == nullptr || panel->Get_Transform() == nullptr)
            continue;

        UI_Transform* transform = panel->Get_Transform();
        transform->Set_Position(Vec2(
            transform->Get_Position().x,
            Lerp(_slotAnims[slotIndex].fromY, _slotAnims[slotIndex].targetY, ratio)));
    }
}

void UI_WorldDialogue::Show_Slot(size_t slotIndex, const RuntimeLine& line)
{
    if (slotIndex >= _slots.size())
        return;

    SlotElements& slot = _slots[slotIndex];
    const bool useLongText = Is_LongText(line.text);

    if (slot.portraitPanel)
        slot.portraitPanel->Open();

    if (!line.portraitTexturePath.empty() && slot.portraitImage)
        CHECK_FAILED(slot.portraitImage->Set_TexturePath(line.portraitTexturePath));

    if (slot.shortText)
    {
        slot.shortText->Set_Text(useLongText ? L"" : line.text);
        if (useLongText)
            slot.shortText->Close();
        else
            slot.shortText->Open();
    }

    if (slot.longText)
    {
        slot.longText->Set_Text(useLongText ? line.text : L"");
        if (useLongText)
            slot.longText->Open();
        else
            slot.longText->Close();
    }
}

void UI_WorldDialogue::Hide_Slot(size_t slotIndex)
{
    if (slotIndex >= _slots.size())
        return;

    SlotElements& slot = _slots[slotIndex];

    if (slot.portraitPanel)
        slot.portraitPanel->Close();

    if (slot.shortText)
    {
        slot.shortText->Set_Text(L"");
        slot.shortText->Close();
    }

    if (slot.longText)
    {
        slot.longText->Set_Text(L"");
        slot.longText->Close();
    }
}

bool UI_WorldDialogue::Is_LongText(const wstring& text) const
{
    return text.find(L'\n') != wstring::npos ||
        text.find(L'\r') != wstring::npos ||
        text.size() > kShortTextMaxLength;
}

float UI_WorldDialogue::Find_PreviousLineY(uint64 lineId) const
{
    for (const auto& [previousLineId, previousY] : _previousLinePositions)
    {
        if (previousLineId == lineId)
            return previousY;
    }

    return kSlotY[0];
}

float UI_WorldDialogue::Get_CurrentTopLineY() const
{
    if (_topLinePanel == nullptr || _topLinePanel->Get_Transform() == nullptr)
        return kTopLineYByCount[1];

    return _topLinePanel->Get_Transform()->Get_Position().y;
}

float UI_WorldDialogue::Get_CurrentBgHeight() const
{
    if (_bg == nullptr || _bg->Get_Transform() == nullptr)
        return kBgHeightByCount[1];

    return _bg->Get_Transform()->Get_Size().y;
}

Unique<UI_WorldDialogue> UI_WorldDialogue::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_WorldDialogue>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_WorldDialogue");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_WorldDialogue::Free()
{
    _topLinePanel.reset();
    _bg.reset();
    _slots = {};
    _root.reset();
    _lines.clear();
    _previousLinePositions.clear();

    UI_RuntimeLoader::Release_Document(_uiByName);

    UI_RuntimePanelBase::Free();
}
