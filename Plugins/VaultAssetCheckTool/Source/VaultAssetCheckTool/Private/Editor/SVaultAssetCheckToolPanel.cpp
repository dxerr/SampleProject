// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/SVaultAssetCheckToolPanel.h"
#include "Editor/VaultAssetCheckToolReportTask.h"
#include "Editor/VaultAssetCheckToolReportFactory.h"
#include "VaultAssetCheckTool.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "EditorStyleSet.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"

#define LOCTEXT_NAMESPACE "VaultAssetCheckToolPanel"

namespace
{
	// 위젯 반영 스로틀 주기(초) — 항목별 로그가 폭주해도 위젯 갱신은 이 간격으로 제한.
	constexpr double kLogFlushIntervalSeconds = 0.1;
	// 로그 버퍼 상한(문자) — 초과 시 앞부분을 잘라 위젯 갱신 비용을 일정하게 유지.
	constexpr int32 kLogMaxChars = 200000;
}

TArray<SVaultAssetCheckToolPanel::FReportEntry> SVaultAssetCheckToolPanel::GetReportEntries()
{
	// 버튼 추가/제거/순서 변경은 이 목록만 수정하면 된다.
	// MakeTaskFn != nullptr 이면 per-asset 스텝 태스크로 실행(청크/가드/정지 적용). 아니면 레거시 동기 실행.
	return {
		{ LOCTEXT("BtnTexture",     "Texture"),       LOCTEXT("DescTexture",     "텍스처/스프라이트/아틀라스의 해상도·포맷·압축·용량 정보를 추출합니다."),       &FVaultAssetCheckToolModule::ExportTextureReport,      &VaultAssetCheckToolReport::MakeTextureTask },
		{ LOCTEXT("BtnSkeletal",    "Skeletal Mesh"), LOCTEXT("DescSkeletal",    "스켈레탈 메시의 LOD·버텍스·머티리얼·본 정보를 추출합니다."),                       &FVaultAssetCheckToolModule::ExportSkeletalMeshReport, &VaultAssetCheckToolReport::MakeSkeletalMeshTask },
		{ LOCTEXT("BtnStatic",      "Static Mesh"),   LOCTEXT("DescStatic",      "스태틱 메시의 LOD·트라이앵글·머티리얼 정보를 추출합니다."),                        &FVaultAssetCheckToolModule::ExportStaticMeshReport,   &VaultAssetCheckToolReport::MakeStaticMeshTask },
		{ LOCTEXT("BtnUMG",         "UMG"),           LOCTEXT("DescUMG",         "위젯 블루프린트의 이미지·폰트·스타일 참조 정보를 추출합니다."),                    &FVaultAssetCheckToolModule::ExportUMGAssetReport,     &VaultAssetCheckToolReport::MakeUMGTask },
		{ LOCTEXT("BtnAnimation",   "Animation"),     LOCTEXT("DescAnimation",   "애니메이션 시퀀스의 길이·프레임·키 정보를 추출합니다."),                           &FVaultAssetCheckToolModule::ExportAnimationReport,    &VaultAssetCheckToolReport::MakeAnimationTask },
		{ LOCTEXT("BtnSound",       "Sound"),         LOCTEXT("DescSound",       "사운드 웨이브의 압축·길이·채널 정보를 추출합니다."),                               &FVaultAssetCheckToolModule::ExportSoundReport,        &VaultAssetCheckToolReport::MakeSoundTask },
		{ LOCTEXT("BtnFont",        "Font"),          LOCTEXT("DescFont",        "폰트의 페이스·로딩 정책·임베드 정보를 추출합니다."),                               &FVaultAssetCheckToolModule::ExportFontReport,         &VaultAssetCheckToolReport::MakeFontTask },
		// Project Info: 에셋을 순회하지 않고 설정값만 읽으므로 청크 이점이 없어 레거시(단일 스텝) 유지
		{ LOCTEXT("BtnProjectInfo", "Project Info"),  LOCTEXT("DescProjectInfo", "프로젝트 및 플랫폼(Android·iOS·Windows) 패키징 설정을 추출합니다."),               &FVaultAssetCheckToolModule::ExportProjectInfoReport,  nullptr },
#if ENGINE_MAJOR_VERSION >= 5
		// Niagara 리포트는 UE5 이상에서만 지원 (UE4에서는 스텁이므로 버튼도 노출하지 않음)
		{ LOCTEXT("BtnNiagara",     "Niagara"),       LOCTEXT("DescNiagara",     "나이아가라 시스템의 이미터·시뮬레이션·렌더러 정보를 추출합니다."),                 &FVaultAssetCheckToolModule::ExportNiagaraReport,      &VaultAssetCheckToolReport::MakeNiagaraTask },
#else
		// Cascade 리포트는 UE4.X 전용 (UE5에서는 스텁이므로 버튼도 노출하지 않음). Niagara와 대칭.
		{ LOCTEXT("BtnCascade",     "Cascade"),       LOCTEXT("DescCascade",     "캐스케이드 파티클 시스템의 이미터·시뮬레이션(CPU/GPU)·렌더 타입·라이트 정보를 추출합니다."), &FVaultAssetCheckToolModule::ExportCascadeReport,      &VaultAssetCheckToolReport::MakeCascadeTask },
#endif
	};
}

