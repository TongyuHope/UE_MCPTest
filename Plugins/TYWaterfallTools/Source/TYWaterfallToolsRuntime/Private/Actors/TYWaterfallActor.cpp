// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/TYWaterfallActor.h"

#include "Components/TYWaterfallMeshComponent.h"
#include "Components/TYWaterfallPathComponent.h"
#include "Components/TYWaterfallSettingsComponent.h"
#include "Components/TYWaterfallVFXComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraSystem.h"
#include "UDynamicMesh.h"

#if WITH_EDITORONLY_DATA
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#endif

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

ATYWaterfallActor::ATYWaterfallActor()
{
	// Tick is available for editor-time generation but remains disabled until a
	// builder has work. This keeps placed waterfalls free of idle tick cost.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(RootComp);

	TopSpline = CreateDefaultSubobject<USplineComponent>(TEXT("TopSpline"));
	TopSpline->SetupAttachment(RootComp);
	TopSpline->SetMobility(EComponentMobility::Movable);
	TopSpline->SetClosedLoop(false);
	TopSpline->ClearSplinePoints(false);
	TopSpline->AddSplinePoint(FVector(-300.0f, 0.0f, 0.0f), ESplineCoordinateSpace::Local, false);
	TopSpline->AddSplinePoint(FVector(300.0f, 0.0f, 0.0f), ESplineCoordinateSpace::Local, false);
	TopSpline->SetSplinePointType(0, ESplinePointType::Curve, false);
	TopSpline->SetSplinePointType(1, ESplinePointType::Curve, false);
	TopSpline->UpdateSpline();

	BakedMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BakedMesh"));
	BakedMeshComponent->SetupAttachment(RootComp);
	BakedMeshComponent->SetMobility(EComponentMobility::Movable);
	BakedMeshComponent->SetVisibility(false);
	BakedMeshComponent->SetHiddenInGame(false);
	BakedMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	DynamicMeshComponent = CreateDefaultSubobject<UTYWaterfallMeshComponent>(TEXT("DynamicMesh"));
	DynamicMeshComponent->SetupAttachment(RootComp);
	DynamicMeshComponent->SetMobility(EComponentMobility::Movable);

	TopVFXComponent = CreateDefaultSubobject<UTYWaterfallVFXComponent>(TEXT("TopVFX"));
	TopVFXComponent->SetupAttachment(RootComp);
	MiddleVFXComponent = CreateDefaultSubobject<UTYWaterfallVFXComponent>(TEXT("MiddleVFX"));
	MiddleVFXComponent->SetupAttachment(RootComp);
	BottomVFXComponent = CreateDefaultSubobject<UTYWaterfallVFXComponent>(TEXT("BottomVFX"));
	BottomVFXComponent->SetupAttachment(RootComp);

#if WITH_EDITORONLY_DATA
	WaterfallSettings = CreateEditorOnlyDefaultSubobject<UTYWaterfallSettingsComponent>(
		TEXT("WaterfallSettings"));
#endif

#if WITH_EDITORONLY_DATA
	KillPlaneComponent = CreateEditorOnlyDefaultSubobject<UStaticMeshComponent>(TEXT("KillPlane"));
	if (KillPlaneComponent)
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshFinder(
			TEXT("/Engine/BasicShapes/Plane"));
		if (PlaneMeshFinder.Succeeded())
		{
			KillPlaneComponent->SetStaticMesh(PlaneMeshFinder.Object);
		}

		KillPlaneComponent->SetupAttachment(RootComp);
		// Match the default simulation termination height while keeping the plane
		// independently movable from the Details panel.
		KillPlaneComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -1000.0f));
		KillPlaneComponent->SetRelativeScale3D(FVector(20.0f));
		KillPlaneComponent->SetMobility(EComponentMobility::Movable);
		// The mesh is only a visual representation. Simulation performs a robust
		// segment/plane test and does not depend on mesh collision.
		// The editor mode enables this preview only for the currently selected
		// waterfall. Keeping it hidden by default prevents scene clutter.
		KillPlaneComponent->SetVisibility(false);
		KillPlaneComponent->SetHiddenInGame(true);
		KillPlaneComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
