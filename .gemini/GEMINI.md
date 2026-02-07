# ExProject Workspace 규칙

## ExFrameWork 가이드라인
ExProject 프로젝트 작업 시 반드시 다음 규칙을 준수하고 숙지해야 합니다:

1. **가이드라인 파일 필수 참조**: 작업 시작 전 `c:\Dev\ExProject\Md\ExFrameWork_Guidelines.md` 파일을 반드시 읽고 숙지할 것
2. **명명 규칙**: 클래스/구조체 접두사 "Ex" 사용 필수 (UEx, AEx, FEx, EEx, IEx)
3. **에셋 명명**: BP_Ex[Name], WBP_Ex[Name], DA_Ex[Name], DT_Ex[Name]
4. **Boolean 변수**: b 접두사 필수 (예: bIsReady)
5. **생성자 규칙**: NewObject 금지 → CreateDefaultSubobject 사용
6. **UE5 베스트 프랙티스**: TObjectPtr<T> 사용 권장, LogExFrameWork 로그 카테고리 사용
7. **데이터 드리븐**: 하드코딩 배제, UPROPERTY로 에디터 노출
8. **네트워킹**: 서버 권한(HasAuthority) 고려 필수
