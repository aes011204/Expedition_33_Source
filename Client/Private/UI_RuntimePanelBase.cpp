#include "UI_RuntimePanelBase.h"
#include "UI_Base.h"
#include "UI_Image.h"
#include "UI_Text.h"
#include "Helper_String.h"

namespace
{
#ifdef _DEBUG
    constexpr uint64 kDebugCaptureCountLogInterval = 30;

    const char* DebugCapture_RenderGroupName(UI_RenderGroup renderGroup)
    {
        switch (renderGroup)
        {
        case UI_RenderGroup::Background: return "Background";
        case UI_RenderGroup::Foreground: return "Foreground";
        default: return "Unknown";
        }
    }

    bool Should_LogDebugCaptureCount()
    {
        static uint64 s_logCounter = 0;
        ++s_logCounter;
        return s_logCounter <= 12 || (s_logCounter % kDebugCaptureCountLogInterval) == 0;
    }
#endif
}

void UI_RuntimePanelBase::Open()
{
    _isOpen = true;

    if (_root)
        _root->Open();

    Apply_EditorDefaultOffsetToRoot();
}

void UI_RuntimePanelBase::Close()
{
    _isOpen = false;

    if (_root)
        _root->Close();
}

void UI_RuntimePanelBase::Toggle()
{
    if (_isOpen)
        Close();
    else
        Open();
}

HRESULT UI_RuntimePanelBase::Render_DebugCapture()
{
    return Render_DebugCapture_Internal(nullptr);
}

HRESULT UI_RuntimePanelBase::Render_DebugCapture(UI_RenderGroup renderGroup)
{
    return Render_DebugCapture_Internal(&renderGroup);
}

HRESULT UI_RuntimePanelBase::Render_DebugCapture_Internal(const UI_RenderGroup* renderGroupFilter)
{
    vector<Shared<UI_Base>> renderTargets{};
    renderTargets.reserve(_uiByName.size());

#ifdef _DEBUG
    uint32 skippedClosedCount = 0;
    uint32 skippedGroupMismatchCount = 0;
    uint32 alphaZeroCount = 0;
#endif

    for (const auto& [name, ui] : _uiByName)
    {
        if (ui == nullptr || !ui->Is_EffectivelyOpen())
        {
#ifdef _DEBUG
            ++skippedClosedCount;
#endif
            continue;
        }

        if (renderGroupFilter != nullptr && ui->Get_RenderGroup() != *renderGroupFilter)
        {
#ifdef _DEBUG
            ++skippedGroupMismatchCount;
#endif
            continue;
        }

        const bool isRenderableCaptureTarget =
            dynamic_pointer_cast<UI_Image>(ui) != nullptr ||
            dynamic_pointer_cast<UI_Text>(ui) != nullptr;

        if (!isRenderableCaptureTarget)
            continue;

#ifdef _DEBUG
        const Vec4 uiColor = ui->Get_Color();
        if (uiColor.w * ui->Get_EffectiveAlphaMultiplier() <= 0.001f)
            ++alphaZeroCount;
#endif

        renderTargets.push_back(ui);
    }

    stable_sort(renderTargets.begin(), renderTargets.end(),
        [](const Shared<UI_Base>& lhs, const Shared<UI_Base>& rhs)
        {
            if (lhs->Get_RenderGroup() != rhs->Get_RenderGroup())
                return static_cast<int32>(lhs->Get_RenderGroup()) < static_cast<int32>(rhs->Get_RenderGroup());

            if (lhs->Get_SortOrder() != rhs->Get_SortOrder())
                return lhs->Get_SortOrder() < rhs->Get_SortOrder();

            return lhs.get() < rhs.get();
        });

    uint32 renderedImageCount = 0;
    uint32 renderedTextCount = 0;

    for (const Shared<UI_Base>& ui : renderTargets)
    {
        if (dynamic_pointer_cast<UI_Image>(ui) != nullptr)
            ++renderedImageCount;
        else if (dynamic_pointer_cast<UI_Text>(ui) != nullptr)
            ++renderedTextCount;

        CHECK_FAILED(ui->Render(), E_FAIL);
    }

#ifdef _DEBUG
    if (renderGroupFilter != nullptr && Should_LogDebugCaptureCount())
    {
        const string rootName = _root ? String::ToString(_root->Get_Name()) : string{ "<null>" };
        LOG_DEBUG(
            "[DebugCaptureCount] root={} group={} image={} text={} skippedClosed={} skippedGroup={} alphaZero={}",
            rootName.c_str(),
            DebugCapture_RenderGroupName(*renderGroupFilter),
            renderedImageCount,
            renderedTextCount,
            skippedClosedCount,
            skippedGroupMismatchCount,
            alphaZeroCount);
    }
#endif

    return S_OK;
}

void UI_RuntimePanelBase::Set_EditorDefaultTestOffset(const Vec2& offset)
{
    _editorDefaultTestOffset = offset;
    Mark_EditorRuntimeTuningDirty();

    Apply_EditorDefaultOffsetToRoot();
}

void UI_RuntimePanelBase::Set_EditorBattleWorldUISize(const Vec2& size)
{
    _editorBattleWorldUISize = size;
    Mark_EditorRuntimeTuningDirty();
}

void UI_RuntimePanelBase::Set_EditorBattleWorldUIOffset(const Vec3& offset)
{
    _editorBattleWorldUIOffset = offset;
    Mark_EditorRuntimeTuningDirty();
}

void UI_RuntimePanelBase::Set_EditorBattleWorldUIRotation(const Vec3& rotation)
{
    _editorBattleWorldUIRotation = rotation;
    Mark_EditorRuntimeTuningDirty();
}

void UI_RuntimePanelBase::Enable_EditorBattleWorldUIPlacement(const Vec2& size, const Vec3& offset, const Vec3& rotation)
{
    _editorBattleWorldUIPlacementEnabled = true;
    _editorBattleWorldUISize = size;
    _editorBattleWorldUIOffset = offset;
    _editorBattleWorldUIRotation = rotation;
    Mark_EditorRuntimeTuningDirty();
}

void UI_RuntimePanelBase::Apply_EditorDefaultOffsetToRoot()
{
    if (_editorPositionDrivenByCode)
        return; 

    if (_root == nullptr || _root->Get_Transform() == nullptr)
        return;

    Capture_EditorDesignPositionIfNeeded();

    Vec2 finalPosition = _editorDesignPosition;
    finalPosition += _editorDefaultTestOffset; 
    _root->Get_Transform()->Set_Position(finalPosition);
}

void UI_RuntimePanelBase::Capture_EditorDesignPositionIfNeeded()
{
    if (_hasEditorDesignPosition)
        return;

    if (_root == nullptr || _root->Get_Transform() == nullptr)
        return;

    _editorDesignPosition = _root->Get_Transform()->Get_Position();
    _hasEditorDesignPosition = true;
}

void UI_RuntimePanelBase::Reset_EditorDesignPositionCache()
{
    _editorDesignPosition = Vec2::Zero;
    _hasEditorDesignPosition = false;
}

void UI_RuntimePanelBase::Apply_EditorOffsets(Vec2& inOutPosition) const
{
    inOutPosition += _editorDefaultTestOffset; 
}

void UI_RuntimePanelBase::Free()
{
    Base::Free();
}
