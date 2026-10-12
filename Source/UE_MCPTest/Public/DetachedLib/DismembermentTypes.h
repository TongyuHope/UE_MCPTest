#pragma once

#include "CoreMinimal.h"
#include "DismembermentTypes.generated.h"

// Keep the original WorldSpace/BoneSpace values for serialized Blueprint data.
UENUM(BlueprintType)
enum class ELimbSpace : uint8
{
	WorldSpace,
	BoneSpace,
	CharacterSpace
};

UENUM(BlueprintType)
enum class ELimbRemovalMode : uint8
{
	HideBone,
	DestroyWholeMeshComponent
};

/** Cleanup of the detached Actor, independently of host removal and physics timers. */
UENUM(BlueprintType)
enum class ELimbCleanupMode : uint8
{
	Time,
	Distance
};

/** Runtime snapshot copied from the spawning component; shared assets are unchanged. */
struct UE_MCPTEST_API FDismembermentCleanupSettings
{
	ELimbCleanupMode Mode = ELimbCleanupMode::Time;
	float MaxLifeSpan = 30.0f;
	float Distance = 3000.0f;
	float CheckInterval = 1.0f;
	float OutOfRangeDuration = 3.0f;

	bool Validate(FString& OutError) const;
};

UENUM(BlueprintType)
enum class EDismembermentLimbMeshTypeOverride : uint8
{
	UseAssetDefault,
	StaticMesh,
	SkeletalMesh
};

/** Final parameters. Also preserves the original Blueprint-callable API's struct. */
USTRUCT(BlueprintType)
struct UE_MCPTEST_API FDismembermentConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment")
	ELimbRemovalMode RemovalMode = ELimbRemovalMode::HideBone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment")
	FName TargetSkeletalMeshComponentName;

	// Retain the original property name for loading old assets and Blueprint pins.
	// New editors use RemovalMode; the compatibility entry point still honors true.
	UPROPERTY(BlueprintReadWrite, Category = "Dismemberment|Legacy")
	bool bHideWholeTargetMesh = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment|Impact")
	FVector ImpactDirection = FVector::ZeroVector;

	/** Linear velocity change, independent of mass. Zero is a valid override. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment|Impact")
	float ImpactForce = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment|Impact")
	FVector AngularImpulseDegrees = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment|Impact")
	ELimbSpace LimbSpace = ELimbSpace::BoneSpace;

	/** Angular velocity change uses a separate space; old assets remain world-space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment|Impact", meta = (AdvancedDisplay))
	ELimbSpace AngularImpulseSpace = ELimbSpace::WorldSpace;

	/** Inherit the cut transform's scale for the spawned model. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment", meta = (AdvancedDisplay))
	bool bUseWorldScale = true;

	ELimbRemovalMode GetRemovalMode() const
	{
		return bHideWholeTargetMesh ? ELimbRemovalMode::DestroyWholeMeshComponent : RemovalMode;
	}
};

/** A named presentation preset only edits impact, never the host/mesh removal target. */
USTRUCT(BlueprintType)
struct UE_MCPTEST_API FDismembermentImpactPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment")
	FVector ImpactDirection = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment")
	float ImpactForce = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment")
	FVector AngularImpulseDegrees = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment")
	ELimbSpace LimbSpace = ELimbSpace::BoneSpace;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment", meta = (AdvancedDisplay))
	ELimbSpace AngularImpulseSpace = ELimbSpace::WorldSpace;

	void ApplyTo(FDismembermentConfig& Config) const
	{
		Config.ImpactDirection = ImpactDirection;
		Config.ImpactForce = ImpactForce;
		Config.AngularImpulseDegrees = AngularImpulseDegrees;
		Config.LimbSpace = LimbSpace;
		Config.AngularImpulseSpace = AngularImpulseSpace;
	}
};

/** Select a registered part/preset, with explicit optional overrides. */
USTRUCT(BlueprintType)
struct UE_MCPTEST_API FDismembermentRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment", meta = (GetOptions = "GetBoneNameOptions"))
	FName BoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment", meta = (GetOptions = "GetPresetNameOptions"))
	FName PresetName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment", meta = (AdvancedDisplay))
	bool bOverrideImpact = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment", meta = (AdvancedDisplay))
	bool bOverrideRemovalMode = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment", meta = (AdvancedDisplay))
	bool bOverrideTargetMesh = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment", meta = (AdvancedDisplay))
	bool bOverrideWorldScale = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment", meta = (AdvancedDisplay,
		EditCondition = "bOverrideImpact || bOverrideRemovalMode || bOverrideTargetMesh || bOverrideWorldScale"))
	FDismembermentConfig OverrideConfig;
};
