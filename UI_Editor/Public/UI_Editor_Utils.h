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

    void Apply_TextureAsset(UI_Object& object, const AssetMeta& assetMeta);
    void Apply_FontAsset(UI_Object& object, const AssetMeta& assetMeta);
}
