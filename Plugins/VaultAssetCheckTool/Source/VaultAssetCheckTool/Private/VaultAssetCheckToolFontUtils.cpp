// Copyright Epic Games, Inc. All Rights Reserved.

#include "VaultAssetCheckToolFontUtils.h"

#include "Dom/JsonObject.h"

// 폰트 글리프 커버리지 정적 분석용 FreeType. FontFaceData raw 바이트를 직접 로드해
// charmap에 매핑된 유니코드 코드포인트를 게임 실행 없이 순회 수집한다.
// 인클루드 가드 패턴은 SlateCore/Private/Fonts/FontCacheFreeType.h 를 따른다.
#ifndef WITH_FREETYPE
	#define WITH_FREETYPE	0
#endif // WITH_FREETYPE

#if PLATFORM_COMPILER_HAS_GENERIC_KEYWORD
	#define generic __identifier(generic)
#endif // PLATFORM_COMPILER_HAS_GENERIC_KEYWORD

#if WITH_FREETYPE
	THIRD_PARTY_INCLUDES_START
	#include "ft2build.h"
	#include FT_FREETYPE_H
	THIRD_PARTY_INCLUDES_END
#endif // WITH_FREETYPE

#if PLATFORM_COMPILER_HAS_GENERIC_KEYWORD
	#undef generic
#endif // PLATFORM_COMPILER_HAS_GENERIC_KEYWORD

namespace VaultAssetCheckToolReport
{
    void ExtractGlyphCoverage(const TArray<uint8>& FontData, const TSharedPtr<FJsonObject>& FaceObject)
    {
#if WITH_FREETYPE
        if (FontData.Num() == 0 || !FaceObject.IsValid())
        {
            return;
        }

        FT_Library Library = nullptr;
        if (FT_Init_FreeType(&Library) != 0)
        {
            return;
        }

        FT_Face Face = nullptr;
        const FT_Error Error = FT_New_Memory_Face(
            Library, FontData.GetData(), static_cast<FT_Long>(FontData.Num()), 0, &Face);
        if (Error != 0 || Face == nullptr)
        {
            FT_Done_FreeType(Library);
            return;
        }

        FaceObject->SetNumberField(TEXT("glyphCount"), (double)Face->num_glyphs);

        // 분류 대상 유니코드 블록 표 (코드포인트 오름차순, 비중첩).
        // 게임 폰트에서 의미 있는 스크립트/기호 블록을 폭넓게 커버하고, 표에 없는 코드포인트만 Other로 모은다.
        struct FUnicodeBlock { uint32 Low; uint32 High; const TCHAR* Name; };
        static const FUnicodeBlock UnicodeBlocks[] =
        {
            { 0x0000,  0x007F,  TEXT("Basic Latin") },
            { 0x0080,  0x00FF,  TEXT("Latin-1 Supplement") },
            { 0x0100,  0x017F,  TEXT("Latin Extended-A") },
            { 0x0180,  0x024F,  TEXT("Latin Extended-B") },
            { 0x0370,  0x03FF,  TEXT("Greek") },
            { 0x0400,  0x04FF,  TEXT("Cyrillic") },
            { 0x0590,  0x05FF,  TEXT("Hebrew") },
            { 0x0600,  0x06FF,  TEXT("Arabic") },
            { 0x0900,  0x097F,  TEXT("Devanagari") },
            { 0x0E00,  0x0E7F,  TEXT("Thai") },
            { 0x1100,  0x11FF,  TEXT("Hangul Jamo") },
            { 0x1E00,  0x1EFF,  TEXT("Latin Extended Additional") }, // 베트남어 등
            { 0x2000,  0x206F,  TEXT("General Punctuation") },
            { 0x20A0,  0x20CF,  TEXT("Currency Symbols") },
            { 0x2190,  0x21FF,  TEXT("Arrows") },
            { 0x2200,  0x22FF,  TEXT("Mathematical Operators") },
            { 0x2460,  0x24FF,  TEXT("Enclosed Alphanumerics") },
            { 0x25A0,  0x25FF,  TEXT("Geometric Shapes") },
            { 0x2600,  0x26FF,  TEXT("Misc Symbols") },
            { 0x2700,  0x27BF,  TEXT("Dingbats") },
            { 0x3000,  0x303F,  TEXT("CJK Symbols and Punctuation") },
            { 0x3040,  0x309F,  TEXT("Hiragana") },
            { 0x30A0,  0x30FF,  TEXT("Katakana") },
            { 0x3130,  0x318F,  TEXT("Hangul Compatibility Jamo") },
            { 0x3400,  0x4DBF,  TEXT("CJK Ext-A") },
            { 0x4E00,  0x9FFF,  TEXT("CJK Unified") },
            { 0xA960,  0xA97F,  TEXT("Hangul Jamo Extended-A") },
            { 0xAC00,  0xD7A3,  TEXT("Hangul Syllables") },
            { 0xD7B0,  0xD7FF,  TEXT("Hangul Jamo Extended-B") },
            { 0xF900,  0xFAFF,  TEXT("CJK Compatibility Ideographs") },
            { 0xFF00,  0xFFEF,  TEXT("Halfwidth and Fullwidth Forms") },
            { 0x20000, 0x2EBEF, TEXT("CJK Ext-B+") }, // 보충면(SMP) 확장 한자
        };
        const int32 BlockNum = UE_ARRAY_COUNT(UnicodeBlocks);

        int64 CoveredCodepoints = 0;
        int64 OtherCount = 0;
        TArray<int64> BlockCounts;
        BlockCounts.AddZeroed(BlockNum);

        // 유니코드 charmap을 선택해야 FT_Get_First/Next_Char가 유니코드 코드포인트를 반환한다.
        if (FT_Select_Charmap(Face, FT_ENCODING_UNICODE) == 0)
        {
            FT_UInt GlyphIndex = 0;
            FT_ULong CharCode = FT_Get_First_Char(Face, &GlyphIndex);
            while (GlyphIndex != 0)
            {
                ++CoveredCodepoints;

                bool bMatched = false;
                for (int32 i = 0; i < BlockNum; ++i)
                {
                    if (CharCode >= UnicodeBlocks[i].Low && CharCode <= UnicodeBlocks[i].High)
                    {
                        ++BlockCounts[i];
                        bMatched = true;
                        break;
                    }
                }
                if (!bMatched)
                {
                    ++OtherCount;
                }

                CharCode = FT_Get_Next_Char(Face, CharCode, &GlyphIndex);
            }
        }

        FaceObject->SetNumberField(TEXT("coveredCodepoints"), (double)CoveredCodepoints);

        // scriptCoverage: 커버하는 블록을 "이름(글자수)" 형태로 상세 나열 (count > 0 인 블록만, 코드포인트 오름차순)
        TArray<FString> CoveredScripts;
        for (int32 i = 0; i < BlockNum; ++i)
        {
            if (BlockCounts[i] > 0)
            {
                CoveredScripts.Add(FString::Printf(TEXT("%s(%lld)"), UnicodeBlocks[i].Name, BlockCounts[i]));
            }
        }
        if (OtherCount > 0)
        {
            CoveredScripts.Add(FString::Printf(TEXT("Other(%lld)"), OtherCount));
        }
        FaceObject->SetStringField(TEXT("scriptCoverage"), FString::Join(CoveredScripts, TEXT(", ")));

        FT_Done_Face(Face);
        FT_Done_FreeType(Library);
#endif // WITH_FREETYPE
    }
}
