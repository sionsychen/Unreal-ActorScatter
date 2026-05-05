// Copyright (c) ActorScatter authors. All rights reserved.

#include "Slate/SAreaScatterPanel.h"

#include "AreaScatterDiagnostics.h"
#include "AreaScatterEditorOps.h"
#include "AreaScatterSampleAssets.h"
#include "AreaScatterTypes.h"
#include "AreaSpawnBatch.h"
#include "AreaSpawnerActor.h"
#include "Components/SplineComponent.h"
#include "RulePassAsset.h"
#include "RuleStackAsset.h"

#include "Editor.h"
#include "Engine/Selection.h"
#include "GameFramework/Actor.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "SAreaScatterPanel"

void SAreaScatterPanel::Construct(const FArguments& InArgs)
{
	if (USelection* Selection = GEditor ? GEditor->GetSelectedActors() : nullptr)
	{
		SelectionDelegateHandle = USelection::SelectionChangedEvent.AddSP(this, &SAreaScatterPanel::OnSelectionChanged);
	}
	PropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &SAreaScatterPanel::OnObjectPropertyChanged);
	RefreshFromSelection();
	LastDiagnosticsText = LOCTEXT("DiagInitial", "Click 'Diagnose' to validate the current setup.");

	const FMargin SectionPad(0.f, 6.f, 0.f, 6.f);
	const FMargin RowPad(0.f, 2.f, 0.f, 2.f);

	ChildSlot
	[
		SNew(SBorder)
		.Padding(12.f)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SVerticalBox)

				// Header
				+ SVerticalBox::Slot().AutoHeight().Padding(SectionPad)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title", "Area Scatter"))
					.Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
				]

				// Stateful next-step text
				+ SVerticalBox::Slot().AutoHeight().Padding(RowPad)
				[
					SNew(STextBlock)
					.Text(this, &SAreaScatterPanel::GetNextStepText)
					.AutoWrapText(true)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(SectionPad) [ SNew(SSeparator) ]

				// Selection status
				+ SVerticalBox::Slot().AutoHeight().Padding(RowPad)
				[
					SNew(STextBlock)
					.Text(this, &SAreaScatterPanel::GetSpawnerStatusText)
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(RowPad)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("Refresh", "Refresh from Selection"))
						.OnClicked(this, &SAreaScatterPanel::OnRefreshClicked)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("PickInWorld", "Pick Selected in World"))
						.ToolTipText(LOCTEXT("PickInWorldTip", "Select the bound spawner in the viewport / outliner."))
						.IsEnabled(this, &SAreaScatterPanel::IsSpawnerValid)
						.OnClicked(this, &SAreaScatterPanel::OnPickInWorldClicked)
					]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(SectionPad) [ SNew(SSeparator) ]

				// Bootstrap
				+ SVerticalBox::Slot().AutoHeight().Padding(RowPad)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BootstrapHeader", "Bootstrap"))
					.Font(FAppStyle::GetFontStyle("BoldFont"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(RowPad)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("GenSamples", "Generate Sample Rules"))
					.ToolTipText(LOCTEXT("GenSamplesTip",
						"Write 3 working URulePassAsset examples to /Game/AreaScatter/Samples/.\n"
						"Existing assets with the same name are skipped."))
					.OnClicked(this, &SAreaScatterPanel::OnGenerateSamplesClicked)
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(SectionPad) [ SNew(SSeparator) ]

				// Rule summary
				+ SVerticalBox::Slot().AutoHeight().Padding(RowPad)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RuleHeader", "Rule"))
					.Font(FAppStyle::GetFontStyle("BoldFont"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(RowPad)
				[
					SNew(STextBlock)
					.Text(this, &SAreaScatterPanel::GetRuleSummaryText)
					.AutoWrapText(true)
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(SectionPad) [ SNew(SSeparator) ]

				// Spawn count override
				+ SVerticalBox::Slot().AutoHeight().Padding(RowPad)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CountHeader", "Spawn Count Override"))
					.Font(FAppStyle::GetFontStyle("BoldFont"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(RowPad)
				[
					SNew(STextBlock)
					.Text(this, &SAreaScatterPanel::GetTargetCountHelperText)
					.AutoWrapText(true)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(RowPad)
				[
					SNew(SSpinBox<int32>)
					.MinValue(0)
					.MaxValue(100000)
					.MinSliderValue(0)
					.MaxSliderValue(2000)
					.Delta(10)
					.IsEnabled(this, &SAreaScatterPanel::IsSpawnerValid)
					.Value(this, &SAreaScatterPanel::GetTargetCountOverrideValue)
					.OnValueChanged(this, &SAreaScatterPanel::OnTargetCountOverrideChanged)
					.OnValueCommitted(this, &SAreaScatterPanel::OnTargetCountOverrideCommitted)
					.ToolTipText(LOCTEXT("CountSpinTip",
						"0 = use each rule's authored TargetCount. >0 = override every enabled pass to this count for the next Spawn. "
						"Edits go through Undo (Ctrl+Z)."))
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(SectionPad) [ SNew(SSeparator) ]

				// Auto-preview toggle
				+ SVerticalBox::Slot().AutoHeight().Padding(RowPad)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)
					[
						SNew(SCheckBox)
						.IsChecked(this, &SAreaScatterPanel::GetAutoPreviewState)
						.IsEnabled(this, &SAreaScatterPanel::IsSpawnerValid)
						.OnCheckStateChanged(this, &SAreaScatterPanel::OnAutoPreviewChanged)
						.ToolTipText(LOCTEXT("AutoPreviewTip",
							"Re-runs Preview ~300ms after any property edit on the bound spawner / its rule / stack passes. "
							"Off = manual Preview only (cheaper for big regions)."))
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AutoPreviewLabel", "Auto Preview on edits (debounced)"))
					]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(SectionPad) [ SNew(SSeparator) ]

				// Last batch
				+ SVerticalBox::Slot().AutoHeight().Padding(RowPad)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BatchHeader", "Last Batch"))
					.Font(FAppStyle::GetFontStyle("BoldFont"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(RowPad)
				[
					SNew(STextBlock)
					.Text(this, &SAreaScatterPanel::GetBatchSummaryText)
					.AutoWrapText(true)
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(SectionPad) [ SNew(SSeparator) ]

				// Diagnostics
				+ SVerticalBox::Slot().AutoHeight().Padding(RowPad)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DiagHeader", "Diagnostics"))
					.Font(FAppStyle::GetFontStyle("BoldFont"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(RowPad)
				[
					SNew(SBorder)
					.Padding(6.f)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
					[
						SNew(STextBlock)
						.Text(this, &SAreaScatterPanel::GetDiagnosticsText)
						.AutoWrapText(true)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(RowPad)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("Diagnose", "Diagnose"))
					.ToolTipText(LOCTEXT("DiagnoseTip",
						"Validate spawner / spline / rule / output / multi-area / cycles. "
						"Each issue includes a fix suggestion."))
					.IsEnabled(this, &SAreaScatterPanel::IsSpawnerValid)
					.OnClicked(this, &SAreaScatterPanel::OnDiagnoseClicked)
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(SectionPad) [ SNew(SSeparator) ]

				// Action buttons
				+ SVerticalBox::Slot().AutoHeight().Padding(RowPad)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0, 0, 4, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("Preview", "Preview"))
						.ToolTipText(LOCTEXT("PreviewTip", "Run sampling + picking, draw debug spheres for picked points (no actors spawned)."))
						.IsEnabled(this, &SAreaScatterPanel::IsSpawnerValid)
						.OnClicked(this, &SAreaScatterPanel::OnPreviewClicked)
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4, 0, 4, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("Spawn", "Spawn"))
						.ToolTipText(LOCTEXT("SpawnTip", "Commit picks: spawn actors / instances / point set, package as a UAreaSpawnBatch (Ctrl+Z reverts)."))
						.IsEnabled(this, &SAreaScatterPanel::IsSpawnerValid)
						.OnClicked(this, &SAreaScatterPanel::OnSpawnClicked)
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4, 0, 0, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("Clear", "Clear Last"))
						.ToolTipText(LOCTEXT("ClearTip", "Destroy all actors / instance host in the last batch."))
						.IsEnabled(this, &SAreaScatterPanel::IsSpawnerValid)
						.OnClicked(this, &SAreaScatterPanel::OnClearClicked)
					]
				]
			]
		]
	];
}

SAreaScatterPanel::~SAreaScatterPanel()
{
	if (SelectionDelegateHandle.IsValid())
	{
		USelection::SelectionChangedEvent.Remove(SelectionDelegateHandle);
	}
	if (PropertyChangedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyChangedHandle);
	}
	if (AutoPreviewTimerHandle.IsValid())
	{
		UnRegisterActiveTimer(AutoPreviewTimerHandle.ToSharedRef());
		AutoPreviewTimerHandle.Reset();
	}
}

bool SAreaScatterPanel::IsSpawnerValid() const
{
	return CurrentSpawner.IsValid();
}

void SAreaScatterPanel::OnSelectionChanged(UObject* /*NewSelection*/)
{
	RefreshFromSelection();
}

void SAreaScatterPanel::RefreshFromSelection()
{
	CurrentSpawner.Reset();
	if (!GEditor) { return; }
	USelection* Sel = GEditor->GetSelectedActors();
	if (!Sel) { return; }
	for (FSelectionIterator It(*Sel); It; ++It)
	{
		if (AAreaSpawnerActor* A = Cast<AAreaSpawnerActor>(*It))
		{
			CurrentSpawner = A;
			break;
		}
	}
}

FText SAreaScatterPanel::GetSpawnerStatusText() const
{
	if (AAreaSpawnerActor* S = CurrentSpawner.Get())
	{
		return FText::Format(LOCTEXT("SpawnerBound", "Bound to: {0}"),
			FText::FromString(S->GetActorLabel()));
	}
	return LOCTEXT("NoSpawner", "No Area Spawner selected.");
}

FText SAreaScatterPanel::GetNextStepText() const
{
	AAreaSpawnerActor* S = CurrentSpawner.Get();
	if (!S)
	{
		return LOCTEXT("StepSelect",
			"Step 1: Place an 'Area Spawner' from the Place Actors panel, then select it. "
			"(Or click 'Generate Sample Rules' below first if /Game/AreaScatter/Samples/ is empty.)");
	}
	if (!S->AreaSpline || S->AreaSpline->GetNumberOfSplinePoints() < 3)
	{
		return LOCTEXT("StepSpline",
			"Step 2: Edit the AreaSpline component on the selected spawner. Alt-drag spline tangents in the viewport "
			"to add points until you have at least 3 (a closed-loop polygon).");
	}
	if (!S->Rule && !S->RuleStack)
	{
		return LOCTEXT("StepRule",
			"Step 3: Assign a Rule (or RuleStack) to the spawner. If you don't have one, click 'Generate Sample Rules' "
			"below and pick DA_Sample_Grass_Spread.");
	}
	// Rule exists — quick check on output
	const URulePassAsset* P = S->Rule;
	if (!P && S->RuleStack && S->RuleStack->Passes.Num() > 0)
	{
		for (URulePassAsset* Pass : S->RuleStack->Passes) { if (Pass) { P = Pass; break; } }
	}
	if (P)
	{
		switch (P->OutputMode)
		{
			case EAreaSpawnOutputMode::Actors:
				if (P->SpawnActorClass.IsNull())
				{
					return LOCTEXT("StepActorClass",
						"Almost there: the rule's OutputMode = Actors but SpawnActorClass is empty. "
						"Open the rule and pick a class (TargetPoint works as a placeholder).");
				}
				break;
			case EAreaSpawnOutputMode::ISM:
			case EAreaSpawnOutputMode::HISM:
				if (P->SpawnStaticMesh.IsNull())
				{
					return LOCTEXT("StepStaticMesh",
						"Almost there: the rule's OutputMode = ISM/HISM but SpawnStaticMesh is empty. "
						"Open the rule and pick a Static Mesh (/Engine/BasicShapes/Cube to test).");
				}
				break;
			default: break;
		}
	}
	return LOCTEXT("StepReady",
		"Ready. Click 'Diagnose' to verify, then 'Preview' to see candidate points, then 'Spawn' to commit. "
		"Ctrl+Z reverts a Spawn.");
}

FText SAreaScatterPanel::GetRuleSummaryText() const
{
	AAreaSpawnerActor* S = CurrentSpawner.Get();
	if (!S) { return LOCTEXT("Dash", "—"); }

	if (URuleStackAsset* Stack = S->RuleStack)
	{
		int32 Enabled = 0;
		const int32 Total = Stack->Passes.Num();
		for (URulePassAsset* P : Stack->Passes)
		{
			if (P && P->bEnabled) { ++Enabled; }
		}
		return FText::Format(LOCTEXT("StackSummary", "RuleStack: {0}  ({1} of {2} passes enabled)"),
			FText::FromString(Stack->GetName()),
			FText::AsNumber(Enabled),
			FText::AsNumber(Total));
	}
	if (URulePassAsset* P = S->Rule)
	{
		const TCHAR* Mode = P->PickMode == EAreaScatterPickMode::Cluster ? TEXT("Cluster") : TEXT("Spread");
		const TCHAR* Out =
			P->OutputMode == EAreaSpawnOutputMode::HISM      ? TEXT("HISM") :
			P->OutputMode == EAreaSpawnOutputMode::ISM       ? TEXT("ISM") :
			P->OutputMode == EAreaSpawnOutputMode::PointSet  ? TEXT("PointSet") :
			                                                   TEXT("Actors");
		return FText::Format(LOCTEXT("PassSummary", "Rule: {0}  [{1} -> {2}, target={3}, slope<={4}°, seed={5}]"),
			FText::FromString(P->GetName()),
			FText::FromString(Mode),
			FText::FromString(Out),
			FText::AsNumber(P->TargetCount),
			FText::AsNumber(P->MaxSlopeDegrees),
			FText::AsNumber(P->Seed));
	}
	return LOCTEXT("NoRule", "No Rule or RuleStack assigned.");
}

FText SAreaScatterPanel::GetBatchSummaryText() const
{
	AAreaSpawnerActor* S = CurrentSpawner.Get();
	if (!S || !S->LastBatch) { return LOCTEXT("NoBatch", "No batch yet."); }

	UAreaSpawnBatch* B = S->LastBatch;
	return FText::Format(LOCTEXT("BatchSummary", "ID: {0}  |  Actors: {1}  |  Created: {2}"),
		FText::FromString(B->BatchId.ToString(EGuidFormats::Short)),
		FText::AsNumber(B->SpawnedActors.Num()),
		FText::AsCultureInvariant(B->CreatedAt.ToString()));
}

FText SAreaScatterPanel::GetDiagnosticsText() const
{
	return LastDiagnosticsText;
}

FText SAreaScatterPanel::GetTargetCountHelperText() const
{
	AAreaSpawnerActor* S = CurrentSpawner.Get();
	if (!S) { return LOCTEXT("CountHelpNoSpawner", "Select a spawner to edit override."); }

	int32 RuleDefault = -1;
	if (S->Rule) { RuleDefault = S->Rule->TargetCount; }
	else if (S->RuleStack)
	{
		for (URulePassAsset* P : S->RuleStack->Passes)
		{
			if (P && P->bEnabled) { RuleDefault = P->TargetCount; break; }
		}
	}

	if (S->TargetCountOverride > 0)
	{
		return FText::Format(
			LOCTEXT("CountHelpActive", "Override active: every enabled pass spawns {0}. Set to 0 to fall back to rule defaults (first rule = {1})."),
			FText::AsNumber(S->TargetCountOverride),
			RuleDefault > 0 ? FText::AsNumber(RuleDefault) : LOCTEXT("Unknown", "?"));
	}
	if (RuleDefault > 0)
	{
		return FText::Format(
			LOCTEXT("CountHelpInactive", "0 = use rule's authored value (currently {0}). Increase to override for the next Spawn."),
			FText::AsNumber(RuleDefault));
	}
	return LOCTEXT("CountHelpNoRule", "0 = use rule defaults. Assign a Rule or RuleStack first.");
}

int32 SAreaScatterPanel::GetTargetCountOverrideValue() const
{
	if (AAreaSpawnerActor* S = CurrentSpawner.Get()) { return S->TargetCountOverride; }
	return 0;
}

void SAreaScatterPanel::OnTargetCountOverrideChanged(int32 NewValue)
{
	// Live drag — just write the value. Final transaction happens on commit.
	if (AAreaSpawnerActor* S = CurrentSpawner.Get())
	{
		S->TargetCountOverride = FMath::Max(0, NewValue);
	}
}

void SAreaScatterPanel::OnTargetCountOverrideCommitted(int32 NewValue, ETextCommit::Type /*CommitType*/)
{
	if (AAreaSpawnerActor* S = CurrentSpawner.Get())
	{
		S->Modify();
		S->TargetCountOverride = FMath::Max(0, NewValue);
	}
}

ECheckBoxState SAreaScatterPanel::GetAutoPreviewState() const
{
	if (AAreaSpawnerActor* S = CurrentSpawner.Get())
	{
		return S->bAutoPreview ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Unchecked;
}

void SAreaScatterPanel::OnAutoPreviewChanged(ECheckBoxState NewState)
{
	if (AAreaSpawnerActor* S = CurrentSpawner.Get())
	{
		S->Modify();
		S->bAutoPreview = (NewState == ECheckBoxState::Checked);
	}
}

bool SAreaScatterPanel::IsRelevantToSpawner(const UObject* Object) const
{
	AAreaSpawnerActor* S = CurrentSpawner.Get();
	if (!S || !Object) { return false; }
	if (Object == S) { return true; }
	if (S->Rule && Object == S->Rule) { return true; }
	if (S->RuleStack)
	{
		if (Object == S->RuleStack) { return true; }
		for (URulePassAsset* P : S->RuleStack->Passes)
		{
			if (P == Object) { return true; }
		}
	}
	for (AAreaSpawnerActor* Other : S->AdditionalAreas) { if (Object == Other) { return true; } }
	for (AAreaSpawnerActor* Other : S->HoleAreas)        { if (Object == Other) { return true; } }
	return false;
}

void SAreaScatterPanel::OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& /*Event*/)
{
	AAreaSpawnerActor* S = CurrentSpawner.Get();
	if (!S || !S->bAutoPreview) { return; }
	if (!IsRelevantToSpawner(Object)) { return; }
	ScheduleAutoPreview();
}

void SAreaScatterPanel::ScheduleAutoPreview()
{
	// Reset existing timer (debounce): UnRegister + register fresh.
	if (AutoPreviewTimerHandle.IsValid())
	{
		UnRegisterActiveTimer(AutoPreviewTimerHandle.ToSharedRef());
		AutoPreviewTimerHandle.Reset();
	}
	AutoPreviewTimerHandle = RegisterActiveTimer(0.3f,
		FWidgetActiveTimerDelegate::CreateSP(this, &SAreaScatterPanel::HandleAutoPreviewTimer));
}

EActiveTimerReturnType SAreaScatterPanel::HandleAutoPreviewTimer(double /*InCurrentTime*/, float /*InDeltaTime*/)
{
	AutoPreviewTimerHandle.Reset();
	if (AAreaSpawnerActor* S = CurrentSpawner.Get())
	{
		if (S->bAutoPreview)
		{
			AreaScatter::HandlePreview(S);
		}
	}
	return EActiveTimerReturnType::Stop;
}

FReply SAreaScatterPanel::OnRefreshClicked()
{
	RefreshFromSelection();
	return FReply::Handled();
}

FReply SAreaScatterPanel::OnPickInWorldClicked()
{
	if (AAreaSpawnerActor* S = CurrentSpawner.Get())
	{
		if (GEditor)
		{
			GEditor->SelectNone(true, true);
			GEditor->SelectActor(S, true, true);
		}
	}
	return FReply::Handled();
}

FReply SAreaScatterPanel::OnPreviewClicked()
{
	if (AAreaSpawnerActor* S = CurrentSpawner.Get()) { AreaScatter::HandlePreview(S); }
	return FReply::Handled();
}

FReply SAreaScatterPanel::OnSpawnClicked()
{
	if (AAreaSpawnerActor* S = CurrentSpawner.Get()) { AreaScatter::HandleSpawn(S); }
	return FReply::Handled();
}

FReply SAreaScatterPanel::OnClearClicked()
{
	if (AAreaSpawnerActor* S = CurrentSpawner.Get()) { AreaScatter::HandleClear(S); }
	return FReply::Handled();
}

FReply SAreaScatterPanel::OnDiagnoseClicked()
{
	const AreaScatter::FDiagReport Report = AreaScatter::DiagnoseSpawner(CurrentSpawner.Get());
	LastDiagnosticsText = FText::FromString(Report.ToMultiLineString());
	return FReply::Handled();
}

FReply SAreaScatterPanel::OnGenerateSamplesClicked()
{
	const int32 Written = AreaScatter::GenerateSampleRules(/*bOverwrite=*/false);
	LastDiagnosticsText = FText::Format(
		LOCTEXT("GenSamplesResult", "Generated {0} sample rule(s) into /Game/AreaScatter/Samples/. Existing assets were left alone."),
		FText::AsNumber(Written));
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
