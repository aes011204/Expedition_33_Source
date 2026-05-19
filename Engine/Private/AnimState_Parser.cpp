#include "pch.h"
#include "AnimState_Parser.h"
#include "GameInstance.h"
#include "Animation.h"
#include <fstream>
#include <iomanip>

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

	string fileName = fs::path(animMapPath).filename().string();

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

	// GlobalEnum 정보 가져오기
	const TypeRegistry::GlobalEnumInfo* pEnumInfo = nullptr;
	if (root.contains("EnumType"))
	{
		string enumTypeStr = root["EnumType"].get<string>();
		// "GlobalEnum::PlayerWorldAnimation" -> "PlayerWorldAnimation"
		size_t lastColon = enumTypeStr.find_last_of(':');
		if (lastColon != string::npos)
			enumTypeStr = enumTypeStr.substr(lastColon + 1);

		GAME->Visit_GlobalEnums([&](const TypeRegistry::GlobalEnumInfo& info)
		{
			if (info.displayName == enumTypeStr)
				pEnumInfo = &info;
		});
	}

	uint32 maxIndex = 0;
	if (pEnumInfo)
	{
		for (int val : pEnumInfo->enumValues)
		{
			if ((uint32)val > maxIndex) maxIndex = (uint32)val;
		}
	}
	else
	{
		for (const auto& mapping : root["Mappings"])
		{
			if (mapping.contains("Index"))
			{
				uint32 idx = mapping["Index"].get<uint32>();
				if (idx > maxIndex) maxIndex = idx;
			}
		}
	}

	outStateAnimIndices.resize(maxIndex + 1, UINT32_MAX);

	// 6. 상태별 애니메이션 매칭
	uint32 mappedCount = 0;
	bool bNeedRewrite = false;

	for (auto& mapping : root["Mappings"])
	{
		if (!mapping.contains("Anim"))
			continue;

		string targetAnimName = mapping["Anim"].get<string>();
		int32 stateIdx = -1;

		// 1. 'State' 이름 기반 매칭 시도
		if (pEnumInfo && mapping.contains("State"))
		{
			string stateName = mapping["State"].get<string>();
			for (size_t i = 0; i < pEnumInfo->enumNames.size(); ++i)
			{
				if (pEnumInfo->enumNames[i] == stateName)
				{
					stateIdx = pEnumInfo->enumValues[i];

					// 현재 JSON에 기록된 Index가 Enum 실제 값과 다르다면 갱신 필요
					if (mapping.contains("Index"))
					{
						if (mapping["Index"].get<int32>() != stateIdx)
						{
							mapping["Index"] = stateIdx;
							bNeedRewrite = true;
						}
					}
					else
					{
						mapping["Index"] = stateIdx;
						bNeedRewrite = true;
					}
					break;
				}
			}
		}

		// 2. 실패 시 'Index' 기반 매칭 시도 (Fallback)
		if (stateIdx == -1 && mapping.contains("Index"))
		{
			stateIdx = mapping["Index"].get<int32>();
		}

		if (stateIdx < 0 || stateIdx >= (int32)outStateAnimIndices.size())
		{
			continue;
		}

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

	// Index 불일치 등으로 데이터 갱신이 필요한 경우 파일 Rewrite
	if (bNeedRewrite)
	{
		std::ofstream outFile(animMapPath);
		if (outFile.is_open())
		{
			outFile << std::setw(4) << root << std::endl;
			LOG_INFO("AnimState Mapping Rewritten (Index Mismatch Fixed): File [{}]", fileName);
		}
	}

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
