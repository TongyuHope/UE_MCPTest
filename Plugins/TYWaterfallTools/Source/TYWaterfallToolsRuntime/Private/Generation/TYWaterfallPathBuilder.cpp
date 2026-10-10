// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generation/TYWaterfallPathBuilder.h"

#include "Actors/TYWaterfallActor.h"
#include "Components/TYWaterfallMeshComponent.h"
#include "Components/TYWaterfallPathComponent.h"
#include "Components/TYWaterfallSettingsComponent.h"
#include "Components/SplineComponent.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

#if WITH_EDITOR
void FTYWaterfallPathBuilder::Initialize(ATYWaterfallActor* InOwner)
{
	Owner = InOwner;
}

bool FTYWaterfallPathBuilder::StartGeneration()
{
	ATYWaterfallActor* Waterfall = Owner.Get();
	if (!IsValid(Waterfall) || IsGenerating())
	{
		return false;
	}

	const FScopedTransaction Transaction(NSLOCTEXT(
		"TYWaterfallTools", "GenerateWaterfallPaths", "Generate Waterfall Paths"));
	Waterfall->Modify();
	ClearGeneratedPaths();

	if (!CreatePaths())
	{
		ClearGeneratedPaths();
		FinishGeneration(ETYWaterfallGenerationState::Failed);
		return false;
	}

	CurrentPathIndex = 0;
	TotalPathCount = PendingPaths.Num();
	State = ETYWaterfallGenerationState::Generating;
	Waterfall->SetActorTickEnabled(true);
	return true;
}

bool FTYWaterfallPathBuilder::CreatePaths()
{
	ATYWaterfallActor* Waterfall = Owner.Get();
	if (!IsValid(Waterfall) || !IsValid(Waterfall->TopSpline)
		|| !IsValid(Waterfall->WaterfallSettings))
	{
		return false;
	}

	const int32 NumPaths = Waterfall->WaterfallSettings->GetNumPaths();
	const FVector2D SpawnRange = Waterfall->WaterfallSettings->GetSpawnRange();
	FRandomStream RandomStream(Waterfall->WaterfallSettings->GetSeed());

	Waterfall->GeneratedPaths.Reserve(NumPaths);
	PendingPaths.Reserve(NumPaths);

	for (int32 PathIndex = 0; PathIndex < NumPaths; ++PathIndex)
	{
		const float PathAlpha = NumPaths > 1
			? static_cast<float>(PathIndex) / static_cast<float>(NumPaths - 1)
			: 0.5f;
		const float SplineTime = FMath::Lerp(SpawnRange.X, SpawnRange.Y, PathAlpha);

		UTYWaterfallPathComponent* Path = NewObject<UTYWaterfallPathComponent>(
			Waterfall, NAME_None, RF_Transactional);
		if (!IsValid(Path))
		{
			return false;
		}

		// Instance components must be owned and registered explicitly. Keeping the
		// strong UPROPERTY reference on the actor also protects them from GC.
		Path->CreationMethod = EComponentCreationMethod::Instance;
		Path->SetupAttachment(Waterfall->RootComp);
		Waterfall->AddInstanceComponent(Path);
		Path->RegisterComponent();
		Path->SetHiddenInGame(true);
		Path->ConfigureSimulation(
			Waterfall->WaterfallSettings->GetInitialSpeed(),
			Waterfall->WaterfallSettings->GetGravity(),
			Waterfall->WaterfallSettings->GetDrag(),
			Waterfall->WaterfallSettings->ShouldEnableWorldCollision(),
			Waterfall->WaterfallSettings->GetFixedDeltaTime(),
			Waterfall->WaterfallSettings->GetMaxSteps(),
			Waterfall->WaterfallSettings->GetTerminationHeight());
		Path->SetSampleSeed(HashCombineFast(
			GetTypeHash(Waterfall->WaterfallSettings->GetSeed()), PathIndex));
		// Golden-ratio hue stepping keeps adjacent paths visually distinct while
		// remaining deterministic when the same number of paths is regenerated.
		const uint8 PathHue = static_cast<uint8>((PathIndex * 137) % 255);
		Path->SetPathDebugColor(FLinearColor::MakeFromHSV8(PathHue, 190, 255));
		Path->SetDrawDebug(Waterfall->WaterfallSettings->ShouldShowPathDebug());

		const float Jitter = RandomStream.FRandRange(
			-Waterfall->WaterfallSettings->GetSpawnJitterDegrees(),
			Waterfall->WaterfallSettings->GetSpawnJitterDegrees());
		if (!Path->InitializeSimulation(
			SplineTime, Jitter, Waterfall->WaterfallSettings->ShouldReverseFlowDirection()))
		{
			Path->DestroyComponent();
			return false;
		}

		Waterfall->GeneratedPaths.Add(Path);
		PendingPaths.Add(Path);
	}

	return PendingPaths.Num() > 0;
}

