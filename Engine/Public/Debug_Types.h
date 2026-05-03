#pragma once

namespace Engine
{
    // 디버깅 렌더 스타일
    struct ENGINE_DLL DebugRenderStyle
    {
        Color color = Color(1.f, 1.f, 1.f, 1.f); // 디버그 도형의 렌더 색상
        float duration = 0.f;                               // 0 이하면 1프레임, 양수면 유지 시간(초)
        bool  depthEnabled = true;                          // true면 depth test 적용, false면 overlay처럼 출력
    };

    // Box
    struct ENGINE_DLL DebugBoxDesc
    {
        Vec3 center = Vec3::Zero;                       // 월드 기준 박스 중심 위치
        Vec3 extents = Vec3(0.5f, 0.5f, 0.5f);  // 월드 기준 박스 반크기
        Quat rotation = Quat::Identity;                 // 월드 기준 박스 회전
        DebugRenderStyle style;                         // 박스 공통 렌더 옵션
    };

    // Sphere
    struct ENGINE_DLL DebugSphereDesc
    {
        Vec3 center = Vec3::Zero;               // 월드 기준 구 중심 위치
        float radius = 0.5f;                    // 월드 기준 구 반지름
        DebugRenderStyle style;                 // 구 공통 렌더 옵션
    };

    // Line
    struct ENGINE_DLL DebugLineDesc
    {
        Vec3 start = Vec3::Zero;                // 월드 기준 선 시작점
        Vec3 end = Vec3::Zero;                  // 월드 기준 선 끝점
        DebugRenderStyle style;                 // 선 공통 렌더 옵션
    };

    struct ENGINE_DLL DebugTraceLineDesc
    {
        Vec3  start = Vec3::Zero;                         // 월드 기준 trace 시작점
        Vec3  end = Vec3::Zero;                           // 월드 기준 trace 원래 끝점
        bool  isHit = false;                              // true면 hit 정보를 기준으로 색상과 보조 도형을 다르게 그린다
        Vec3  hitPoint = Vec3::Zero;                      // isHit가 true일 때 사용할 히트 위치
        Vec3  hitNormal = Vec3::Up;                       // isHit가 true일 때 사용할 히트 노멀
        Color missColor = Color(1.f, 0.f, 0.f, 1.f);    // miss일 때 trace 선에 사용할 색상
        Color hitColor = Color(0.f, 1.f, 0.f, 1.f);     // hit일 때 start -> hitPoint 구간에 사용할 색상
        Color remainderColor = Color(1.f, 0.f, 0.f, 1.f); // hit 이후 남은 구간을 표시할 때 사용할 색상
        Color hitPointColor = Color(1.f, 1.f, 0.f, 1.f);  // hitPoint 구를 그릴 때 사용할 색상
        Color hitNormalColor = Color(0.f, 1.f, 1.f, 1.f); // hitNormal 선을 그릴 때 사용할 색상
        float duration = 0.f;                             // trace 디버그 전체 유지 시간
        bool  depthEnabled = true;                        // trace 디버그 전체 depth test 적용 여부
        bool  drawHitPoint = true;                        // true면 hitPoint 위치에 디버그 구를 그린다
        bool  drawHitNormal = true;                       // true면 hitPoint에서 hitNormal 방향 보조선을 그린다
        bool  drawRemainderOnHit = false;                 // true면 hit 이후 남은 구간도 별도 색상으로 그린다
        float hitPointRadius = 0.08f;                     // hitPoint 구 반지름
        float hitNormalLength = 0.5f;                     // hitNormal 선 길이
    };

    struct ENGINE_DLL DebugMeshDesc
    {
        Shared<class Model> model;
        Matrix worldMatrix = Matrix::Identity;
        DebugRenderStyle style;
    };

    struct DebugLifetime
    {
        float remainingTime = 0.f; // duration 기반 요청의 남은 시간
        bool isOneFrame = true;    // true면 다음 Update에서 제거되는 1프레임 요청
    };

    struct DebugBoxEntry
    {
        BoundingOrientedBox box;               // 실제 렌더에 사용할 박스 정보
        Color color = Color(1.f, 1.f, 1.f, 1.f); // 박스 렌더 색상
        DebugLifetime lifetime;                // 박스 유지 시간 정보
    };

    struct DebugSphereEntry
    {
        BoundingSphere sphere;                 // 실제 렌더에 사용할 구 정보
        Color color = Color(1.f, 1.f, 1.f, 1.f); // 구 렌더 색상
        DebugLifetime lifetime;                // 구 유지 시간 정보
    };

    struct DebugLineEntry
    {
        Vec3 start = Vec3::Zero;               // 선 시작점
        Vec3 end = Vec3::Zero;                 // 선 끝점
        Color color = Color(1.f, 1.f, 1.f, 1.f); // 선 렌더 색상
        DebugLifetime lifetime;                // 선 유지 시간 정보
    };

    // Mesh는 근데 쓴 적이 없음
    struct DebugMeshEntry
    {
        Shared<class ModelCom> model;
        Matrix worldMatrix;
        Color color = Color(1.f, 1.f, 1.f, 1.f);
        DebugLifetime lifetime;
    };
}
