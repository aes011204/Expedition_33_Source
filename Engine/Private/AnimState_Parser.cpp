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

		if (stateIdx < 0 || stateIdx >= static_cast<int32>(outStateAnimIndices.size()))
		{
			continue;
		}

		bool found = false;
		for (uint32 i = 0; i < static_cast<uint32>(originalAnimations.size()); ++i)
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

			//LOG_WARN("AnimState_Parser - Match Failed: JSON Name [{}], Model Anims: [{}]",
			//	targetAnimName, availableNamesStr);
		}
	}

	// Index 불일치 등으로 데이터 갱신이 필요한 경우 파일 Rewrite
	if (bNeedRewrite)
	{
		std::ofstream outFile(animMapPath);
		if (outFile.is_open())
		{
			outFile << std::setw(4) << root << std::endl;
			//LOG_INFO("AnimState Mapping Rewritten (Index Mismatch Fixed): File [{}]", fileName);
		}
	}

	string enumTypeStr = root.contains("EnumType") ? root["EnumType"].get<string>() : "None";

	//LOG_INFO("AnimState Mapping Loaded : File [{}], EnumType [{}], Mapped [{}/{}] states.",
	//	fileName, enumTypeStr, mappedCount, outStateAnimIndices.size());

	return true;
}

bool AnimState_Parser::Load_StateAnimations(
	const wstring& sourceModelPath,
	const vector<Shared<Animation>>& originalAnimations,
	OUT vector<uint32>& outStateAnimIndices,
	OUT wstring* outFoundEnumType,
	const string& targetPrefabGuid,
	const string& targetPrefabName)
{
	if (sourceModelPath.empty()) return false;

	fs::path modelDir = fs::path(sourceModelPath).parent_path();
	if (!fs::exists(modelDir)) return false;

	fs::path firstFallback = "";
	fs::path guidMatch = "";

	for (const auto& entry : fs::directory_iterator(modelDir))
	{
		if (!entry.is_regular_file()) continue;
		string filename = entry.path().filename().string();
		if (filename.find(".animmap.json") == string::npos) continue;

		if (firstFallback.empty())
		{
			firstFallback = entry.path();
		}

		if (!targetPrefabName.empty() || !targetPrefabGuid.empty())
		{
			std::ifstream file(entry.path());
			if (file.is_open())
			{
				json data;
				try 
				{ 
					file >> data; 
					if (!targetPrefabName.empty() && data.value("PrefabName", "") == targetPrefabName)
					{
						return Load_StateAnimations_FromFile(entry.path().wstring(), originalAnimations, outStateAnimIndices, outFoundEnumType);
					}

					if (!targetPrefabGuid.empty() && data.value("PrefabGuid", "") == targetPrefabGuid)
					{
						guidMatch = entry.path();
					}
				} 
				catch (...) {}
			}
		}
	}

	if (!guidMatch.empty())
	{
		return Load_StateAnimations_FromFile(guidMatch.wstring(), originalAnimations, outStateAnimIndices, outFoundEnumType);
	}

	if (!targetPrefabName.empty() || !targetPrefabGuid.empty())
	{
		return false;
	}

	if (!firstFallback.empty())
	{
		return Load_StateAnimations_FromFile(firstFallback.wstring(), originalAnimations, outStateAnimIndices, outFoundEnumType);
	}

	return false;
}

