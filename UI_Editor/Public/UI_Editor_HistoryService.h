#pragma once

#include "UI_Document.h"

namespace UIEditor
{
    class UI_Editor_HistoryService final
    {
    public:
        UI_Editor_HistoryService() = default;
        ~UI_Editor_HistoryService() = default;

    public:
        bool Is_Dirty() const { return _isDirty; }
        bool Can_Undo() const;
        bool Can_Redo() const;
        bool Is_EditTransactionOpen() const { return _isEditTransactionOpen; }

        void Mark_Dirty(const UI_Document& document);
        void Clear_Dirty(const UI_Document& document);
        void Clear_History();

        void Push_UndoSnapshot(const UI_Document& document, ObjectId selectedObjectId, const vector<ObjectId>& selectedObjectIds);
        void Begin_EditTransaction(const UI_Document& document, ObjectId selectedObjectId, const vector<ObjectId>& selectedObjectIds);
        void Commit_EditTransactionIfChanged(const UI_Document& document, ObjectId selectedObjectId, const vector<ObjectId>& selectedObjectIds);

        bool Undo(UI_Document& document, ObjectId& selectedObjectId, vector<ObjectId>& selectedObjectIds);
        bool Redo(UI_Document& document, ObjectId& selectedObjectId, vector<ObjectId>& selectedObjectIds);

    private:
        struct HistoryState
        {
            UI_Document document = {};
            ObjectId selectedObjectId = 0;
            vector<ObjectId> selectedObjectIds = {};
            string documentSignature = "";
        };

    private:
        string Make_DocumentSignature(const UI_Document& document) const;
        HistoryState Capture_HistoryState(const UI_Document& document, ObjectId selectedObjectId, const vector<ObjectId>& selectedObjectIds) const;
        bool Is_SameHistoryState(const HistoryState& lhs, const HistoryState& rhs) const;
        bool Is_CurrentDocumentClean(const UI_Document& document) const;
        void Push_HistoryState(vector<HistoryState>& historyStack, const HistoryState& historyState) const;
        void Apply_HistoryState(const HistoryState& historyState, UI_Document& document, ObjectId& selectedObjectId, vector<ObjectId>& selectedObjectIds);

    private:
        bool _isDirty = false;
        string _cleanDocumentSignature = "";
        vector<HistoryState> _undoStack = {};
        vector<HistoryState> _redoStack = {};
        bool _isEditTransactionOpen = false;
        HistoryState _editTransactionStartState = {};
    };
}
