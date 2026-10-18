#pragma once

#include "UI_AnimationTypes.h"

namespace UIEditor
{
    class UI_AnimationAssetSerializer final
    {
    public:
        UI_AnimationAssetSerializer() = delete;

    public:
        static bool Save_ToJsonFile(const UI_AnimationAsset& asset, const wstring& filePath);
        static bool Load_FromJsonFile(UI_AnimationAsset& asset, const wstring& filePath);
        static json To_Json(const UI_AnimationAsset& asset);
        static bool From_Json(UI_AnimationAsset& asset, const json& root);
    };

    class UI_StateAnimationAssetSerializer final
    {
    public:
        UI_StateAnimationAssetSerializer() = delete;

    public:
        static bool Save_ToJsonFile(const UI_StateAnimationAsset& asset, const wstring& filePath);
        static bool Load_FromJsonFile(UI_StateAnimationAsset& asset, const wstring& filePath);
        static json To_Json(const UI_StateAnimationAsset& asset);
        static bool From_Json(UI_StateAnimationAsset& asset, const json& root);
    };
}
