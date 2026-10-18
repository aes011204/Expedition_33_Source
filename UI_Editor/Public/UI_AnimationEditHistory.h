#pragma once

namespace UIEditor
{
    template<typename THistoryState>
    class UI_AnimationEditHistory final
    {
    public:
        UI_AnimationEditHistory() = default;
        ~UI_AnimationEditHistory() = default;

    public:
        bool Can_Undo() const { return !_undoStack.empty(); }
        bool Can_Redo() const { return !_redoStack.empty(); }

        void Reset(const THistoryState& baseState)
        {
            _undoStack.clear();
            _redoStack.clear();
            _baseState = baseState;
        }

        template<typename TIsSameFunc>
        void Capture_Edit(const THistoryState& currentState, TIsSameFunc&& isSame)
        {
            if (isSame(_baseState, currentState))
                return;

            Push_HistoryState(_undoStack, _baseState, isSame);
            _redoStack.clear();
            _baseState = currentState;
        }

        template<typename TIsSameFunc>
        bool Undo(const THistoryState& currentState, THistoryState& outState, TIsSameFunc&& isSame)
        {
            if (_undoStack.empty())
                return false;

            Push_HistoryState(_redoStack, currentState, isSame);
            outState = _undoStack.back();
            _undoStack.pop_back();
            _baseState = outState;
            return true;
        }

        template<typename TIsSameFunc>
        bool Redo(const THistoryState& currentState, THistoryState& outState, TIsSameFunc&& isSame)
        {
            if (_redoStack.empty())
                return false;

            Push_HistoryState(_undoStack, currentState, isSame);
            outState = _redoStack.back();
            _redoStack.pop_back();
            _baseState = outState;
            return true;
        }

    private:
        template<typename TIsSameFunc>
        void Push_HistoryState(vector<THistoryState>& historyStack, const THistoryState& historyState, TIsSameFunc&& isSame)
        {
            if (!historyStack.empty() && isSame(historyStack.back(), historyState))
                return;

            historyStack.push_back(historyState);
            if (historyStack.size() > _historyLimit)
                historyStack.erase(historyStack.begin());
        }

    private:
        static constexpr size_t _historyLimit = 100;

    private:
        vector<THistoryState> _undoStack = {};
        vector<THistoryState> _redoStack = {};
        THistoryState _baseState = {};
    };
}
