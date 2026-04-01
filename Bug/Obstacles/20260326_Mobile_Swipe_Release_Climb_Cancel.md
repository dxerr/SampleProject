# Bug Report: 모바일 스와이프 시 Climb(파쿠르) 장애물 넘기 실패(Cancel) 문제

## 이슈 개요
- **증상**: 러너 게임 플레이 중 `clim`(Climb) 타입의 장애물을 넘어갈 때, 타겟팅 점프/파쿠르 로직이 발동되지 않거나 도중에 취소되어 충돌하는 현상이 간헐적으로 발생함.
- **특징**: 스와이프(위로 드래그) 컨트롤 시 자주 발생하며, 출력 로그상 Jump 이벤트를 발동(`ACTIVATED`)한 직후 아주 짧은 시간 안에 점프 해제(`DEACTIVATED`) 및 `OnJumpRequested.Broadcast(false)`가 강제로 호출됨.

## 원인 분석 (Root Cause)

1. **너무 짧은 간격의 Jump 해제 이벤트 전송 (Swipe Release)**
   현재 모바일 터치패드 뷰모델(`ExRunnerInputViewModel.cpp`)의 설계상 스와이프를 하면 다음과 같은 흐름이 발생합니다.
   - **스와이프 중**: 드래그 이동량이 점프 임계값(Threshold)을 넘는 순간 `[ExJump] *** ACTIVATED ***` 로그와 함께 `RequestJumpAction(true)`가 호출됩니다.
   - **손가락을 뗄 때**: 스와이프 동작 직후 반드시 화면에서 손가락을 떼게 되는데, 이때 `OnTouchPadReleased()`가 호출되며 하드코딩된 값 `HandleJumpInput(0.0f, -9999.0f)`을 통해 즉시 `[ExJump] *** DEACTIVATED ***` 상태로 전환, `RequestJumpAction(false)`를 전송합니다.

2. **`ExRunnerInputComponent`의 즉각적인 false 브로드캐스트**
   안정적인 이벤트 전달을 위해 `UExRunnerInputComponent::RequestJumpAction` 내부에는 `bIsTriggered`가 false일 경우 바로 `OnJumpRequested.Broadcast(false)`를 날리도록 구현되어 있습니다.

3. **`AC_TraversalLogic` (파쿠르/클라임 시스템)과 입력 취소 충돌**
   `Climb` 장애물 위를 뛰어넘는 동작은 언리얼 파쿠르 시스템(`AC_TraversalLogic`)에 의해 판단됩니다. 
   스와이프 발동(true) 후 불과 몇 프레임(또는 Music Beat 1~2틱 수준)만에 터치를 뗌으로써 `OnJumpRequested(false)`가 파쿠르 시스템에 전달되고, 파쿠르 시스템 측에서는 점프 키가 너무 일찍 떼어졌다(입력 유지 실패 또는 Release 취소 동작)고 판단하여 Vault나 Mantle 모션을 중단(Cancel)해 버립니다.
   (반면 키보드 입력부는 `Completed` 바인딩이 제외되어 있어 점프를 짧게 눌러도 false 브로드캐스트 자체가 발생하지 않으므로 이 버그를 피하고 있었습니다.)

## 해결방안 제안 (Resolution Plan)

모바일 스와이프 점프 액션은 **버튼 홀드(Hold)** 방식이 아닌 **트리거 단발성 이벤트(Flick/Swipe)**로 취급하는 것이 러너 장르에 적합합니다. 따라서 다음과 같은 수정 방향을 권장합니다.

1. **터치 Release 시 Jump False 전송 제한**
   스와이프로 인한 점프 시에는 `OnTouchPadReleased` 함수 내의 `HandleJumpInput(0.0f, -9999.0f);` 호출 부분을 제거하거나, 일정 시간(예: 0.2초) 타임아웃 딜레이를 두고 false를 전송하도록 타이머 처리를 도입합니다.
   
2. **OnJumpRequested(false) 브로드캐스트 분기 세분화 (선택사항)**
   점프 홀드 시간에 비례해 점프 체공 시간이 달라지는 로직(슈퍼 마리오 식 가변 점프)이 굳이 필요한 것이 아니라면, 점프 해제(False) 이벤트를 Traversal 시스템이 읽고 취소(Cancel)하지 못하도록 `AC_TraversalLogic` 내에서 Release 기반의 Cancel 처리를 막아야 합니다.

**결론**: 점프를 발동한 직후 스와이프를 위해 화면에서 손을 떼는 행위가 **점프 키 뗌(홀드 해제)**으로 직접 번역되어, 파쿠르 시스템 컴포넌트(`AC_TraversalLogic`)를 도중에 강제 중단시키는 것이 버그의 근본 원인입니다. `ExRunnerInputViewModel.cpp`에서 Swipe로 인한 점프 Release(`DEACTIVATED`) 처리를 모바일 Runner 환경에 맞게 완화해야 합니다.

---

## 💡 추가 원인 분석 (스크린샷 기반 - 캐릭터 회전/방향성 문제)

주인님께서 공유해주신 스크린샷과 곡선(Curve) 트랙 환경을 추가로 분석한 결과, 점프가 씹히거나 실패하는 **또 다른 치명적 원인**을 발견했습니다.

