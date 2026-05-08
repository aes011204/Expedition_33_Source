#pragma once

#include "UI_Editor_Window.h"
#include "UI_Document.h"

namespace UIEditor
{
    class UI_Editor_Manager;

    class UI_Hierarchy_View final : public UI_Editor_Window
    {
    public:
        explicit UI_Hierarchy_View(UI_Editor_Manager& editorManager);
        ~UI_Hierarchy_View() override = default;

    public:
        void Render() override;

    private:
        void Render_Children(ObjectId parentId);
        void Render_ContextMenu(ObjectId objectId);
        void Render_CreateMenu();
        void Render_RenamePopup();
        void Apply_PendingActions();
        void Begin_RenameObject(ObjectId objectId);

    public:
        static Shared<UI_Hierarchy_View> Create(UI_Editor_Manager& editorManager);

    private:
        UI_Editor_Manager& _editorManager;
        char _renameBuffer[128] = {};
        ObjectId _renamingObjectId = 0;
        bool _openRenamePopup = false;
        UI_ObjectType _pendingCreateObjectType = UI_ObjectType::Image;
        bool _hasPendingCreateObject = false;
        ObjectId _pendingDuplicateObjectId = 0;
        ObjectId _pendingDeleteObjectId = 0;
    };
}
