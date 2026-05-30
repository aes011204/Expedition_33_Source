#include "pch.h"
#include "UI_AnimationAssetService.h"

#include <commdlg.h>

namespace UIEditor
{
    namespace
    {
        constexpr wchar_t kDefaultAnimationName[] = L"NewUIAnimation";
        constexpr wchar_t kDefaultAnimationFileName[] = L"NewUIAnimation.uianim.json";
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

        wstring Make_SafeAnimationFileName(const string& assetName)
        {
            wstring safeName = Trim_Copy(String::ToWString(assetName));
            if (safeName.empty())
                safeName = kDefaultAnimationName;

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
                safeName = kDefaultAnimationName;

            return safeName + L".uianim.json";
        }

        wstring Resolve_DefaultPublicAnimationDirectory()
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

    }

    bool UI_AnimationAssetService::Show_SaveAssetDialog(wstring& outFilePath) const
    {
        wchar_t filePathBuffer[MAX_PATH] = {};
        if (!outFilePath.empty())
            wcsncpy_s(filePathBuffer, outFilePath.c_str(), _TRUNCATE);
        else
        {
            const wstring defaultFileName = Make_SafeAnimationFileName(_asset.name);
            wcsncpy_s(filePathBuffer, defaultFileName.empty() ? kDefaultAnimationFileName : defaultFileName.c_str(), _TRUNCATE);
        }

        const wstring initialDirectory = Resolve_DefaultAnimationDirectory();

        OPENFILENAMEW openFileName = {};
        openFileName.lStructSize = sizeof(openFileName);
        openFileName.lpstrFilter = L"UI Animation Files\0*.uianim.json\0JSON Files\0*.json\0All Files\0*.*\0";
        openFileName.lpstrFile = filePathBuffer;
        openFileName.nMaxFile = MAX_PATH;
        if (!initialDirectory.empty())
            openFileName.lpstrInitialDir = initialDirectory.c_str();
        openFileName.lpstrDefExt = L"uianim.json";
        openFileName.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

        if (FALSE == GetSaveFileNameW(&openFileName))
            return false;

        outFilePath = filePathBuffer;
        Ensure_AnimationExtension(outFilePath);
        return !outFilePath.empty();
    }

    bool UI_AnimationAssetService::Show_LoadAssetDialog(wstring& outFilePath) const
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

    wstring UI_AnimationAssetService::Resolve_DefaultAnimationDirectory() const
    {
        return Resolve_DefaultPublicAnimationDirectory();
    }

    void UI_AnimationAssetService::Ensure_AnimationExtension(wstring& filePath) const
    {
        if (filePath.empty())
            return;

        fs::path path(filePath);
        const wstring lowerFileName = To_Lower(path.filename().wstring());
        if (Ends_With(lowerFileName, L".uianim.json"))
            return;

        path.replace_extension(L".uianim.json");
        filePath = path.wstring();
    }

}
