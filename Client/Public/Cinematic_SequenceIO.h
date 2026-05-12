#pragma once
#include "Base.h"
#include "Cinematic_Types.h"

NS_BEGIN(Client)
class Cinematic_SequenceIO : public Base
{
public:
    Cinematic_SequenceIO();
    virtual ~Cinematic_SequenceIO() = default;

public:
    static void SaveSequenceToFile(const CINEMATIC_SEQUENCE& sequence, const string& filePath);
    static bool LoadSequenceFromFile(const string& filePath, CINEMATIC_SEQUENCE& outSequence);
};

NS_END
