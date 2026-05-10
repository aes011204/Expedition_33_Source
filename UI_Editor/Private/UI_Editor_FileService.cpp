#include "pch.h"
#include "UI_Editor_FileService.h"

#include <commdlg.h>

namespace UIEditor
{
    namespace
    {
        constexpr wchar_t kDefaultJsonFileName[] = L"UI_Layout.json";

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
                LOG_WARN("UI Editor json directory create failed: {} ({})", exception.what(), String::ToString(directory.wstring()));
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
                LOG_WARN("UI Editor resource root search failed: {}", exception.what());
            }

            return false;
        }

        vector<fs::path> Build_DefaultJsonDirectoryCandidates(const fs::path& startPath)
        {
            vector<fs::path> candidates;
            if (startPath.empty())
                return candidates;

            candidates.push_back(startPath / L"..\\Bin\\Resources\\UI\\UI_Json");
            candidates.push_back(startPath / L"..\\Client\\Bin\\Resources\\UI\\UI_Json");
            candidates.push_back(startPath / L"..\\..\\Client\\Bin\\Resources\\UI\\UI_Json");
            candidates.push_back(startPath / L"..\\..\\..\\Client\\Bin\\Resources\\UI\\UI_Json");
            candidates.push_back(startPath / L"Bin\\Resources\\UI\\UI_Json");
            candidates.push_back(startPath / L"Client\\Bin\\Resources\\UI\\UI_Json");

            return candidates;
        }
    }

    HRESULT UI_Editor_FileService::Initialize()
    {
        _defaultJsonDirectory = Resolve_DefaultJsonDirectory();
        return S_OK;
    }

    bool UI_Editor_FileService::Save_Document(const UI_Document& document, bool forceSaveAs)
    {
        if (_currentDocumentPath.empty() || forceSaveAs)
        {
            wstring filePath = _currentDocumentPath;
            if (!Show_SaveFileDialog(filePath))
                return false;

            Ensure_JsonExtension(filePath);

            if (!document.Save_ToJsonFile(filePath))
                return false;

            _currentDocumentPath = filePath;
            return true;
        }

        return document.Save_ToJsonFile(_currentDocumentPath);
    }

    bool UI_Editor_FileService::Load_Document(UI_Document& document)
    {
        wstring filePath = {};
        if (!Show_LoadFileDialog(filePath))
            return false;

        if (!document.Load_FromJsonFile(filePath))
            return false;

        _currentDocumentPath = filePath;
        return true;
    }

    UI_FileConfirmResult UI_Editor_FileService::Confirm_SaveIfDirty(
        bool isDirty,
        const wchar_t* actionName,
        const UI_Document& document)
    {
        if (!isDirty)
            return UI_FileConfirmResult::ContinueWithoutSave;

        const wstring actionText = (nullptr != actionName && L'\0' != actionName[0]) ? actionName : L"continue";
        const wstring message = L"The current UI document has unsaved changes.\n\nSave before you " + actionText + L"?";

        const int result = MessageBoxW(
            nullptr,
            message.c_str(),
            L"UI Editor",
            MB_ICONQUESTION | MB_YESNOCANCEL);

        if (result == IDCANCEL)
            return UI_FileConfirmResult::Cancel;

        if (result == IDYES)
            return Save_Document(document, false) ? UI_FileConfirmResult::Saved : UI_FileConfirmResult::Cancel;

        return UI_FileConfirmResult::ContinueWithoutSave;
    }

    bool UI_Editor_FileService::Show_SaveFileDialog(wstring& outFilePath) const
    {
        wchar_t filePathBuffer[MAX_PATH] = {};
        if (!outFilePath.empty())
        {
            wcsncpy_s(filePathBuffer, outFilePath.c_str(), _TRUNCATE);
        }
        else
        {
            wcsncpy_s(filePathBuffer, kDefaultJsonFileName, _TRUNCATE);
        }

        const wstring initialDirectory = !_defaultJsonDirectory.empty() ? _defaultJsonDirectory : Resolve_DefaultJsonDirectory();

        OPENFILENAMEW openFileName = {};
        openFileName.lStructSize = sizeof(openFileName);
        openFileName.lpstrFilter = L"JSON Files\0*.json\0All Files\0*.*\0";
        openFileName.lpstrFile = filePathBuffer;
        openFileName.nMaxFile = MAX_PATH;
        if (!initialDirectory.empty())
            openFileName.lpstrInitialDir = initialDirectory.c_str();
        openFileName.lpstrDefExt = L"json";
        openFileName.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

        if (FALSE == GetSaveFileNameW(&openFileName))
            return false;

        outFilePath = filePathBuffer;
        Ensure_JsonExtension(outFilePath);
        return !outFilePath.empty();
    }

    bool UI_Editor_FileService::Show_LoadFileDialog(wstring& outFilePath) const
    {
        wchar_t filePathBuffer[MAX_PATH] = {};
        const wstring initialDirectory = !_defaultJsonDirectory.empty() ? _defaultJsonDirectory : Resolve_DefaultJsonDirectory();

        OPENFILENAMEW openFileName = {};
        openFileName.lStructSize = sizeof(openFileName);
        openFileName.lpstrFilter = L"JSON Files\0*.json\0All Files\0*.*\0";
        openFileName.lpstrFile = filePathBuffer;
        openFileName.nMaxFile = MAX_PATH;
        if (!initialDirectory.empty())
            openFileName.lpstrInitialDir = initialDirectory.c_str();
        openFileName.lpstrDefExt = L"json";
        openFileName.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

        if (FALSE == GetOpenFileNameW(&openFileName))
            return false;

        outFilePath = filePathBuffer;
        return !outFilePath.empty();
    }

    wstring UI_Editor_FileService::Resolve_DefaultJsonDirectory() const
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
            LOG_WARN("UI Editor current path query failed: {}", exception.what());
        }

        for (const fs::path& startPath : startPaths)
        {
            fs::path resourceRoot = {};
            if (Try_FindResourceRootFromStart(startPath, resourceRoot))
            {
                const fs::path jsonDirectory = (resourceRoot / L"UI" / L"UI_Json").lexically_normal();
                if (Try_CreateDirectory(jsonDirectory))
                    return jsonDirectory.wstring();
            }
        }

        vector<fs::path> fallbackCandidates;
        for (const fs::path& startPath : startPaths)
        {
            vector<fs::path> candidates = Build_DefaultJsonDirectoryCandidates(startPath);
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
                LOG_WARN("UI Editor fallback json directory failed: {} ({})", exception.what(), String::ToString(candidate.wstring()));
            }
        }

        LOG_WARN("UI Editor default json directory could not be resolved.");
        return {};
    }

    void UI_Editor_FileService::Ensure_JsonExtension(wstring& filePath) const
    {
        if (filePath.empty())
            return;

        fs::path path = fs::path(filePath);
        if (path.extension().empty())
            path.replace_extension(L".json");

        filePath = path.wstring();
    }
}
