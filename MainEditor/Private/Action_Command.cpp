#include "pch.h"
#include "Action_Command.h"

Action_Command::Action_Command(function<void()> onUndo, function<void()> onRedo, string desc)
    : _onUndo(onUndo), _onRedo(onRedo), _desc(desc)
{
}

void Action_Command::Execute()
{
}

void Action_Command::Undo()
{
    if (_onUndo)
        _onUndo();
}

void Action_Command::Redo()
{
    if (_onRedo)
        _onRedo();
}

Shared<Action_Command> Action_Command::Create(function<void()> onUndo, function<void()> onRedo, string desc)
{
    return make_shared<Action_Command>(onUndo, onRedo, desc);
}

void Action_Command::Free()
{
    ICommand::Free();
}
