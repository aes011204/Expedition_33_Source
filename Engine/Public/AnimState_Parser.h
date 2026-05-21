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

    static bool Save_StateAnimations_ToFile(
        const wstring& savePath,
        const string& enumTypeName,
        const string& prefabName,
        const vector<string>& stateNames,
        const vector<int32>& stateIndices,
        const unordered_map<string, string>& animationMapping);

	static string Get_PrefabGuid(const string &prefabName);

	// static class 명 기반으로 프리팹명을 역서칭
	static string Find_PrefabName_ByStaticClass(const string &ownerClassName, const string &selfClassName);
};

NS_END
