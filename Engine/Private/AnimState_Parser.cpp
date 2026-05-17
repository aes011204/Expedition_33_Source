#include "pch.h"
#include "AnimState_Parser.h"
#include "GameInstance.h"
#include "Animation.h"

bool AnimState_Parser::Load_StateAnimations_FromFile(
	const wstring& animMapPath,
	const vector<Shared<Animation>>& originalAnimations,
	OUT vector<uint32>& outStateAnimIndices,
	OUT wstring* outFoundEnumType)
{
	outStateAnimIndices.clear();

	if (animMapPath.empty() || !fs::exists(animMapPath))
	{
		return false;
	}

	std::ifstream file(animMapPath);
	if (!file.is_open())
	{
		return false;
	}

	json root;
	try
	{
		file >> root;
	}
	catch (...)
	{
		return false;
	}

	if (!root.contains("Mappings") || !root["Mappings"].is_array())
	{
		return false;
	}

	if (root.contains("EnumType") && outFoundEnumType)
	{
		string enumStr = root["EnumType"].get<string>();
		*outFoundEnumType = String::ToWString(enumStr);
	}

	uint32 maxIndex = 0;
	for (const auto& mapping : root["Mappings"])
	{
		if (mapping.contains("Index"))
		{
			uint32 idx = mapping["Index"].get<uint32>();
			if (idx > maxIndex) maxIndex = idx;
		}
	}
	outStateAnimIndices.resize(maxIndex + 1, UINT32_MAX);
// 6. 상태별 애니메이션 매칭
uint32 mappedCount = 0;
for (const auto& mapping : root["Mappings"])
{
	if (!mapping.contains("Index") || !mapping.contains("Anim"))
		continue;

	uint32 stateIdx = mapping["Index"].get<uint32>();
	string targetAnimName = mapping["Anim"].get<string>();

	bool found = false;
	for (uint32 i = 0; i < (uint32)originalAnimations.size(); ++i)
	{
		string availableName = originalAnimations[i]->Get_AnimationName();
		if (availableName == targetAnimName)
		{
			outStateAnimIndices[stateIdx] = i;
			mappedCount++;
			found = true;
			break;
		}
	}

	if (!found)
	{
		string availableNamesStr = "";
		for (const auto& anim : originalAnimations) 
			availableNamesStr += anim->Get_AnimationName() + ", ";

		LOG_WARN("AnimState_Parser - Match Failed: JSON Name [{}], Model Anims: [{}]", 
			targetAnimName, availableNamesStr);
	}
}

	string fileName = fs::path(animMapPath).filename().string();
	string enumTypeStr = root.contains("EnumType") ? root["EnumType"].get<string>() : "None";

	LOG_INFO("AnimState Mapping Loaded : File [{}], EnumType [{}], Mapped [{}/{}] states.",
		fileName, enumTypeStr, mappedCount, outStateAnimIndices.size());

	return true;
}

bool AnimState_Parser::Load_StateAnimations(
	const wstring& sourceModelPath,
	const vector<Shared<Animation>>& originalAnimations,
	OUT vector<uint32>& outStateAnimIndices,
	OUT wstring* outFoundEnumType,
	const string& targetPrefabGuid)
{
	if (sourceModelPath.empty()) return false;

	fs::path modelDir = fs::path(sourceModelPath).parent_path();
	if (!fs::exists(modelDir)) return false;

	fs::path bestMatch = "";

	for (const auto& entry : fs::directory_iterator(modelDir))
	{
		if (!entry.is_regular_file()) continue;
		string filename = entry.path().filename().string();
		if (filename.find(".animmap.json") == string::npos) continue;

		// 1. 타겟 GUID가 주어졌다면 파일 내부를 까서 확인
		if (!targetPrefabGuid.empty())
		{
			std::ifstream file(entry.path());
			if (file.is_open())
			{
				json data;
				try 
				{ 
					file >> data; 
					if (data.value("PrefabGuid", "") == targetPrefabGuid)
					{
						bestMatch = entry.path();
						break; // 정확한 매칭을 찾았으므로 즉시 종료
					}
				} 
				catch (...) {}
			}
		}

		// 2. 일치하는 것을 아직 못 찾았다면 첫 번째 발견된 파일을 저장 (Fallback)
		if (bestMatch.empty())
		{
			bestMatch = entry.path();
		}
	}

	if (!bestMatch.empty())
	{
		return Load_StateAnimations_FromFile(bestMatch.wstring(), originalAnimations, outStateAnimIndices, outFoundEnumType);
	}

	return false;
}

string AnimState_Parser::Get_PrefabGuid(const string& prefabName)
{
	if (prefabName.empty())
		return "";

	string prefabPath = "data/json/prefabs/" + prefabName + ".prefab.json";
	return GAME->Find_AssetGUID(String::ToWString(prefabPath));
}
