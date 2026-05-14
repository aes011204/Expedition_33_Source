#pragma once
#include "Base.h"

NS_BEGIN(Engine)

class Animation;

class ENGINE_DLL AnimState_Parser final 
{
public:
	static bool Load_StateAnimations(
		const wstring& sourceModelPath,
		const vector<Shared<Animation>>& originalAnimations,
		OUT vector<uint32>& outStateAnimIndices,
		OUT wstring* outFoundEnumType = nullptr);
};

NS_END
