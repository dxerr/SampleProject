# UE5.8 회귀 B — IK Retarget None 스팸 수정 계획 (B안: 옛 경로 패치)

> **상태**: 승인 대기 (구현 미착수)
> **관련**: `UE5.8_Engine_Upgrade_Plan.md` 부록 A (Phase 0-E 런타임 검증의 차단 이슈)
> **작성일**: 2026-06-23
> **선행 관계**: 본 수정이 5.8 기준선 합격(런타임 검증)의 차단 요소. 합격 후 ExCore 마이그레이션 진행.

---

## 1. 문제 정의

UE 5.8 전환 후 PIE 실행 시 IK Retarget 관련 **"Accessed None" 에러가 매 프레임 대량 발생**한다.

| ABP | 경로 | 실측 스팸량(6/22 PIE) |
|-----|------|------------------|
| `ABP_Ex_GenericRetarget` | `/ExRunnerPlay/RetargetedCharacters/` (플러그인) | 2,502건 |
| `ABP_GenericRetarget` | `/Game/Blueprints/RetargetedCharacters/` | 1,937건 |

- 두 ABP는 **동일 구조의 복사본** 관계(`_Ex`가 러너 플러그인 사본).
- 사용 캐릭터: `ABP_Ex_*` ← `BP_Shinbi`(러너) / `ABP_*` ← `BP_Echo`·`BP_Manny`·`BP_Quinn`·`BP_Twinblast`·`BP_UE4_Mannequin`·`BP_Kellan`·`BP_Echo1` (총 8개).
- **실제 기능 손실**: 런타임 IK 동적 오버라이드(커브 기반 발 IK/블렌드 보정)가 적용되지 않음. **정적 기본 리타게팅은 정상 동작**.

---

## 2. 원인 분석 (확정)

### 2-1. 직접 원인 — op 이름 조회 실패
두 ABP의 `UpdateRetargetProfile` 함수 그래프가 다음 체인을 매 프레임 실행한다:

```
Input Profile
  → [Get Op Controller From Retarget Profile]  (In Retarget Op Name = "Retarget IK Goals")
  → [Cast To IKRetargetIKChainsController]
  → [Get Settings] (Chains to Retarget 배열 = RetargetIKChainSettings[])
  → [For Each] → [Break Retarget IKChain Settings] → 커브 보정(Lerp) → [Make] → [Set Settings]
```

5.8 RTG에서 op `"Retarget IK Goals"` 조회 시 **index `-1`(존재하지 않음)** 반환:
```
get_index_of_op_by_name("Retarget IK Goals") == -1   # 양쪽 RTG 모두
```
→ `Get Op Controller` 가 nullptr → Cast 실패(None) → `Get/Set Settings` 가 매 프레임 None 접근 → 스팸.

### 2-2. 근본 원인 — 5.8 아키텍처 변경 (공식 문서 근거)
1. **op 재명명/분할**: 5.8 RTG PostLoad 마이그레이션(`IKChainsOpSplitIntoSeparateOps`)이 통합 IKChains op(`Retarget IK Goals`)을 **`Blend to Source` op으로 변환**한다. (op 클래스 `IKRetargetIKChainsOp`/컨트롤러는 하위호환용으로 존재하나, 업그레이드된 RTG에는 해당 op 인스턴스가 더 이상 없음.)
2. **워크플로 교체**: 5.8은 **Retarget Profiles → Override Sets** 로 대체. `GetOpControllerFromRetargetProfile` + 매 프레임 read-modify-write 패턴 자체가 deprecated 옛 경로.
   - 5.8 정공법: 리타게터 에셋에 Override Set 선언 → `Retarget Pose From Mesh` AnimNode 입력 핀으로 노출 → 게임플레이 상태 직접 전달.

### 2-3. RTG op 위치 대조 (마이그레이션 매핑 증거)
| 위치 | 5.7 샘플 RTG | 우리 5.8 RTG (자동 업그레이드 후) |
|---|---|---|
| 3 | **Retarget IK Goals** (IKChains op) | **Blend to Source** |
> 그 외 7개 op(Pelvis Motion / Retarget FK Chains / Stride Warp / Speed Plant / Run IK Rig / Pole Vector / Remap Curves)은 동일. **딱 한 자리만 변환**됨 → IKChains op → Blend to Source op 매핑 확정.

### 2-4. 기성 정답지 부재 (리서치 결론)
- Epic의 **5.8 Game Animation Sample조차 옛 경로 그대로 출시**(ABP·RTG가 5.7과 byte 동일, `.uproject`만 5.8 태그). 즉 Epic도 자기 샘플을 미수정/미이관.
- 글로벌 포럼·레딧에 **본 증상(op None 스팸)의 직접 해결 사례 없음**. 다른 GASP 리타게팅 이슈(앞기울임 등)와 무관.
- → 외부 에셋 교체/Migrate로는 해결 불가. **직접 수정 필요.**

---

## 3. 해결 방침 — B안 (옛 경로 패치)

deprecated된 Retarget Profile 경로를 유지하되, 조회 대상 op을 5.8 매핑에 맞게 교체하여 **스팸 제거 + 동적 오버라이드 동작 복구**.

### 3-1. 핵심 매핑
| 옛 (5.7/현재 ABP) | 새 (5.8) |
|---|---|
| op 이름 `"Retarget IK Goals"` | `"Blend to Source"` |
| 캐스트 `IKRetargetIKChainsController` | `IKRetargetBlendToSourceController` |
| settings 구조체 `RetargetIKChainSettings` | `IKRetargetBlendToSourceChainSettings` |