TSharedPtr<IVaultReportTask> SVaultAssetCheckToolPanel::MakeTask(const FReportEntry& Entry)
{
	// 변환된 리포트는 per-asset 스텝 태스크, 아직 미변환이면 레거시(단일 스텝) 어댑터로 폴백.
	if (Entry.MakeTaskFn)
	{
		return Entry.MakeTaskFn();
	}
	return MakeShared<FLegacyReportTask>(Entry.Label, Entry.Func);
}

FString SVaultAssetCheckToolPanel::GetDefaultOutputDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("VaultAssetCheckTool"));
}

FString SVaultAssetCheckToolPanel::GetCurrentOutputPath() const
{
	FString Path;
	if (OutputPathBox.IsValid())
	{
		Path = OutputPathBox->GetText().ToString().TrimStartAndEnd();
	}
	return Path.IsEmpty() ? GetDefaultOutputDir() : Path;
}

void SVaultAssetCheckToolPanel::EnsureOutputDir(const FString& Path) const
{
	IFileManager::Get().MakeDirectory(*Path, /*Tree=*/true);
}

void SVaultAssetCheckToolPanel::Construct(const FArguments& InArgs)
{
	Runner.OnLog = FVaultJobLog::CreateSP(this, &SVaultAssetCheckToolPanel::AppendLog);
	Runner.OnProgress = FVaultJobProgress::CreateSP(this, &SVaultAssetCheckToolPanel::HandleProgress);
	Runner.OnFinished = FVaultJobFinished::CreateSP(this, &SVaultAssetCheckToolPanel::HandleFinished);

	const TArray<FReportEntry> Reports = GetReportEntries();

	// 리포트 목록: 각 행 = [버튼] + [설명 라벨]
	TSharedRef<SVerticalBox> ButtonList = SNew(SVerticalBox);
	for (int32 EntryIndex = 0; EntryIndex < Reports.Num(); ++EntryIndex)
	{
		const FReportEntry& Entry = Reports[EntryIndex];
		ButtonList->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 3.0f))
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(6.0f, 4.0f))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(150.0f)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.ContentPadding(FMargin(8.0f, 6.0f))
						.Text(Entry.Label)
						.IsEnabled(this, &SVaultAssetCheckToolPanel::AreControlsEnabled)
						.OnClicked(this, &SVaultAssetCheckToolPanel::OnRunSingle, EntryIndex)
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(FMargin(10.0f, 0.0f, 2.0f, 0.0f))
				[
					SNew(STextBlock)
					.Text(Entry.Description)
					.AutoWrapText(true)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]
		];
	}

	ChildSlot
	[
		SNew(SSplitter)
		.Orientation(Orient_Vertical)

		// ===== 상단: 컨트롤 =====
		+ SSplitter::Slot()
		.Value(0.5f)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(8.0f))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(2.0f, 2.0f, 2.0f, 8.0f))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PanelTitle", "Vault Asset Check Tool"))
					.Font(FEditorStyle::GetFontStyle("HeadingExtraSmall"))
				]

				// 출력 경로
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(2.0f, 0.0f, 2.0f, 4.0f))
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth().VAlign(VAlign_Center)
					.Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
					[
						SNew(STextBlock).Text(LOCTEXT("OutputPathLabel", "출력 경로"))
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SAssignNew(OutputPathBox, SEditableTextBox)
						.Text(FText::FromString(GetDefaultOutputDir()))
						.HintText(LOCTEXT("OutputPathHint", "리포트를 저장할 폴더 경로"))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth().VAlign(VAlign_Center)
					.Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
					[
						SNew(SButton)
						.Text(LOCTEXT("OpenFolder", "폴더 열기"))
						.ContentPadding(FMargin(8.0f, 4.0f))
						.OnClicked(this, &SVaultAssetCheckToolPanel::OnOpenOutputFolder)
					]
				]

				// 전체 출력 / 정지
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(2.0f, 4.0f))
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ContentPadding(FMargin(8.0f, 6.0f))
						.Text(LOCTEXT("ExportAll", "전체 출력"))
						.IsEnabled(this, &SVaultAssetCheckToolPanel::AreControlsEnabled)
						.OnClicked(this, &SVaultAssetCheckToolPanel::OnRunAll)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth().VAlign(VAlign_Center)
					.Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
					[
						SNew(SButton)
						.ContentPadding(FMargin(8.0f, 6.0f))
						.Text(LOCTEXT("Stop", "정지"))
						.IsEnabled(this, &SVaultAssetCheckToolPanel::IsStopEnabled)
						.OnClicked(this, &SVaultAssetCheckToolPanel::OnStop)
					]
				]

				// 진행바
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(2.0f, 0.0f, 2.0f, 6.0f))
				[
					SNew(SBox).HeightOverride(8.0f)
					[
						SNew(SProgressBar)
						.Percent(this, &SVaultAssetCheckToolPanel::GetProgressPercent)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 2.0f))
				[
					SNew(SSeparator)
				]

				// 리포트 버튼 목록 (스크롤)
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						ButtonList
					]
				]
			]
		]

		// ===== 하단: 로그 =====
		+ SSplitter::Slot()
		.Value(0.5f)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(8.0f))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 4.0f))
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("LogTitle", "출력 로그"))
						.Font(FEditorStyle::GetFontStyle("HeadingExtraSmall"))
					]

					// 우측 끝: "자세히" 체크박스 (항목별 상세 로그 토글)
					+ SHorizontalBox::Slot()
					.AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SCheckBox)
						.IsChecked(this, &SVaultAssetCheckToolPanel::GetVerboseCheckState)
						.OnCheckStateChanged(this, &SVaultAssetCheckToolPanel::OnVerboseCheckChanged)
						[
							SNew(STextBlock).Text(LOCTEXT("VerboseLabel", "자세히"))
						]
					]
				]

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					// 읽기전용 멀티라인 텍스트박스 → 드래그 선택 + Ctrl+C 복사 가능
					SAssignNew(LogTextBox, SMultiLineEditableTextBox)
					.IsReadOnly(true)
					.AllowMultiLine(true)
					.AlwaysShowScrollbars(true)
					.Text(LOCTEXT("LogReady", "리포트 버튼 또는 [전체 출력]을 눌러 시작하세요."))
				]
			]
		]
	];
}

