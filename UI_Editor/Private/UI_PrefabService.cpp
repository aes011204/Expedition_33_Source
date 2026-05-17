#include "pch.h"
#include "UI_PrefabService.h"
#include "UI_DocumentSerializer.h"

namespace UIEditor
{
    namespace
    {
        constexpr int32 kCurrentPrefabVersion = 2;
        constexpr wchar_t kPrefabExtension[] = L".uiprefab.json";
        constexpr const char* kDefaultPrefabName = "UIPrefab";

        fs::path Get_ModuleDirectory()
        {
            wchar_t modulePath[MAX_PATH] = {};
            if (0 == GetModuleFileNameW(nullptr, modulePath, MAX_PATH))
                return {};

            return fs::path(modulePath).parent_path();
        }

        bool Try_CreateDirectory(const fs::path& directory)
        {
            if (directory.empty())
                return false;

            try
            {
                if (fs::exists(directory))
                    return fs::is_directory(directory);

                return fs::create_directories(directory);
            }
            catch (const exception& exception)
            {
                LOG_WARN("UI prefab directory create failed: {} ({})", exception.what(), String::ToString(directory.wstring()));
                return false;
            }
        }

        bool Try_FindResourceRootFromStart(const fs::path& startPath, fs::path& outResourceRoot)
        {
            if (startPath.empty())
                return false;

            try
            {
                fs::path start = fs::absolute(startPath).lexically_normal();
                for (fs::path currentPath = start; !currentPath.empty(); currentPath = currentPath.parent_path())
                {
                    const fs::path candidate = (currentPath / L"Client" / L"Bin" / L"Resources").lexically_normal();
                    if (fs::exists(candidate) && fs::is_directory(candidate))
                    {
                        outResourceRoot = candidate;
                        return true;
                    }

                    if (currentPath == currentPath.parent_path())
                        break;
                }
            }
            catch (const exception& exception)
            {
                LOG_WARN("UI prefab resource root search failed: {}", exception.what());
            }

            return false;
        }

        vector<fs::path> Build_FallbackRootCandidates(const fs::path& startPath)
        {
            vector<fs::path> candidates;
            if (startPath.empty())
                return candidates;

            candidates.push_back(startPath / L"..\\Bin\\Resources\\UI\\UI_Prefabs");
            candidates.push_back(startPath / L"..\\Client\\Bin\\Resources\\UI\\UI_Prefabs");
            candidates.push_back(startPath / L"..\\..\\Client\\Bin\\Resources\\UI\\UI_Prefabs");
            candidates.push_back(startPath / L"..\\..\\..\\Client\\Bin\\Resources\\UI\\UI_Prefabs");
            candidates.push_back(startPath / L"Bin\\Resources\\UI\\UI_Prefabs");
            candidates.push_back(startPath / L"Client\\Bin\\Resources\\UI\\UI_Prefabs");
            return candidates;
        }

        string Read_String(const json& object, const char* key, const string& fallbackValue)
        {
            const auto iter = object.find(key);
            if (iter == object.end() || !iter->is_string())
                return fallbackValue;

            return iter->get<string>();
        }

        int32 Read_Int(const json& object, const char* key, int32 fallbackValue)
        {
            const auto iter = object.find(key);
            if (iter == object.end() || !iter->is_number_integer())
                return fallbackValue;

            return iter->get<int32>();
        }

        string Sanitize_FileName(string fileName)
        {
            if (fileName.empty())
                fileName = kDefaultPrefabName;

            for (char& character : fileName)
            {
                switch (character)
                {
                case '<':
                case '>':
                case ':':
                case '"':
                case '/':
                case '\\':
                case '|':
                case '?':
                case '*':
                    character = '_';
                    break;
                default:
                    break;
                }
            }

            while (!fileName.empty() && (fileName.back() == ' ' || fileName.back() == '.'))
                fileName.pop_back();

            return fileName.empty() ? kDefaultPrefabName : fileName;
        }

