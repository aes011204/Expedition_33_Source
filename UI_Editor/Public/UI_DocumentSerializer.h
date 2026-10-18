#pragma once

#include "UI_Document.h"

namespace UIEditor
{
    class UI_DocumentSerializer final
    {
    public:
        UI_DocumentSerializer() = delete;

    public:
        static bool Save_ToJsonFile(const UI_Document& document, const wstring& filePath);
        static bool Load_FromJsonFile(UI_Document& document, const wstring& filePath);
        static json To_Json(const UI_Document& document);
        static bool From_Json(UI_Document& document, const json& root);
        static json Object_ToJson(const UI_Object& object);
        static bool Try_ReadObject(const json& item, UI_Object& outObject);
    };
}
