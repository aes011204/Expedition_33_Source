# Client 아키텍처 규칙 및 컨벤션

## 1. Player 이동 및 상태 제어 시스템 (MVC + State Pattern)

현재 프로젝트의 캐릭터(Player) 제어는 다음과 같은 역할 분담을 따릅니다.

*   **Controller (`PlayerControllerCom`)**: 
    *   **역할 (의도 결정)**: `InputCom`을 통해 사용자의 입력을 받아 해석합니다. (예: Ctrl 키가 눌렸을 때 `_isWalkToggled` 상태를 반전).
    *   완성된 의도(`isWalkDown`를 걷기 토글용으로 재사용, `isRunPressed`를 전력질주 용으로 사용 등)를 `MovementInput` 구조체에 담아 `MovementCom`에 전달합니다.
*   **Model (`Engine/MovementCom`)**: 
    *   **역할 (물리 및 이동 계산)**: `PlayerControllerCom`이 넘겨준 `MovementInput` 플래그를 바탕으로 실제 이동 속도를 결정합니다.
    *   `isRunPressed` -> Sprint 속도 (`sprintSpeed`)
    *   `isWalkDown` -> Walk 속도 (`walkSpeed`)
    *   둘 다 아니면 -> Run 속도 (`runSpeed`)
    *   정규화된 방향 벡터에 결정된 타겟 속도를 곱하여 최종 위치 및 `_velocity`를 업데이트합니다.
*   **State (`Player_State`, `Player_Move` 등)**: 
    *   **역할 (애니메이션 및 상태 전이)**: 물리적 위치 이동에는 관여하지 않으며, 현재 입력 상태와 `MovementCom`의 상태를 기반으로 **애니메이션 재생**과 **다음 상태(Idle/Move/Jump)로의 전이**만을 판단합니다.
    *   **애니메이션 중복 재생 방지**: `_bodyPlayer->Get_AnimationIndex()` (블렌딩 중인 타겟 인덱스 포함)을 비교하여, 변경하려는 인덱스와 현재 타겟 인덱스가 다를 때만 `Set_Animation`을 호출해야 프레임 단위의 애니메이션 깜빡임이나 무한 루프를 방지할 수 있습니다.

## 2. 주요 코드 컨벤션 (C++)
*   **들여쓰기**: Tabs 사용 (공백 4칸 기준, Visual Studio 설정).
*   **중괄호 스타일**: Allman (BSD) 스타일. 여는 중괄호는 항상 새 줄에 위치.
*   **포인터/참조자**: `Type *Var`, `Type &Var` 형태로 변수명 쪽에 붙여 씁니다.
*   **스마트 포인터**: 엔진 및 클라이언트 전반에 걸쳐 `Shared<T>`, `Weak<T>`, `Unique<T>` 및 `ComPtr<T>`를 기본적으로 활용합니다.