void SVaultAssetCheckToolPanel::StartTasks(const TArray<TSharedPtr<IVaultReportTask>>& Tasks)
{
	if (Runner.IsRunning())
	{
		return;
	}
	ProgressPercent = 0.0f; // 새 실행은 항상 0부터 — 이전 진척도 잔상 방지
	EnsureOutputDir(GetCurrentOutputPath());
	Runner.Start(Tasks, GetCurrentOutputPath());
}

FReply SVaultAssetCheckToolPanel::OnRunSingle(int32 EntryIndex)
{
	const TArray<FReportEntry> Reports = GetReportEntries();
	if (!Reports.IsValidIndex(EntryIndex))
	{
		return FReply::Handled();
	}

	TArray<TSharedPtr<IVaultReportTask>> Tasks;
	Tasks.Add(MakeTask(Reports[EntryIndex]));
	StartTasks(Tasks);
	return FReply::Handled();
}

FReply SVaultAssetCheckToolPanel::OnRunAll()
{
	TArray<TSharedPtr<IVaultReportTask>> Tasks;
	for (const FReportEntry& Entry : GetReportEntries())
	{
		Tasks.Add(MakeTask(Entry));
	}
	StartTasks(Tasks);
	return FReply::Handled();
}

FReply SVaultAssetCheckToolPanel::OnStop()
{
	Runner.RequestCancel();
	ProgressPercent = 0.0f; // 정지 즉시 진행바 리셋(다음 틱의 종료 처리와 무관하게 시각적 동기화)
	return FReply::Handled();
}

