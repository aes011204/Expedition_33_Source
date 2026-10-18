#include "pch.h"
#include "UI_StateAnimationAssetService.h"

#include <commdlg.h>

namespace UIEditor
{
    namespace
    {
        constexpr wchar_t kDefaultStateAnimationName[] = L"NewUIStateAnimation";
        constexpr wchar_t kDefaultStateAnimationFileName[] = L"NewUIStateAnimation.uistateanim.json";

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
                LOG_WARN("UI animation directory create failed: {} ({})", exception.what(), String::ToString(directory.wstring()));
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
                LOG_WARN("UI animation resource root search failed: {}", exception.what());
            }

            return false;
        }

        bool Ends_With(const wstring& value, const wstring& suffix)
        {
            if (value.size() < suffix.size())
                return false;

            return equal(suffix.rbegin(), suffix.rend(), value.rbegin());
        }

        bool Starts_With(const wstring& value, const wstring& prefix)
        {
            if (value.size() < prefix.size())
                return false;

            return equal(prefix.begin(), prefix.end(), value.begin());
        }

        wstring To_Lower(wstring value)
        {
            transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](wchar_t ch)
                {
                    return static_cast<wchar_t>(towlower(ch));
                });
            return value;
        }

        wstring Trim_Copy(const wstring& value)
        {
            const auto beginIter = find_if_not(
                value.begin(),
                value.end(),
                [](wchar_t ch)
                {
                    return iswspace(ch) != 0;
                });

            if (beginIter == value.end())
                return {};

            const auto endIter = find_if_not(
                value.rbegin(),
                value.rend(),
                [](wchar_t ch)
                {
                    return iswspace(ch) != 0;
                }).base();

            return wstring(beginIter, endIter);
        }

        wstring Make_SafeStateAnimationFileName(const string& assetName)
        {
            wstring safeName = Trim_Copy(String::ToWString(assetName));
            if (safeName.empty())
                safeName = kDefaultStateAnimationName;

            for (wchar_t& ch : safeName)
            {
                switch (ch)
                {
                case L'<':
                case L'>':
                case L':':
                case L'"':
                case L'/':
                case L'\\':
                case L'|':
                case L'?':
                case L'*':
                    ch = L'_';
                    break;
                default:
                    if (ch < 32)
                        ch = L'_';
                    break;
                }
            }

            safeName = Trim_Copy(safeName);
            if (safeName.empty())
                safeName = kDefaultStateAnimationName;

            return safeName + L".uistateanim.json";
        }

        wstring Resolve_DefaultAnimationDirectory()
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
                LOG_WARN("UI animation current path query failed: {}", exception.what());
            }

            for (const fs::path& startPath : startPaths)
            {
                fs::path resourceRoot = {};
                if (Try_FindResourceRootFromStart(startPath, resourceRoot))
                {
                    const fs::path animationDirectory = (resourceRoot / L"UI" / L"UI_Animations").lexically_normal();
                    if (Try_CreateDirectory(animationDirectory))
                        return animationDirectory.wstring();
                }
            }

            const vector<fs::path> fallbackCandidates = {
                L"..\\Bin\\Resources\\UI\\UI_Animations",
                L"..\\Client\\Bin\\Resources\\UI\\UI_Animations",
                L"..\\..\\Client\\Bin\\Resources\\UI\\UI_Animations",
                L"..\\..\\..\\Client\\Bin\\Resources\\UI\\UI_Animations",
                L"Client\\Bin\\Resources\\UI\\UI_Animations",
            };

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
                    LOG_WARN("UI animation fallback directory failed: {} ({})", exception.what(), String::ToString(candidate.wstring()));
                }
            }

            return {};
        }

        bool Try_MakeAnimationResourcePath(const wstring& filePath, string& outResourcePath)
        {
            outResourcePath.clear();
            if (filePath.empty())
                return false;

            try
            {
                fs::path selectedPath = fs::weakly_canonical(fs::path(filePath));
                fs::path resourceRoot = {};
                if (!Try_FindResourceRootFromStart(selectedPath.parent_path(), resourceRoot))
                    return false;

                resourceRoot = fs::weakly_canonical(resourceRoot);
                fs::path relativePath = selectedPath.lexically_relative(resourceRoot);
                if (relativePath.empty())
                    return false;

                for (const fs::path& part : relativePath)
                {
                    if (part.native() == L"..")
                        return false;
                }

                const wstring lowerRelativePath = To_Lower(relativePath.generic_wstring());
                if (!Starts_With(lowerRelativePath, L"ui/ui_animations/"))
                    return false;

                const wstring lowerFileName = To_Lower(relativePath.filename().wstring());
                constexpr wchar_t kAnimationExtension[] = L".uianim.json";
                const wstring animationExtension = kAnimationExtension;
                if (!Ends_With(lowerFileName, animationExtension))
                    return false;

                wstring fileNameWithoutExtension = relativePath.filename().wstring();
                fileNameWithoutExtension.resize(fileNameWithoutExtension.size() - animationExtension.size());
                relativePath.replace_filename(fileNameWithoutExtension);

                outResourcePath = String::ToString(relativePath.generic_wstring());
                return !outResourcePath.empty();
            }
            catch (const exception& exception)
            {
                LOG_WARN("UI animation resource path convert failed: {} ({})", exception.what(), String::ToString(filePath));
                return false;
            }
        }

        bool Has_ValidTargetAnimationBinding(const UI_AnimationAsset& animationAsset)
        {
            for (const UI_AnimationTargetBinding& targetBinding : animationAsset.targets)
            {
                if (!targetBinding.targetName.empty() && !targetBinding.tracks.empty())
                    return true;
            }

            return false;
        }
    }

    bool UI_StateAnimationAssetService::Show_SaveAssetDialog(wstring& outFilePath) const
    {
        wchar_t filePathBuffer[MAX_PATH] = {};
        if (!outFilePath.empty())
            wcsncpy_s(filePathBuffer, outFilePath.c_str(), _TRUNCATE);
        else
        {
            const wstring defaultFileName = Make_SafeStateAnimationFileName(_asset.name);
            wcsncpy_s(filePathBuffer, defaultFileName.empty() ? kDefaultStateAnimationFileName : defaultFileName.c_str(), _TRUNCATE);
        }

        const wstring initialDirectory = Resolve_DefaultStateAnimationDirectory();

        OPENFILENAMEW openFileName = {};
        openFileName.lStructSize = sizeof(openFileName);
        openFileName.lpstrFilter = L"UI State Animation Files\0*.uistateanim.json\0JSON Files\0*.json\0All Files\0*.*\0";
        openFileName.lpstrFile = filePathBuffer;
        openFileName.nMaxFile = MAX_PATH;
        if (!initialDirectory.empty())
            openFileName.lpstrInitialDir = initialDirectory.c_str();
        openFileName.lpstrDefExt = L"uistateanim.json";
        openFileName.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

        if (FALSE == GetSaveFileNameW(&openFileName))
            return false;

        outFilePath = filePathBuffer;
        Ensure_StateAnimationExtension(outFilePath);
        return !outFilePath.empty();
    }

    bool UI_StateAnimationAssetService::Show_LoadAssetDialog(wstring& outFilePath) const
    {
        wchar_t filePathBuffer[MAX_PATH] = {};
        const wstring initialDirectory = Resolve_DefaultStateAnimationDirectory();

        OPENFILENAMEW openFileName = {};
        openFileName.lStructSize = sizeof(openFileName);
        openFileName.lpstrFilter = L"UI State Animation Files\0*.uistateanim.json\0JSON Files\0*.json\0All Files\0*.*\0";
        openFileName.lpstrFile = filePathBuffer;
        openFileName.nMaxFile = MAX_PATH;
        if (!initialDirectory.empty())
            openFileName.lpstrInitialDir = initialDirectory.c_str();
        openFileName.lpstrDefExt = L"uistateanim.json";
        openFileName.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

        if (FALSE == GetOpenFileNameW(&openFileName))
            return false;

        outFilePath = filePathBuffer;
        return !outFilePath.empty();
    }

    bool UI_StateAnimationAssetService::Show_SelectAnimationDialog(wstring& outFilePath) const
    {
        wchar_t filePathBuffer[MAX_PATH] = {};
        const wstring initialDirectory = Resolve_DefaultAnimationDirectory();

        OPENFILENAMEW openFileName = {};
        openFileName.lStructSize = sizeof(openFileName);
        openFileName.lpstrFilter = L"UI Animation Files\0*.uianim.json\0JSON Files\0*.json\0All Files\0*.*\0";
        openFileName.lpstrFile = filePathBuffer;
        openFileName.nMaxFile = MAX_PATH;
        if (!initialDirectory.empty())
            openFileName.lpstrInitialDir = initialDirectory.c_str();
        openFileName.lpstrDefExt = L"uianim.json";
        openFileName.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

        if (FALSE == GetOpenFileNameW(&openFileName))
            return false;

        outFilePath = filePathBuffer;
        return !outFilePath.empty();
    }

    bool UI_StateAnimationAssetService::Select_AnimationForEvent(UI_StateAnimationEvent eventType)
    {
        wstring filePath = {};
        if (!Show_SelectAnimationDialog(filePath))
            return false;

        UI_AnimationAsset animationAsset = {};
        if (!UI_AnimationAssetSerializer::Load_FromJsonFile(animationAsset, filePath))
        {
            LOG_WARN("UI state animation asset select failed to load: {}", String::ToString(filePath));
            return false;
        }

        const bool isPublicAnimation = Is_PublicAnimation(animationAsset);
        const bool isTargetAnimation = Is_TargetAnimation(animationAsset);
        if (!isPublicAnimation && !isTargetAnimation)
        {
            LOG_WARN("UI state animation only accepts Public_Animation or Target_Animation assets: {}", String::ToString(filePath));
            return false;
        }

        string resourcePath = {};
        if (!Try_MakeAnimationResourcePath(filePath, resourcePath))
        {
            LOG_WARN("UI state animation asset must be under Resources/UI/UI_Animations: {}", String::ToString(filePath));
            return false;
        }

        if (isTargetAnimation && !Has_ValidTargetAnimationBinding(animationAsset))
        {
            LOG_WARN("UI state animation Target_Animation has no valid target binding: {}", String::ToString(filePath));
            return false;
        }

        Set_EventAnimationPath(eventType, resourcePath);
        return true;
    }

    wstring UI_StateAnimationAssetService::Resolve_DefaultStateAnimationDirectory() const
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
            LOG_WARN("UI state animation current path query failed: {}", exception.what());
        }

        for (const fs::path& startPath : startPaths)
        {
            fs::path resourceRoot = {};
            if (Try_FindResourceRootFromStart(startPath, resourceRoot))
            {
                const fs::path stateAnimationDirectory = (resourceRoot / L"UI" / L"UI_StateAnimations").lexically_normal();
                if (Try_CreateDirectory(stateAnimationDirectory))
                    return stateAnimationDirectory.wstring();
            }
        }

        const vector<fs::path> fallbackCandidates = {
            L"..\\Bin\\Resources\\UI\\UI_StateAnimations",
            L"..\\Client\\Bin\\Resources\\UI\\UI_StateAnimations",
            L"..\\..\\Client\\Bin\\Resources\\UI\\UI_StateAnimations",
            L"..\\..\\..\\Client\\Bin\\Resources\\UI\\UI_StateAnimations",
            L"Client\\Bin\\Resources\\UI\\UI_StateAnimations",
        };

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
                LOG_WARN("UI state animation fallback directory failed: {} ({})", exception.what(), String::ToString(candidate.wstring()));
            }
        }

        return {};
    }

    void UI_StateAnimationAssetService::Ensure_StateAnimationExtension(wstring& filePath) const
    {
        if (filePath.empty())
            return;

        fs::path path(filePath);
        const wstring lowerFileName = To_Lower(path.filename().wstring());
        if (Ends_With(lowerFileName, L".uistateanim.json"))
            return;

        path.replace_extension(L".uistateanim.json");
        filePath = path.wstring();
    }
}
