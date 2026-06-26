# UE 5.7.4 → 5.8 엔진 업그레이드 계획 (ExCore 마이그레이션 선행 과제)

> **버전:** v1.0
> **작성:** 2026-06-23
> **프로젝트:** ExFrameWork
> **상태:** 승인 대기 (구현 전 보고서 — `ExFrameWork_Guidelines.md` §3.1)
> **선후 관계:** 본 업그레이드 완료가 `ExRunner_To_ExCore_Migration_Plan.md`(Phase 1~6)의 **선행 조건**입니다.

---

## 0. 배경 및 판단

- UE **5.8 정식 출시** (2026-06). Epic 공지상 **UE5 계열의 마지막 메이저 릴리스**이며, 이후 5.x는 버그/리그레션 픽스만 제공됩니다 → 장기 유지보수 기준선으로 5.8을 채택하는 것이 합리적.
- 현재 상태:
  - 로컬 엔진 `c:\wz\UnrealEngine` = **소스 빌드** (`github.com/EpicGames/UnrealEngine`, 브랜치 `5.7`, **5.7.4**)
  - `ExFrameWork.uproject` `EngineAssociation` = GUID(`{AE6A701E-...}`) → 소스 빌드 로컬 등록 방식
- **업그레이드 = 소스 엔진 재빌드 경로**: 런처 교체가 아니라 5.8 소스 체크아웃 → 재빌드 → 재등록 → 프로젝트 재생성/재컴파일.

### 왜 마이그레이션보다 먼저인가
ExCore 마이그레이션(컴포넌트 이동/리팩토링)을 5.7에서 끝낸 뒤 5.8로 올리면, **엔진 API 깨짐과 구조 리팩토링 변경이 한 빌드에 뒤섞여** 원인 분리가 불가능해집니다. 먼저 5.8에서 **현 구조 그대로 빌드/PIE/서버가 통과하는 안정 기준선**을 확보한 뒤 마이그레이션을 진행합니다.

---

## 1. 영향도 분석 (이 프로젝트가 쓰는 5.8 변경 모듈)

릴리스 노트 + 코드 정합성 기준. ⚠ = 코드 수정 가능성 높음.

| 모듈 | 5.8 변경 (릴리스 노트 요지) | 이 프로젝트 영향 | 위험 |
|---|---|---|---|
| **Mover** ⚠ | 레이어드 무브 **사전 스케줄링**, **롤백/예측 확장**, NavWalking/based-movement 개선, ChaosMover 확장 | 커스텀 `FLayeredMove_LaneCorrection`이 `GenerateMove(FMoverTickStartData&, FMoverTimeStep&, UMoverComponent*, UMoverBlackboard*, FProposedMove&)` 오버라이드 → **시그니처/베이스 변경 시 컴파일 깨짐 최우선** | **높음** |
| **Network Prediction** ⚠ | **Iris 복제 옵션** 추가, adaptive time dilation | Mover가 NetworkPrediction 기반 → 동기화 동작 변화 가능, 서버 검증 필수 | 중간 |
| **GameFeatures / ModularGameplay** | (세부 미확정 — 5.8 노트/헤더 확인 필요) | ExCore·ExRunnerPlay 둘 다 GameFeature 플러그인 → 로드/활성화 흐름 회귀 점검 | 중간 |
| **MVVM (ModelViewViewModel)** | (세부 미확정) | `ExPlayerStatsViewModel`/`ExRunnerStatsViewModel` 등 다수 → 바인딩 API 회귀 점검 | 중간 |
| **CommonUI** | (세부 미확정) | 팝업/토스트/모달 위젯 다수 | 중간 |
| **OnlineSubsystem / EOS** ⚠ | (세부 미확정 — EOS SDK 버전 동반 상승 가능) | `ExNetwork`/`UExOnlineSubsystem`/매치메이킹 → EOS 연동 회귀 위험 | 중간 |
| GAS / Enhanced Input | (세부 미확정) | GAS·EnhancedInput 의존 → 표준 사용이면 영향 적음 | 낮음 |

> ⚠ **정확도 주의**: 위 "세부 미확정" 항목은 현 로컬 엔진이 5.7이라 5.8 헤더로 직접 대조 불가. **Phase 0-B(컴파일 패스)에서 실제 컴파일 에러로 확정**합니다. 추정으로 선제 수정하지 않습니다.

