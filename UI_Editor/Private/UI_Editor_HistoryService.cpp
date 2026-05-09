#include "pch.h"
#include "UI_Editor_HistoryService.h"

namespace UIEditor
{
    namespace
    {
        constexpr size_t kMaxHistoryStateCount = 100;
    }

    bool UI_Editor_HistoryService::Can_Undo() const
    {
        return !_undoStack.empty();
    }

    bool UI_Editor_HistoryService::Can_Redo() const
    {
        return !_redoStack.empty();
    }

    void UI_Editor_HistoryService::Mark_Dirty(const UI_Document& document)
    {
        _isDirty = !Is_CurrentDocumentClean(document);
    }

    void UI_Editor_HistoryService::Clear_Dirty(const UI_Document& document)
    {
        _cleanDocumentSignature = Make_DocumentSignature(document);
        _isDirty = false;
    }

    void UI_Editor_HistoryService::Clear_History()
    {
        _undoStack.clear();
        _redoStack.clear();
        _isEditTransactionOpen = false;
        _editTransactionStartState = {};
    }

    void UI_Editor_HistoryService::Push_UndoSnapshot(const UI_Document& document, ObjectId selectedObjectId)
    {
        if (_isEditTransactionOpen)
            return;

        Push_HistoryState(_undoStack, Capture_HistoryState(document, selectedObjectId));
        _redoStack.clear();
    }

    void UI_Editor_HistoryService::Begin_EditTransaction(const UI_Document& document, ObjectId selectedObjectId)
    {
        if (_isEditTransactionOpen)
            return;

        _editTransactionStartState = Capture_HistoryState(document, selectedObjectId);
        _isEditTransactionOpen = true;
    }

    void UI_Editor_HistoryService::Commit_EditTransactionIfChanged(const UI_Document& document, ObjectId selectedObjectId)
    {
        if (!_isEditTransactionOpen)
            return;

        const HistoryState currentState = Capture_HistoryState(document, selectedObjectId);
        if (!Is_SameHistoryState(_editTransactionStartState, currentState))
        {
            Push_HistoryState(_undoStack, _editTransactionStartState);
            _redoStack.clear();
            _isDirty = !Is_CurrentDocumentClean(document);
        }

        _isEditTransactionOpen = false;
        _editTransactionStartState = {};
    }

    bool UI_Editor_HistoryService::Undo(UI_Document& document, ObjectId& selectedObjectId)
    {
        if (!Can_Undo())
            return false;

        if (_isEditTransactionOpen)
            Commit_EditTransactionIfChanged(document, selectedObjectId);

        const HistoryState currentState = Capture_HistoryState(document, selectedObjectId);
        const HistoryState undoState = _undoStack.back();
        _undoStack.pop_back();

        Push_HistoryState(_redoStack, currentState);
        Apply_HistoryState(undoState, document, selectedObjectId);
        return true;
    }

    bool UI_Editor_HistoryService::Redo(UI_Document& document, ObjectId& selectedObjectId)
    {
        if (!Can_Redo())
            return false;

        if (_isEditTransactionOpen)
            Commit_EditTransactionIfChanged(document, selectedObjectId);

        const HistoryState currentState = Capture_HistoryState(document, selectedObjectId);
        const HistoryState redoState = _redoStack.back();
        _redoStack.pop_back();

        Push_HistoryState(_undoStack, currentState);
        Apply_HistoryState(redoState, document, selectedObjectId);
        return true;
    }

    string UI_Editor_HistoryService::Make_DocumentSignature(const UI_Document& document) const
    {
        return document.To_Json().dump();
    }

    UI_Editor_HistoryService::HistoryState UI_Editor_HistoryService::Capture_HistoryState(
        const UI_Document& document,
        ObjectId selectedObjectId) const
    {
        HistoryState historyState = {};
        historyState.document = document;
        historyState.selectedObjectId = selectedObjectId;
        historyState.documentSignature = Make_DocumentSignature(document);
        return historyState;
    }

    bool UI_Editor_HistoryService::Is_SameHistoryState(const HistoryState& lhs, const HistoryState& rhs) const
    {
        return lhs.selectedObjectId == rhs.selectedObjectId && lhs.documentSignature == rhs.documentSignature;
    }

    bool UI_Editor_HistoryService::Is_CurrentDocumentClean(const UI_Document& document) const
    {
        return !_cleanDocumentSignature.empty() && Make_DocumentSignature(document) == _cleanDocumentSignature;
    }

    void UI_Editor_HistoryService::Push_HistoryState(vector<HistoryState>& historyStack, const HistoryState& historyState) const
    {
        if (!historyStack.empty() && Is_SameHistoryState(historyStack.back(), historyState))
            return;

        historyStack.push_back(historyState);
        if (historyStack.size() > kMaxHistoryStateCount)
            historyStack.erase(historyStack.begin());
    }

    void UI_Editor_HistoryService::Apply_HistoryState(
        const HistoryState& historyState,
        UI_Document& document,
        ObjectId& selectedObjectId)
    {
        document = historyState.document;
        selectedObjectId = nullptr != document.Find_Object(historyState.selectedObjectId) ? historyState.selectedObjectId : 0;
        _isDirty = !Is_CurrentDocumentClean(document);
    }
}
