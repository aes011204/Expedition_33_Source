#include <Windows.h>
#include <objbase.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cwctype>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace
{
    std::string ToLower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
        return value;
    }

    fs::path FindRepoRootFrom(const fs::path& start)
    {
        std::error_code ec{};
        fs::path cur = fs::absolute(start, ec).lexically_normal();
        if (ec)
            return {};

        std::vector<fs::path> probes{};
        probes.push_back(cur);
        if (cur.has_parent_path())
            probes.push_back(cur.parent_path());

        for (fs::path p : probes)
        {
            while (!p.empty())
            {
                if (fs::exists(p / "Expedition_158.sln"))
                    return p;

                if (!p.has_parent_path())
                    break;
                p = p.parent_path();
            }
        }

        return {};
    }

    bool EndsWithIgnoreCase(const std::string& value, const std::string& suffix)
    {
        if (value.size() < suffix.size())
            return false;

        return ToLower(value.substr(value.size() - suffix.size())) == ToLower(suffix);
    }

    std::string GenerateGuidString()
    {
        GUID guid{};
        if (FAILED(CoCreateGuid(&guid)))
            return {};

        wchar_t buffer[64]{};
        if (StringFromGUID2(guid, buffer, static_cast<int>(std::size(buffer))) <= 0)
            return {};

        std::wstring wide(buffer);
        wide.erase(std::remove(wide.begin(), wide.end(), L'{'), wide.end());
        wide.erase(std::remove(wide.begin(), wide.end(), L'}'), wide.end());

        std::string out;
        out.reserve(wide.size());

        for (wchar_t ch : wide)
        {
            out.push_back(static_cast<char>(std::towlower(ch)));
        }

        return out;
    }

    fs::path ResolveTextureAbsolutePath(const fs::path& resourcesRoot, const json& textureNode)
    {
        if (textureNode.is_string())
        {
            const std::string path = textureNode.get<std::string>();
            if (path.empty())
                return {};

            fs::path p(path);
            if (p.is_absolute())
                return p.lexically_normal();
            return (resourcesRoot / p).lexically_normal();
        }

        if (textureNode.is_object())
        {
            const std::string fallbackPath = textureNode.value("texture_path", std::string{});
            if (fallbackPath.empty())
                return {};

            fs::path p(fallbackPath);
            if (p.is_absolute())
                return p.lexically_normal();
            return (resourcesRoot / p).lexically_normal();
        }

        return {};
    }

    std::string ToGenericRelative(const fs::path& resourcesRoot, const fs::path& absPath)
    {
        std::error_code ec{};
        const fs::path rel = fs::relative(absPath, resourcesRoot, ec);
        if (!ec)
            return rel.generic_string();
        return absPath.generic_string();
    }

    std::string EnsureMetaAndGetGuid(const fs::path& assetAbsPath)
    {
        const fs::path metaPath = fs::path(assetAbsPath.wstring() + L".meta");
        json meta{};

        if (fs::exists(metaPath))
        {
            std::ifstream in(metaPath);
            if (in.is_open())
            {
                try
                {
                    in >> meta;
                }
                catch (...)
                {
                    meta = json::object();
                }
            }
        }

        if (!meta.is_object())
            meta = json::object();

        std::string guid = meta.value("guid", std::string{});
        if (guid.empty())
            guid = GenerateGuidString();

        meta["guid"] = guid;
        if (!meta.contains("type") || !meta["type"].is_string())
            meta["type"] = "Texture";

        std::ofstream out(metaPath);
        if (out.is_open())
            out << meta.dump(4);

        return guid;
    }

    void ConvertTextureEntry(json& texturesObj, const char* slotName, const fs::path& resourcesRoot, uint32_t& convertedCount, uint32_t& createdMetaCount)
    {
        if (!texturesObj.contains(slotName))
            return;

        json& node = texturesObj[slotName];
        const fs::path absPath = ResolveTextureAbsolutePath(resourcesRoot, node);
        if (absPath.empty())
            return;
        if (!fs::exists(absPath))
            return;

        const fs::path metaPath = fs::path(absPath.wstring() + L".meta");
        const bool metaExisted = fs::exists(metaPath);

        const std::string guid = EnsureMetaAndGetGuid(absPath);
        if (guid.empty())
            return;

        if (!metaExisted)
            ++createdMetaCount;

        const std::string fallbackRel = ToGenericRelative(resourcesRoot, absPath);

        json out{};
        out["texture_guid"] = guid;
        out["texture_path"] = fallbackRel;
        node = std::move(out);
        ++convertedCount;
    }

    bool ConvertOneMaterialFile(const fs::path& materialPath, const fs::path& resourcesRoot, uint32_t& convertedCount, uint32_t& createdMetaCount)
    {
        std::ifstream in(materialPath);
        if (!in.is_open())
            return false;

        json root{};
        try
        {
            in >> root;
        }
        catch (...)
        {
            return false;
        }

        if (!root.is_object() || !root.contains("instances") || !root["instances"].is_array())
            return false;

        bool dirty = false;
        for (json& instanceNode : root["instances"])
        {
            if (!instanceNode.is_object())
                continue;
            if (!instanceNode.contains("textures") || !instanceNode["textures"].is_object())
                continue;

            json& textures = instanceNode["textures"];
            const uint32_t before = convertedCount;

            ConvertTextureEntry(textures, "baseColor", resourcesRoot, convertedCount, createdMetaCount);
            ConvertTextureEntry(textures, "normal", resourcesRoot, convertedCount, createdMetaCount);
            ConvertTextureEntry(textures, "orm", resourcesRoot, convertedCount, createdMetaCount);
            ConvertTextureEntry(textures, "emissive", resourcesRoot, convertedCount, createdMetaCount);
            ConvertTextureEntry(textures, "opacityMask", resourcesRoot, convertedCount, createdMetaCount);
            ConvertTextureEntry(textures, "hairDepth", resourcesRoot, convertedCount, createdMetaCount);
            ConvertTextureEntry(textures, "hairID", resourcesRoot, convertedCount, createdMetaCount);
            ConvertTextureEntry(textures, "irisHeight", resourcesRoot, convertedCount, createdMetaCount);
            ConvertTextureEntry(textures, "irisMask", resourcesRoot, convertedCount, createdMetaCount);

            if (convertedCount != before)
                dirty = true;
        }

        if (!dirty)
            return true;

        std::ofstream out(materialPath);
        if (!out.is_open())
            return false;
        out << root.dump(4);
        return true;
    }
}

