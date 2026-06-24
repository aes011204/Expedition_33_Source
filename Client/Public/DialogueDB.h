#pragma once
#include "Base.h"

NS_BEGIN(Client)
struct DialogueData;

class DialogueDB : public Base
{
public:
    const DialogueData* FindDialogueData(const string& dialogueKey) const;
    void LoadFromFile(const string& filePath);
    void Clear();

private:
    unordered_map<string, DialogueData> _dialogueMap;

public:
    static Unique<DialogueDB> Create();
    virtual void    Free() override;
    
};

NS_END

