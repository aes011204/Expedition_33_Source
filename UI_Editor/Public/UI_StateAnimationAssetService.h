#pragma once

#include "UI_AnimationAssetSerializer.h"
#include "UI_AnimationEditHistory.h"

namespace UIEditor
{
    class UI_StateAnimationAssetService final
    {
    public:
        UI_StateAnimationAssetService();
        ~UI_StateAnimationAssetService() = default;

    public:
        const UI_StateAnimationAsset& Get_Asset() const { return _asset; }
        const wstring& Get_AssetFilePath() const { return _assetFilePath; }
        bool Is_Dirty() const { return _assetDirty; }
        bool Can_UndoAssetEdit() const { return _editHistory.Can_Undo(); }
        bool Can_RedoAssetEdit() const { return _editHistory.Can_Redo(); }

        void New_Asset();
        bool Save_Asset(bool forceSaveAs);
        bool Load_Asset();
        bool Confirm_DiscardDirtyAsset(const wchar_t* actionName);
        bool Undo_AssetEdit();
        bool Redo_AssetEdit();
        void Set_AssetName(const string& assetName);
        void Set_EventAnimationPath(UI_StateAnimationEvent eventType, const string& animationPath);
        bool Select_AnimationForEvent(UI_StateAnimationEvent eventType);

    private:
        bool Show_SaveAssetDialog(wstring& outFilePath) const;
        bool Show_LoadAssetDialog(wstring& outFilePath) const;
        bool Show_SelectAnimationDialog(wstring& outFilePath) const;
        wstring Resolve_DefaultStateAnimationDirectory() const;
        void Ensure_StateAnimationExtension(wstring& filePath) const;
        void Mark_AssetDirty();
        struct UI_StateAnimationAssetHistoryState
        {
            UI_StateAnimationAsset asset = {};
        };
        UI_StateAnimationAssetHistoryState Capture_HistoryState() const;
        void Restore_HistoryState(const UI_StateAnimationAssetHistoryState& historyState);
        bool Is_SameHistoryAsset(const UI_StateAnimationAssetHistoryState& lhs, const UI_StateAnimationAssetHistoryState& rhs) const;
        void Reset_EditHistory();

    private:
        UI_StateAnimationAsset _asset = {};
        wstring _assetFilePath = {};
        bool _assetDirty = false;
        UI_AnimationEditHistory<UI_StateAnimationAssetHistoryState> _editHistory = {};
    };
}
