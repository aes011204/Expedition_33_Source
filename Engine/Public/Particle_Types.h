#pragma once

#include "Engine_Typedef.h"

NS_BEGIN(Engine)

// point particle 검증 경로에서 사용할 instanced draw 제출 방식.
enum class PointParticleDrawMode : uint8
{
    Direct,                      // 기존 DrawInstanced 경로로 제출한다.
    DrawInstancedIndirect,       // CPU가 작성한 args buffer로 DrawInstancedIndirect를 호출한다.
    DrawIndexedInstancedIndirect // CPU가 작성한 args buffer로 DrawIndexedInstancedIndirect를 호출한다.
};

// GPU instance buffer에 저장되는 per-instance world/lifetime/color payload.
struct ParticleInstanceVertex
{
    Vec4 right{};                          // 인스턴스 월드 행렬의 right 행.
    Vec4 up{};                             // 인스턴스 월드 행렬의 up 행.
    Vec4 look{};                           // 인스턴스 월드 행렬의 look 행.
    Vec4 translation{};                    // 인스턴스 월드 행렬의 위치 행.
    Vec2 lifeTime{};                       // x: 최대 생존 시간, y: 누적 생존 시간.
    Vec4 startColor{ 1.f, 1.f, 1.f, 1.f }; // particle 생성 시점의 색상/알파.
    Vec4 endColor{ 1.f, 1.f, 1.f, 1.f };   // particle 수명 종료 시점의 색상/알파.
};

// Particle shader가 사용하는 vertex + instance input layout.
struct ParticleInstanceLayout
{
    static constexpr uint32 numElements = 8;

    // WORLD 0~3은 semantic name WORLD + semantic index 0~3으로 표현한다.
    static constexpr D3D11_INPUT_ELEMENT_DESC Elements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 72, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 88, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
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
    Vec3 pivot{};                                                    // 퍼짐 기준점.
    Vec2 speed{};                                                    // 랜덤 속도 범위.
    Vec2 lifeTime{};                                                 // 랜덤 생존 시간 범위.
    Vec4 startColorMin{ 1.f, 1.f, 1.f, 1.f };                        // Initial Color에서 샘플링할 시작 색상 하한.
    Vec4 startColorMax{ 1.f, 1.f, 1.f, 1.f };                        // Initial Color에서 샘플링할 시작 색상 상한.
    Vec4 endColorMin{ 1.f, 1.f, 1.f, 1.f };                          // Color Over Life에서 샘플링할 종료 색상 하한.
    Vec4 endColorMax{ 1.f, 1.f, 1.f, 1.f };                          // Color Over Life에서 샘플링할 종료 색상 상한.
    bool loop{};                                                     // true면 생존 시간이 끝난 인스턴스를 재생성한다.
    PointParticleDrawMode drawMode{ PointParticleDrawMode::Direct }; // Stage2 indirect draw 검증용 제출 방식.
};

// compute shader가 instance payload를 갱신하는 point particle 검증용 desc.
struct ComputePointParticleDesc : public InstanceSpawnDesc
{
    Vec2 lifeTime{};                                                 // compute shader가 누적 시간을 계산할 때 사용할 생존 시간 범위.
    Vec4 startColorMin{ 1.f, 1.f, 1.f, 1.f };                        // GPU Sprite preview 시작 색상/알파 하한.
    Vec4 startColorMax{ 1.f, 1.f, 1.f, 1.f };                        // GPU Sprite preview 시작 색상/알파 상한.
    Vec4 endColorMin{ 1.f, 1.f, 1.f, 1.f };                          // GPU Sprite preview 종료 색상/알파 하한.
    Vec4 endColorMax{ 1.f, 1.f, 1.f, 1.f };                          // GPU Sprite preview 종료 색상/알파 상한.
    Vec2 radius{};                                                   // 화면 확인용 원형 이동 반경 범위. authoring 의미로 승격하지 않는다.
    PointParticleDrawMode drawMode{ PointParticleDrawMode::Direct }; // compute particle 검증 경로의 draw 제출 방식.
    uint32 computeShaderLevelIndex{};                                // compute shader prototype이 등록된 level index. Engine은 Client의 LevelType을 알지 않는다.
    wstring computeShaderPrototypeTag{};                             // instance payload를 갱신할 compute shader component prototype tag.
};

NS_END
