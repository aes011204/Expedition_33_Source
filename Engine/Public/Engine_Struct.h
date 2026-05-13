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

    bool castShadow = false;
};

using ShadowLightDesc = struct tagShadowLightDesc
{
    Vec3    eye;
    Vec3    at;
    float   fovy = DirectX::XMConvertToRadians(120.f); // rad
    float   aspect = 16.f / 9.f;
    float   zn = 0.1f;
    float   zf = 1000.f;

    float   bias = 0.03f;
    float   intensity = 0.5f;
};

using AssetMeta = struct tagAssetMeta
{
    std::string guid = {};          /* 데이터의 고유 guid */
    std::string type = {};          /* Model, Animation, Texture 등 */
    std::string modelType = {};     /* 필요할 때에만 사용해서 타입별 분리 */
    std::wstring fullPath = {};     /* 절대 경로 */
    std::wstring relativePath = {}; /* 리소스 루트 기준 상대 경로로, json/테이블에서 사람이 확인하기 쉬운 경로 */
};


struct DamageEvent
{
    float damage = 1.f;
    Shared<GameObject> causer; // 때린 놈
};

struct ColliderShapeDesc
{
    CollisionShape shape = CollisionShape::AABB;

    Vec3 center = Vec3::Zero;
    Vec3 extents = Vec3(0.5f, 0.5f, 0.5f);
    Vec3 radians = Vec3::Zero;

    float radius = 0.5f;

};
}
