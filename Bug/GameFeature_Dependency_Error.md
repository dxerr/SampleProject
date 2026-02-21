# GameFeature ErrorWaitingForDependencies 문제

**키워드**: `ErrorWaitingForDependencies`, `GameFeature`, `BuiltInInitialFeatureState`, `GameFeatureData`, `CheatManager`, `AddCheats`
**발생 날짜**: 2026-02-21
**해결 여부**: ✅ 해결됨

---

## 증상

- GameFeature 플러그인에 `GameFeatureAction_AddCheats` Action이 추가되어 있음에도 `UFUNCTION(Exec)` 명령이 콘솔에서 인식되지 않음 (`Command not recognized`)
- `GetAll CheatManagerExtension Name` 시 해당 Extension 인스턴스가 목록에 없음
- GameFeatureData 에셋 에디터에서 **현재 상태**가 `ErrorWaitingForDependencies`로 표시됨
- Output Log에 GameFeature 관련 로그가 전혀 출력되지 않음

---

## 원인 분석

### 핵심 원인: 의존 GameFeature 플러그인에 `GameFeatureData (.uasset)` 없거나, `BuiltInInitialFeatureState`가 낮은 경우

엔진 동작 흐름:
```
ExRunnerPlay 활성화 시도
→ WaitingForDependencies: 의존성 플러그인(ExCore 등) 상태 확인
→ FindOrCreatePluginDependencyStateMachines 호출
→ ResolvePluginDependency → GetGameFeaturePluginURL
→ Plugins/GameFeatures/ 경로의 플러그인은 모두 GameFeature로 인식
→ 해당 플러그인의 StateMachine이 Registered~Active 범위에 없으면
→ ErrorWaitingForDependencies 오류!
```

### 조건

| 상황 | 결과 |
|---|---|
| 의존 플러그인에 `GameFeatureData (.uasset)` 없음 | Registered 단계 실패 → ErrorWaitingForDependencies |
| 의존 플러그인 `BuiltInInitialFeatureState: "Registered"` && 타이밍 문제 | ErrorWaitingForDependencies 가능성 |
| `Plugins/GameFeatures/` 경로의 플러그인은 무조건 GFP로 인식됨 | 의존성 추가 시 GameFeatureData 생성 필수 |

---

## 해결 방법

### Step 1: 의존 GFP에 GameFeatureData 에셋 생성

에디터 Content Browser에서 의존 GFP의 Content 루트에 `PluginName.uasset` (GameFeatureData 클래스) 생성.

또는 MCP Python으로 생성:
```python
import unreal
gfd_class = unreal.load_class(None, '/Script/GameFeatures.GameFeatureData')
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
new_asset = asset_tools.create_asset('ExCore', '/ExCore', gfd_class, None)
unreal.EditorAssetLibrary.save_asset(new_asset.get_path_name(), only_if_is_dirty=False)
```

### Step 2: 의존 GFP의 `BuiltInInitialFeatureState` 설정

의존되는 플러그인의 `.uplugin` 파일:
```diff
- "BuiltInInitialFeatureState": "Registered"
+ "BuiltInInitialFeatureState": "Active"
```

---

## 체크리스트 (자주 발생하므로 신규 GFP 추가 시 반드시 확인)

- [ ] `Plugins/GameFeatures/` 하위의 모든 플러그인에 **동명의 `GameFeatureData (.uasset)`** 생성 여부
- [ ] 다른 GFP의 의존성이 되는 GFP의 `BuiltInInitialFeatureState`가 `"Active"`인지 확인
- [ ] 새 GFP 추가 시 `uplugin` Plugins 배열에 기재된 다른 GFP들이 에디터에서 Active 상태인지 확인
- [ ] GameFeatureData 에셋을 새로 만들었다면 **에디터 재시작** 후 적용 여부 확인
