#pragma once

#include "UI_Document.h"

namespace UIEditor
{
    class UI_ClipboardService final
    {
    public:
        UI_ClipboardService() = default;
        ~UI_ClipboardService() = default;

    public:
        bool Has_CopiedUIObjects() const;
        bool Copy_FromDocument(const UI_Document& document, const vector<ObjectId>& rootObjectIds);
        bool Paste_ToDocument(UI_Document& document, ObjectId parentId, vector<ObjectId>& outCreatedRootObjectIds);

    private:
        void Collect_ObjectAndChildren(const UI_Document& document, ObjectId objectId, vector<ObjectId>& outObjectIds) const;

    private:
        vector<UI_Object> _clipboardObjects = {};
        vector<ObjectId> _clipboardRootObjectIds = {};
        int32 _clipboardPasteCount = 0;
    };
}
