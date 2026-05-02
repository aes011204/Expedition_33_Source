#pragma once

using namespace::std;
using ModelDataHeader = struct tagModelDataHeader
{
    string modelName;
    bool   isSkeletal;
    uint32 numBones;
    uint32 numMeshes;
    uint32 numMaterials;
    uint32 numAnimations;
};



using ModelMeshData = struct tagModelMeshHeader
{
    
};

using ModelAnimationData = struct tagModelAnimationData
{
    
};

using ModelMaterialData = struct tagModelMaterialData
{
    
};

using ModelChannelData = struct tagChannelData
{
    
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
