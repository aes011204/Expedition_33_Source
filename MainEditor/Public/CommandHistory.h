#pragma once

NS_BEGIN(Engine)
class ICommand;
NS_END

NS_BEGIN(MainEditor)

class CommandHistory final
{
public:
    explicit CommandHistory() = default;
    virtual ~CommandHistory() = default;
    
public:
    void Execute(Shared<ICommand> command);
    void Undo();
    void Redo();

    void Clear(); // 스택 비우기

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
