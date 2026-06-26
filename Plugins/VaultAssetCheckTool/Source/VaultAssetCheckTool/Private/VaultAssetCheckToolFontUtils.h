#pragma once

#include "CoreMinimal.h"

class FJsonObject;

namespace VaultAssetCheckToolReport
{
    /**
     * FontFaceData raw 바이트를 FreeType에 직접 로드해 글리프 커버리지를 추출하고,
     * FaceObject에 다음 컬럼을 채운다 (게임 실행 없이 에셋만으로 정적 분석).
     *   - glyphCount        : 폰트 총 글리프 수 (FT_Face::num_glyphs)
     *   - coveredCodepoints : charmap에 매핑된 유니코드 코드포인트 총 개수
     *   - scriptCoverage    : 폰트가 커버하는 유니코드 블록을 "이름(글자수)"로 상세 나열한 문자열
     *                         (예: "Basic Latin(95), Hangul Syllables(2350), CJK Unified(4888)")
     *
     * WITH_FREETYPE이 꺼진 빌드에서는 아무 일도 하지 않는다.
     */
    void ExtractGlyphCoverage(const TArray<uint8>& FontData, const TSharedPtr<FJsonObject>& FaceObject);
}
