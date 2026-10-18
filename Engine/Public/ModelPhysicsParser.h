#pragma once
#include "ChainModifier.h"

NS_BEGIN(Engine)

class ENGINE_DLL ModelPhysicsParser
{
public:
    static vector<ChainModifier::CapsuleColliderDesc> LoadCapsulesFromJSON(const wstring& filePath);
};

NS_END