int wmain(int argc, wchar_t* argv[])
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    fs::path materialsRoot{};
    fs::path resourcesRoot{};

    if (argc >= 2)
        materialsRoot = argv[1];
    if (argc >= 3)
        resourcesRoot = argv[2];

    std::error_code ec{};
    if (argc < 2)
    {
        const fs::path repoRoot = FindRepoRootFrom(fs::current_path(ec));
        if (!repoRoot.empty())
        {
            materialsRoot = repoRoot / LR"(Client\Bin\Resources\Materials)";
            resourcesRoot = repoRoot / LR"(Client\Bin\Resources)";
        }
        else
        {
            materialsRoot = LR"(Client\Bin\Resources\Materials)";
            resourcesRoot = LR"(Client\Bin\Resources)";
        }
    }
    else if (argc == 2)
    {
        const fs::path repoRoot = FindRepoRootFrom(fs::current_path(ec));
        if (!repoRoot.empty())
            resourcesRoot = repoRoot / LR"(Client\Bin\Resources)";
    }

    materialsRoot = fs::absolute(materialsRoot, ec).lexically_normal();
    resourcesRoot = fs::absolute(resourcesRoot, ec).lexically_normal();

    if (!fs::exists(materialsRoot) || !fs::is_directory(materialsRoot))
    {
        std::wcerr << L"[MaterialGuidConverter] invalid materials root: " << materialsRoot << L"\n";
        std::wcerr << L"Usage: MaterialGuidConverter.exe [materialsRoot] [resourcesRoot]\n";
        return 1;
    }

    if (!fs::exists(resourcesRoot) || !fs::is_directory(resourcesRoot))
    {
        std::wcerr << L"[MaterialGuidConverter] invalid resources root: " << resourcesRoot << L"\n";
        std::wcerr << L"Usage: MaterialGuidConverter.exe [materialsRoot] [resourcesRoot]\n";
        return 1;
    }

    std::wcout << L"[MaterialGuidConverter] materialsRoot=" << materialsRoot << L"\n";
    std::wcout << L"[MaterialGuidConverter] resourcesRoot=" << resourcesRoot << L"\n";

    uint32_t fileCount = 0;
    uint32_t convertedCount = 0;
    uint32_t createdMetaCount = 0;
    uint32_t failedCount = 0;

    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(materialsRoot))
    {
        if (!entry.is_regular_file())
            continue;

        const std::string filename = entry.path().filename().string();
        if (!EndsWithIgnoreCase(filename, ".material.json"))
            continue;

        ++fileCount;
        if (!ConvertOneMaterialFile(entry.path(), resourcesRoot, convertedCount, createdMetaCount))
            ++failedCount;
    }

    std::wcout
        << L"[MaterialGuidConverter] files=" << fileCount
        << L", convertedTextures=" << convertedCount
        << L", createdMeta=" << createdMetaCount
        << L", failedFiles=" << failedCount
        << L"\n";

    CoUninitialize();
    return failedCount == 0 ? 0 : 2;
}
