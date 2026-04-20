#!/bin/bash
# Claude Code PostToolUse hook: Write/Edit 후 에셋 파일 검증 (ExFrameWork 커스터마이즈)
#
# Exit 동작:
#   exit 0 = 성공 또는 경고만 (비차단)
#   exit 1 = 차단 에러 (빌드 브레이킹 이슈)
#
# Input schema (PostToolUse for Write/Edit):
# { "tool_name": "Write", "tool_input": { "file_path": "...", "content": "..." } }

INPUT=$(cat)

# 파일 경로 파싱
if command -v jq >/dev/null 2>&1; then
    FILE_PATH=$(echo "$INPUT" | jq -r '.tool_input.file_path // empty')
else
    FILE_PATH=$(echo "$INPUT" | grep -oE '"file_path"[[:space:]]*:[[:space:]]*"[^"]*"' | sed 's/"file_path"[[:space:]]*:[[:space:]]*"//;s/"$//')
fi

# 경로 구분자 정규화 (Windows 백슬래시 → 슬래시)
FILE_PATH=$(echo "$FILE_PATH" | sed 's|\\|/|g')

WARNINGS=""
ERRORS=""

FILENAME=$(basename "$FILE_PATH")
EXTENSION="${FILENAME##*.}"

# C++ 헤더 파일 검증
if echo "$FILE_PATH" | grep -qE '\.(h|cpp)$'; then

    # generated.h 위치 확인 (헤더 파일만)
    if echo "$FILE_PATH" | grep -q '\.h$' && [ -f "$FILE_PATH" ]; then
        GENERATED_LINE=$(grep -n '\.generated\.h"' "$FILE_PATH" 2>/dev/null | tail -1 | cut -d: -f1)
        TOTAL_INCLUDE_LINES=$(grep -n '^#include' "$FILE_PATH" 2>/dev/null | tail -1 | cut -d: -f1)
        if [ -n "$GENERATED_LINE" ] && [ -n "$TOTAL_INCLUDE_LINES" ]; then
            if [ "$GENERATED_LINE" != "$TOTAL_INCLUDE_LINES" ]; then
                WARNINGS="$WARNINGS\n  HEADER: $FILE_PATH - .generated.h 는 반드시 마지막 #include 여야 합니다 (CLAUDE.md 1.9)"
            fi
        fi
    fi
fi

# ExFrameWork 에셋 네이밍 규칙 검증 (Content 폴더 내)
if echo "$FILE_PATH" | grep -qiE '/Content/'; then
    # Blueprint Class: BP_Ex 접두사 확인
    if echo "$FILENAME" | grep -qiE '^BP_' && ! echo "$FILENAME" | grep -qE '^BP_Ex'; then
        WARNINGS="$WARNINGS\n  NAMING: $FILE_PATH - Blueprint 에셋은 'BP_Ex[Name]' 형식 권장 (CLAUDE.md 1.2)"
    fi
    # Widget Blueprint: WBP_Ex 접두사 확인
    if echo "$FILENAME" | grep -qiE '^WBP_' && ! echo "$FILENAME" | grep -qE '^WBP_Ex'; then
        WARNINGS="$WARNINGS\n  NAMING: $FILE_PATH - Widget Blueprint는 'WBP_Ex[Name]' 형식 권장 (CLAUDE.md 1.2)"
    fi
    # Data Asset: DA_Ex 접두사 확인
    if echo "$FILENAME" | grep -qiE '^DA_' && ! echo "$FILENAME" | grep -qE '^DA_Ex'; then
        WARNINGS="$WARNINGS\n  NAMING: $FILE_PATH - Data Asset은 'DA_Ex[Name]' 형식 권장 (CLAUDE.md 1.2)"
    fi
fi

# JSON 파일 유효성 검증
if echo "$FILE_PATH" | grep -qE '\.json$'; then
    if [ -f "$FILE_PATH" ]; then
        PYTHON_CMD=""
        for cmd in python python3 py; do
            if command -v "$cmd" >/dev/null 2>&1; then
                PYTHON_CMD="$cmd"
                break
            fi
        done
        if [ -n "$PYTHON_CMD" ]; then
            if ! "$PYTHON_CMD" -m json.tool "$FILE_PATH" > /dev/null 2>&1; then
                ERRORS="$ERRORS\n  FORMAT: $FILE_PATH 는 유효한 JSON이 아닙니다 — 계속하기 전에 구문 오류를 수정하세요"
            fi
        fi
    fi
fi

# 경고 출력 (비차단)
if [ -n "$WARNINGS" ]; then
    echo -e "=== ExFrameWork 에셋 검증: 경고 ===$WARNINGS\n==================================\n(경고는 권고사항입니다. 최종 커밋 전에 수정하세요.)" >&2
fi

# 에러 출력 및 차단
if [ -n "$ERRORS" ]; then
    echo -e "=== ExFrameWork 에셋 검증: 오류 (차단) ===$ERRORS\n===========================================\n진행하기 전에 이 오류를 수정하세요." >&2
    exit 1
fi

exit 0