---

## 2. 업그레이드 Phase

### Phase 0-A — 준비 / 안전장치
- [ ] 현재 5.7.4 상태 **전체 커밋 & 태그**(`pre-ue58-upgrade`)로 롤백 지점 고정 (게임 리포 + 필요 시 엔진 리포).
- [ ] 5.8 빌드를 **별도 경로 또는 별도 워크트리**에 두어 5.7 환경을 보존(빌드 실패 시 즉시 복귀 가능).
- [ ] 디스크/시간 예산 확보(엔진 풀 리빌드 = 수십 GB, 수 시간).

### Phase 0-B — 엔진 획득 & 빌드
- [ ] `c:\wz\UnrealEngine`에서 `5.8`(또는 `5.8.x` 최신 태그) fetch/checkout.
- [ ] `Setup.bat` → `GenerateProjectFiles.bat` → `UnrealEditor` Development 빌드.
- [ ] 빌드된 5.8 엔진을 로컬 등록(`UnrealVersionSelector` / `-register`), `EngineAssociation` 갱신.

### Phase 0-C — 프로젝트 재생성 & 컴파일 (핵심 작업 구간)
- [ ] `ExFrameWork.uproject` 우클릭 → Generate Project Files (5.8 기준).
- [ ] `ExFrameWork` + 전 플러그인(`ExCoreRuntime`, `ExRunnerPlayRuntime`, `ExNetwork`, `Nwiro`, `VaultAssetCheckTool`) 컴파일.
- [ ] **컴파일 에러를 모듈별로 분류·수정**. 예상 1순위:
  - `FLayeredMove_LaneCorrection::GenerateMove` 시그니처/베이스 클래스 정합.
  - Mover SyncState/Blackboard/ProposedMove 관련 타입 변경.
  - NetworkPrediction 복제 경로 변경.
- [ ] 각 수정은 **5.8 엔진 헤더를 근거로** 진행하고, 깨진 지점/수정 내용을 `Bug/` 또는 본 문서 부록에 기록.

### Phase 0-D — 에셋 / 데이터 회귀
- [ ] 에디터 최초 기동 시 에셋 자동 업버전 — **무결성 확인 후에만 저장**(대량 uasset 변경은 사용자 승인 후 커밋).
- [ ] GameFeature(ExCore/ExRunnerPlay) 활성화·쿠킹 룰 정상 동작 확인(§ 가이드라인 4.2).
- [ ] MVVM/CommonUI 위젯 바인딩 비주얼 회귀 점검.

### Phase 0-E — 검증 (기준선 합격 조건)
- [ ] 에디터 PIE: 러너 핵심 루프(오토런/레인 보정/장애물/아이템/버프/스탯 UI) 정상.
- [ ] **데디케이티드 서버 + 클라 2인** 매치: 위치 동기화, 버프 복제, 매치 페이즈 정상(Mover/NetworkPrediction 변경 영향 집중 검증).
- [ ] EOS 로그인/매치메이킹 경로 정상.
- [ ] 패키징(쿠킹) 1회 성공.
- ✅ **합격 시 = 5.8 기준선 확립** → 비로소 ExCore 마이그레이션(Phase 1) 착수.

---

## 3. 산출물 / 커밋 전략
- 엔진 업그레이드 커밋과 마이그레이션 커밋을 **분리**(원인 추적성).
- 권장 순서: ① `[Engine] UE 5.8 업그레이드 — 컴파일/빌드 정합` → ② (에셋 업버전이 크면 별도 커밋) → ③ 이후 마이그레이션 커밋들.
- 모든 커밋/푸시는 가이드라인 §3.2에 따라 **사용자 명시 요청 후** 수행.

## 4. 롤백 기준
- Phase 0-C 컴파일 난항 또는 0-E 검증 실패가 일정 임계 초과 시: `pre-ue58-upgrade` 태그로 즉시 복귀, 5.7 유지하며 차단 이슈를 별도 보고.

---

---

## 부록 A — 업그레이드 사이드 이펙트 로그 (실측)

> 2026-06-23 5.8 전환 후 실제 빌드/실행에서 발견된 이슈를 발생 순서대로 기록.