#endif

#if WITH_EDITOR
	PathBuilder.Initialize(this);
	MeshBuilder.Initialize(this);
#endif
}

void ATYWaterfallActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

#if WITH_EDITOR
	if (PathBuilder.IsGenerating())
	{
		PathBuilder.TickGeneration();
	}
#endif
}

#if WITH_EDITOR
void ATYWaterfallActor::GeneratePaths()
{
	PathBuilder.StartGeneration();
}

void ATYWaterfallActor::CancelPathGeneration()
{
	PathBuilder.CancelGeneration();
}

void ATYWaterfallActor::ClearGeneratedPaths()
{
	if (PathBuilder.IsGenerating())
	{
		PathBuilder.CancelGeneration();
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT(
		"TYWaterfallTools", "ClearWaterfallPaths", "Clear Waterfall Paths"));
	Modify();
	PathBuilder.ClearGeneratedPaths();
}

void ATYWaterfallActor::GenerateMesh()
{
	MeshBuilder.BuildMesh();
}

void ATYWaterfallActor::GeneratePerPathMesh()
{
	GenerateMesh();
}

void ATYWaterfallActor::ClearDynamicMesh()
{
	const FScopedTransaction Transaction(NSLOCTEXT(
		"TYWaterfallTools", "ClearWaterfallMesh", "Clear Waterfall Mesh"));
	Modify();
	MeshBuilder.ClearMesh();
}

namespace
{
FTYWaterfallFXPointData MakeFXPoint(const FTYWaterfallSample& Sample)
{
	FTYWaterfallFXPointData Result;
	Result.Position = Sample.Position;
	Result.ForwardDirection = Sample.Tangent.GetSafeNormal();

	// Build an orthonormal frame so Niagara meshes and sprites do not inherit
	// skew when collision normals are nearly parallel to the flow direction.
	Result.UpDirection = FVector::VectorPlaneProject(
		Sample.Normal, Result.ForwardDirection).GetSafeNormal();
	if (Result.UpDirection.IsNearlyZero())
	{
		Result.UpDirection = FVector::VectorPlaneProject(
			FVector::UpVector, Result.ForwardDirection).GetSafeNormal();
	}
	if (Result.UpDirection.IsNearlyZero())
	{
		Result.UpDirection = FVector::RightVector;
	}
	Result.RightDirection = FVector::CrossProduct(
		Result.UpDirection, Result.ForwardDirection).GetSafeNormal();
	Result.UpDirection = FVector::CrossProduct(
		Result.ForwardDirection, Result.RightDirection).GetSafeNormal();
	return Result;
}
}

TArray<FTYWaterfallFXPointData> ATYWaterfallActor::GetTopFXPointData() const
{
	TArray<FTYWaterfallFXPointData> Result;
	Result.Reserve(GeneratedPaths.Num());
	for (const UTYWaterfallPathComponent* Path : GeneratedPaths)
	{
		if (IsValid(Path) && !Path->GetResampledSamples().IsEmpty())
		{
			Result.Add(MakeFXPoint(Path->GetResampledSamples()[0]));
		}
	}
	return Result;
}

TArray<FTYWaterfallFXPointData> ATYWaterfallActor::GetMiddleFXPointData() const
{
	TArray<FTYWaterfallFXPointData> Result;
	for (const UTYWaterfallPathComponent* Path : GeneratedPaths)
	{
		if (!IsValid(Path))
		{
			continue;
		}

		const TArray<FTYWaterfallSample>& Samples = Path->GetResampledSamples();
		for (int32 SampleIndex = 1; SampleIndex + 1 < Samples.Num(); ++SampleIndex)
		{
			Result.Add(MakeFXPoint(Samples[SampleIndex]));
		}
	}
	return Result;
}

