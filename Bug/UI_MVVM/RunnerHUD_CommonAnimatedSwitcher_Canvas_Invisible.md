# 버그: 러너 HUD 전체 미표시 (CommonAnimatedSwitcher 자식 구조 오류)

## 키워드
`WBP_ExRunnerHUDLayout` `CommonAnimatedSwitcher` `HUD 안보임` `Canvas Panel` `MVVM` `View Binding`

---

## 현상
- 러너 게임 플레이 시 **HUD UI 전체**가 화면에 표시되지 않음
- 조이패드 버튼UI(WBP_ExButtonPad), 스피드바(WBP_ExRunnerSpeedBar) 등 모든 HUD 요소 미표시
- 출력 로그에는 HUD 생성 성공 메시지 정상 출력
  ```
  LogExCorePC: HUD Widget Created & Added to Viewport.
  ```
- `CommonAnimatedSwitcher_65`의 `Active Widget Index`는 정상적으로 0으로 확인됨

---

## 원인
`WBP_ExRunnerHUDLayout`의 위젯 계층 구조에서 **일반 Canvas Panel이 `CommonAnimatedSwitcher_65`의 자식으로 잘못 배치**된 것이 원인.

`CommonAnimatedSwitcher`는 일반 `WidgetSwitcher`와 달리 자식 위젯이 **`UCommonActivatableWidget`을 상속**해야 활성화(Activate) 처리가 가능하다.
일반 Canvas Panel은 `UCommonActivatableWidget`이 아니므로 CommonAnimatedSwitcher가 Active Index = 0이어도 화면에 표시하지 못한다.

### 잘못된 구조 (문제 발생)
```
CommonAnimatedSwitcher_65
  └── [Canvas Panel]          ← ❌ CommonActivatableWidget이 아님
        ├── WBP_ExRunnerSpeedBar
        ├── InputPadSwitcher
        └── ...
```

### 올바른 구조 (해결 후)
```
[Canvas Panel] (최상위)
  ├── CommonAnimatedSwitcher_65   ← Phase 전환용 (자식에 CommonActivatableWidget만 사용)
  └── [Canvas Panel]              ← HUD 요소 (스위처 외부에 독립 배치)
        ├── WBP_ExRunnerSpeedBar
        ├── InputPadSwitcher
        └── ...
```

---

## 발생 경위
`ExRunnerMatchViewModel` 클래스를 `BP_ExRunnerMatchView`로 변경하면서 View Binding 재연결 작업 중, `WBP_ExRunnerHUDLayout`의 위젯 계층에서 Canvas Panel이 `CommonAnimatedSwitcher_65` 하위로 잘못 이동됨.

---

## 해결 방법
`WBP_ExRunnerHUDLayout` 블루프린트 Designer에서:
1. `CommonAnimatedSwitcher_65` 하위에 있는 **Canvas Panel을 드래그하여 CommonAnimatedSwitcher_65 외부**로 이동
2. 최상위 Canvas Panel의 직접 자식으로 배치
3. **Compile → Save**

---

## 주의사항
- `CommonAnimatedSwitcher`의 자식에는 반드시 **`CommonActivatableWidget` 파생 위젯**만 배치할 것
- View Binding 재연결, 위젯 리팩토링 작업 후에는 **Hierarchy 구조를 반드시 확인**할 것
- MVVM View Binding 변경 시 Designer의 위젯 계층이 의도치 않게 변경될 수 있음