        bool Has_PrefabExtension(const fs::path& filePath)
        {
            const wstring fileName = filePath.filename().wstring();
            if (fileName.size() < wcslen(kPrefabExtension))
                return false;

            const wstring suffix = fileName.substr(fileName.size() - wcslen(kPrefabExtension));
            return _wcsicmp(suffix.c_str(), kPrefabExtension) == 0;
        }

        void Collect_ObjectAndChildren(const UI_Document& document, ObjectId objectId, vector<ObjectId>& outObjectIds)
        {
            if (objectId == 0)
                return;

            outObjectIds.push_back(objectId);
            for (const UI_Object& object : document.Get_Objects())
            {
                if (object.parentId == objectId)
                    Collect_ObjectAndChildren(document, object.id, outObjectIds);
            }
        }

        bool Is_DescendantOfAnyRoot(const UI_Document& document, ObjectId objectId, const unordered_set<ObjectId>& rootObjectIdSet)
        {
            const UI_Object* object = document.Find_Object(objectId);
            while (nullptr != object && object->parentId != 0)
            {
                if (rootObjectIdSet.find(object->parentId) != rootObjectIdSet.end())
                    return true;

                object = document.Find_Object(object->parentId);
            }

            return false;
        }

        vector<ObjectId> Normalize_RootObjectIds(const UI_Document& document, const vector<ObjectId>& objectIds)
        {
            vector<ObjectId> normalizedRootObjectIds;
            unordered_set<ObjectId> rootObjectIdSet;

            for (ObjectId objectId : objectIds)
            {
                if (objectId == 0 || nullptr == document.Find_Object(objectId))
                    continue;

                if (Is_DescendantOfAnyRoot(document, objectId, rootObjectIdSet))
                    continue;

                vector<ObjectId> filteredRootObjectIds;
                filteredRootObjectIds.reserve(normalizedRootObjectIds.size());
                for (ObjectId rootObjectId : normalizedRootObjectIds)
                {
                    if (!document.Is_Descendant(objectId, rootObjectId))
                        filteredRootObjectIds.push_back(rootObjectId);
                    else
                        rootObjectIdSet.erase(rootObjectId);
                }

                normalizedRootObjectIds = move(filteredRootObjectIds);
                if (rootObjectIdSet.insert(objectId).second)
                    normalizedRootObjectIds.push_back(objectId);
            }

            return normalizedRootObjectIds;
        }
    }

    HRESULT UI_PrefabService::Initialize()
    {
        _rootDirectory = Resolve_PrefabRootDirectory();
        return _rootDirectory.empty() ? E_FAIL : S_OK;
    }

    bool UI_PrefabService::Ensure_RootDirectory() const
    {
        return !_rootDirectory.empty() && Try_CreateDirectory(fs::path(_rootDirectory));
    }

    wstring UI_PrefabService::Build_PrefabFilePath(const wstring& directoryPath, const string& prefabName) const
    {
        fs::path directory = directoryPath.empty() ? fs::path(_rootDirectory) : fs::path(directoryPath);
        string safeName = Sanitize_FileName(prefabName);
        fs::path filePath = directory / String::ToWString(safeName);

        const wstring fileName = filePath.filename().wstring();
        if (fileName.size() < wcslen(kPrefabExtension) ||
            _wcsicmp(fileName.substr(fileName.size() - wcslen(kPrefabExtension)).c_str(), kPrefabExtension) != 0)
        {
            filePath += kPrefabExtension;
        }

        return filePath.lexically_normal().wstring();
    }

