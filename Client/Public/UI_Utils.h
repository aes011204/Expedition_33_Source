#pragma once

#include "UI_Base.h"
#include "GameInstance.h"

namespace Client::UI_Utils
{
    // json 이름으로 UI 찾기
    inline Shared<UI_Base> Find_UI(
        const umap<wstring, Shared<UI_Base>>& uiByName, const wstring& name)
    {
        auto iter = uiByName.find(name);
        if (iter == uiByName.end())
            return nullptr;

        return iter->second;
    }

    // Find_UI로 찾은 UI를 원하는 타입으로 캐스팅해서 가져오기
    template<typename T>
    Shared<T> Find_TypedUI(
        const umap<wstring, Shared<UI_Base>>& uiByName,
        const wstring& name,
        const char* ownerTag = "UI")
    {
        Shared<UI_Base> ui = Find_UI(uiByName, name);
        if (ui == nullptr)
            return nullptr;

        Shared<T> typedUI = dynamic_pointer_cast<T>(ui);
        if (typedUI == nullptr)
            LOG_WARN("{} ui type mismatch: {}", ownerTag, String::ToString(name));

        return typedUI;
    }

    // 월드 좌표를 현재 화면 좌표로 투영하기
    inline bool Try_ProjectWorldToScreen(const Vec3& worldPos, Vec2& outScreenPos)
    {
        const Matrix* view = GAME->Get_Transform(D3DTS::View);
        const Matrix* proj = GAME->Get_Transform(D3DTS::Proj);

        if (view == nullptr || proj == nullptr)
            return false;

        const float width = static_cast<float>(max(1u, GAME->Get_ViewportWidth()));
        const float height = static_cast<float>(max(1u, GAME->Get_ViewportHeight()));

        const Matrix viewProj = (*view) * (*proj);
        // 월드 -> 클립
        Vec4 clip{};
        Vec4::Transform(Vec4(worldPos.x, worldPos.y, worldPos.z, 1.f), viewProj, clip);

        if (clip.w <= 0.f)
            return false; // 카메라 뒤

        const float ndcX = clip.x / clip.w;
        const float ndcY = clip.y / clip.w;
        const float ndcZ = clip.z / clip.w;

        // 프러스텀 밖이면 표시 X
        if (ndcZ < 0.f || ndcZ > 1.f)
            return false;

        // 좌표 보정
        outScreenPos.x = (ndcX * 0.5f + 0.5f) * width;
        outScreenPos.y = (-ndcY * 0.5f + 0.5f) * height;

        return true;
    }

    // keyframe animation asset의 재생 길이를 초 단위로 계산한다.
    inline float Get_KeyframeAnimationDurationSec(const UI_KeyframeAnimationAsset& asset)
    {
        const float safeFps = static_cast<float>(max(1, asset.fps));
        return max(0.f, static_cast<float>(asset.endFrame - asset.startFrame)) / safeFps;
    }

}

