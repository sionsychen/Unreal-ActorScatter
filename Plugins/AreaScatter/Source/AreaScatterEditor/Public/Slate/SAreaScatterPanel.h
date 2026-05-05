// Copyright (c) ActorScatter authors. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class AAreaSpawnerActor;
class STextBlock;

/**
 * Phase-2/3 control panel — auto-binds to selected AAreaSpawnerActor and
 * surfaces stateful guidance (next-step text, diagnostics, sample-rule bootstrap)
 * so the tool itself documents the workflow.
 */
class SAreaScatterPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAreaScatterPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SAreaScatterPanel();

private:
	TWeakObjectPtr<AAreaSpawnerActor> CurrentSpawner;
	FDelegateHandle SelectionDelegateHandle;
	FDelegateHandle PropertyChangedHandle;

	// Latest diagnostics result, refreshed on Diagnose / Spawn / Preview / refresh.
	FText LastDiagnosticsText;

	// Active timer for debounced auto-preview. Created on demand, cleared in dtor.
	TSharedPtr<class FActiveTimerHandle> AutoPreviewTimerHandle;

	FText GetSpawnerStatusText() const;
	FText GetNextStepText() const;
	FText GetRuleSummaryText() const;
	FText GetBatchSummaryText() const;
	FText GetDiagnosticsText() const;
	FText GetTargetCountHelperText() const;
	bool  IsSpawnerValid() const;

	int32 GetTargetCountOverrideValue() const;
	void OnTargetCountOverrideChanged(int32 NewValue);
	void OnTargetCountOverrideCommitted(int32 NewValue, ETextCommit::Type CommitType);

	ECheckBoxState GetAutoPreviewState() const;
	void OnAutoPreviewChanged(ECheckBoxState NewState);

	void OnObjectPropertyChanged(UObject* Object, struct FPropertyChangedEvent& Event);
	bool IsRelevantToSpawner(const UObject* Object) const;
	void ScheduleAutoPreview();
	EActiveTimerReturnType HandleAutoPreviewTimer(double InCurrentTime, float InDeltaTime);

	void OnSelectionChanged(UObject* NewSelection);
	void RefreshFromSelection();

	FReply OnRefreshClicked();
	FReply OnPreviewClicked();
	FReply OnSpawnClicked();
	FReply OnClearClicked();
	FReply OnPickInWorldClicked();
	FReply OnDiagnoseClicked();
	FReply OnGenerateSamplesClicked();
};