bool AnimState_Parser::Save_StateAnimations_ToFile(
    const wstring& savePath,
    const string& enumTypeName,
    const string& prefabName,
    const vector<string>& stateNames,
    const vector<int32>& stateIndices,
    const unordered_map<string, string>& animationMapping)
{
    if (savePath.empty())
        return false;
    json root;
    root["EnumType"] = enumTypeName;
    // 최상위 Prefab 기준 저장 (ModelGuid 미저장)
    if (!prefabName.empty())
    {
        root["PrefabName"] = prefabName;
        string guid = Get_PrefabGuid(prefabName);
        if (!guid.empty())
            root["PrefabGuid"] = guid;
    }
    json mappings = json::array();
    for (size_t i = 0; i < stateNames.size(); ++i)
    {
        const auto it = animationMapping.find(stateNames[i]);
        if (it == animationMapping.end() || it->second.empty())
            continue;
        json entry;
        entry["Index"] = stateIndices[i];
        entry["State"] = stateNames[i];
        entry["Anim"] = it->second;
        mappings.push_back(entry);
    }
    root["Mappings"] = mappings;
    std::ofstream file(savePath);
    if (!file.is_open())
        return false;
    file << std::setw(4) << root << std::endl;
    return true;
}

string AnimState_Parser::Get_PrefabGuid(const string &prefabName)
{
	if (prefabName.empty())
	{
		return "";
	}
	wstring assetRoot = GAME->Get_AssetRoot();
    fs::path prefabsDir = fs::path(assetRoot) / "Data" / "json" / "Prefabs";
    fs::path metaPath = prefabsDir / (prefabName + ".prefab.json.meta");

    if (!fs::exists(metaPath))
    {
        for (const auto& entry : fs::recursive_directory_iterator(prefabsDir))
        {
            if (!entry.is_regular_file())
                continue;

            if (entry.path().filename() == (prefabName + ".prefab.json.meta"))
            {
                metaPath = entry.path(); // 프리팹이 Player/Monster 같은 하위 폴더로 이동해도 meta를 찾을 수 있게 한다.
                break;
            }
        }
    }

    if (!fs::exists(metaPath))
    {
        return "";
    }

	std::ifstream file(metaPath);
	if (!file.is_open())
	{
		return "";
	}

	json metaData;
	try
	{
		file >> metaData;
		return metaData.value("guid", "");
	}
	catch (...)
	{
		return "";
	}
}

string AnimState_Parser::Find_PrefabName_ByStaticClass(const string &ownerClassName, const string &selfClassName)
{
	wstring assetRoot = GAME->Get_AssetRoot();
	fs::path prefabsDir = fs::path(assetRoot) / "Data" / "json" / "Prefabs";
	if (!fs::exists(prefabsDir))
	{
		return "";
	}

	// 1. 파일 이름 기반 1차 고속 매칭
	if (!ownerClassName.empty())
	{
		fs::path candidate = prefabsDir / (ownerClassName + ".prefab.json");
		if (fs::exists(candidate))
		{
			return ownerClassName;
		}
	}
	if (!selfClassName.empty())
	{
		fs::path candidate = prefabsDir / (selfClassName + ".prefab.json");
		if (fs::exists(candidate))
		{
			return selfClassName;
		}
	}

	// 2. 2차 폴더 전체 탐색 (Fallback)
	for (const auto &entry : fs::recursive_directory_iterator(prefabsDir))
	{
		if (!entry.is_regular_file())
		{
			continue;
		}
		if (entry.path().extension() != ".json" || entry.path().stem().extension() != ".prefab")
		{
			continue;
		}

		std::ifstream file(entry.path());
		if (!file.is_open())
		{
			continue;
		}

		json data;
		try
		{
			file >> data;
			string rootClass = data.value("static_class", "");
			if (!ownerClassName.empty() && rootClass == ownerClassName)
			{
				return entry.path().stem().stem().string();
			}
			if (!selfClassName.empty() && rootClass == selfClassName)
			{
				return entry.path().stem().stem().string();
			}

			if (data.contains("parts") && data["parts"].is_array())
			{
				for (const auto &part : data["parts"])
				{
					if (part.contains("object") && part["object"].contains("static_class"))
					{
						string partClass = part["object"].value("static_class", "");
						if (!selfClassName.empty() && partClass == selfClassName)
						{
							return entry.path().stem().stem().string();
						}
					}
				}
			}
		}
		catch (...)
		{
			continue;
		}
	}

	return "";
}
