#pragma once
#include "Base.h"
#include "UI_Dialogue.h"

NS_BEGIN(Client)

struct DialogueRequest;
struct DialogueData;
struct DialogueLine;
class DialogueManager : public Base
{
public:
    DialogueManager();
    virtual ~DialogueManager() = default;

public:
    void StartDialogue(const DialogueRequest& request);
    void ContinueDialogue();
    void EndDialogue(bool restoreWorldState = true);
    void MoveToLine(int lineIndex);
    void Update();
    void Reset_LastDialogueNpcAnimationToStart();
    bool IsActive() const { return _isActive; }
    bool IsInteractLocked() const { return _waitInteractKeyRelease; }

    void Set_DialogueOverride(const string& sourceDialogueKey, const string& targetDialogueKey);
    string Resolve_DialogueKey(const string& dialogueKey) const;

private:
    void AlignDialogueActors();
    void SelectChoice();
    void ApplyChoice();
    bool ExecuteLineAction(const string& action);
    const DialogueLine* GetCurrentLine();
    void ApplyCurrentLine();

private:
    string              _currentDialogueKey;
    const DialogueData* _currentDialogueData;
    int                 _currentLineIndex = 0;
    int                 _selectIndex = 0;
    bool                _isActive = false;
    bool                _waitInteractKeyRelease = false;
    bool                _isChoiceVisible = false;

    Shared<GameObject>  _dialoguePlayerObject;
    Shared<GameObject>  _dialogueNpcObject;
    Unique<UI_Dialogue> _uiDialogue = {};
    uint32              _uiDialogueLevelIndex = 0;

    string              _lastAnimationKey;
    float               _lastAnimationOffsetY = 0.f;
    string              _startAnimationKey;
    float               _startAnimationOffsetY = 0.f;
    Vec3                _startNpcRotation = Vec3::Zero;
    bool                _hasStartNpcRotation = false;

    umap<string, string> _dialogueOverrides{};

public:
    static Unique<DialogueManager> Create();
    virtual void Free() override;
};



NS_END
