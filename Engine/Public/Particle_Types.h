#pragma once

#include "Engine_Typedef.h"

NS_BEGIN(Engine)

// GPU instance buffer에 저장되는 per-instance world/lifetime payload.
struct ParticleInstanceVertex
{
    Vec4 right{};       // 인스턴스 월드 행렬의 right 행.
    Vec4 up{};          // 인스턴스 월드 행렬의 up 행.
    Vec4 look{};        // 인스턴스 월드 행렬의 look 행.
    Vec4 translation{}; // 인스턴스 월드 행렬의 위치 행.
    Vec2 lifeTime{};    // x: 최대 생존 시간, y: 누적 생존 시간.
};

// Point particle shader가 사용하는 vertex + instance input layout.
struct PointParticleInstanceLayout
{
    static constexpr uint32 numElements = 6;

    // WORLD 0~3은 semantic name WORLD + semantic index 0~3으로 표현한다.
    static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };
};

// 인스턴스 생성에 공통으로 필요한 spawn 규칙.
struct InstanceSpawnDesc
{
    uint32 instanceCount{}; // 생성할 인스턴스 수.
    Vec3 center{};          // 스폰 영역 중심.
    Vec3 range{};           // 스폰 영역 크기.
    Vec2 scale{};           // 랜덤 스케일 범위.
};

// point particle clone 생성과 runtime 갱신에 필요한 desc.
struct PointParticleDesc : public InstanceSpawnDesc
{
    Vec3 pivot{};    // 퍼짐 기준점.
    Vec2 speed{};    // 랜덤 속도 범위.
    Vec2 lifeTime{}; // 랜덤 생존 시간 범위.
    bool loop{};     // true면 생존 시간이 끝난 인스턴스를 재생성한다.
};

NS_END
