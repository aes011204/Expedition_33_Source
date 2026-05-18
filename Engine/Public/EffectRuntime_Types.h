#pragma once
#include <variant>
#include "Engine_Typedef.h"
#include "GameObject.h"
#include "Particle_Types.h"

NS_BEGIN(Engine)

// Effect 내부 emitter의 concrete runtime 종류를 구분한다.
enum class EffectEmitterKind : uint8
{
    ComputeSprite, // compute-first sprite emitter runtime이다.
};

// EffectSprite shader가 어떤 기준으로 sprite facing 축을 고를지 결정한다.
enum class EffectSpriteScreenAlignment : uint32
{
    FacingCameraPosition = 0, // particle 위치에서 camera position을 바라보는 기본 billboard다.
    Rectangle = 1,            // 카메라 화면 평면 right/up을 따라가는 직사각형 billboard다.
    Square = 2,               // 카메라 화면 평면 기준에서 더 큰 축으로 정사각형을 만든다.
    AwayFromCenter = 3,       // emitter 중심에서 particle 바깥 방향을 바라보는 billboard다.
    Velocity = 4,             // compute가 기록한 현재 속도 방향을 바라보는 billboard다.
};

// compute sprite emitter가 실제 runtime consumer에게 넘길 최소 payload다.
struct ComputeSpriteEmitterDesc : public GameObject::GAMEOBJECT_DESC
{
    PointParticleDrawMode drawMode{ PointParticleDrawMode::DrawIndexedInstancedIndirect }; // compute sprite 정식 draw 제출 방식이다.

    string textureId{ "Texture_Effect_DefaultTexture" };      // main particle texture id다.
    string blendMode{ "AlphaBlend" };                         // material bridge의 blend mode 기록값이다.
    string opacitySource{ "Alpha" };                          // opacity source selector다.
    Vec4 tint{ 1.f, 1.f, 1.f, 1.f };                          // texture sample에 곱할 tint다.
    float intensity{ 1.f };                                   // sprite 색상 밝기/발광감 계수다.
    float opacityPower{ 1.f };                                // opacity sharpening 계수다.
    string noiseTextureId{};                                  // 선택 noise texture id다.
    float noiseStrength{ 0.f };                               // noise modulation 강도다.
    uint32 subUVRows{ 1 };                                    // flipbook row 수다.
    uint32 subUVCols{ 1 };                                    // flipbook column 수다.
    PointParticleSubUVFrameOverLifeDesc subUVFrameOverLife{}; // 수명 진행률 기준 SubUV frame 규칙이다.
    PointParticleSizeByLifeDesc sizeByLife{};                 // 수명 진행률 기준 크기 배율 규칙이다.
    PointParticleMotionDesc motion{};                         // motion authoring module을 compute payload로 압축한 규칙이다.
    PointParticleRotationDesc rotation{};                     // rotation authoring module을 compute payload로 압축한 규칙이다.
    PointParticleSpawnDesc spawn{};                           // Spawn 모듈이 만든 연속/버스트 생성 규칙이다.
    PointParticleInitialLocationDesc initialLocation{};        // InitialLocation 모듈이 만든 시작 위치 offset 분포다.
    PointParticleSphereLocationDesc sphereLocation{};          // SphereLocation 모듈이 만든 시작 위치 구형 offset 분포다.
    float duration{ 1.f };                                     // emitter 한 loop가 재생되는 시간이다.
    uint32 loopCount{ 0 };                                     // 0은 무한 반복, 1 이상은 지정 횟수 재생이다.
    float delay{ 0.f };                                        // loop 시작 전 spawn/draw를 막는 지연 시간이다.
    bool delayFirstLoopOnly{ false };                          // true면 첫 loop에만 delay를 적용한다.
    bool killOnDeactivate{ false };                            // 외부 deactivate 계약이 생기면 active particle을 즉시 죽일지 나타낸다.
    bool killOnCompleted{ false };                             // 마지막 loop 완료 시 active particle을 즉시 죽일지 나타낸다.
    bool useMaxDrawCount{ true };                              // maxDrawCount를 draw 상한으로 적용할지 나타낸다.
    uint32 maxDrawCount{ 500 };                                // 동시에 그릴 particle 수 상한이다. buffer capacity는 바꾸지 않는다.
    EffectSpriteScreenAlignment screenAlignment{
        EffectSpriteScreenAlignment::FacingCameraPosition
    }; // EffectSprite shader가 preview/runtime sprite 축을 고르는 기준이다.

    Vec3 center{ 0.f, 0.f, 0.f };             // concrete emitter 내부 fallback/초기 center다. root transform이 있으면 후속 sync가 덮어쓴다.
    uint32 instanceCount{ 96 };               // 생성할 particle 수다.
    Vec3 spawnRange{ 1.f, 1.f, 1.f };         // Phase2 spawn shape 전까지 사용할 기본 box spawn 범위다.
    Vec2 sizeMin{ 0.12f, 0.12f };             // InitialSize의 가로/세로 크기 하한이다.
    Vec2 sizeMax{ 0.28f, 0.28f };             // InitialSize의 가로/세로 크기 상한이다.
    Vec2 lifeTime{ 1.f, 2.f };                // compute payload가 사용할 생존 시간 범위다.
    Vec4 startColorMin{ 1.f, 1.f, 1.f, 1.f }; // Initial Color 하한이다.
    Vec4 startColorMax{ 1.f, 1.f, 1.f, 1.f }; // Initial Color 상한이다.
    Vec4 endColorMin{ 1.f, 1.f, 1.f, 1.f };   // Color Over Life 하한이다.
    Vec4 endColorMax{ 1.f, 1.f, 1.f, 1.f };   // Color Over Life 상한이다.
};

using EffectEmitterConcreteDesc = variant<ComputeSpriteEmitterDesc>;

struct EffectEmitterDefinition
{
    uint32 id{};                                                // authoring/runtime 추적용 emitter id다.
    string name{};                                              // 디버그/로그용 emitter 이름이다.
    EffectEmitterKind kind{ EffectEmitterKind::ComputeSprite }; // 생성할 concrete emitter 종류다.
    bool enabled{ true };                                       // root가 runtime child 생성 대상으로 볼지 결정하는 플래그다.

    Vec3 localPosition{ 0.f, 0.f, 0.f };        // effect root 기준 emitter local 위치다.
    Vec3 localRotationDegrees{ 0.f, 0.f, 0.f }; // effect root 기준 emitter local 회전이다.
    bool useLocalSpace{ false };                // true면 root transform을 따라가는 local-space emitter다.

    EffectEmitterConcreteDesc concreteDesc{ ComputeSpriteEmitterDesc{} }; // concrete emitter가 직접 소비할 payload다.
};

// 저장/로드 결과로 공유되는 effect 정적 정의다.
struct EffectDefinition
{
    string name{};                              // effect asset/display 이름이다.
    vector<EffectEmitterDefinition> emitters{}; // effect를 구성하는 emitter definition 목록이다.
};

// EffectInstance root spawn 시 전달하는 runtime desc다.
struct EffectInstanceDesc : public GameObject::GAMEOBJECT_DESC
{
    Shared<const EffectDefinition> definition{}; // root가 재생할 정적 effect 정의다.
};

NS_END