    bool UI_PrefabService::Save_Prefab(
        const UI_Document& document,
        const vector<ObjectId>& rootObjectIds,
        const string& prefabName,
        const wstring& filePath) const
    {
        const vector<ObjectId> normalizedRootObjectIds = Normalize_RootObjectIds(document, rootObjectIds);
        if (normalizedRootObjectIds.empty())
        {
            LOG_WARN("UI prefab save failed: no selected root objects.");
            return false;
        }

        vector<ObjectId> objectIds;
        unordered_set<ObjectId> objectIdSet;
        for (ObjectId rootObjectId : normalizedRootObjectIds)
        {
            vector<ObjectId> rootAndChildren;
            Collect_ObjectAndChildren(document, rootObjectId, rootAndChildren);
            for (ObjectId objectId : rootAndChildren)
            {
                if (objectIdSet.insert(objectId).second)
                    objectIds.push_back(objectId);
            }
        }

        if (objectIds.empty())
            return false;

        ImVec2 prefabMin = ImVec2(FLT_MAX, FLT_MAX);
        for (ObjectId objectId : objectIds)
        {
            const ImVec2 worldPosition = document.Get_WorldPosition(objectId);
            prefabMin.x = min(prefabMin.x, worldPosition.x);
            prefabMin.y = min(prefabMin.y, worldPosition.y);
        }

        unordered_set<ObjectId> rootObjectIdSet(normalizedRootObjectIds.begin(), normalizedRootObjectIds.end());

        json root = {};
        root["version"] = kCurrentPrefabVersion;
        root["name"] = prefabName.empty() ? kDefaultPrefabName : prefabName;
        root["rootObjectIds"] = json::array();
        root["objects"] = json::array();

        for (ObjectId rootObjectId : normalizedRootObjectIds)
            root["rootObjectIds"].push_back(rootObjectId);

        for (ObjectId objectId : objectIds)
        {
            const UI_Object* sourceObject = document.Find_Object(objectId);
            if (nullptr == sourceObject)
                continue;

            UI_Object prefabObject = *sourceObject;
            if (rootObjectIdSet.find(prefabObject.id) != rootObjectIdSet.end())
            {
                const ImVec2 worldPosition = document.Get_WorldPosition(prefabObject.id);
                prefabObject.parentId = 0;
                prefabObject.position = ImVec2(worldPosition.x - prefabMin.x, worldPosition.y - prefabMin.y);
            }

            root["objects"].push_back(UI_DocumentSerializer::Object_ToJson(prefabObject));
        }

        try
        {
            const fs::path path = fs::path(filePath);
            if (!Try_CreateDirectory(path.parent_path()))
                return false;

            ofstream file{ path };
            if (!file.is_open())
            {
                LOG_ERROR("UI prefab save failed: cannot open {}", String::ToString(filePath));
                return false;
            }

            file << root.dump(4);
            if (!file.good())
            {
                LOG_ERROR("UI prefab save failed while writing {}", String::ToString(filePath));
                return false;
            }

            LOG_INFO("UI prefab saved: {}", String::ToString(filePath));
            return true;
        }
        catch (const exception& exception)
        {
            LOG_ERROR("UI prefab save exception: {} ({})", exception.what(), String::ToString(filePath));
            return false;
        }
    }