### A-1. [해결] Target.cs 빌드 설정 버전 불일치 (UBT 설정 단계 실패)
- **증상**: `ExFrameWorkEditor` 빌드가 컴파일 전 3.8초 만에 `Failed (OtherCompilationError)`.
  ```
  ExFrameWorkEditor modifies the values of properties:
  [ UnreachableCodeWarningLevel: Off != Error, ReturnTypeWarningLevel: Off != Error,
    DanglingWarningLevel: Off != Error ]. This is not allowed, as ExFrameWorkEditor
  has build products in common with UnrealEditor.
  ```
- **원인**: UE 5.8이 `BuildSettingsVersion.V7`에서 위 3개 경고 레벨 기본값을 `Error`로 변경. 소스 빌드된 UnrealEditor는 엔진 기본값(V7)으로 컴파일됨. 프로젝트 타겟은 `V6`(해당 경고 Off) → 산출물 공유 환경에서 설정 충돌.
- **조치**: `ExFrameWork.Target.cs` / `ExFrameWorkEditor.Target.cs` 의 `DefaultBuildSettings` **V6 → V7** (UBT 권고 정공법, 엔진 기본값과 정렬).
- **후속 관찰 필요**: V7로 `ReturnType/Dangling/UnreachableCode` 경고가 **Error**로 승격 → 우리 모듈 코드에서 신규 컴파일 에러 발생 가능 (A-3에서 추적).

### A-2. [해결] Nwiro 플러그인 제거
- **배경**: 빌드 로그에 Nwiro Build.cs의 CS0618 경고(×2, `ShadowVariableWarningLevel` UE5.6부터 deprecated)가 출현. 추가로 `Nwiro Pro`는 마켓플레이스 AI 도구로 **EngineVersion 5.7.0 전용**이라 5.8 호환성 미보장.
- **판단**: 프로젝트 내 외부 의존(.Build.cs/include/config) 전무, git 미추적 third-party 플러그인 → 사용자 지시로 제거.
- **조치**: `ExFrameWork.uproject`의 `Nwiro` 플러그인 항목 삭제 + `Plugins/NWIROThed5df227d2a13V1/` 폴더 삭제 + 프로젝트 파일 재생성. (CS0618 경고도 함께 소거됨)

### A-3. [해결] 엔진/게임 C++ 컴파일 결과 — 5.8 빌드 성공
- **핵심**: 엔진 본체 + 게임 모듈(`ExFrameWork`/`ExCoreRuntime`/`ExRunnerPlayRuntime`)은 **에러 0**. 우려했던 Mover 커스텀 `FLayeredMove_LaneCorrection::GenerateMove`도 5.8 API와 정합(무수정 통과).
- 빌드 차단은 **부속 third-party/툴 플러그인 3종**에 한정되었고 아래와 같이 처리하여 `Result: Succeeded` 달성.

#### A-3-1. [해결] VaultAssetCheckTool (에디터 툴, 미추적)
- `ReportUtils.h`: 삼항 연산자 피연산자 타입 불일치(C2446) → `if/else`로 FString 명시 구성.
- `ReportProjectInfo.cpp`: 5.8에서 제거된 `UIOSRuntimeSettings::bSupportsMetalMRT`/`bSupportAppleA8` → `#if ENGINE_MINOR_VERSION < 8` 버전 가드.

#### A-3-2. [해결] OnlineSubsystemEOS (프로젝트 오버라이드, README §4 절차)
- 깨짐: 로컬(5.7 기반) 복사본이 5.8의 `IVoiceChatUser` `final` 메서드 + format-string constexpr 강화에 걸림.
- 조치: **엔진 5.8 `OnlineSubsystemEOS/Source` 전체로 갱신**(53파일, .uplugin 포함 — VoiceChat 의존성/신규 파일 3쌍 반영). 엔진판이 5.8 대응 완료라 위 에러 해소.
- **Device ID 로그인 패치 재적용**(README §4 핵심): 5.8 구조에 맞춰 ① `OnlineSubsystemEOS.Build.cs` `bAddUserLoginInfo` 기본값 `true`(=`ADD_USER_LOGIN_INFO=1`, PC에서 `Options.UserLoginInfo` 채워짐) ② `UserManagerEOS.cpp` 3개 지점 빈 DisplayName→`"Player"` 폴백. 모두 `// [Ex] Modified` 마커.
  - ⚠️ 런타임 검증 필요: PC Device ID 로그인이 5.8에서도 `EOS_InvalidParameters` 없이 성공하는지 데디 서버 테스트(0-E).