1. **스로틀/가로 스와이프 조작 시 발생하는 캡슐 회전 왜곡**
   - 모바일에서 위로 스와이프(점프)할 때, 손가락이 완벽한 수직이 아니라 약간 좌우 대각선으로 움직이기 쉽습니다.
   - 뷰모델(`ExRunnerInputViewModel::OnTouchPadMoved`)에서 이 좌우 오프셋(`NormalizedOffset.X`)을 곧바로 `RequestLookAction`으로 넘기고, 무브먼트 컴포넌트(`ExRunnerMovementComponent::ProduceInput_Implementation`)는 이를 받아 `TargetLookYawOffset`으로 변환합니다.
   
2. **`OrientationIntent` (캐릭터 정면 방향)의 강제 트위스트**
   - 무브먼트 컴포넌트는 캐릭터의 시선(`Inputs.OrientationIntent`)을 경로의 접선(`ForwardDir`)에서 `TargetLookYawOffset`만큼 회전(RotateAngleAxis)시켜버립니다.
   - 즉, 캐릭터가 **스플라인(트랙)의 정면을 보지 않고 약간 좌/우측 옆을 바라보면서 대각선으로 달리는(점프하는) 상태**가 됩니다. (스크린샷에서 캐릭터가 우측 밖을 보며 꼬여있는 모습이 바로 이 현상입니다.)

3. **`AC_TraversalLogic` 레이캐스트(Raycast) 빗나감**
   - 파쿠르/클라이밍 장애물을 인식하는 시스템(`AC_TraversalLogic`)은 캐릭터의 **정면 방향(Actor Forward Vector)**으로 레이캐스트를 쏴서 장애물을 감지합니다.
   - 캐릭터의 몸이 대각선으로 틀어져 있으므로, 정면에서 다가오는 `clim` 장애물을 쏘는 레이캐스트가 엉뚱한 대각선 허공을 뚫고 지나가 **장애물 감지에 완전히 실패(Miss)**하게 됩니다. 감지가 안 되니 파쿠르 로직 자체가 시작조차 못 하고 일반 점프만 발생하거나 그대로 충돌합니다.

### 💡 보완된 해결 방안 (Rotation Fix)
- 점프 중(또는 스와이프 수직 임계값 돌파 시)에는 `NormalizedOffset.X` 입력을 차단하거나 `TargetLookYawOffset`을 0으로 스냅(강제 정렬)하여, 점프하는 순간만큼은 캐릭터가 경로의 완전한 정면(`ForwardDir`)을 바라보도록 `OrientationIntent`를 보정해야 합니다.
- 이렇게 하면 캐릭터의 시선과 점프 방향이 트랙 전방 장애물과 일치하게 되어, 레이캐스트 탐지가 정상적으로 꽂혀 파쿠르 점프 로직이 확실하게 발동할 것입니다!

---

## 💡 추가 원인 분석 2 (종잇장처럼 얇은 장애물 두께 문제)

주인님께서 지적해주신 이미지 상의 **"너무 얇은 두께"** 역시 파쿠르 엔진 실패의 핵심 원인 중 하나입니다.

1. **Traversal(파쿠르) Ledge 탐지 센서의 구조적 한계**
   `AC_TraversalLogic`은 뛰어넘기(Vault)를 판단하기 위해 장애물 정면을 스피어 트레이스(Sphere Trace)로 친 다음, 장애물 상단 라인을 찾기 위해 약간 앞쪽 공중에서 아래로 트레이스를 또다시 쏘아 올립니다(Front / Back Ledge 확인).
   하지만 장애물의 두께(Depth)가 1~5cm 수준의 사실상 **종잇장(Plane)**에 가까울 경우 센서 구체 반경(통상 20~30cm)보다 작아져, 아래로 꽂는 트레이스가 장애물의 지붕 라인 안쪽이 아니라 **완전히 뒤편 허공**으로 떨어지거나 **두께 0으로 인한 버그 히트**를 일으켜 지형분석(Ledge Detection)에 실패합니다.

2. **데이터 에셋(Data Asset) 설정 오류 (원인)**
   장애물의 생성 시 두께(X)는 `UExObstacleDefinition` 데이터 에셋에 정의된 `MinSize.X`와 `MaxSize.X` 사이에서 랜덤으로 결정됩니다. 만약 `MinSize.X`가 1~5cm 등 지나치게 작게 설정되어 있다면 종잇장 장애물이 생성됩니다.
   `ExObstacleStrategy_Climb.cpp`에는 너무 두꺼울 때(30.0f) 깎아내는 로직만 있고 최솟값은 기획 데이터(`MinSize.X`)에 전적으로 의존하고 있습니다.

### 💡 보완된 해결 방안 (데이터 수정)
- 코드 수정 없이(하드코딩 배제), 에디터 내 장애물 데이터 에셋(`ExObstacleDefinition` 등 엑셀/데이터 테이블)을 열어서 **Climb 계열 장애물의 `MinSize.X` 값을 최소 15.0 ~ 20.0 (cm) 이상으로 기입(조절)**해 주시면 종잇장 현상을 근본적으로 해결할 수 있습니다! 레이캐스트 구체가 탐지할 수 있는 최소 공간을 확보해주세요.