    bool UI_PrefabService::Load_Prefab(const wstring& filePath, UI_PrefabAsset& outPrefab) const
    {
        outPrefab = {};

        try
        {
            const fs::path path = fs::path(filePath);
            if (!Has_PrefabExtension(path))
                return false;

            ifstream file{ path };
            if (!file.is_open())
            {
                LOG_WARN("UI prefab load skipped: cannot open {}", String::ToString(filePath));
                return false;
            }

            json root = {};
            file >> root;
            if (!root.is_object())
            {
                LOG_WARN("UI prefab load skipped: root is not an object ({})", String::ToString(filePath));
                return false;
            }

            const int32 version = Read_Int(root, "version", 1);
            if (version > kCurrentPrefabVersion)
                LOG_WARN("UI prefab version {} is newer than supported version {}. Trying compatible load.", version, kCurrentPrefabVersion);

            UI_PrefabAsset prefab = {};
            prefab.name = Read_String(root, "name", String::ToString(path.stem().wstring()));

            const auto rootObjectIdsIter = root.find("rootObjectIds");
            if (rootObjectIdsIter != root.end() && rootObjectIdsIter->is_array())
            {
                unordered_set<ObjectId> rootObjectIdSet;
                for (const json& rootObjectIdItem : *rootObjectIdsIter)
                {
                    if (!rootObjectIdItem.is_number_unsigned() && !rootObjectIdItem.is_number_integer())
                        continue;

                    const ObjectId rootObjectId = rootObjectIdItem.get<ObjectId>();
                    if (rootObjectId != 0 && rootObjectIdSet.insert(rootObjectId).second)
                        prefab.rootObjectIds.push_back(rootObjectId);
                }
            }

            const auto objectsIter = root.find("objects");
            if (objectsIter == root.end() || !objectsIter->is_array())
            {
                LOG_WARN("UI prefab load skipped: objects array missing ({})", String::ToString(filePath));
                return false;
            }

            unordered_set<ObjectId> objectIdSet;
            for (const json& objectItem : *objectsIter)
            {
                UI_Object object = {};
                if (!UI_DocumentSerializer::Try_ReadObject(objectItem, object))
                    continue;

                if (!objectIdSet.insert(object.id).second)
                {
                    LOG_WARN("UI prefab load skipped duplicated object id {} ({})", object.id, String::ToString(filePath));
                    continue;
                }

                prefab.objects.push_back(move(object));
            }

            if (prefab.objects.empty())
            {
                LOG_WARN("UI prefab load skipped: no valid objects ({})", String::ToString(filePath));
                return false;
            }

            if (prefab.rootObjectIds.empty())
            {
                for (const UI_Object& object : prefab.objects)
                {
                    if (object.parentId == 0)
                        prefab.rootObjectIds.push_back(object.id);
                }
            }

            unordered_set<ObjectId> objectIds;
            for (const UI_Object& object : prefab.objects)
                objectIds.insert(object.id);

            prefab.rootObjectIds.erase(
                remove_if(
                    prefab.rootObjectIds.begin(),
                    prefab.rootObjectIds.end(),
                    [&objectIds](ObjectId objectId)
                    {
                        return objectIds.find(objectId) == objectIds.end();
                    }),
                prefab.rootObjectIds.end());

            if (prefab.rootObjectIds.empty())
            {
                LOG_WARN("UI prefab load skipped: no valid root objects ({})", String::ToString(filePath));
                return false;
            }

            outPrefab = move(prefab);
            return true;
        }
        catch (const exception& exception)
        {
            LOG_WARN("UI prefab load exception: {} ({})", exception.what(), String::ToString(filePath));
            return false;
        }
    }

    wstring UI_PrefabService::Resolve_PrefabRootDirectory() const
    {
        vector<fs::path> startPaths;

        const fs::path moduleDirectory = Get_ModuleDirectory();
        if (!moduleDirectory.empty())
            startPaths.push_back(moduleDirectory);

        try
        {
            startPaths.push_back(fs::current_path());
        }
        catch (const exception& exception)
        {
            LOG_WARN("UI prefab current path query failed: {}", exception.what());
        }

        for (const fs::path& startPath : startPaths)
        {
            fs::path resourceRoot = {};
            if (Try_FindResourceRootFromStart(startPath, resourceRoot))
            {
                const fs::path prefabDirectory = (resourceRoot / L"UI" / L"UI_Prefabs").lexically_normal();
                if (Try_CreateDirectory(prefabDirectory))
                    return prefabDirectory.wstring();
            }
        }

        vector<fs::path> fallbackCandidates;
        for (const fs::path& startPath : startPaths)
        {
            vector<fs::path> candidates = Build_FallbackRootCandidates(startPath);
            fallbackCandidates.insert(fallbackCandidates.end(), candidates.begin(), candidates.end());
        }

        for (const fs::path& candidate : fallbackCandidates)
        {
            try
            {
                const fs::path normalizedCandidate = fs::absolute(candidate).lexically_normal();
                if (fs::exists(normalizedCandidate) && fs::is_directory(normalizedCandidate))
                    return normalizedCandidate.wstring();
            }
            catch (...)
            {
            }
        }

        for (const fs::path& candidate : fallbackCandidates)
        {
            try
            {
                const fs::path normalizedCandidate = fs::absolute(candidate).lexically_normal();
                if (Try_CreateDirectory(normalizedCandidate))
                    return normalizedCandidate.wstring();
            }
            catch (const exception& exception)
            {
                LOG_WARN("UI prefab fallback directory failed: {} ({})", exception.what(), String::ToString(candidate.wstring()));
            }
        }

        LOG_WARN("UI prefab root directory could not be resolved.");
        return {};
    }
}
