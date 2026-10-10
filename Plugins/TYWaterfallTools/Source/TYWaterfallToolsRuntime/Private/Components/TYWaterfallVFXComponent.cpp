// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/TYWaterfallVFXComponent.h"

#include "NiagaraDataInterfaceArrayFunctionLibrary.h"

namespace
{
const FName PositionArrayName(TEXT("User.TY_PositionArray"));
const FName ForwardArrayName(TEXT("User.TY_ForwardArray"));
const FName UpArrayName(TEXT("User.TY_UpArray"));
const FName RightArrayName(TEXT("User.TY_RightArray"));
}

UTYWaterfallVFXComponent::UTYWaterfallVFXComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = false;
	SetUsingAbsoluteLocation(false);
	SetUsingAbsoluteRotation(false);
	SetUsingAbsoluteScale(false);
}

void UTYWaterfallVFXComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshNiagaraParameters();
}

void UTYWaterfallVFXComponent::SetPointData(
	const TArray<FTYWaterfallFXPointData>& InPointData,
	float BoundsPadding)
{
	StoredBoundsPadding = FMath::Max(BoundsPadding, 0.0f);
	PointData.Reset(InPointData.Num());
	const FTransform WorldToComponent = GetComponentTransform().Inverse();
	for (const FTYWaterfallFXPointData& WorldPoint : InPointData)
	{
		FTYWaterfallFXPointData& LocalPoint = PointData.AddDefaulted_GetRef();
		LocalPoint = WorldPoint;
		LocalPoint.Position = WorldToComponent.TransformPosition(WorldPoint.Position);
		LocalPoint.ForwardDirection = WorldToComponent.TransformVectorNoScale(
			WorldPoint.ForwardDirection).GetSafeNormal();
		LocalPoint.UpDirection = WorldToComponent.TransformVectorNoScale(
			WorldPoint.UpDirection).GetSafeNormal();
		LocalPoint.RightDirection = WorldToComponent.TransformVectorNoScale(
			WorldPoint.RightDirection).GetSafeNormal();
	}
	RefreshNiagaraParameters();
}

void UTYWaterfallVFXComponent::ClearPointData()
{
	PointData.Reset();
	RefreshNiagaraParameters();
}

void UTYWaterfallVFXComponent::RefreshNiagaraParameters()
{
	if (IsTemplate())
	{
		return;
	}

	TArray<FVector> Positions;
	TArray<FVector> ForwardDirections;
	TArray<FVector> UpDirections;
	TArray<FVector> RightDirections;
	Positions.Reserve(PointData.Num());
	ForwardDirections.Reserve(PointData.Num());
	UpDirections.Reserve(PointData.Num());
	RightDirections.Reserve(PointData.Num());

	FBox LocalBounds(ForceInit);
	for (const FTYWaterfallFXPointData& Point : PointData)
	{
		Positions.Add(Point.Position);
		ForwardDirections.Add(Point.ForwardDirection);
		UpDirections.Add(Point.UpDirection);
		RightDirections.Add(Point.RightDirection);
		LocalBounds += Point.Position;
	}

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		this, PositionArrayName, Positions);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		this, ForwardArrayName, ForwardDirections);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		this, UpArrayName, UpDirections);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		this, RightArrayName, RightDirections);

	if (!PointData.IsEmpty())
	{
		SetSystemFixedBounds(LocalBounds.ExpandBy(StoredBoundsPadding));
	}
	else
	{
		SetSystemFixedBounds(FBox(ForceInit));
	}

	const bool bCanRun = GetAsset() != nullptr && !PointData.IsEmpty();
	SetVisibility(bCanRun, true);
	if (bCanRun)
	{
		Activate(true);
	}
	else
	{
		DeactivateImmediate();
	}
}
