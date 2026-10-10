// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TYWaterfallSettingsComponent.generated.h"

class UMaterialInterface;
class UNiagaraSystem;

/** Authoring settings shared by all paths generated for one waterfall actor. */
UCLASS(ClassGroup = (Waterfall), meta = (DisplayName = "TY Waterfall Settings"))
class TYWATERFALLTOOLSRUNTIME_API UTYWaterfallSettingsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTYWaterfallSettingsComponent();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	int32 GetNumPaths() const { return FMath::Max(NumPaths, 1); }
	int32 GetSeed() const { return Seed; }
	FVector2D GetSpawnRange() const;
	float GetSpawnJitterDegrees() const { return FMath::Max(SpawnJitterDegrees, 0.0f); }
	bool ShouldReverseFlowDirection() const { return bReverseFlowDirection; }
	int32 GetSimulationStepsPerFrame() const { return FMath::Max(SimulationStepsPerFrame, 1); }
	float GetInitialSpeed() const { return FMath::Max(InitialSpeed, 0.0f); }
	FVector GetGravity() const { return Gravity; }
	float GetDrag() const { return FMath::Clamp(Drag, 0.0f, 1.0f); }
	bool ShouldEnableWorldCollision() const { return bEnableWorldCollision; }
	float GetFixedDeltaTime() const { return FMath::Max(FixedDeltaTime, 0.001f); }
	int32 GetMaxSteps() const { return FMath::Max(MaxSteps, 1); }
	float GetTerminationHeight() const { return TerminationHeight; }
	bool ShouldGenerateSingular() const { return bGenerateSingular; }
	bool ShouldGeneratePerPath() const { return bGeneratePerPath; }
	bool ShouldGenerateCross() const { return bGenerateCross; }
	bool ShouldGenerateSplash() const { return bGenerateSplash; }
	float GetRibbonWidth() const { return FMath::Max(RibbonWidth, 1.0f); }
	float GetCrossWidth() const { return FMath::Max(CrossWidth, 1.0f); }
	float GetBottomWidthScale() const { return FMath::Max(BottomWidthScale, 0.01f); }
	int32 GetPerPathSubdivisions() const { return FMath::Clamp(PerPathSubdivisions, 0, 32); }
	int32 GetCrossSubdivisions() const { return FMath::Clamp(CrossSubdivisions, 0, 32); }
	float GetMeshSampleSpacing() const { return FMath::Max(MeshSampleSpacing, 1.0f); }
	FVector2D GetBaseUVScale() const { return BaseUVScale; }
	float GetSplashFrontRadius() const { return FMath::Max(SplashFrontRadius, 1.0f); }
	float GetSplashBackRadius() const { return FMath::Max(SplashBackRadius, 1.0f); }
	int32 GetSplashRadialSegments() const { return FMath::Clamp(SplashRadialSegments, 3, 128); }
	int32 GetSplashRings() const { return FMath::Clamp(SplashRings, 1, 32); }
	UMaterialInterface* GetSingularMaterial() const { return SingularMaterial; }
	UMaterialInterface* GetPerPathMaterial() const { return PerPathMaterial; }
	UMaterialInterface* GetCrossMaterial() const { return CrossMaterial; }
	UMaterialInterface* GetSplashMaterial() const { return SplashMaterial; }
	const TSoftObjectPtr<UNiagaraSystem>& GetTopNiagaraSystem() const { return TopNiagaraSystem; }
	const TSoftObjectPtr<UNiagaraSystem>& GetMiddleNiagaraSystem() const { return MiddleNiagaraSystem; }
	const TSoftObjectPtr<UNiagaraSystem>& GetBottomNiagaraSystem() const { return BottomNiagaraSystem; }
	float GetNiagaraSampleSpacing() const { return FMath::Max(NiagaraSampleSpacing, 1.0f); }
	float GetNiagaraBoundsPadding() const { return FMath::Max(NiagaraBoundsPadding, 0.0f); }
	bool ShouldShowPathDebug() const { return bShowPathDebug; }
	bool ShouldShowMeshWireframe() const { return bShowMeshWireframe; }
#if WITH_EDITOR
	bool ShouldShowBakedMesh() const { return bShowBakedMesh; }
	void SetShowBakedMesh(bool bInShowBakedMesh);
#endif

