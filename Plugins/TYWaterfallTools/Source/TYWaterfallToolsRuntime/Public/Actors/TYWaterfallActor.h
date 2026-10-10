// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/TYWaterfallFXPointData.h"
#include "GameFramework/Actor.h"
#include "Generation/TYWaterfallMeshBuilder.h"
#include "Generation/TYWaterfallPathBuilder.h"
#include "TYWaterfallActor.generated.h"

class USceneComponent;
class USplineComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UTYWaterfallMeshComponent;
class UTYWaterfallPathComponent;
class UTYWaterfallSettingsComponent;
class UTYWaterfallVFXComponent;

/** Base actor that owns the editable waterfall authoring components. */
UCLASS(Blueprintable, meta = (DisplayName = "TY Waterfall"))
class TYWATERFALLTOOLSRUNTIME_API ATYWaterfallActor : public AActor
{
	GENERATED_BODY()

public:
	ATYWaterfallActor();
	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }
#endif

	UFUNCTION(BlueprintPure, Category = "Waterfall|Components")
	USplineComponent* GetTopSpline() const { return TopSpline; }

	UFUNCTION(BlueprintPure, Category = "Waterfall|Components")
	UStaticMeshComponent* GetBakedMeshComponent() const { return BakedMeshComponent; }

	UFUNCTION(BlueprintPure, Category = "Waterfall|Components")
	UTYWaterfallMeshComponent* GetDynamicMeshComponent() const { return DynamicMeshComponent; }

	UFUNCTION(BlueprintPure, Category = "Waterfall|Components")
	UTYWaterfallVFXComponent* GetTopVFXComponent() const { return TopVFXComponent; }

	UFUNCTION(BlueprintPure, Category = "Waterfall|Components")
	UTYWaterfallVFXComponent* GetMiddleVFXComponent() const { return MiddleVFXComponent; }

	UFUNCTION(BlueprintPure, Category = "Waterfall|Components")
	UTYWaterfallVFXComponent* GetBottomVFXComponent() const { return BottomVFXComponent; }

#if WITH_EDITOR
	/** Creates and starts frame-budgeted generation of all configured paths. */
	UFUNCTION(CallInEditor, Category = "Waterfall|Simulation")
	void GeneratePaths();

	/** Cancels generation and removes incomplete paths. */
	UFUNCTION(CallInEditor, Category = "Waterfall|Simulation")
	void CancelPathGeneration();

	/** Removes all generated path components. */
	UFUNCTION(CallInEditor, Category = "Waterfall|Simulation")
	void ClearGeneratedPaths();

	UFUNCTION(BlueprintPure, Category = "Waterfall|Simulation")
	bool IsGeneratingPaths() const { return PathBuilder.IsGenerating(); }

	UFUNCTION(BlueprintPure, Category = "Waterfall|Simulation")
	float GetPathGenerationProgress() const { return PathBuilder.GetProgress(); }

	/** Converts all completed paths into the combined waterfall mesh. */
	UFUNCTION(CallInEditor, Category = "Waterfall|Mesh")
	void GenerateMesh();

	/** Compatibility entry point retained for existing editor scripts and assets. */
	UFUNCTION(BlueprintCallable, Category = "Waterfall|Mesh",
		meta = (DeprecatedFunction, DeprecationMessage = "Use GenerateMesh instead."))
	void GeneratePerPathMesh();

	/** Removes the generated dynamic mesh without deleting the source paths. */
	UFUNCTION(CallInEditor, Category = "Waterfall|Mesh")
	void ClearDynamicMesh();

	/** Rebuilds Top, Middle and Bottom Niagara point arrays from generated paths. */
	UFUNCTION(CallInEditor, Category = "Waterfall|FX")
	void RefreshNiagaraEffects();

	/** Clears Niagara point arrays and deactivates all three systems. */
	UFUNCTION(CallInEditor, Category = "Waterfall|FX")
	void ClearNiagaraEffects();

	TArray<FTYWaterfallFXPointData> GetTopFXPointData() const;
	TArray<FTYWaterfallFXPointData> GetMiddleFXPointData() const;
	TArray<FTYWaterfallFXPointData> GetBottomFXPointData() const;
#endif

#if WITH_EDITOR
	UFUNCTION(BlueprintPure, Category = "Waterfall|Components")
	UStaticMeshComponent* GetKillPlaneComponent() const { return KillPlaneComponent; }

	UTYWaterfallSettingsComponent* GetWaterfallSettings() const { return WaterfallSettings; }
	void SetPathDebugVisible(bool bVisible);
	/** Shows the authoring plane only while the Waterfall editor mode owns this actor. */
	void SetKillPlaneEditorVisible(bool bVisible);
	/** Assigns the generated asset and switches component visibility without rebuilding geometry. */
	void SetBakedStaticMesh(UStaticMesh* StaticMesh);
	void SetShowBakedMesh(bool bShowBakedMesh);
#endif

protected:
	friend struct FTYWaterfallPathBuilder;
	friend struct FTYWaterfallMeshBuilder;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> RootComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USplineComponent> TopSpline;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> BakedMeshComponent;

	/** Runtime-visible preview mesh; stage 7 will bake this into BakedMeshComponent. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTYWaterfallMeshComponent> DynamicMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTYWaterfallVFXComponent> TopVFXComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTYWaterfallVFXComponent> MiddleVFXComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTYWaterfallVFXComponent> BottomVFXComponent;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTYWaterfallSettingsComponent> WaterfallSettings;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "Generated Paths", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UTYWaterfallPathComponent>> GeneratedPaths;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kill Plane", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> KillPlaneComponent;
#endif

#if WITH_EDITOR
	FTYWaterfallPathBuilder PathBuilder;
	FTYWaterfallMeshBuilder MeshBuilder;
#endif
};
