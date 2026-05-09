#pragma once

#include "Engine_Define.h"
#include <filesystem>
#include <fstream>

namespace AnimationNameUtils
{
    namespace fs = std::filesystem;

    struct AssetAnimationInfo
    {
        fs::path filePath{};
        string stem{};
        string displayName{};
    };

    inline string Make_DisplayNameFallback(const string& animationName)
    {
        string result = animationName;

        const char* genericSuffixes[] =
        {
            "_Unreal Take",
            "|Unreal Take",
            " Unreal Take",
            "_Default Take",
            "|Default Take",
            " Default Take",
        };

        for (const char* suffix : genericSuffixes)
        {
            const size_t suffixLen = strlen(suffix);
            if (result.size() >= suffixLen &&
                result.compare(result.size() - suffixLen, suffixLen, suffix) == 0)
            {
                result.erase(result.size() - suffixLen);
                break;
            }
        }

        return result.empty() ? animationName : result;
    }

    inline string Read_DisplayNameFromMeta(const fs::path& animPath)
    {
        const string fallback = Make_DisplayNameFallback(animPath.stem().string());
        const fs::path metaPath = animPath.wstring() + L".meta";

        try
        {
            if (!fs::exists(metaPath))
                return fallback;

            std::ifstream file(metaPath);
            if (!file.is_open())
                return fallback;

            json root{};
            file >> root;
            file.close();

            string displayName = root.value("displayName", string{});
            if (!displayName.empty())
                return displayName;

            root["displayName"] = fallback;

            std::ofstream outFile(metaPath);
            if (outFile.is_open())
                outFile << root.dump(4);

            return fallback;
        }
        catch (...)
        {
            return fallback;
        }
    }

    inline vector<AssetAnimationInfo> Collect_AssetAnimationInfos(const wstring& sourceModelPath)
    {
        vector<AssetAnimationInfo> infos{};
        if (sourceModelPath.empty())
            return infos;

        const fs::path modelDir = fs::path(sourceModelPath).parent_path();
        if (!fs::exists(modelDir))
            return infos;

        const auto scanDirectory = [&infos](const fs::path& directory)
        {
            if (!fs::exists(directory) || !fs::is_directory(directory))
                return;

            for (const auto& entry : fs::directory_iterator(directory))
            {
                if (!entry.is_regular_file() || entry.path().extension() != ".anim")
                    continue;

                AssetAnimationInfo info{};
                info.filePath = entry.path();
                info.stem = entry.path().stem().string();
                info.displayName = Read_DisplayNameFromMeta(entry.path());
                infos.push_back(move(info));
            }
        };

        scanDirectory(modelDir);
        scanDirectory(modelDir / "Animations");

        ::sort(infos.begin(), infos.end(), [](const AssetAnimationInfo& lhs, const AssetAnimationInfo& rhs)
        {
            return String::ToLowerCopy(lhs.stem) < String::ToLowerCopy(rhs.stem);
        });

        infos.erase(::unique(infos.begin(), infos.end(), [](const AssetAnimationInfo& lhs, const AssetAnimationInfo& rhs)
        {
            return String::ToLowerCopy(lhs.stem) == String::ToLowerCopy(rhs.stem);
        }), infos.end());

        return infos;
    }

    inline string Extract_PlaybackLeafName(const string& playbackName)
    {
        const size_t pipePos = playbackName.find_last_of('|');
        if (pipePos == string::npos)
            return playbackName;

        return playbackName.substr(pipePos + 1);
    }

    inline int Score_AssetMatch(const AssetAnimationInfo& info, const string& playbackName)
    {
        const string leaf = String::ToLowerCopy(Extract_PlaybackLeafName(playbackName));
        const string stem = String::ToLowerCopy(info.stem);
        const string displayName = String::ToLowerCopy(info.displayName);

        if (leaf.empty())
            return 0;

        if (stem == leaf || displayName == leaf)
            return 100;

        if (stem.find(leaf) != string::npos || displayName.find(leaf) != string::npos)
            return 80;

        if (leaf.find(stem) != string::npos || leaf.find(displayName) != string::npos)
            return 70;

        if (playbackName.find('|') == string::npos &&
            (stem.find("default") != string::npos || displayName.find("default") != string::npos))
            return 20;

        return 0;
    }

    inline int32 Find_BestAssetMatchIndex(const vector<AssetAnimationInfo>& infos, const string& playbackName, const vector<bool>& used)
    {
        int32 bestIndex = -1;
        int bestScore = 0;

        for (int32 i = 0; i < static_cast<int32>(infos.size()); ++i)
        {
            if (i < static_cast<int32>(used.size()) && used[i])
                continue;

            const int score = Score_AssetMatch(infos[i], playbackName);
            if (score > bestScore)
            {
                bestScore = score;
                bestIndex = i;
            }
        }

        return bestIndex;
    }

    inline int32 Find_BestPlaybackMatchIndex(const AssetAnimationInfo& info, const vector<string>& playbackNames, const vector<bool>& used)
    {
        int32 bestIndex = -1;
        int bestScore = 0;

        for (int32 i = 0; i < static_cast<int32>(playbackNames.size()); ++i)
        {
            if (i < static_cast<int32>(used.size()) && used[i])
                continue;

            const int score = Score_AssetMatch(info, playbackNames[i]);
            if (score > bestScore)
            {
                bestScore = score;
                bestIndex = i;
            }
        }

        return bestIndex;
    }
}
