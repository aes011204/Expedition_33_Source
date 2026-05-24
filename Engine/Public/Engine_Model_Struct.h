#pragma once

using namespace::std;
using namespace::Engine;
using namespace::DirectX::SimpleMath;

constexpr int32 maxModelNameSize = 256;
constexpr int32 maxBoneCount = 950;

using ModelDataHeader = struct tagModelDataHeader
{
    bool   isSkeletal;
    uint32 maxTextureSize;
    uint32 numMaterials;
    uint32 numMeshes;
    uint32 numBones;
};

using ModelMeshData = struct tagModelMeshHeader
{
    string      name;
    uint32      materialIndex{};
    uint32      numVertices{};
    uint32      numIndices{};
    uint32      numBoneRefs{};

    void*       vertices;
    void*       indices;
    void*       boneIndices;   
    void*       offsetMatrices;
};

using ModelBoneData = struct tagModelBone
{
    string      name;
    int32       parentBoneIndex{ -1 };
    Matrix      localTransform{};
    Matrix      offsetMatrix{};
};

using ModelTextureSlot = struct tagModelTextureSlot
{
    string      path;

    uint32      typeIndex{ 0 };   
    uint32      textureIndex{ 0 };

    Vec2        uvScale{ 1.f, 1.f };
    Vec2        uvOffset{ 0.f, 0.f };
    float       uvRotation{ 0.f };
    float       blendFactor{ 1.f };
    uint32      blendOp{};
    uint32      uvChannel{};
    uint32      wrapMode{};
};

using ModelMaterialData = struct tagModelMaterialData
{
    string      name;

    Vec4        diffuseColor{};
    Vec4        ambientColor{};
    Vec4        specularColor{};
    Vec4        emissiveColor{};

    float       shininess{};
    float       opacity{ 1.f };
    float       roughness{ 0.5f };
    float       metalness{ 0.f };

    uint32      numSlots{};
    void*       modelTextureSlots;
};

using ModelAnimationData = struct tagModelAnimationData
{
    string      name;

    double      duration{};
    double      ticksPerSecond{};
    uint32      numChannels{};
    Vec3        rootTotalTranslation{};
    Vec4        rootTotalRotation{};
};

using ModelChannelData = struct tagChannelData
{
    string      boneName;

    int32       boneIndex{ -1 };
    uint32      numKeyframes{};
    void*       keyframes;
};

// Model InGame Struct 인게임에서 사용할 구조체

using RootMotionFrame = struct tagRootMotionFrame
{
    Vec3 scale{ 1.f, 1.f, 1.f };
    Quat rotation{ 0.f, 0.f, 0.f, 1.f };
    Vec3 position{ 0.f, 0.f, 0.f };
};

using ModelCollisionMesh = struct tagModelCollisionMesh
{
    string          name{};
    vector<Vec3>    positions;
    vector<uint32>  indices;
};

using ModelBoneNode = struct tagModelBoneNode
{
    int32 boneIndex{ -1 };
    float boneWeight{};
};

using ModelBoneRotationLimits = struct tagModelBoneRotationLimits
{
    float minYaw{}, maxYaw{};
    float minPitch{}, maxPitch{};
};

/** Assimp aiTextureType Summary (0 ~ 27) */
//  0: NONE							- 텍스처 없음 (더미 수치)
//  1: DIFFUSE						- 기본 색상 (고전 방식 알베도)
//  2: SPECULAR						- 반사광 강도 및 하이라이트 영역
//  3: AMBIENT						- 환경광 반사 수치 (그림자 보정용)
//  4: EMISSIVE						- 자기 발광 (빛이 직접 나오는 효과)
//  5: HEIGHT						- 높이 맵 (범프 매핑용 그레이스케일)
//  6: NORMALS						- 노멀 맵 (탄젠트 공간 표면 굴곡 상세 표현)
//  7: SHININESS						- 광택 지수 (Phong 반사 지수 결정)
//  8: OPACITY						- 불투명도 (투명도 조절용)
//  9: DISPLACEMENT					- 정점 변위 (실제 정점 위치 이동용)
// 10: LIGHTMAP						- 구워진 조명 데이터 또는 AO
// 11: REFLECTION					- 완전 거울 반사 효과 (환경 매핑용)
// 12: BASE_COLOR					- PBR 알베도 (금속/비금속 공통 기본 컬러)
// 13: NORMAL_CAMERA				- PBR 표준 노멀 맵
// 14: EMISSION_COLOR				- PBR 발광 컬러 값
// 15: METALNESS					- PBR 금속성 수치 (Metalness Workflow)
// 16: DIFFUSE_ROUGHNESS			- PBR 표면 거칠기 (Roughness Workflow)
// 17: AMBIENT_OCCLUSION			- 구석진 곳의 환경광 차폐도 (AO)
// 18: UNKNOWN						- 정의되지 않은 미확인 타입
// 19: SHEEN						- 천/벨벳 등의 미세 섬유 광택 효과
// 20: CLEARCOAT					- 표면 위의 투명 코팅층 효과 (자동차 도색 등)
// 21: TRANSMISSION					- 유리/액체 등 투과 및 굴절 효과
// 22: MAYA_BASE					- Maya 엔진 전용 베이스 컬러
// 23: MAYA_SPECULAR				- Maya 엔진 전용 스펙큘러
// 24: MAYA_SPECULAR_COLOR			- Maya 엔진 전용 스펙큘러 컬러
// 25: MAYA_SPECULAR_ROUGHNESS		- Maya 엔진 전용 스펙큘러 거칠기
// 26: ANISOTROPY					- 비등방성 반사 효과 (머리카락, 브러시드 메탈)
// 27: GLTF_METALLIC_ROUGHNESS		- glTF 전용 패킹 (Blue: Metal, Green: Rough)
