#!/bin/bash
# Claude Code PreToolUse hook: git push 명령 검증 (ExFrameWork)
# 보호된 브랜치로의 푸시 시 경고
# Exit 0 = 허용, Exit 2 = 차단
#
# Input schema (PreToolUse for Bash):
# { "tool_name": "Bash", "tool_input": { "command": "git push origin main" } }

INPUT=$(cat)

# 명령 파싱
if command -v jq >/dev/null 2>&1; then
    COMMAND=$(echo "$INPUT" | jq -r '.tool_input.command // empty')
else
    COMMAND=$(echo "$INPUT" | grep -oE '"command"[[:space:]]*:[[:space:]]*"[^"]*"' | sed 's/"command"[[:space:]]*:[[:space:]]*"//;s/"$//')
fi

# git push 명령만 처리
if ! echo "$COMMAND" | grep -qE '^git[[:space:]]+push'; then
    exit 0
fi

CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD 2>/dev/null)
MATCHED_BRANCH=""

# 보호된 브랜치 확인
for branch in main master develop; do
    if [ "$CURRENT_BRANCH" = "$branch" ]; then
        MATCHED_BRANCH="$branch"
        break
    fi
    if echo "$COMMAND" | grep -qE "[[:space:]]${branch}([[:space:]]|$)"; then
        MATCHED_BRANCH="$branch"
        break
    fi
done

if [ -n "$MATCHED_BRANCH" ]; then
    echo "ExFrameWork: 보호된 브랜치 '$MATCHED_BRANCH' 로의 푸시가 감지되었습니다." >&2
    echo "체크리스트: 빌드 성공, 컴파일 에러 없음, S1/S2 버그 없음 확인 후 진행하세요." >&2
    echo "CLAUDE.md 3.2: 사용자의 명시적 요청 후에만 커밋/푸시를 수행합니다." >&2
    # 경고만 하고 허용. 차단하려면 아래 주석 해제:
    # echo "BLOCKED: $MATCHED_BRANCH 브랜치 푸시는 사용자 승인이 필요합니다." >&2
    # exit 2
fi

exit 0
