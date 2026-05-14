#include "pch.h"
#include "AnimState_Parser.h"
#include "GameInstance.h"
#include "Animation.h"

bool AnimState_Parser::Load_StateAnimations(
    const wstring& sourceModelPath,
    const vector<Shared<Animation>>& originalAnimations,
    OUT vector<uint32>& outStateAnimIndices,
    OUT wstring* outFoundEnumType)
{
    outStateAnimIndices.clear();
    if (sourceModelPath.empty()) return false;

    // 1. 모델 파일과 같은 위치에서 .meta 파일을 직접 읽어 GUID를 획득합니다.
    // 팀원 간 절대 경로가 다르더라도 .meta 파일 내의 GUID는 동일하므로 가장 안전합니다.
    fs::path modelFilePath(sourceModelPath);
    fs::path modelDir = modelFilePath.parent_path();
    fs::path metaPath = sourceModelPath + L".meta";

    string myGuid;
    if (fs::exists(metaPath))
    {
        std::ifstream metaFile(metaPath);
        if (metaFile.is_open())
        {
            json metaJson;
            metaFile >> metaJson;
            myGuid = metaJson.value("guid", "");
        }
    }

    // .meta에서 찾지 못한 경우에만 에셋 매니저 검색 (하위 호환성)
    if (myGuid.empty())
    {
        myGuid = GAME->Find_AssetGUID(sourceModelPath);
    }

    if (myGuid.empty()) return false;

    // 2. 모델 파일이 위치한 디렉토리에서 .animmap.json 파일을 검색합니다.
    if (!fs::exists(modelDir)) return false;

    for (const auto& entry : fs::directory_iterator(modelDir))
    {
        if (entry.is_regular_file() &&
            entry.path().filename().string().find(".animmap.json") != string::npos)
        {
            std::ifstream file(entry.path());
            if (!file.is_open()) continue;

            json root;
            try {
                file >> root;
            }
            catch (...) { continue; }

            if (!root.contains("ModelGuid")) continue;

            // 로컬 .meta에서 읽어온 GUID와 json의 ModelGuid를 비교하여 일치하는 매핑 파일만 로드합니다.
            if (root["ModelGuid"].get<string>() == myGuid)
            {
                if (root.contains("EnumType") && outFoundEnumType)
                {
                    string enumStr = root["EnumType"].get<string>();
                    *outFoundEnumType = wstring(enumStr.begin(), enumStr.end());
                }

                if (root.contains("Mappings"))
                {
                    uint32 maxIndex = 0;
                    for (const auto& mapping : root["Mappings"]) {
                        uint32 idx = mapping["Index"].get<uint32>();
                        if (idx > maxIndex) maxIndex = idx;
                    }

                    outStateAnimIndices.resize(maxIndex + 1, UINT32_MAX);

                    for (const auto& mapping : root["Mappings"])
                    {
                        uint32 stateIdx = mapping["Index"].get<uint32>();
                        string animName = mapping["Anim"].get<string>();

                        for (uint32 i = 0; i < originalAnimations.size(); ++i)
                        {
                            if (strcmp(originalAnimations[i]->Get_AnimationName().c_str(), animName.c_str()) == 0)
                            {
                                outStateAnimIndices[stateIdx] = i;
                                break;
                            }
                        }
                    }
                }

                // 로깅
                uint32 validMappingCount = 0;
                for (uint32 idx : outStateAnimIndices) { if (idx != UINT32_MAX) validMappingCount++; }

                string enumTypeStr = root.contains("EnumType") ? root["EnumType"].get<string>() : "Unknown";
                string modelName = modelFilePath.stem().string();

                LOG_INFO("AnimState Mapping Auto-Loaded : Model [{}], EnumType [{}], Mapped [{}] states.",
                    modelName, enumTypeStr, validMappingCount);

                return true;
            }
        }
    }

    return false;
}