protected:
	/** Number of paths distributed across the selected part of the top spline. */
	UPROPERTY(EditAnywhere, Category = "Paths", meta = (ClampMin = "1", ClampMax = "256"))
	int32 NumPaths = 8;

	/** Seed used for deterministic spawn-direction variation. */
	UPROPERTY(EditAnywhere, Category = "Paths")
	int32 Seed = 1337;

	/** Normalized start and end positions along the top spline. */
	UPROPERTY(EditAnywhere, Category = "Paths")
	FVector2D SpawnRange = FVector2D(0.0f, 1.0f);

	/** Maximum random yaw applied to each path's initial direction. */
	UPROPERTY(EditAnywhere, Category = "Paths", meta = (ClampMin = "0.0", ClampMax = "45.0"))
	float SpawnJitterDegrees = 2.0f;

	/** Flips the flow to the opposite side of the top spline. */
	UPROPERTY(EditAnywhere, Category = "Paths")
	bool bReverseFlowDirection = false;

	UPROPERTY(EditAnywhere, Category = "Simulation", meta = (ClampMin = "0.0"))
	float InitialSpeed = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Simulation")
	FVector Gravity = FVector(0.0f, 0.0f, -980.0f);

	UPROPERTY(EditAnywhere, Category = "Simulation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Drag = 0.1f;

	/** Enables collision and sliding against scene geometry. Kill Plane always remains active. */
	UPROPERTY(EditAnywhere, Category = "Simulation", meta = (DisplayName = "Enable World Collision"))
	bool bEnableWorldCollision = true;

	UPROPERTY(EditAnywhere, Category = "Simulation", meta = (ClampMin = "0.001", ClampMax = "0.1"))
	float FixedDeltaTime = 0.016f;

	UPROPERTY(EditAnywhere, Category = "Simulation", meta = (ClampMin = "1"))
	int32 MaxSteps = 80;

	UPROPERTY(EditAnywhere, Category = "Simulation")
	float TerminationHeight = -1000.0f;

	/** Connect all paths into one continuous waterfall sheet. */
	UPROPERTY(EditAnywhere, Category = "Mesh", meta = (DisplayName = "Generate Singular"))
	bool bGenerateSingular = true;

	/** Generate one water-surface ribbon for every simulated path. */
	UPROPERTY(EditAnywhere, Category = "Mesh", meta = (DisplayName = "Generate Per-Path"))
	bool bGeneratePerPath = true;

	/** Generate one perpendicular ribbon for every simulated path. */
	UPROPERTY(EditAnywhere, Category = "Mesh", meta = (DisplayName = "Generate Cross"))
	bool bGenerateCross = true;

	/** Generate a radial splash surface at every path endpoint. */
	UPROPERTY(EditAnywhere, Category = "Mesh", meta = (DisplayName = "Generate Splash"))
	bool bGenerateSplash = true;

	/** Total width of every generated Per Path ribbon, measured in Unreal units. */
	UPROPERTY(EditAnywhere, Category = "Mesh", meta = (ClampMin = "1.0", EditCondition = "bGeneratePerPath", EditConditionHides))
	float RibbonWidth = 50.0f;

	/** Width of the perpendicular Cross plane generated along every path. */
	UPROPERTY(EditAnywhere, Category = "Mesh", meta = (ClampMin = "1.0", EditCondition = "bGenerateCross", EditConditionHides))
	float CrossWidth = 50.0f;

	/** Multiplier applied at the path endpoint; Ribbon/Cross Width remain the top widths. */
	UPROPERTY(EditAnywhere, Category = "Mesh", meta = (DisplayName = "Bottom Width Scale", ClampMin = "0.01", UIMin = "0.1", UIMax = "5.0", EditCondition = "bGeneratePerPath || bGenerateCross", EditConditionHides))
	float BottomWidthScale = 1.0f;

	/** Interior vertices across each Per-Path ribbon, matching WaterfallTools. */
	UPROPERTY(EditAnywhere, Category = "Mesh", meta = (ClampMin = "0", ClampMax = "32", EditCondition = "bGeneratePerPath", EditConditionHides))
	int32 PerPathSubdivisions = 5;

	/** Interior vertices across each Cross plane, matching WaterfallTools. */
	UPROPERTY(EditAnywhere, Category = "Mesh", meta = (ClampMin = "0", ClampMax = "32", EditCondition = "bGenerateCross", EditConditionHides))
	int32 CrossSubdivisions = 2;

	/** Target distance between adjacent ribbon rows. Smaller values create denser meshes. */
	UPROPERTY(EditAnywhere, Category = "Mesh", meta = (ClampMin = "1.0", EditCondition = "bGenerateSingular || bGeneratePerPath || bGenerateCross", EditConditionHides))
	float MeshSampleSpacing = 25.0f;

	/** Multiplies UV0, UV1 and UV2.Y using the WaterfallTools material protocol. */
	UPROPERTY(EditAnywhere, Category = "Mesh", meta = (DisplayName = "Base UV Scale", EditCondition = "bGenerateSingular || bGeneratePerPath || bGenerateCross", EditConditionHides))
	FVector2D BaseUVScale = FVector2D(1.0f, 1.0f);

	/** Distance the splash extends in the incoming flow direction. */
	UPROPERTY(EditAnywhere, Category = "Mesh", meta = (ClampMin = "1.0", EditCondition = "bGenerateSplash", EditConditionHides))
	float SplashFrontRadius = 300.0f;

	/** Distance the splash extends behind the path endpoint. */
	UPROPERTY(EditAnywhere, Category = "Mesh", meta = (ClampMin = "1.0", EditCondition = "bGenerateSplash", EditConditionHides))
	float SplashBackRadius = 75.0f;

	/** Number of vertices around each splash ring. */
	UPROPERTY(EditAnywhere, Category = "Mesh", meta = (ClampMin = "3", ClampMax = "128", EditCondition = "bGenerateSplash", EditConditionHides))
	int32 SplashRadialSegments = 16;

	/** Number of concentric rings between the endpoint and splash perimeter. */
	UPROPERTY(EditAnywhere, Category = "Mesh", meta = (ClampMin = "1", ClampMax = "32", EditCondition = "bGenerateSplash", EditConditionHides))
	int32 SplashRings = 4;

	/** Material used by the continuous Singular sheet in slot 0. */
	UPROPERTY(EditAnywhere, Category = "Material", meta = (EditCondition = "bGenerateSingular", EditConditionHides))
	TObjectPtr<UMaterialInterface> SingularMaterial;

	/** Material used by Per-Path ribbons in slot 1. */
	UPROPERTY(EditAnywhere, Category = "Material", meta = (DisplayName = "Per-Path Material", EditCondition = "bGeneratePerPath", EditConditionHides))
	TObjectPtr<UMaterialInterface> PerPathMaterial;

	/** Material used by perpendicular Cross ribbons in slot 2. */
	UPROPERTY(EditAnywhere, Category = "Material", meta = (EditCondition = "bGenerateCross", EditConditionHides))
	TObjectPtr<UMaterialInterface> CrossMaterial;

	/** Material used by endpoint Splash surfaces in slot 3. */
	UPROPERTY(EditAnywhere, Category = "Material", meta = (EditCondition = "bGenerateSplash", EditConditionHides))
	TObjectPtr<UMaterialInterface> SplashMaterial;

	/** Niagara system fed with the first sample from every generated path. */
	UPROPERTY(EditAnywhere, Category = "Niagara")
	TSoftObjectPtr<UNiagaraSystem> TopNiagaraSystem;

	/** Niagara system fed with distance-spaced interior samples from every path. */
	UPROPERTY(EditAnywhere, Category = "Niagara")
	TSoftObjectPtr<UNiagaraSystem> MiddleNiagaraSystem;

	/** Niagara system fed with the last sample from every generated path. */
	UPROPERTY(EditAnywhere, Category = "Niagara")
	TSoftObjectPtr<UNiagaraSystem> BottomNiagaraSystem;

	/** Distance between adjacent Middle Niagara points along each path. */
	UPROPERTY(EditAnywhere, Category = "Niagara", meta = (ClampMin = "1.0"))
	float NiagaraSampleSpacing = 100.0f;

	/** Extra local-space extent added around Niagara point bounds. */
	UPROPERTY(EditAnywhere, Category = "Niagara", meta = (ClampMin = "0.0"))
	float NiagaraBoundsPadding = 100.0f;

	/** Total simulation steps processed across all paths during one editor frame. */
	UPROPERTY(EditAnywhere, Category = "Performance", meta = (ClampMin = "1", ClampMax = "10000"))
	int32 SimulationStepsPerFrame = 128;

	/** Show each generated path as a spline with its own stable debug color. */
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bShowPathDebug = true;

	/** Draw the generated mesh triangle edges and vertices while authoring. */
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bShowMeshWireframe = false;

	/** Switches the viewport and packaged actor from generated geometry to the baked asset. */
	UPROPERTY(EditAnywhere, Category = "Bake", meta = (DisplayName = "Show Baked Mesh"))
	bool bShowBakedMesh = false;
};
