#pragma once

#include "Engine_Define.h"
#include "UI_Editor_ImGuiInclude.h"
#include "UI_Editor_Enum.h"
#include "UI_AnimationTypes.h"

NS_BEGIN(UIEditor)

using ObjectId = uint32;

struct UI_EDITOR_DESC
{
    HWND hWnd = nullptr;
    WinMode winMode = WinMode::Win;
    uint32 viewportWidth = 1600;
    uint32 viewportHeight = 900;
    bool assetManagerReady = false;
};

struct UI_Object
{
    ObjectId id = 0;
    ObjectId parentId = 0;
    string name = "";
    UI_ObjectType type = UI_ObjectType::Image;
    ImVec2 position = ImVec2(0.f, 0.f);
    ImVec2 size = ImVec2(100.f, 100.f);
    ImVec2 scale = ImVec2(1.f, 1.f);
    ImVec2 pivot = ImVec2(0.5f, 0.5f);
    ImVec2 anchorMin = ImVec2(0.f, 0.f);
    ImVec2 anchorMax = ImVec2(0.f, 0.f);
    float rotationRadians = 0.f;
    ImVec4 color = ImVec4(1.f, 1.f, 1.f, 1.f);
    bool isOpenOnCreate = true;
    bool visible = true;
    bool locked = false;
    int32 sortOrder = 0;
    UI_RenderGroup renderGroup = UI_RenderGroup::Foreground;
    string text = "";
    float fontSize = 32.f;
    UI_TextAlign textAlign = UI_TextAlign::Left;
    bool textWrap = false;
    string fontTag = "";
    string fontGuid = "";
    string fontPath = "";
    string textureGuid = "";
    string texturePath = "";
    UI_ImageRenderMode imageRenderMode = UI_ImageRenderMode::Normal;
    string maskTextureGuid = "";
    string maskTexturePath = "";
    UI_MaskTextureMode maskTextureMode = UI_MaskTextureMode::Alpha;
    string patternMaskTextureGuid = "";
    string patternMaskTexturePath = "";
    string stateAnimationPath = "";
    vector<UI_AnimationClip> animationClips = {};
};

NS_END
