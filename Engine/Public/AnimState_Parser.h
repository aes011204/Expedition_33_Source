#pragma once
#include "Base.h"

NS_BEGIN(Engine)

class Animation;

class ENGINE_DLL AnimState_Parser final 
{
public:
	// 특정 파일을 직접 로드
	static bool Load_StateAnimations_FromFile(
		const wstring& animMapPath,
		const vector<Shared<Animation>>& originalAnimations,
		OUT vector<uint32>& outStateAnimIndices,
		OUT wstring* outFoundEnumType = nullptr);

	// 기존 방식: 폴더 내 검색 (targetPrefabGuid 일치 우선)
	static bool Load_StateAnimations(
		const wstring& sourceModelPath,
		const vector<Shared<Animation>>& originalAnimations,
		OUT vector<uint32>& outStateAnimIndices,
		OUT wstring* outFoundEnumType = nullptr,
		const string& targetPrefabGuid = "");

	static string Get_PrefabGuid(const string& prefabName);
};

NS_END