#### A-3-3. [교체 완료] Sentry (크래시 리포팅, 추적본)

**임시 비활성 → 1.15.0으로 교체 (2026-06-26)**

**5.7→5.8 전환 시 발생했던 에러 (1.8.0 기준):**
- 깨짐: 5.7판 SDK(v1.8.0)가 5.8 API 변경 3종에 걸림.
  - `FJsonObject::Values` 키 타입 `FString` → `UE::FSharedString` 변경 → `TMap::Add`/`GetKeys` 호출 컴파일 에러 (`CrashReporter.cpp:80`, `Converters.cpp:298/350`)
  - `PLATFORM_64BITS` 매크로 5.8에서 폐기 (64비트 전용 선언)
  - `FOnBackBufferReadyToPresent` 델리게이트 파라미터 변경 (`FTextureRHIRef` → `ISlateViewportProvider` + `GetBackBufferResource()`)
- 임시 조치: ① `ExFrameWork.uproject` Sentry `Enabled:false` ② `ExFrameWork.Build.cs`/`ExRunnerPlayRuntime.Build.cs` 의존 주석 처리 ③ `ExRunnerPlay.uplugin` Sentry 플러그인 의존성 제거

**교체 내용 (1.8.0 → 1.15.0):**
- 참고 PR: #1390 (JSON/PLATFORM_64BITS), #1440 (UE5.8 공식 지원), #1451 (델리게이트 시그니처)
- `sentry-unreal-1.15.0-engine5.8.zip` 다운로드 → `Plugins/Sentry/` 전체 교체
- 임시 비활성 3곳 복원:
  - `ExFrameWork.uproject` Sentry `Enabled:true`
  - `ExFrameWork.Build.cs` / `ExRunnerPlayRuntime.Build.cs` 의존 복원
  - `ExRunnerPlay.uplugin` Sentry 플러그인 의존성 재추가
- ⚠️ 빌드 후 런타임 검증 필요 (A-4 항목에 포함)

#### A-3-4. [기록] 잔여 비치명 경고 (5.8 회귀 아님, 별도 정리)
- `ExCore.uplugin`이 `OnlineSubsystem`/`OnlineSubsystemUtils`/`ExNetwork`를 플러그인 의존성 미선언(모듈은 의존).
- `ExFrameWork.uproject`가 `BinkMedia` 미선언(모듈은 `BinkMediaPlayer` 의존).
- → 빌드/실행 무관한 의존성 선언 위생 항목.

### A-4. 다음 단계 (Phase 0-D/0-E 런타임 검증 — 미실행)
- [ ] **Sentry 1.15.0 빌드 통과 확인** (프로젝트 재생성 + 컴파일 — A-3-3 교체 직후).
- [ ] 에디터 기동 + 에셋 자동 업버전 무결성 확인.
- [ ] PIE: 러너 핵심 루프(오토런/레인 보정/장애물/아이템/버프/스탯 UI).
- [ ] **데디 서버 + 클라 2인**: 위치 동기화 / 버프 복제 / 매치 페이즈 / **EOS Device ID 로그인(패치 검증)**.
- [ ] 패키징(쿠킹) 1회.

---

## 변경 이력
| 날짜 | 버전 | 내용 |
|------|------|------|
| 2026-06-23 | v1.0 | 초안. 5.7.4→5.8 소스 빌드 업그레이드 경로, 모듈 영향도(Mover 레이어드무브 최우선), Phase 0-A~E, 마이그레이션 선행 관계 정의 |
| 2026-06-23 | v1.1 | 5.8 전환 실행 착수. 부록 A 추가: Target.cs V6→V7(해결), Nwiro Build.cs CS0618(정리 대상), 풀 빌드 진행 |
| 2026-06-26 | v1.2 | A-3-3 Sentry 1.8.0→1.15.0 교체 완료. PR #1390/#1440/#1451 수정 사항 반영된 버전으로 교체. uproject Enabled 복원, Build.cs 의존 복원, ExRunnerPlay.uplugin 의존성 복원 |
