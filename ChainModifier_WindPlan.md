# ChainModifier Wind Plan

## Goal

- `ChainModifier`에 바람 힘을 추가한다.
- `ModifyReplace`와 `ModifyAddictive`가 서로 다른 기준 포즈를 써도 동일한 wind 입력 경로를 공유한다.
- 스카프는 "애니메이션 포즈를 기본으로 유지 + 바람에 의한 후행"만 받도록 구성한다.

## Current Integration Point

- 바람 힘은 `Engine/Private/ChainModifier.cpp`의 `Solve_Verlet()`에서 적용한다.
- 현재 Verlet 적분은 아래 순서로 진행된다.
  1. base force(`gravity`)
  2. movement inertia offset
  3. wind force
  4. pose restore
  5. collision
  6. distance / bend constraint

## Wind Input

`ChainModifierDesc::runtime`

- `windDirection`
  - 월드 공간 방향 벡터
  - 길이는 무시하고 정규화해서 사용한다.
- `windScale`
  - 월드 공간 힘의 크기
  - `0`이면 wind 비활성

## Wind Force Rule

- root에 가까운 본은 덜 흔들리고 tip에 가까운 본은 더 흔들리게 해야 한다.
- 현재는 `nodeIndex / (nodeCount - 1)`의 제곱값을 가중치로 사용한다.
- 즉:
  - root 근처: 거의 영향 없음
  - 중간: 약간 영향
  - tip: 가장 크게 영향

## Mode Policy

### ModifyReplace

- 바인드 기반 / 기존 체인 기준 포즈를 사용한다.
- wind는 "기존 체인 자세를 밀어내는 추가 force"로 동작한다.
- 기존 로직과 동일하게 최종 로컬을 다시 써준다.

### ModifyAddictive

- 현재 프레임 애니메이션 포즈를 `poseLocal`, `poseLocalDir`로 캡처한다.
- wind는 이 애니메이션 포즈를 기준으로 한 secondary motion으로만 동작한다.
- 즉 "원래 애니메이션 실루엣 + 바람 후행" 구조다.

## Scarf Preset Policy

- 스카프는 `ModifyAddictive`
- `gravity = 0`
- `movementInertiaScale = 0`
- `maxMovementOffset = 0`
- wind만으로 흔들림을 만든다.

## Future Extension

### 1. Gust

- `windScale`에 시간 기반 envelope를 곱한다.
- 예:
  - `baseWind`
  - `gustAmplitude`
  - `gustFrequency`

### 2. Turbulence

- 체인마다 위상차를 둔 noise / sin을 적용한다.
- 같은 캐릭터 안에서도 좌우 스카프가 완전히 같은 패턴으로 흔들리지 않게 한다.

### 3. Per-group Wind Multiplier

- `BoneChainGroupDesc`에 `windMul`을 추가한다.
- 예:
  - 짧은 띠: `0.6`
  - 긴 스카프: `1.2`

### 4. Lateral Wind

- 필요하면 wind를 체인 방향에 수직인 성분으로 투영해 "펴지는 느낌"보다 "옆으로 날리는 느낌"을 강화한다.
- 현재 1차 구현은 단순 월드 방향 force다.
