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
    DXGI_SWAP_EFFECT swapEffect = DXGI_SWAP_EFFECT_DISCARD;
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

    bool castShadow = false;    // 이 light를 그림자용 주광원으로 할 건지
};

enum class CascadeLevel : uint32
{
    Close = 0u,
    Normal,
    Far,
    Count
};

// 카메라의 close/normal/far 모드에 따라서 shadow cascade 구간을 조절하기 위한 프리셋
struct ShadowCascadePreset
{
    float focusNear = 0.1f;
    float focusFar = 110.f;
    float cascadeSplitLambda = 0.8f;
    float cascadeBlendDistance = 1.7f;
};

struct ShadowCascadePresetSet
{
    ShadowCascadePreset close{};
    ShadowCascadePreset normal{};
    ShadowCascadePreset farRange{};
};

using ShadowLightDesc = struct tagShadowLightDesc
{
    static constexpr uint32 SHADOW_CASCADE_COUNT = 4;

    Vec3    eye;
    Vec3    at;

    float   fovy = DirectX::XMConvertToRadians(120.f); // rad
    float   aspect = 16.f / 9.f;
    float   zn = 0.1f;
    float   zf = 1000.f;

    float   bias = 0.03f;
    float   intensity = 0.8f;
    bool    enabled = true;

    // Cascade Shadow 추가
    bool followCameraFrustum = true;     // true면 메인 카메라 frustum 기준으로 shadow view/proj를 매 프레임 갱신한다.
    float focusNear = 0.1f;              // shadow가 커버할 카메라 기준 시작 거리.
    float focusFar = 110.f;              // 근거리 cascade 품질을 위해 너무 멀리 잡지 않는다.
    float cascadeBlendDistance = 1.7f;    // cascade 경계 blend 구간 길이.
    float cascadeSplitLambda = 0.8f;     // 0번 cascade에 더 공격적으로 해상도를 몰아준다.
    float lightDistancePadding = 30.f;   // light view eye를 frustum center에서 얼마나 뒤로 뺄지.
    float xyPadding = 4.f;               // frustum AABB 좌우/상하 여유.
    float zPadding = 80.f;               // light space depth 여유.
    uint32 shadowMapWidth = 4096;
    uint32 shadowMapHeight = 4096;
};

using AssetMeta = struct tagAssetMeta
{
    std::string guid = {};          /* 데이터의 고유 guid */
    std::string type = {};          /* Model, Animation, Texture 등 */
    std::string modelType = {};     /* 필요할 때에만 사용해서 타입별 분리 */
    std::wstring fullPath = {};     /* 절대 경로 */
    std::wstring relativePath = {}; /* 리소스 루트 기준 상대 경로로, json/테이블에서 사람이 확인하기 쉬운 경로 */
};

using AssetDeltaScanResult = struct tagAssetDeltaScanResult
{
    std::wstring scanRoot = {};
    std::vector<std::wstring> discoveredAssetPaths = {};
    std::vector<AssetMeta> existingMetaAssets = {};
    std::vector<std::wstring> missingMetaAssetPaths = {};
    std::vector<std::wstring> staleAssetPaths = {};
    bool scanSucceeded = false;
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

struct ComputeSkinningCBData
{
    uint32 vertexCount = 0;
    uint32 pad0 = 0;
    uint32 pad1 = 0;
    uint32 pad2 = 0;
};

struct ViewportSize
{
    uint32 width = 1600;
    uint32 height = 900;
};


// Facial 적용을 위한 Morph 구조체
struct MorphVertexRange
{
    uint32 start{};
    uint32 count{};
};

struct MorphVertexDelta
{
    uint32 vertexIndex{};
    float weightScale{ 1.f };
    Vec3 deltaPosition{};
    Vec3 deltaNormal{};
};

struct MorphMeshData
{
    uint32 meshIndex{};
    std::string meshName{};
    uint32 vertexCount{};
    std::vector<MorphVertexRange> vertexRanges;
    std::vector<MorphVertexDelta> vertexDeltas;
};

struct MorphData
{
    std::string sourceName{};
    std::vector<std::string> targets;
    std::vector<MorphMeshData> meshes;
};

}
