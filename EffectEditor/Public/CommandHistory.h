#pragma once

NS_BEGIN(Engine)
class ICommand;
NS_END

NS_BEGIN(EffectEditor)

class CommandHistory final
{
public:
    CommandHistory() = default;
    ~CommandHistory() = default;

public:
    void Execute(const Shared<ICommand>& command);
    void Undo();
    void Redo();

    void Clear();

    bool CanUndo() const { return !_undoStack.empty(); }
    bool CanRedo() const { return !_redoStack.empty(); }

private:
    vector<Shared<ICommand>> _undoStack;
    vector<Shared<ICommand>> _redoStack;

    static constexpr size_t MAX_HISTORY = 100;

public:
    static Unique<CommandHistory> Create();
};

NS_END