void FTYWaterfallPathBuilder::TickGeneration()
{
	ATYWaterfallActor* Waterfall = Owner.Get();
	if (!IsGenerating() || !IsValid(Waterfall) || !IsValid(Waterfall->WaterfallSettings))
	{
		FinishGeneration(ETYWaterfallGenerationState::Failed);
		return;
	}

	// One shared budget prevents cost from scaling to NumPaths * Budget. A path
	// may finish early, in which case the unused budget moves to the next path.
	int32 RemainingBudget = Waterfall->WaterfallSettings->GetSimulationStepsPerFrame();
	while (RemainingBudget > 0 && CurrentPathIndex < PendingPaths.Num())
	{
		UTYWaterfallPathComponent* Path = PendingPaths[CurrentPathIndex].Get();
		if (!IsValid(Path))
		{
			FinishGeneration(ETYWaterfallGenerationState::Failed);
			return;
		}

		const int32 StepsUsed = Path->AdvanceSimulation(RemainingBudget);
		RemainingBudget -= FMath::Max(StepsUsed, 1);

		if (Path->HasCompletedSimulation())
		{
			++CurrentPathIndex;
		}
	}

	if (CurrentPathIndex >= PendingPaths.Num())
	{
		FinishGeneration(ETYWaterfallGenerationState::Completed);
	}
}

void FTYWaterfallPathBuilder::CancelGeneration()
{
	if (!IsGenerating())
	{
		return;
	}

	State = ETYWaterfallGenerationState::Cancelling;
	ClearGeneratedPaths();
	FinishGeneration(ETYWaterfallGenerationState::Idle);
}

void FTYWaterfallPathBuilder::ClearGeneratedPaths()
{
	ATYWaterfallActor* Waterfall = Owner.Get();
	if (!IsValid(Waterfall))
	{
		PendingPaths.Reset();
		return;
	}

	// The mesh is derived from these paths. Clear it first so Undo/Redo and
	// failed regenerations never leave geometry that represents stale inputs.
	if (IsValid(Waterfall->DynamicMeshComponent))
	{
		Waterfall->DynamicMeshComponent->Modify();
		Waterfall->DynamicMeshComponent->ClearWaterfallMesh();
	}
	Waterfall->ClearNiagaraEffects();

	for (UTYWaterfallPathComponent* Path : Waterfall->GeneratedPaths)
	{
		if (IsValid(Path))
		{
			Path->Modify();
			Path->DestroyComponent();
		}
	}

	Waterfall->GeneratedPaths.Reset();
	PendingPaths.Reset();
	CurrentPathIndex = 0;
	Waterfall->MarkPackageDirty();
}

void FTYWaterfallPathBuilder::FinishGeneration(ETYWaterfallGenerationState ResultState)
{
	if (ATYWaterfallActor* Waterfall = Owner.Get())
	{
		Waterfall->SetActorTickEnabled(false);
		if (ResultState == ETYWaterfallGenerationState::Completed)
		{
			Waterfall->RefreshNiagaraEffects();
			Waterfall->MarkPackageDirty();
		}
	}

	State = ResultState;
	PendingPaths.Reset();
	CurrentPathIndex = 0;
}

float FTYWaterfallPathBuilder::GetProgress() const
{
	if (State == ETYWaterfallGenerationState::Completed)
	{
		return 1.0f;
	}
	return TotalPathCount <= 0
		? 0.0f
		: static_cast<float>(CurrentPathIndex) / static_cast<float>(TotalPathCount);
}
#endif
