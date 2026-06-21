#pragma once

struct DialogueChoice
{
    string text;
    int nextLineIndex;
};

struct DialogueLine
{
    int lineIndex;
    string cameraPresetId;
    vector<DialogueChoice> choices;
};

struct DialogueData
{
    string dialogueKey;
    vector<DialogueLine> lines;
};

struct DialogueRequest
{
    Shared<GameObject> speaker;
    string speakerName;
    string dialogueKey;
};
