---
description: ExProject 프로젝트 시작 전 필수 가이드라인 확인
---

# ExProject 프로젝트 시작 워크플로우

이 워크플로우는 ExProject 작업을 시작하기 전에 반드시 실행해야 합니다.

## 필수 단계

1. **ExFrameWork_Guidelines.md 파일 확인**
   - 경로: `c:\Dev\ExProject\Md\ExFrameWork_Guidelines.md`
   - 이 파일에는 프로젝트의 모든 코딩 규칙과 컨벤션이 정의되어 있습니다.

// turbo
2. **가이드라인 파일 읽기**
   ```
   view_file AbsolutePath=c:\Dev\ExProject\Md\ExFrameWork_Guidelines.md
   ```

## 주요 규칙 요약

### 명명 규칙
- 클래스/구조체 접두사: **"Ex"** 사용 필수
  - UObject: `UEx...`, AActor: `AEx...`, 구조체: `FEx...`, Enum: `EEx...`, Interface: `IEx...`
- Boolean 변수: `b` 접두사 필수 (예: `bIsReady`)
- 에셋: `BP_Ex[Name]`, `WBP_Ex[Name]`, `DA_Ex[Name]`, `DT_Ex[Name]`

### 코딩 원칙
- 접근 지정자: 기본적으로 접근 용이하게 작성
- 데이터 드리븐 설계: 하드코딩 배제, UPROPERTY 노출
- 단일 책임 원칙 준수
- 생성자 내 NewObject 금지 (CreateDefaultSubobject 사용)

### 폴더 구조
- `Struct/`: USTRUCT 정의
- `Data/`: UDataAsset 및 관련 구조체
- `Util/`: 유틸리티 함수 (Math, Find 등)

### UE5 베스트 프랙티스
- TObjectPtr<T> 사용 권장
- LogExFrameWork 로그 카테고리 사용
- 서버 권한(HasAuthority) 고려 필수

### 문서 관리
- `Architecture/`: 시스템 아키텍처, 설계 문서
- `Guides/`: 사용 가이드, 튜토리얼
- `Bug/`: 크리티컬 이슈 및 해결 방법
- `Legacy/`: 참고용 오래된 문서