FReply SVaultAssetCheckToolPanel::OnOpenOutputFolder()
{
	const FString AbsolutePath = FPaths::ConvertRelativePathToFull(GetCurrentOutputPath());
	if (FPaths::DirectoryExists(AbsolutePath))
	{
		FPlatformProcess::ExploreFolder(*AbsolutePath);
	}
	else
	{
		AppendLog(FString::Printf(TEXT("[알림] 폴더가 아직 없습니다: %s"), *AbsolutePath), /*bDetailOnly=*/false);
	}
	return FReply::Handled();
}

namespace
{
	// 버퍼 상한 초과 시 앞부분을 잘라 위젯 갱신 비용을 일정하게 유지.
	void CapLogBuffer(FString& Buffer)
	{
		if (Buffer.Len() <= kLogMaxChars)
		{
			return;
		}
		Buffer = Buffer.Right(kLogMaxChars * 3 / 4);
		int32 NewlineIdx = INDEX_NONE;
		if (Buffer.FindChar(TEXT('\n'), NewlineIdx))
		{
			Buffer = Buffer.RightChop(NewlineIdx + 1);
		}
		Buffer = FString(TEXT("...(이전 로그 생략)...\n")) + Buffer;
	}
}

void SVaultAssetCheckToolPanel::AppendLog(const FString& Line, bool bDetailOnly)
{
	const FString Stamp = FDateTime::Now().ToString(TEXT("%H:%M:%S"));
	const FString Entry = FString::Printf(TEXT("[%s] %s\n"), *Stamp, *Line);

	// 상세 버퍼에는 항상 기록(체크박스 무관). 간단 라인은 간단 버퍼에도 기록.
	DetailedLog += Entry;
	CapLogBuffer(DetailedLog);
	if (!bDetailOnly)
	{
		SimpleLog += Entry;
		CapLogBuffer(SimpleLog);
	}

	bLogDirty = true;
	// 실행 중이 아니면(틱 갱신이 없으므로) 즉시 반영, 실행 중이면 스로틀 반영.
	FlushLog(/*bForce=*/!Runner.IsRunning());
}

void SVaultAssetCheckToolPanel::FlushLog(bool bForce)
{
	if (!bLogDirty || !LogTextBox.IsValid())
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();
	if (!bForce && (Now - LastLogFlushSeconds) < kLogFlushIntervalSeconds)
	{
		return; // 스로틀: 아직 갱신 간격이 안 됨
	}

	LastLogFlushSeconds = Now;
	bLogDirty = false;
	LogTextBox->SetText(FText::FromString(bVerboseLog ? DetailedLog : SimpleLog));
	LogTextBox->ScrollTo(ETextLocation::EndOfDocument);
}

void SVaultAssetCheckToolPanel::HandleProgress(float Percent, const FString& /*Phase*/)
{
	ProgressPercent = Percent;
	FlushLog(/*bForce=*/false); // 매 틱 스로틀 반영 → 자세히 로그도 0.1초 간격으로 표시
}

ECheckBoxState SVaultAssetCheckToolPanel::GetVerboseCheckState() const
{
	return bVerboseLog ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SVaultAssetCheckToolPanel::OnVerboseCheckChanged(ECheckBoxState NewState)
{
	bVerboseLog = (NewState == ECheckBoxState::Checked);
	// 표시 버퍼를 즉시 전환(이미 쌓인 로그가 소급해서 보임/숨김).
	bLogDirty = true;
	FlushLog(/*bForce=*/true);
}

void SVaultAssetCheckToolPanel::HandleFinished(bool bInterrupted)
{
	// 중단 시에는 0, 정상 완료 시에는 100%로 확정.
	ProgressPercent = bInterrupted ? 0.0f : 1.0f;
	FlushLog(/*bForce=*/true); // 종료 직전 마지막 로그까지 확실히 표시
}

bool SVaultAssetCheckToolPanel::AreControlsEnabled() const
{
	return !Runner.IsRunning();
}

bool SVaultAssetCheckToolPanel::IsStopEnabled() const
{
	return Runner.IsRunning();
}

TOptional<float> SVaultAssetCheckToolPanel::GetProgressPercent() const
{
	return TOptional<float>(ProgressPercent);
}

#undef LOCTEXT_NAMESPACE
