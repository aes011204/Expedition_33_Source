#pragma once
#include "Base.h"

NS_BEGIN(Client)
struct DialogueData;
struct DialogueLine;
class DialogueManager : public Base
{
public:
    DialogueManager();
    virtual ~DialogueManager() = default;

public:
    void StartDialogue(const string& dialogueKey);
    void ContinueDialogue();
    void EndDialogue();
    void MoveToLine(int lineIndex);
    void Update();
    bool IsActive() const { return _isActive; }
    bool IsInteractLocked() const { return _waitInteractKeyRelease; }

private:
    void SelectChoice();
    void ApplyChoice();
    const DialogueLine* GetCurrentLine();
    void ApplyCurrentLine();

private:
    string              _currentDialogueKey;
    const DialogueData* _currentDialogueData;
    int                 _currentLineIndex = 0;
    int                 _selectIndex = 0;
    bool                _isActive = false;
    bool                _waitInteractKeyRelease = false;

public:
    static Unique<DialogueManager> Create();
    virtual void Free() override;
};



NS_END
