# Sentry 토큰 노출로 인한 Git Push 거부 문제 해결

## ❌ 오류 증상 (Git)
`git push` 시도 시 다음과 같은 에러 발생과 함께 Push가 거부됨 (Rejected):
```
remote: —— Sentry Personal Token —————————————————————————————        
remote: locations:        
remote:   - commit: <commit_hash>        
remote:     path: sentry.properties:4        
remote:  (?) To push, remove secret from commit(s) or follow this URL to allow the secret.        
remote:  https://github.com/dxerr/SampleProject/security/secret-scanning/unblock-secret/...
! [remote rejected] RunnerV3 -> RunnerV3 (push declined due to repository rule violations)
```

## 🔍 원인 분석
- `sentry.properties` 파일의 4번 라인에 Sentry 인증을 위한 `auth.token` 값이 포함되어 있었습니다.
- GitHub의 **Secret Scanning** 기능이 해당 토큰(민감한 정보)을 감지하여, 보안 유출을 방지하기 위해 원격 저장소로의 Push를 자동으로 차단했습니다.

## ✅ 해결 방식
1. **토큰 정보 및 Git 추적 제거**: `sentry.properties` 파일 내의 `auth.token` 라인을 제거하고, `git rm --cached` 명령을 사용하여 Git 저장소의 추적 대상에서 제외하였습니다.
2. **템플릿 파일 생성**: 각 PC에서 설정을 개별적으로 관리할 수 있도록 `sentry.properties.template` 파일을 생성하여 Git에 추가하였습니다.
3. **Git Ignore 설정**: 로컬의 `sentry.properties` 파일이 다시 Commit되지 않도록 `.gitignore`에 등록하였습니다.
4. **Push 완료**: 보인 및 충돌 문제가 해결된 템플릿 구조를 원격 저장소에 Push 완료하였습니다.

## 📋 타 PC에서의 후속 조치
1. `sentry.properties.template` 파일을 복사하여 `sentry.properties` 파일을 생성합니다.
2. 각자의 환경에 맞는 `auth.token` 값을 입력합니다. (또는 시스템 환경 변수 `SENTRY_AUTH_TOKEN`을 사용합니다.)


> [!IMPORTANT]
> 개인 인증 토큰(Personal Access Token)은 절대로 Git 환경에 노출되어서는 안 됩니다. 로컬 환경에서만 관리하거나 환경 변수를 사용하는 것이 안전합니다.
