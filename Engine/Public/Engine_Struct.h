#pragma once

#include "Engine_Typedef.h"

namespace Engine
{
class GameObject;

using ENGINE_DESC = struct tagEngineDesc
{
    HINSTANCE hInstance;
    HWND hWnd;
    WinMode winMode;
    unsigned int numLevels;
    unsigned int viewportWidth;
    unsigned int viewportHeight;
    std::string projectName;
};

using KEYFRAME = struct tagKeyFrame
{
    Vec3 scale;
    Quat rotation;
    Vec3 translation;

    float trackPosition; /* 현재 키프레임상태는 어떤 위치에서 표현되어야하는가? */
};

using LIGHT_DESC = struct tagLightDesc
{
    LightType type;

    Vec4 direction;
    Vec4 position;
    float range;

    Vec4 diffuse;
    Vec4 ambient;
    Vec4 specular;
};

using FAssetMeta = struct tagAssetMeta
{
    std::string guid = {};          /* 에셋을 lazy-load/key lookup으로 찾기 위한 고유 식별자 */
    std::string type = {};          /* ResourceLoader가 어떤 로더를 태울지 판단하는 에셋 타입 */
    std::string modelType = {};     /* meshbin처럼 같은 확장자 안에서 static/anim 등을 구분하기 위한 보조 타입 */
    std::wstring fullPath = {};     /* 실제 파일 시스템에서 접근할 절대 경로 */
    std::wstring relativePath = {}; /* 리소스 루트 기준 상대 경로로, json/테이블에서 사람이 확인하기 쉬운 경로 */
};

struct RenderPerfSummary
{
    bool valid{};
    uint32 renderCallCount{};
    float renderAvgMs{};
    float renderMaxMs{};
    float priorityAvgMs{};
    float nonBlendAvgMs{};
    float blendAvgMs{};
    float uiAvgMs{};

    // TODO(RuntimePerf): deferred/shadow/light/combine pass가 추가되면 Renderer counter와 연결한다.
    float shadowAvgMs{};
    float shadowMaxMs{};
    float lightsAvgMs{};
    float combinedAvgMs{};
};

struct CollisionPerfSummary
{
    bool valid{};
    uint32 updateFrameCount{};
    float activeCollidersAvg{};
    float pairsPerSec{};
    float intersectsPerSec{};
    float hitsPerSec{};
    float updateAvgMs{};
    float updateMaxMs{};
};

struct RuntimePerfSummary
{
    float windowSec{};
    float approxFps{};
    RenderPerfSummary render{};
    CollisionPerfSummary collision{};
};

struct DamageEvent
{
    float damage = 1.f;
    Shared<GameObject> causer; // 때린 놈
};
}
