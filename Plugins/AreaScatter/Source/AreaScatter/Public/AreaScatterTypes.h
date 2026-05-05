// Copyright (c) ActorScatter authors. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AreaScatterTypes.generated.h"

// DECLARE_LOG_CATEGORY_EXTERN doesn't export across DLLs. Spell it out so the
// editor module can link against the same FLogCategory instance.
struct FLogCategoryLogAreaScatter : public FLogCategory<ELogVerbosity::Log, ELogVerbosity::All>
{
	FORCEINLINE FLogCategoryLogAreaScatter() : FLogCategory(TEXT("LogAreaScatter")) {}
};
extern AREASCATTER_API FLogCategoryLogAreaScatter LogAreaScatter;

USTRUCT(BlueprintType)
struct AREASCATTER_API FAreaScatterCandidate
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AreaScatter")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AreaScatter")
	FVector Normal = FVector::UpVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AreaScatter")
	float SlopeDegrees = 0.f;
};

USTRUCT(BlueprintType)
struct AREASCATTER_API FAreaScatterAvoidShape
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AreaScatter")
	FVector Center = FVector::ZeroVector;

	// Half-extent in world space (XY only used for Phase 1 AABB filter).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AreaScatter")
	FVector Extent = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AreaScatter")
	FName ActorName = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AreaScatter")
	FName ClassName = NAME_None;

	// Source actor — held weakly so MeshSDF mode can query its physics bodies at pick time.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AreaScatter")
	TWeakObjectPtr<AActor> SourceActor;
};
