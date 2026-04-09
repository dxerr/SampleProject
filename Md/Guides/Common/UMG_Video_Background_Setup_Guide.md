# UMG 배경 영상(MediaPlayer) 셋업 가이드

## 개요
메뉴 화면 등에서 루프 동영상을 UMG 위젯의 배경으로 재생하는 방법을 정리한다.

**에셋 연결 흐름:**
```
MP4 파일 → FileMediaSource → MediaPlayer → MediaTexture → UI Material → UMG Image 위젯
```

**핵심 원칙:**
- Image 위젯은 영상을 **"표시"** 하는 역할 (Material 경유)
- MediaPlayer는 영상을 **"재생"** 하는 역할
- 이 둘은 Blueprint에서 직접 연결하지 않는다 — 에셋 레벨에서 이미 연결되어 있다

---

## 1. 영상 파일 권장 스펙

| 항목 | 권장값 | 비고 |
|------|--------|------|
| 포맷 | H.264 MP4 (Main Profile) | Android/Windows 모두 하드웨어 디코딩 지원 |
| 해상도 | 1280×720 (모바일) / 1920×1080 (PC) | Galaxy S22 타겟 기준 720p 권장 |
| 프레임레이트 | 30fps | |
| 비트레이트 | 8~10 Mbps | 품질/용량 트레이드오프 |
| 길이 | 15~20초 루프 | 시작/끝 프레임이 자연스럽게 이어지도록 편집 |
| 오디오 | 없음 (mute) | BGM은 MetaSounds로 별도 관리 |

> **피해야 할 포맷:** H.265/HEVC (일부 Android 미지원), VP9/WebM (UE5 공식 지원 불안정), ProRes (PC 전용)

---

## 2. 에셋 생성 (에디터)

Content Browser에서 총 3개 에셋을 생성한다.

### 2-1. File Media Source
1. Content Browser → 우클릭 → **Media → File Media Source**
2. 이름: `MS_MenuBackground`
3. 디테일 패널 → `File Path`에 MP4 파일 경로 지정

### 2-2. Media Player + Media Texture (동시 생성)
1. Content Browser → 우클릭 → **Media → Media Player**
2. 이름: `MP_MenuBackground`
3. 생성 다이얼로그에서 **"Video output MediaTexture asset"** 반드시 체크
4. `MP_MenuBackground_Video` (MediaTexture)가 자동으로 함께 생성됨

> **참고:** MediaTexture를 별도로 만들 필요 없다. MediaPlayer 생성 시 체크박스 하나로 자동 생성된다.

---

## 3. UI Material 생성

### 3-1. Material 생성
1. Content Browser → 우클릭 → **Material** → `M_UI_VideoBackground`

### 3-2. Material 디테일 속성 설정
- **Material Domain:** `User Interface`
- **Blend Mode:** `Translucent` (페이드 인/아웃 필요 시) 또는 `Opaque` (단순 표시)

### 3-3. Material 노드 구성

1. Material 그래프에서 우클릭 → **TextureSampleParameter2D** 노드 추가
2. 노드 디테일에서 설정:
   - **Parameter Name:** `VideoTexture`
   - **Texture:** `MP_MenuBackground_Video` (MediaTexture 에셋 선택)
   - **Sampler Type:** `Color` ★★★ (아래 주의사항 필독)
   - **Sampler Source:** `From texture asset`
3. 노드 출력 연결:
   - **RGB** → `Final Color` 핀
   - **A** → `Opacity` 핀 (Blend Mode가 Translucent일 때)

4. (선택) ScalarParameter 노드 추가:
   - **Parameter Name:** `Opacity`, **Default:** 1.0
   - Multiply 노드를 거쳐 Opacity 핀에 연결
   - 런타임에 `SetScalarParameterValue("Opacity", Value)`로 페이드 제어 가능

**완성된 그래프:**
```
TextureSampleParameter2D ("VideoTexture")
  Sampler Type: Color
  Sampler Source: From texture asset
  Texture: MP_MenuBackground_Video
    │
    ├─ RGB ────────────────→ [Final Color]
    │
    └─ A ─── [Multiply] ──→ [Opacity]
                  │
     ScalarParameter ───┘
     "Opacity" (Default: 1.0)
```

### ★ Sampler Type 주의사항 (트러블슈팅)

**반드시 `Color`로 설정한다. `External`이 아니다.**

| 설정 | 결과 |
|------|------|
| `Color` | ✅ PC/에디터 정상, Android 빌드 시 UE5가 자동으로 External 전환 |
| `External` | ❌ SM6(PC/에디터)에서 컴파일 에러 발생 |

- `Sampler Source`가 `From texture asset`이면 UE5가 **플랫폼별로 자동 전환**한다
- MediaTexture 에셋 자체가 Android 빌드 시 External OES로 처리하라는 정보를 내부에 보유
- Material 노드에서 `External`로 강제 지정하면 다음 에러 발생:
  ```
  [SM6] (Node TextureSample) Sampler type is External, should be Color 
  for /ExRunnerPlay/Movie/MP_MenuBackground_Video.MP_MenuBackground_Video
  ```

---

## 4. UMG 위젯 설정

### 4-1. Image 위젯 배치
1. 위젯 디자이너에서 `Image` 위젯 추가 → 이름: `IMG_Background`
2. **Anchor:** Full Screen Stretch (좌상단+우하단 고정)
3. **Appearance → Brush → Image:** `M_UI_VideoBackground` (Material 선택)
4. Brush Tint: White (1,1,1,1)로 유지

### 4-2. Hierarchy 순서 (중요!)

