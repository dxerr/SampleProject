#!/bin/bash
# Claude Code PreToolUse hook: git commit 명령 검증 (ExFrameWork 커스터마이즈)
# Exit 0 = 허용, Exit 2 = 차단 (stderr가 Claude에게 표시됨)
#
# Input schema (PreToolUse for Bash):
# { "tool_name": "Bash", "tool_input": { "command": "git commit -m ..." } }

INPUT=$(cat)

# 명령 파싱 -- jq 사용 가능 시 우선, 없으면 grep 폴백
if command -v jq >/dev/null 2>&1; then
    COMMAND=$(echo "$INPUT" | jq -r '.tool_input.command // empty')
else
    COMMAND=$(echo "$INPUT" | grep -oE '"command"[[:space:]]*:[[:space:]]*"[^"]*"' | sed 's/"command"[[:space:]]*:[[:space:]]*"//;s/"$//')
fi

# git commit 명령만 처리
if ! echo "$COMMAND" | grep -qE '^git[[:space:]]+commit'; then
    exit 0
fi

# 스테이징된 파일 목록 가져오기
STAGED=$(git diff --cached --name-only 2>/dev/null)
if [ -z "$STAGED" ]; then
    exit 0
fi

WARNINGS=""

# ExFrameWork: C++ 파일에서 하드코딩된 게임플레이 값 체크
CPP_FILES=$(echo "$STAGED" | grep -E '\.(cpp|h)$')
if [ -n "$CPP_FILES" ]; then
    while IFS= read -r file; do
        if [ -f "$file" ]; then
            # 매직 넘버 체크 (UPROPERTY로 노출되지 않은 숫자 상수)
            if grep -nE '(damage|health|speed|rate|chance|cost|duration|Damage|Health|Speed|Rate|Chance|Cost|Duration)[[:space:]]*=[[:space:]]*[0-9]+\.?[0-9]*[^f;]' "$file" 2>/dev/null | grep -v "UPROPERTY\|//\|/\*" | grep -q .; then
                WARNINGS="$WARNINGS\nWARN: $file 에 하드코딩된 게임플레이 값이 있을 수 있습니다. UPROPERTY(EditAnywhere) 사용 권장."
            fi
        fi
    done <<< "$CPP_FILES"
fi

# ExFrameWork: TODO/FIXME 미할당 체크
SRC_FILES=$(echo "$STAGED" | grep -E '\.(cpp|h)$')
if [ -n "$SRC_FILES" ]; then
    while IFS= read -r file; do
        if [ -f "$file" ]; then
            if grep -nE '(TODO|FIXME)[^(]' "$file" 2>/dev/null | grep -q .; then
                WARNINGS="$WARNINGS\nSTYLE: $file 에 담당자 없는 TODO/FIXME가 있습니다. TODO(이름) 형식 사용 권장."
            fi
        fi
    done <<< "$SRC_FILES"
fi

# ExFrameWork: Ex 접두사 누락 체크 (새 클래스 선언)
if [ -n "$CPP_FILES" ]; then
    while IFS= read -r file; do
        if [ -f "$file" ] && echo "$file" | grep -q '\.h$'; then
            # class U/A/F/E 선언에서 Ex 접두사 없는 것 체크
            if grep -nE '^[[:space:]]*(class|struct)[[:space:]]+(U|A|F|E|I)[A-Z][a-zA-Z]+[[:space:]]' "$file" 2>/dev/null | grep -vE '(UEx|AEx|FEx|EEx|IEx)' | grep -q .; then
                WARNINGS="$WARNINGS\nNAMING: $file 에 Ex 접두사 없는 클래스/구조체가 있을 수 있습니다. (규칙: UEx..., AEx..., FEx...)"
            fi
        fi
    done <<< "$CPP_FILES"
fi

# 경고 출력 (비차단) 후 커밋 허용
if [ -n "$WARNINGS" ]; then
    echo -e "=== ExFrameWork 커밋 검증 경고 ===$WARNINGS\n================================" >&2
fi

exit 0
