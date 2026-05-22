#pragma once

#include "UI_Document.h"

namespace UIEditor
{
    const char* Get_ObjectTypeName(UI_ObjectType objectType);
    const char* Get_TextAlignName(UI_TextAlign textAlign);
    const char* Get_ImageRenderModeName(UI_ImageRenderMode imageRenderMode);
    ImU32 Get_ObjectFillColor(UI_ObjectType objectType);
    bool Is_ContainerObjectType(UI_ObjectType objectType);

    string Normalize_AssetRelativePath(const AssetMeta& assetMeta);
    string Make_ClientRuntimeResourcePath(const AssetMeta& assetMeta);
    wstring Resolve_ClientRuntimeResourcePath(const string& runtimePath);
    string Get_AssetFileName(const AssetMeta& assetMeta);
    bool Is_UITextureAsset(const AssetMeta& assetMeta);
    bool Is_UISpriteFontAsset(const AssetMeta& assetMeta);
    vector<AssetMeta> Collect_UISpriteFontAssets(const string& searchText);
    string Get_FontAssetStableId(const AssetMeta& assetMeta);
    bool Is_FontAssetApplied(const UI_Object& object, const AssetMeta& assetMeta);
    string Make_FontTag(const AssetMeta& assetMeta);
    bool Matches_Search(const AssetMeta& assetMeta, const string& searchText);

    ImTextureID To_ImTextureID(ID3D11ShaderResourceView* srv);
    float Get_UITextRuntimeScale(const UI_Object& object);
    ImVec2 Get_UIObjectRuntimeSize(const UI_Object& object);
    bool Build_ScreenClipRect(const ImVec2& minPosition, const ImVec2& maxPosition, D3D11_RECT& outRect);
    wstring Build_SafeSpriteFontText(const SpriteFont& font, const wstring& text);

    struct UI_WorldObjectFrame
    {
        ObjectId id = 0;
        ImVec2 worldPosition = ImVec2(0.f, 0.f);
        ImVec2 worldCenter = ImVec2(0.f, 0.f);
        ImVec2 worldSize = ImVec2(0.f, 0.f);
        float worldRotationRadians = 0.f;
        ImVec2 worldBoundsMin = ImVec2(0.f, 0.f);
        ImVec2 worldBoundsMax = ImVec2(0.f, 0.f);
        ImVec2 worldQuad[4] = {};
        ImVec2 worldAabbMin = ImVec2(0.f, 0.f);
        ImVec2 worldAabbMax = ImVec2(0.f, 0.f);
    };

    UI_WorldObjectFrame Build_UIWorldObjectFrame(
        ObjectId objectId,
        const ImVec2& worldPosition,
        const ImVec2& worldCenter,
        const ImVec2& worldSize,
        float worldRotationRadians);
    UI_WorldObjectFrame Build_UIWorldObjectFrame(const UI_Document& document, ObjectId objectId);
    bool Is_PointInsideUIWorldObjectFrame(const ImVec2& worldPoint, const UI_WorldObjectFrame& frame);

    void Apply_TextureAsset(UI_Object& object, const AssetMeta& assetMeta);
    void Apply_FontAsset(UI_Object& object, const AssetMeta& assetMeta);
}