UMG에서 **Hierarchy 목록의 아래쪽 위젯이 화면에서 앞(위)에 렌더링**된다.
배경 Image는 반드시 **Hierarchy 최상단(첫 번째 자식)**에 배치해야 가장 뒤에 렌더링된다.

```
[Overlay / Canvas Panel]
  ├─ IMG_Background    ← 첫 번째 = 가장 뒤에 렌더링 (배경)
  ├─ PlayButton        ← 그 위에 렌더링
  ├─ MenuStack
  └─ OtherUI           ← 마지막 = 가장 앞에 렌더링
```

**흔한 실수:** 배경 Image를 Hierarchy 맨 아래에 두면 버튼을 완전히 가려버린다.
**해결:** 디자이너의 Hierarchy 패널에서 `IMG_Background`를 드래그하여 맨 위로 이동

> **Render Opacity**는 투명도 조절 속성이다. 렌더링 순서(Z-Order)와는 무관하며,
> Z-Order를 바꾸려면 반드시 Hierarchy 위치를 변경해야 한다.

---

## 5. Blueprint 재생 코드

### 5-1. 변수 추가

위젯 Blueprint의 Variables에서 다음 2개 변수를 추가한다.

| 변수명 | 타입 | Default Value |
|--------|------|---------------|
| `MediaPlayer` | Media Player (Object Reference) | `MP_MenuBackground` |
| `MediaSource` | File Media Source (Object Reference) | `MS_MenuBackground` |

> 변수 생성 후 **Compile** → Default Value 드롭다운에서 에셋 선택 가능

### ★ Image 위젯 Cast 금지 (트러블슈팅)

**Image 위젯 변수를 `Cast To MediaPlayer` 하면 안 된다.**

Image(`UImage`)와 MediaPlayer(`UMediaPlayer`)는 완전히 다른 클래스이며,
Cast는 항상 실패한다. 컴파일러 경고:
```
'Media Player' does not inherit from 'Image' (Cast To MediaPlayer would always fail).
```

MediaPlayer는 위젯이 아닌 **독립 에셋**이므로, 별도 Object Reference 변수로 참조한다.

### 5-2. Event Graph (Blueprint)

**재생 시작 (Event Pre Construct 또는 Event Construct):**
```
Event Pre Construct
  → MediaPlayer (Get) → Set Looping (Looping: ✓)
  → MediaPlayer (Get) → Open Source (Media Source: MediaSource 변수)
  → MediaPlayer (Get) → Set Play On Open (Play On Open: ✓)
```

**재생 종료 (Event Destruct):**
```
Event Destruct
  → MediaPlayer (Get) → Close
```

> **Close 호출이 중요하다.** 호출하지 않으면 위젯이 제거되어도 
> MediaPlayer가 백그라운드에서 디코딩을 계속하여 메모리 누수가 발생한다.

### 5-3. C++ 구현 시

```cpp
// 헤더
UPROPERTY(EditDefaultsOnly, Category = "Media")
TObjectPtr<UMediaPlayer> MediaPlayer;

UPROPERTY(EditDefaultsOnly, Category = "Media")
TObjectPtr<UFileMediaSource> MediaSource;
```

```cpp
// NativeConstruct
void UMyMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (MediaPlayer && MediaSource)
    {
        MediaPlayer->PlayOnOpen = true;
        MediaPlayer->SetLooping(true);
        MediaPlayer->OpenSource(MediaSource);
    }
}

// NativeDestruct — 리소스 해제 필수
void UMyMenuWidget::NativeDestruct()
{
    if (MediaPlayer)
    {
        MediaPlayer->Close();
    }
    Super::NativeDestruct();
}
```

`.Build.cs`에 모듈 의존성 추가:
```csharp
PublicDependencyModuleNames.Add("MediaAssets");
```

---

## 6. 에셋 연결 관계 전체 요약

```
[MP4 파일]
    ↓ (File Path 참조)
[FileMediaSource: MS_MenuBackground]
    ↓ (OpenSource 호출로 연결)
[MediaPlayer: MP_MenuBackground]        ← Blueprint에서 제어 (Play/Stop/Close)
    ↓ (자동 비디오 출력)
[MediaTexture: MP_MenuBackground_Video]  ← 에셋 레벨에서 자동 연결됨
    ↓ (TextureSample 노드에서 샘플링)
[Material: M_UI_VideoBackground]         ← Domain: User Interface, Sampler: Color
    ↓ (Brush로 설정)
[UMG Image: IMG_Background]              ← Hierarchy 최상단 배치
```

---

## 7. Android 빌드 체크리스트

- [ ] Project Settings → Android → Media Player 기본 플레이어가 `ElectraPlayer`인지 확인
- [ ] Material Sampler Type이 `Color`인지 확인 (`External` 아님)
- [ ] MP4 코덱이 H.264 Main Profile인지 확인 (H.265 사용 금지)
- [ ] 영상 에셋이 GameFeature 플러그인 외부(`/Game/`)에 있을 경우,
      `DefaultGame.ini`의 `DirectoriesToAlwaysCook`에 해당 경로 추가
- [ ] `.Build.cs`에 `"MediaAssets"` 모듈 의존성 추가 확인
- [ ] 위젯 Destruct에서 `MediaPlayer->Close()` 호출 확인 (메모리 누수 방지)

---

## 변경 이력

| 날짜 | 내용 |
|------|------|
| 2026-04-08 | 초안 작성 — 에셋 생성, Material 셋업, Sampler Type 트러블슈팅, Hierarchy 순서, BP 재생 코드 |
