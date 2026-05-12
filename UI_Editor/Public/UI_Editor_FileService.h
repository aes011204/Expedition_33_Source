#pragma once

#include "UI_Document.h"

namespace UIEditor
{
    enum class UI_FileConfirmResult
    {
        Cancel,
        ContinueWithoutSave,
        Saved,
    };

    class UI_Editor_FileService final
    {
    public:
        UI_Editor_FileService() = default;
        ~UI_Editor_FileService() = default;

    public:
        HRESULT Initialize();

        const wstring& Get_CurrentDocumentPath() const { return _currentDocumentPath; }
        void Clear_CurrentDocumentPath() { _currentDocumentPath.clear(); }

        bool Save_Document(const UI_Document& document, bool forceSaveAs);
        bool Load_Document(UI_Document& document);
        UI_FileConfirmResult Confirm_SaveIfDirty(bool isDirty, const wchar_t* actionName, const UI_Document& document);

    private:
        bool Show_SaveFileDialog(wstring& outFilePath) const;
        bool Show_LoadFileDialog(wstring& outFilePath) const;
        wstring Resolve_DefaultJsonDirectory() const;
        void Ensure_JsonExtension(wstring& filePath) const;

    private:
        wstring _currentDocumentPath = {};
        wstring _defaultJsonDirectory = {};
    };
}
