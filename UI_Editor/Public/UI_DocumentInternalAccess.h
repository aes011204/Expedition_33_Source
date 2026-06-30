#pragma once

#include "UI_Document.h"

namespace UIEditor
{
    class UI_ClipboardService;
    class UI_DocumentSerializer;
    class UI_Editor_FileService;
    class UI_Editor_Manager;
    class UI_BrushPresetService;
    class UI_ObjectCommandService;
    class UI_ObjectEditService;
    class UI_TransformService;

    class UI_DocumentInternalAccess final
    {
        friend class UI_ClipboardService;
        friend class UI_DocumentSerializer;
        friend class UI_Editor_FileService;
        friend class UI_Editor_Manager;
        friend class UI_BrushPresetService;
        friend class UI_ObjectCommandService;
        friend class UI_ObjectEditService;
        friend class UI_TransformService;

    private:
        UI_DocumentInternalAccess() = delete;

        static ObjectId Create_Object(UI_Document& document, UI_ObjectType objectType, ObjectId parentId);
        static ObjectId Duplicate_Object(UI_Document& document, ObjectId objectId, const ImVec2& offset);
        static vector<ObjectId> Import_PrefabObjects(
            UI_Document& document,
            const vector<UI_Object>& sourceObjects,
            const vector<ObjectId>& rootObjectIds,
            ObjectId parentId,
            const ImVec2& rootOffset);
        static bool Delete_Object(UI_Document& document, ObjectId objectId);
        static bool Change_Parent(UI_Document& document, ObjectId objectId, ObjectId newParentId, bool keepWorldPosition);

        static bool Set_WorldPosition(UI_Document& document, ObjectId objectId, const ImVec2& worldPosition);
        static bool Set_WorldRect(UI_Document& document, ObjectId objectId, const ImVec2& worldPosition, const ImVec2& worldSize);
        static bool Set_PivotKeepWorldRect(UI_Document& document, ObjectId objectId, const ImVec2& pivot);
        static bool Set_AnchorsKeepWorldRect(UI_Document& document, ObjectId objectId, const ImVec2& anchorMin, const ImVec2& anchorMax);

        static bool Load_FromJsonFile(UI_Document& document, const wstring& filePath);
        static void Replace_LoadedContent(UI_Document& document, string canvasName, const ImVec2& canvasSize, vector<UI_Object> objects);
        static void Set_CanvasName(UI_Document& document, const string& canvasName);
        static void Reset(UI_Document& document);

        static UI_Object* Find_Object(UI_Document& document, ObjectId objectId);
        static vector<UI_Object>& Get_MutableObjects(UI_Document& document);
    };
}
