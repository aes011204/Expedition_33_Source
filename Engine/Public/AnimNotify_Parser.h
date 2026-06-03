#pragma once

#include "AnimNotify_Types.h"

NS_BEGIN(Engine)

class ENGINE_DLL AnimNotify_Parser final
{
public:
    AnimNotify_Parser() = default;
    ~AnimNotify_Parser() = default;

public:
    static json To_Json(const AnimNotifyAsset& asset);
    static bool From_Json(const json& root, AnimNotifyAsset& outAsset);

    static bool Save_ToFile(const wstring& filePath, const AnimNotifyAsset& asset);
    static bool Load_FromFile(const wstring& filePath, AnimNotifyAsset& outAsset);

    static bool Load_ModelAsset(const string& modelGuid, AnimNotifyAsset& outAsset);
    static bool Save_AnimationToSplitFile(const string& modelGuid, const AnimNotifyData& animationData, float animationLengthSec = -1.f);

    // 노티파이 저장 루트 폴더 경로 반환
    static fs::path Get_BaseFolderPath();
    // model의 guid를 추적해서, 모델이 가지고있는 애니메이션 가져오기
    static fs::path Get_ModelNotifyFilePath(const string& modelGuid);
    static fs::path Get_ModelNotifyFolderPath(const string& modelGuid);
    static fs::path Get_AnimationNotifyFilePath(const string& modelGuid, const string& animationName);
    static fs::path Get_LegacyModelNotifyFilePath(const string& modelGuid);

    // 애니메이션 이름으로 현재 asset 안의 notify 찾기
    static AnimNotifyData* Find_Animation(AnimNotifyAsset& asset, const string& animationName);
    static const AnimNotifyData* Find_Animation(const AnimNotifyAsset& asset, const string& animationName);

    // 애니메이션 이름에 해당하는 notify 데이터를 반환하고, 없으면 추가하기
    static AnimNotifyData& Get_OrAddAnimation(AnimNotifyAsset& asset, const string& animationName);

    static void Ensure_DefaultTracks(AnimNotifyData& animationData);

    // 저장/로드 전후로 시간값, ID, 정렬 상태를 보정
    static void Sort_And_Clamp(AnimNotifyData& animationData, float animationLengthSec = -1.f);


public:
    // 시간값을 애니메이션 길이 범위 안으로 제한
    static float Clamp_TimeSec(float value, float animationLengthSec);

    // 현재 애니메이션 데이터 기준 다음 notify ID 계산
    static int32 Compute_NextNotifyId(const AnimNotifyData& anim);
    static int32 Compute_NextStateId(const AnimNotifyData& anim);

private:
    static string Get_ModelNotifyStem(const string& modelGuid);
    static string Sanitize_FileName(const string& value);
    static void Merge_AnimationIntoAsset(AnimNotifyAsset& asset, AnimNotifyData&& animationData);
};

NS_END
