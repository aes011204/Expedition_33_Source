#pragma once

#include "UI_Editor_Defines.h"

namespace UIEditor
{
    enum class UI_ObjectType
    {
        Image,
        Text,
    };

    struct UI_Object
    {
        ObjectId id = 0;
        string name = "";
        UI_ObjectType type = UI_ObjectType::Image;
        ImVec2 position = ImVec2(0.f, 0.f);
        ImVec2 size = ImVec2(100.f, 100.f);
        ImVec2 scale = ImVec2(1.f, 1.f);
        float rotationRadians = 0.f;
        ImVec4 color = ImVec4(1.f, 1.f, 1.f, 1.f);
        bool isOpenOnCreate = true;
        int32 sortOrder = 0;
        string text = "";
        float fontSize = 32.f;
        string fontTag = "";
        string fontGuid = "";
        string fontPath = "";
        string textureGuid = "";
        string texturePath = "";
    };

    class UI_Document final
    {
    public:
        UI_Document();

        const ImVec2& Get_CanvasSize() const;
        const vector<UI_Object>& Get_Objects() const;
        UI_Object* Find_Object(ObjectId objectId);
        const UI_Object* Find_Object(ObjectId objectId) const;
        void Set_CanvasSize(const ImVec2& canvasSize);

        void Reset();

    private:
        ImVec2 _canvasSize = ImVec2(1600.f, 900.f);
        vector<UI_Object> _objects = {};
        
    };
}
