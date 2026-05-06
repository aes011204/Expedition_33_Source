#include "pch.h"
#include "UI_Document.h"

namespace UIEditor
{
    UI_Document::UI_Document()
    {
        Reset();
    }

    void UI_Document::Reset()
    {
        _canvasSize = ImVec2(1280.f, 720.f);

        _objects.clear();

        UI_Object imageObject = {};
        imageObject.id = 1;
        imageObject.name = "Smoke Image";
        imageObject.type = UI_ObjectType::Image;
        imageObject.position = ImVec2(100.f, 140.f);
        imageObject.size = ImVec2(384.f, 39.f);
        imageObject.color = ImVec4(0.f, 0.f, 0.f, 1.f);
        imageObject.sortOrder = 0;
        imageObject.texturePath = "UI_Editor_Test/Image/T_HealthBarOutline.png";
        _objects.push_back(imageObject);

        UI_Object textObject = {};
        textObject.id = 2;
        textObject.name = "Smoke Text";
        textObject.type = UI_ObjectType::Text;
        textObject.position = ImVec2(100.f, 80.f);
        textObject.size = ImVec2(300.f, 40.f);
        textObject.color = ImVec4(1.f, 1.f, 1.f, 1.f);
        textObject.sortOrder = 1;
        textObject.text = "UI Smoke Test";
        textObject.fontTag = "Font_UI_Smoke";
        _objects.push_back(textObject);
    }

    const ImVec2& UI_Document::Get_CanvasSize() const
    {
        return _canvasSize;
    }

    const vector<UI_Object>& UI_Document::Get_Objects() const
    {
        return _objects;
    }

    UI_Object* UI_Document::Find_Object(ObjectId objectId)
    {
        for (UI_Object& object : _objects)
        {
            if (object.id == objectId)
                return &object;
        }

        return nullptr;
    }

    const UI_Object* UI_Document::Find_Object(ObjectId objectId) const
    {
        for (const UI_Object& object : _objects)
        {
            if (object.id == objectId)
                return &object;
        }

        return nullptr;
    }

    void UI_Document::Set_CanvasSize(const ImVec2& canvasSize)
    {
        _canvasSize.x = max(1.f, canvasSize.x);
        _canvasSize.y = max(1.f, canvasSize.y);
    }
}
