// Registered parts: static/skeletal candidates, removal defaults and named impact presets.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "DetachedLib/DismembermentTypes.h"
#include "DismembermentConfigData.generated.h"

class UStaticMesh;
class USkeletalMesh;

UENUM(BlueprintType)
enum class EDismembermentLimbMeshType : uint8
{
	StaticMesh,
	SkeletalMesh
};

/**
 * 单个骨骼部位的断肢配置：对应"掉落的断肢模型"列表。
 * 可以为一个部位配置多个候选模型，断肢时随机选取一个。
 */
USTRUCT(BlueprintType)
struct UE_MCPTEST_API FSKMDismembermentConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment")
	TArray<TObjectPtr<UStaticMesh>> DismembermentMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment")
	TArray<TObjectPtr<USkeletalMesh>> DismembermentSkeletalMeshes;

	/** Defaults used by registered requests; old direct calls retain their parameters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment")
	FDismembermentConfig DefaultConfig;

	/** Named impact variations. Removal/target still come from DefaultConfig. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment")
	TMap<FName, FDismembermentImpactPreset> ImpactPresets;

	bool ValidForMeshType(EDismembermentLimbMeshType MeshType) const;

	bool Valid() const
	{
		for (const TObjectPtr<UStaticMesh>& Mesh : DismembermentMeshes)
		{
			if (IsValid(Mesh)) return true;
		}
		for (const TObjectPtr<USkeletalMesh>& Mesh : DismembermentSkeletalMeshes)
		{
			if (IsValid(Mesh)) return true;
		}
		return false;
	}

	USkeletalMesh* GetRandomDismembermentSkeletalMesh() const
	{
		// 随机选择有效资源，避免数组中存在空引用时偶尔生成失败。
		int32 ValidCount = 0;
		for (const TObjectPtr<USkeletalMesh>& Mesh : DismembermentSkeletalMeshes)
		{
			if (IsValid(Mesh)) ++ValidCount;
		}
		if (ValidCount == 0) return nullptr;

		int32 SelectedValidIndex = FMath::RandRange(0, ValidCount - 1);
		for (const TObjectPtr<USkeletalMesh>& Mesh : DismembermentSkeletalMeshes)
		{
			if (IsValid(Mesh) && SelectedValidIndex-- == 0) return Mesh;
		}
		return nullptr;
	}

	UStaticMesh* GetRandomDismembermentMesh() const
	{
		int32 ValidCount = 0;
		for (const TObjectPtr<UStaticMesh>& Mesh : DismembermentMeshes)
		{
			if (IsValid(Mesh)) ++ValidCount;
		}
		if (ValidCount == 0) return nullptr;

		int32 SelectedValidIndex = FMath::RandRange(0, ValidCount - 1);
		for (const TObjectPtr<UStaticMesh>& Mesh : DismembermentMeshes)
		{
			if (IsValid(Mesh) && SelectedValidIndex-- == 0) return Mesh;
		}
		return nullptr;
	}
};

/**
 * 断肢配置数据资产。
 * 在内容浏览器中通过「右键 -> Miscellaneous -> Data Asset -> DismembermentConfigData」创建。
 */
UCLASS(BlueprintType)
class UE_MCPTEST_API UDismembermentConfigData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Resolve defaults -> preset impact -> explicit overrides. Never mutates the asset. */
	bool ResolveRequest(const FDismembermentRequest& Request, FDismembermentConfig& OutConfig,
		FString& OutError) const;

	/** Asset-only validation. Host skeleton/component checks belong to the component. */
	void GetConfigurationErrors(TArray<FString>& OutErrors) const;

	static bool ValidateMesh(UStaticMesh* Mesh, FString& OutError);
	static bool ValidateMesh(USkeletalMesh* Mesh, FString& OutError);
	static bool ValidateConfig(const FDismembermentConfig& Config, FString& OutError);
	static bool ValidateCollisionProfile(FName ProfileName, FString& OutError);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** 骨骼名 -> 断肢配置。只有出现在这里的骨骼才能被断肢。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment")
	TMap<FName, FSKMDismembermentConfig> DismembermentConfigs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment")
	EDismembermentLimbMeshType LimbMeshType = EDismembermentLimbMeshType::StaticMesh;

	/** Avoidance ends on a timer, independently of landing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dismemberment")
	FCollisionProfileName DismembermentLimbCollisionProfile = FCollisionProfileName(TEXT("BlockAllDynamic"));
};

