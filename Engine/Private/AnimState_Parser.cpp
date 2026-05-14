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

    string myGuid = GAME->Find_AssetGUID(sourceModelPath);

    fs::path modelDir = GAME->Get_AssetRoot() / fs::path(sourceModelPath).parent_path();
    if (!fs::exists(modelDir)) return false;

    for (const auto& entry : fs::directory_iterator(modelDir))
    {
        if (entry.is_regular_file() && entry.path().filename().string().find(".animmap.json") != string::npos)
        {
            std::ifstream file(entry.path());
            if (!file.is_open()) continue;

            json root; file >> root;
            if (!root.contains("ModelGuid")) continue;

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

                    // 빈 곳은 -1로 넉넉하게 사이즈 확보
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

                uint32 validMappingCount = 0;
                for (uint32 idx : outStateAnimIndices)
                {
                    if (idx != UINT32_MAX) validMappingCount++;
                }
            
                string enumTypeStr = root.contains("EnumType") ? root["EnumType"].get<string>() : "Unknown";
                string modelName = fs::path(sourceModelPath).stem().string();

                LOG_INFO("AnimState Mapping Auto-Loaded : Model [{}], EnumType [{}], Mapped [{}] states.",
                    modelName, enumTypeStr, validMappingCount);

                for (uint32 stateIdx = 0; stateIdx < outStateAnimIndices.size(); ++stateIdx)
                {
                    if (outStateAnimIndices[stateIdx] != UINT32_MAX)
                    {
                        LOG_INFO("  - State [{}] -> AnimIndex [{}] ({})", 
                            stateIdx, 
                            outStateAnimIndices[stateIdx], 
                            originalAnimations[outStateAnimIndices[stateIdx]]->Get_AnimationName().c_str());
                    }
                }

                return true;
            }
        }
    }

    return false;
}

