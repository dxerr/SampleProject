#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace VaultAssetCheckToolReport
{
    inline FString GetGeneratedAt()
    {
        return FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));
    }

    inline FString GetProjectVersion()
    {
        FString ProjectVersion;
        if (GConfig && GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), ProjectVersion, GGameIni))
        {
            if (!ProjectVersion.IsEmpty())
            {
                return ProjectVersion;
            }
        }
        return FEngineVersion::Current().ToString();
    }

    inline void FillCommonMeta(TSharedPtr<FJsonObject> RootObject, const FString& Category, const TArray<TSharedPtr<FJsonValue>>* Notices = nullptr)
    {
        RootObject->SetStringField(TEXT("Category"), Category);
        RootObject->SetStringField(TEXT("ProductName"), FApp::GetProjectName());
        RootObject->SetStringField(TEXT("Version"), GetProjectVersion());
        RootObject->SetStringField(TEXT("GeneratedAt"), GetGeneratedAt());
        if (Notices)
        {
            RootObject->SetArrayField(TEXT("Notices"), *Notices);
        }
        else
        {
            RootObject->SetArrayField(TEXT("Notices"), TArray<TSharedPtr<FJsonValue>>());
        }
    }

    inline TSharedPtr<FJsonObject> MakeTable(
        const FString& Name,
        const FString& Summary,
        const TArray<TSharedPtr<FJsonValue>>& Rows)
    {
        TArray<TSharedPtr<FJsonValue>> Headers;
        TSet<FString> SeenHeaders;

        auto AddHeader = [&Headers, &SeenHeaders](const FString& Key)
        {
            if (SeenHeaders.Contains(Key))
            {
                return;
            }
            SeenHeaders.Add(Key);
            Headers.Add(MakeShareable(new FJsonValueString(Key)));
        };

        TFunction<void(const FString&, const TSharedPtr<FJsonValue>&)> CollectHeaders;
        CollectHeaders = [&AddHeader, &CollectHeaders](const FString& Prefix, const TSharedPtr<FJsonValue>& Value)
        {
            if (!Value.IsValid())
            {
                return;
            }

            if (Value->Type == EJson::Object)
            {
                const TSharedPtr<FJsonObject> Obj = Value->AsObject();
                if (!Obj.IsValid())
                {
                    return;
                }
                for (const auto& Pair : Obj->Values)
                {
                    // UE5.8: 삼항 연산자의 양 피연산자 타입 불일치(C2446) 회피 — FString 변수로 명시 구성
                    FString NextKey;
                    if (Prefix.IsEmpty())
                    {
                        NextKey = Pair.Key;
                    }
                    else
                    {
                        NextKey = Prefix + TEXT(".") + Pair.Key;
                    }
                    CollectHeaders(NextKey, Pair.Value);
                }
                return;
            }

            if (Value->Type == EJson::Array)
            {
                const TArray<TSharedPtr<FJsonValue>>& Arr = Value->AsArray();
                if (Arr.Num() == 0)
                {
                    AddHeader(Prefix);
                    return;
                }
                for (const TSharedPtr<FJsonValue>& Item : Arr)
                {
                    CollectHeaders(Prefix, Item);
                }
                return;
            }

            AddHeader(Prefix);
        };

        for (const TSharedPtr<FJsonValue>& RowValue : Rows)
        {
            CollectHeaders(TEXT(""), RowValue);
        }

        TSharedPtr<FJsonObject> TableObject = MakeShareable(new FJsonObject);
        TableObject->SetStringField(TEXT("Name"), Name);
        TableObject->SetStringField(TEXT("Summary"), Summary);
        TableObject->SetArrayField(TEXT("Headers"), Headers);
        TableObject->SetArrayField(TEXT("Rows"), Rows);
        return TableObject;
    }

    inline FString MakeFileName(const FString& Workbook, const FString& Sheet)
    {
        return FString::Printf(TEXT("[%s]%s.json"), *Workbook, *Sheet);
    }

    inline FString ResolveBaseDir(const FString& OutputPath)
    {
        if (OutputPath.IsEmpty())
        {
            return FPaths::ProjectSavedDir();
        }

        if (FPaths::DirectoryExists(OutputPath))
        {
            return OutputPath;
        }

        FString DirOnly = FPaths::GetPath(OutputPath);
        return DirOnly.IsEmpty() ? FPaths::ProjectSavedDir() : DirOnly;
    }

    inline FString BuildOutputPath(const FString& OutputPath, const FString& Workbook, const FString& Sheet)
    {
        const FString BaseDir = ResolveBaseDir(OutputPath);
        return FPaths::Combine(BaseDir, MakeFileName(Workbook, Sheet));
    }

    inline bool SaveReportJson(const FString& OutputPath, const FString& Workbook, const FString& Sheet, const TSharedPtr<FJsonObject>& RootObject)
    {
        FString JsonString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
        FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
        const FString FinalOutputPath = BuildOutputPath(OutputPath, Workbook, Sheet);
        return FFileHelper::SaveStringToFile(JsonString, *FinalOutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    inline TSharedPtr<FJsonObject> BuildSingleTableReport(
        const FString& Category,
        const FString& TableName,
        const FString& Summary,
        const TArray<TSharedPtr<FJsonValue>>& Rows,
        const TArray<TSharedPtr<FJsonValue>>* Notices = nullptr)
    {
        TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
        FillCommonMeta(RootObject, Category, Notices);

        TArray<TSharedPtr<FJsonValue>> TablesArray;
        TablesArray.Add(MakeShareable(new FJsonValueObject(MakeTable(TableName, Summary, Rows))));
        RootObject->SetArrayField(TEXT("Tables"), TablesArray);
        return RootObject;
    }

    inline TArray<TSharedPtr<FJsonValue>> MakeNoticeList()
    {
        return TArray<TSharedPtr<FJsonValue>>();
    }

    inline void AddNotice(
        TArray<TSharedPtr<FJsonValue>>& Notices,
        const FString& Item,
        const FString& Description)
    {
        TSharedPtr<FJsonObject> NoticeObject = MakeShareable(new FJsonObject);
        NoticeObject->SetStringField(TEXT("Item"), Item);
        NoticeObject->SetStringField(TEXT("Description"), Description);
        Notices.Add(MakeShareable(new FJsonValueObject(NoticeObject)));
    }
}
