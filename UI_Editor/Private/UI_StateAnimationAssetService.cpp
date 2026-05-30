#include "pch.h"
#include "UI_StateAnimationAssetService.h"

namespace UIEditor
{
    UI_StateAnimationAssetService::UI_StateAnimationAssetService()
    {
        New_Asset();
    }

    void UI_StateAnimationAssetService::New_Asset()
    {
        _asset = {};
        _assetFilePath.clear();
        _assetDirty = false;
        Reset_EditHistory();
    }

    bool UI_StateAnimationAssetService::Save_Asset(bool forceSaveAs)
    {
        wstring filePath = _assetFilePath;
        if (forceSaveAs || filePath.empty())
        {
            if (!Show_SaveAssetDialog(filePath))
                return false;
        }

        Ensure_StateAnimationExtension(filePath);
        if (!UI_StateAnimationAssetSerializer::Save_ToJsonFile(_asset, filePath))
            return false;

        _assetFilePath = filePath;
        _assetDirty = false;
        return true;
    }

    bool UI_StateAnimationAssetService::Load_Asset()
    {
        wstring filePath = {};
        if (!Show_LoadAssetDialog(filePath))
            return false;

        UI_StateAnimationAsset loadedAsset = {};
        if (!UI_StateAnimationAssetSerializer::Load_FromJsonFile(loadedAsset, filePath))
            return false;

        _asset = move(loadedAsset);
        _assetFilePath = filePath;
        _assetDirty = false;
        Reset_EditHistory();
        return true;
    }

    bool UI_StateAnimationAssetService::Confirm_DiscardDirtyAsset(const wchar_t* actionName)
    {
        if (!_assetDirty)
            return true;

        const wstring actionText = (nullptr != actionName && L'\0' != actionName[0]) ? actionName : L"continue";
        const wstring message = L"The current UI state animation asset has unsaved changes.\n\nSave before you " + actionText + L"?";
        const int result = MessageBoxW(nullptr, message.c_str(), L"UI State Animation", MB_ICONQUESTION | MB_YESNOCANCEL);

        if (result == IDCANCEL)
            return false;

        if (result == IDYES)
            return Save_Asset(false);

        return true;
    }

    bool UI_StateAnimationAssetService::Undo_AssetEdit()
    {
        UI_StateAnimationAssetHistoryState undoState = {};
        if (!_editHistory.Undo(
            Capture_HistoryState(),
            undoState,
            [this](const UI_StateAnimationAssetHistoryState& lhs, const UI_StateAnimationAssetHistoryState& rhs)
            {
                return Is_SameHistoryAsset(lhs, rhs);
            }))
        {
            return false;
        }

        Restore_HistoryState(undoState);
        _assetDirty = true;
        return true;
    }

    bool UI_StateAnimationAssetService::Redo_AssetEdit()
    {
        UI_StateAnimationAssetHistoryState redoState = {};
        if (!_editHistory.Redo(
            Capture_HistoryState(),
            redoState,
            [this](const UI_StateAnimationAssetHistoryState& lhs, const UI_StateAnimationAssetHistoryState& rhs)
            {
                return Is_SameHistoryAsset(lhs, rhs);
            }))
        {
            return false;
        }

        Restore_HistoryState(redoState);
        _assetDirty = true;
        return true;
    }

    void UI_StateAnimationAssetService::Set_AssetName(const string& assetName)
    {
        if (_asset.name == assetName)
            return;

        _asset.name = assetName;
        Mark_AssetDirty();
    }

    void UI_StateAnimationAssetService::Set_EventAnimationPath(UI_StateAnimationEvent eventType, const string& animationPath)
    {
        const size_t eventIndex = static_cast<size_t>(eventType);
        if (eventIndex >= _asset.eventAnimationPaths.size() || _asset.eventAnimationPaths[eventIndex] == animationPath)
            return;

        _asset.eventAnimationPaths[eventIndex] = animationPath;
        Mark_AssetDirty();
    }

    void UI_StateAnimationAssetService::Mark_AssetDirty()
    {
        _editHistory.Capture_Edit(
            Capture_HistoryState(),
            [this](const UI_StateAnimationAssetHistoryState& lhs, const UI_StateAnimationAssetHistoryState& rhs)
            {
                return Is_SameHistoryAsset(lhs, rhs);
            });

        _assetDirty = true;
    }

    UI_StateAnimationAssetService::UI_StateAnimationAssetHistoryState UI_StateAnimationAssetService::Capture_HistoryState() const
    {
        UI_StateAnimationAssetHistoryState historyState = {};
        historyState.asset = _asset;
        return historyState;
    }

    void UI_StateAnimationAssetService::Restore_HistoryState(const UI_StateAnimationAssetHistoryState& historyState)
    {
        _asset = historyState.asset;
    }

    bool UI_StateAnimationAssetService::Is_SameHistoryAsset(
        const UI_StateAnimationAssetHistoryState& lhs,
        const UI_StateAnimationAssetHistoryState& rhs) const
    {
        return UI_StateAnimationAssetSerializer::To_Json(lhs.asset) == UI_StateAnimationAssetSerializer::To_Json(rhs.asset);
    }

    void UI_StateAnimationAssetService::Reset_EditHistory()
    {
        _editHistory.Reset(Capture_HistoryState());
    }
}
