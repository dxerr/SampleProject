# Slide 시 바닥 뚫림 (Android Shipping) 버그 리포트

## 개요 (Overview)
- **키워드:** #Shipping #Android #MoverPlugin #GameAnimationSample #Slide #EncroachingBlockingGeometry #TeleportTo
- **현상:** Development 빌드에서는 정상 작동하나, Android Shipping 빌드에서만 캐릭터가 Slide(슬라이딩) 상태 진입 시, 캡슐 및 스켈레탈 메쉬가 `BP_ExFloorChunk` 아래로 파고들어 이동하는 현상 발생 (Crouch 상태는 정상).
- **관련 시스템:** 언리얼 엔진 5 Game Animation Sample (Mover 플러그인 기반 이동 처리)

## 원인 분석 (Troubleshooting Process)
광범위한 리서치 및 에픽게임즈 포럼의 Mover 플러그인 이슈 트래킹 결과를 분석한 결과, 이 현상은 **Mover 플러그인의 캡슐 컴포넌트 업데이트 방식**과 **스켈레탈 메쉬(Skeletal Mesh)의 Collision(충돌) 처리 최적화**가 겹치며 발생하는 알려진(Known) 이슈입니다.

1. **애니메이션 커브 최적화 (Anim Curve Stripping)**
   - Game Animation Sample(GASP)에서 캐릭터가 슬라이딩하거나 자세를 낮출 때, 발이 바닥을 파고들지 않도록(Mesh Z Offset) 보정해주는 값은 애니메이션 시퀀스 내부에 들어있는 **애니메이션 커브(Animation Curve) 값**으로 계산됩니다.
   - Development 빌드에서는 모든 커브 데이터를 추적하여 런타임에 읽어들일 수 있습니다. (메쉬 위치가 완벽히 보정됨)
   - 그러나 **Shipping 빌드**에서는 모바일 메모리를 아끼기 위해 "직접적으로 모프 타겟이나 머티리얼에 연결되지 않은 블루프린트 전용 커브 데이터"를 모조리 버려버립니다(Strip 처리).
   - 결과적으로 슬라이딩을 할 때 커브 값을 0.0으로 반환하게 되어, 메쉬를 위로 들어올려주지 못하고 그대로 캡슐 중심점을 따라 지하실로 직행하게 됩니다.

## 해결 방법 (Resolution)
이 문제를 해결하려면 패키징 시 **애니메이션 커브 데이터가 삭제(Strip)되지 않도록 보호**해야 합니다.

1. **스켈레탈 메쉬에서 커브 유지 설정 적용**
   - 문제가 되는 캐릭터의 **Skelton(스켈레톤) 에셋**을 엽니다.
   - 우측 패널의 **Anim Curves(애니메이션 커브)** 창을 엽니다.
   - 슬라이드 높이/오프셋 판정에 관여하는 커브들(예: `DisableLegIK`, `RootOffset`, `ZOffset` 등 관련된 Float 커브)의 **체크박스 옵션을 점검**하여, 무조건 빌드에 포함되도록 설정해야 합니다.
2. **프로젝트 세팅 커브 최적화 제한 (대안)**
   - Project Settings -> Engine -> Animation 항목에서 `Anim Curve Compression` 류의 세팅이나 `Strip Anim Data On Dedicated Server` 등의 옵션을 확인합니다.
3. **Mover 플러그인 또는 블루프린트 내부 보정**
   - 만약 커브로 해결이 안 된다면 `ExRunnerCharacter_Mover_Child` 내 등에서 '슬라이드 실행 중'일 때 강제로 메쉬의 `Relative Location Z` 값을 보정재입력(Lerp)하는 로직을 하드코딩으로 넣어주는 것도 강력한 해결책입니다.