`IKRetargetBlendToSourceChainSettings` 필드: `target_chain_name`, `blend_to_source`, `rotation_alpha`, `translation_alpha`, `translation_per_axis_alpha`, `apply_pelvis_offset`.

### 3-2. ⚠️ 선행 확정 필요 — 실제 보정 필드 (Phase 1)
현 ABP의 `Break` 노드는 Blend to Source가 갖지 않는 필드(`Enable IK`, `Static Offset`, `Static Local/Rotation Offset`, `Scale Vertical`, `Extension`)도 노출한다. **이들이 커브로 실제 보정(write)되는지, 단순 passthrough인지**에 따라 작업 범위가 갈린다:

| 보정되는 필드 | 추가로 손봐야 할 op / 컨트롤러 |
|---|---|
| Blend To Source 계열만 | `Blend to Source` 단일 op (= 최소 작업, 깔끔) |
| `Enable IK` 포함 | `Run IK Rig` op (`IKRetargetRunIKRigController`, `enable_ik`) 추가 |
| `Static Offset` 포함 | `Offset Goals` op (`IKRetargetOffsetGoalsChainSettings`) 추가 |
| `Scale Vertical` 포함 | `Scale Goals` op (`IKRetargetScaleGoalsChainSettings`) 추가 |
| `Extension` 포함 | `Stretch Chain` op (`IKRetargetStretchChainOpSettings`) 추가 |

→ **Phase 1에서 그래프(`Lerp`/`Make Retarget IKChain Settings` 배선)를 정밀 확인**하여 실제 보정 필드 목록을 확정한 뒤 최종 작업 범위를 픽스한다. (가설: Blend to Source 단일 op 경로가 유력 — 메모리 분석상 `BlendToSource` 24회 참조.)

---

## 4. 구현 단계

> 그래프 편집은 Python(UHT) 직접 편집 불가 → **에디터에서 수동 수정**. MCP는 검증/조회에 활용.

- **Phase 1 — 보정 필드 확정**
  - [ ] `ABP_Ex_GenericRetarget` → `UpdateRetargetProfile` 그래프 오른쪽(Break→Lerp→Make→Set Settings) 정밀 확인.
  - [ ] 커브로 실제 write되는 필드 목록 확정 → §3-2 표로 작업 범위(단일 op vs 다중 op) 픽스.
  - [ ] 픽스 결과를 본 문서에 추가 기록 후 Phase 2 착수.

- **Phase 2 — `ABP_Ex_GenericRetarget` 수정 (러너 우선)**
  - [ ] `Get Op Controller From Retarget Profile`의 op 이름 → `"Blend to Source"`.
  - [ ] `Cast To` 대상 → `IKRetargetBlendToSourceController`.
  - [ ] `Get/Set Settings` 및 `Break/Make` 구조체 → `IKRetargetBlendToSourceChainSettings` 기준 재배선.
  - [ ] (다중 op로 확정 시) 해당 op별 조회/캐스트/Set 분기 추가.
  - [ ] 컴파일 → 에러/경고 0 확인.

- **Phase 3 — `ABP_GenericRetarget` 동일 수정**
  - [ ] Phase 2와 동일 절차 반영 (복사본 관계이므로 동형).

- **Phase 4 — 검증 & 저장**
  - [ ] PIE: `ABP_Ex_*`(Shinbi) + `ABP_*`(Manny 등) 캐릭터로 실행 → **로그 None 스팸 0건** 확인.
  - [ ] 시각 회귀: 달리기/레인 보정 시 발 IK·블렌드 동작이 정상(또는 정적 대비 개선)인지 육안 확인.
  - [ ] `LogMover out-of-band 145건`(회귀 A 잔여 증상) 동반 해소 여부 재확인.
  - [ ] SaveAll → 변경 .uasset 2개 커밋 대기 보고 (커밋은 승인 후).

---

## 5. 리스크 / 예상 사이드 이펙트

- **deprecated API 의존**: B안은 옛 Retarget Profile 경로 유지. 향후 5.x에서 해당 API 완전 제거 시 재작업 필요 → §6 후속 과제로 분리.
- **다중 op 분기 시 복잡도 증가**: Phase 1에서 보정 필드가 여러 op에 걸치면 그래프 수정량이 ABP당 크게 증가.
- **8개 캐릭터 동시 영향**: `ABP_GenericRetarget` 수정은 7개 캐릭터에 일괄 적용 → Phase 4 검증을 캐릭터별로 샘플링.
- **수동 그래프 편집 오류 가능성**: 핀 누락/오배선 → 컴파일 통과해도 런타임 None 잔존 가능. PIE 스팸 0 확인을 필수 게이트로.
- **에셋 재저장 = 5.8 포맷 베이크**: 저장 시 .uasset이 5.8로 고정(되돌리기는 git). 커밋 전 사용자 승인.

---

## 6. 후속 과제 (별도 분리)

**Override Set 마이그레이션 (5.8 정공법)**: `UpdateRetargetProfile` 매 프레임 패턴을 제거하고, 리타게터 에셋의 Override Set + `Retarget Pose From Mesh` AnimNode 입력 핀 기반으로 재설계. 범위가 크고 Epic 샘플도 미적용 → 5.8 기준선 합격 후 독립 과제로 평가.

---

## 변경 이력
| 날짜 | 버전 | 내용 |
|------|------|------|
| 2026-06-23 | v1.0 | 초안. 원인 확정(op 재명명 + Profiles→Override Sets), B안 매핑(IK Goals→Blend to Source), Phase 1 보정필드 확정 선행, 구현/검증 단계 정의 |