TArray<FTYWaterfallFXPointData> ATYWaterfallActor::GetBottomFXPointData() const
{
	TArray<FTYWaterfallFXPointData> Result;
	Result.Reserve(GeneratedPaths.Num());
	for (const UTYWaterfallPathComponent* Path : GeneratedPaths)
	{
		if (IsValid(Path) && !Path->GetResampledSamples().IsEmpty())
		{
			Result.Add(MakeFXPoint(Path->GetResampledSamples().Last()));
		}
	}
	return Result;
}

void ATYWaterfallActor::RefreshNiagaraEffects()
{
	if (!IsValid(WaterfallSettings) || GeneratedPaths.IsEmpty())
	{
		ClearNiagaraEffects();
		return;
	}

	Modify();
	for (UTYWaterfallPathComponent* Path : GeneratedPaths)
	{
		if (IsValid(Path))
		{
			Path->BuildResampledSamples(WaterfallSettings->GetNiagaraSampleSpacing());
		}
	}

	const float BoundsPadding = WaterfallSettings->GetNiagaraBoundsPadding();
	auto ConfigureComponent = [BoundsPadding](
		UTYWaterfallVFXComponent* Component,
		const TSoftObjectPtr<UNiagaraSystem>& System,
		const TArray<FTYWaterfallFXPointData>& Points)
	{
		if (!IsValid(Component))
		{
			return;
		}
		Component->Modify();
		Component->SetAsset(System.LoadSynchronous());
		Component->SetPointData(Points, BoundsPadding);
	};

	ConfigureComponent(TopVFXComponent,
		WaterfallSettings->GetTopNiagaraSystem(), GetTopFXPointData());
	ConfigureComponent(MiddleVFXComponent,
		WaterfallSettings->GetMiddleNiagaraSystem(), GetMiddleFXPointData());
	ConfigureComponent(BottomVFXComponent,
		WaterfallSettings->GetBottomNiagaraSystem(), GetBottomFXPointData());
	MarkPackageDirty();
}

void ATYWaterfallActor::ClearNiagaraEffects()
{
	for (UTYWaterfallVFXComponent* Component : {
		TopVFXComponent.Get(), MiddleVFXComponent.Get(), BottomVFXComponent.Get() })
	{
		if (IsValid(Component))
		{
			Component->Modify();
			Component->ClearPointData();
		}
	}
	MarkPackageDirty();
}

void ATYWaterfallActor::SetPathDebugVisible(bool bVisible)
{
	for (UTYWaterfallPathComponent* Path : GeneratedPaths)
	{
		if (IsValid(Path))
		{
			Path->SetDrawDebug(bVisible);
			Path->MarkRenderStateDirty();
		}
	}
}

void ATYWaterfallActor::SetKillPlaneEditorVisible(bool bVisible)
{
	if (IsValid(KillPlaneComponent))
	{
		KillPlaneComponent->SetVisibility(bVisible, false);
		KillPlaneComponent->SetHiddenInGame(true);
	}
}

void ATYWaterfallActor::SetBakedStaticMesh(UStaticMesh* StaticMesh)
{
	if (IsValid(BakedMeshComponent))
	{
		BakedMeshComponent->Modify();
		BakedMeshComponent->SetStaticMesh(StaticMesh);
		BakedMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MarkPackageDirty();
	}
}

void ATYWaterfallActor::SetShowBakedMesh(bool bShowBakedMesh)
{
	const bool bHasBakedMesh = IsValid(BakedMeshComponent)
		&& IsValid(BakedMeshComponent->GetStaticMesh());
	const bool bUseBakedMesh = bShowBakedMesh && bHasBakedMesh;

	if (IsValid(BakedMeshComponent))
	{
		BakedMeshComponent->SetVisibility(bUseBakedMesh);
		BakedMeshComponent->SetHiddenInGame(false);
	}
	if (IsValid(DynamicMeshComponent))
	{
		DynamicMeshComponent->SetVisibility(!bUseBakedMesh
			&& DynamicMeshComponent->GetDynamicMesh()->GetTriangleCount() > 0);
	}
}
#endif
