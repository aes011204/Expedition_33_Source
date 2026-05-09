#pragma once

#include "UI_AnimationTypes.h"

namespace UIEditor
{
    class UI_AnimationAssetSerializer final
    {
    public:
        UI_AnimationAssetSerializer() = delete;

    public:
        static bool Save_ToJsonFile(const UIAnimationAsset& asset, const wstring& filePath);
        static bool Load_FromJsonFile(UIAnimationAsset& asset, const wstring& filePath);
        static json To_Json(const UIAnimationAsset& asset);
        static bool From_Json(UIAnimationAsset& asset, const json& root);
    };
}
