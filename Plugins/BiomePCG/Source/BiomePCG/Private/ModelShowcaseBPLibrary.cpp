// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModelShowcaseBPLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "EditorUtilityLibrary.h"
#include "Engine/Selection.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextRenderActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Internationalization/Text.h"
#if WITH_EDITOR

struct FShowcaseRow
{
	FString FolderName;
	FString FolderPath;
	TArray<UStaticMesh*> Meshes;
};

/** 自动适配时每个模型的扩展信息 */
struct FMeshAutoAdaptInfo
{
	int32 LongestAxis = 2;       // 0=X, 1=Y, 2=Z
	float LongestSize = 0.0f;
	bool bNeedsRotation = false; // X或Y为最长且与行轴不同时需旋转
	int32 RowAxisBoundIdx = 1;  // 行方向间距用此轴的 Bound
	int32 ColAxisBoundIdx = 0;  // 列方向用此轴的 Bound
	float RotationYaw = 0.0f;  // Z 轴旋转角度（度）
};

/** Content Browser 返回 /All/Game/...，AssetRegistry 需要 /Game/... */
static FString NormalizeContentPath(const FString& Path)
{
	FString Result = Path.Replace(TEXT("\\"), TEXT("/"));
	if (Result.StartsWith(TEXT("/All/")))
	{
		Result = Result.Mid(5);  // 去掉 "/All/"
	}
	else if (Result.StartsWith(TEXT("All/")))
	{
		Result = Result.Mid(4);  // 去掉 "All/"
	}
	if (!Result.StartsWith(TEXT("/")))
	{
		Result = TEXT("/") + Result;
	}
	if (Result.EndsWith(TEXT("/")))
	{
		Result = Result.LeftChop(1);
	}
	return Result;
}

static void GatherShowcaseRowsRecursive(
	const FString& RootPath,
	const FString& CurrentPath,
	const TMap<FString, TArray<UStaticMesh*>>& PathToMeshes,
	TArray<FShowcaseRow>& OutRows)
{
	TArray<UStaticMesh*> DirectMeshes;
	if (const TArray<UStaticMesh*>* Found = PathToMeshes.Find(CurrentPath))
	{
		DirectMeshes = *Found;
	}

	TSet<FString> SubFolders;
	for (const auto& Pair : PathToMeshes)
	{
		FString Path = Pair.Key;
		if (Path.StartsWith(CurrentPath) && Path.Len() > CurrentPath.Len())
		{
			FString Rest = Path.Mid(CurrentPath.Len());
			if (Rest.StartsWith(TEXT("/"))) Rest = Rest.Mid(1);
			int32 SlashPos;
			if (Rest.FindChar(TEXT('/'), SlashPos)) Rest = Rest.Left(SlashPos);
			if (!Rest.IsEmpty())
			{
				SubFolders.Add(CurrentPath + TEXT("/") + Rest);
			}
		}
	}

	if (DirectMeshes.Num() > 0)
	{
		FShowcaseRow Row;
		Row.FolderPath = CurrentPath;
		Row.FolderName = FPaths::GetCleanFilename(CurrentPath);
		if (Row.FolderName.IsEmpty()) Row.FolderName = TEXT("Root");
		Row.Meshes = DirectMeshes;
		OutRows.Add(Row);
	}

	TArray<FString> SubFolderArray = SubFolders.Array();
	SubFolderArray.Sort();
	for (const FString& SubPath : SubFolderArray)
	{
		GatherShowcaseRowsRecursive(RootPath, SubPath, PathToMeshes, OutRows);
	}
}

static void GetMeshBoundsExtent(UStaticMesh* Mesh, FVector& OutExtent)
{
	if (!Mesh)
	{
		OutExtent = FVector::ZeroVector;
		return;
	}
	FBox Box = Mesh->GetBoundingBox();
	OutExtent = Box.GetExtent();  // 半尺寸
}

static float GetMeshHeightMeters(UStaticMesh* Mesh)
{
	if (!Mesh) return 0.0f;
	FBox Box = Mesh->GetBoundingBox();
	return Box.GetSize().Z / 100.0f;
}

/** 获取模型在指定轴上的半尺寸（用于间距计算）*/
static float GetMeshExtentOnAxis(UStaticMesh* Mesh, int32 Axis)
{
	if (!Mesh) return 0.0f;
	FVector Extent;
	GetMeshBoundsExtent(Mesh, Extent);
	if (Axis == 0) return Extent.X;
	if (Axis == 1) return Extent.Y;
	return Extent.Z;
}

/** 获取模型在指定轴上的全尺寸 (|BoundMin|+|BoundMax|) = 2*extent */
static float GetMeshFullSizeOnAxis(UStaticMesh* Mesh, int32 Axis)
{
	return GetMeshExtentOnAxis(Mesh, Axis) * 2.0f;
}

/** EShowcaseSortAxis -> Axis index (0=X, 1=Y, 2=Z)；Auto 返回 -1 */
static int32 SortAxisToIndex(EShowcaseSortAxis Axis)
{
	switch (Axis)
	{
		case EShowcaseSortAxis::Auto: return -1;
		case EShowcaseSortAxis::X: return 0;
		case EShowcaseSortAxis::Y: return 1;
		case EShowcaseSortAxis::Z: return 2;
	}
	return -1;
}

/** 自动适配：获取模型最长轴 (0=X,1=Y,2=Z) 及该轴的 |BoundMax|+|BoundMin| */
static void GetMeshLongestAxis(UStaticMesh* Mesh, int32& OutLongestAxis, float& OutLongestSize)
{
	OutLongestAxis = 2;
	OutLongestSize = 0.0f;
	if (!Mesh) return;
	FBox Box = Mesh->GetBoundingBox();
	float Sizes[3] = {
		FMath::Abs(Box.Min.X) + FMath::Abs(Box.Max.X),
		FMath::Abs(Box.Min.Y) + FMath::Abs(Box.Max.Y),
		FMath::Abs(Box.Min.Z) + FMath::Abs(Box.Max.Z)
	};
	for (int32 i = 0; i < 3; i++)
	{
		if (Sizes[i] > OutLongestSize)
		{
			OutLongestSize = Sizes[i];
			OutLongestAxis = i;
		}
	}
}

/** 获取模型 Z 高度（UE 单位 cm）*/
static float GetMeshHeightCm(UStaticMesh* Mesh)
{
	if (!Mesh) return 0.0f;
	return Mesh->GetBoundingBox().GetSize().Z;
}

/** 所有 Text 绕 Z 轴（高度轴）旋转 180°；文件夹名在 X 为行轴时旋转 90° */
static const FRotator TextZRotation180(0.0f, 180.0f, 0.0f);
static const FRotator TextZRotation90(0.0f, 90.0f, 0.0f);

/** 生成高度 Text，按公式 scale = (模型bound|max|+|min|) / (text bound|max|+|min|) 适配模型；ForcedScale>0 时强制使用；ModelRowFullOverride>=0 时覆盖行轴尺寸（自动适配旋转模型用）*/
static AActor* SpawnHeightTextActor(UWorld* World, FVector Location, const FString& Text, UStaticMesh* ModelMesh,
	const FColor& TextColor, int32 RowAxisIdx, float ForcedScale = 0.0f, bool bCenterAxisHeight = false, float OriginZ = 0.0f, float ModelRowFullOverride = -1.0f)
{
	if (!World || !ModelMesh) return nullptr;

	FBox ModelBox = ModelMesh->GetBoundingBox();
	float ModelRowFull = (ModelRowFullOverride >= 0.0f) ? ModelRowFullOverride : GetMeshFullSizeOnAxis(ModelMesh, RowAxisIdx);

	// 先于初步位置生成，再根据 text bounds 计算最终 Z
	Location.Z = OriginZ + 100.0f;  // 临时 Z
	const FRotator& HeightTextRotation = (RowAxisIdx == 0) ? TextZRotation90 : TextZRotation180;
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATextRenderActor* TextActor = World->SpawnActor<ATextRenderActor>(ATextRenderActor::StaticClass(), Location, HeightTextRotation, SpawnParams);
	if (!TextActor || !TextActor->GetTextRender()) return nullptr;

	TextActor->GetTextRender()->SetText(FText::FromString(Text));
	TextActor->GetTextRender()->SetWorldSize(50.0f);
	TextActor->GetTextRender()->SetHorizontalAlignment(EHTA_Center);
	TextActor->GetTextRender()->SetVerticalAlignment(EVRTA_TextBottom);
	TextActor->GetTextRender()->SetTextRenderColor(TextColor);
	TextActor->SetActorScale3D(FVector(1.0f));
	TextActor->GetTextRender()->UpdateBounds();

	FVector TextOrigin, TextBoxExtent;
	TextActor->GetActorBounds(false, TextOrigin, TextBoxExtent);
	float TextBoundMinZAbs = TextBoxExtent.Z;  // |text.Bound.Min.z|，中心在原点时 Min.z = -Extent

	// location.z：勾选中心轴高度 = |模型BoundMax.z| + (|text.BoundMin.z|*1.1)；不勾选 = (|模型Max|+|模型Min|) + (|text.BoundMin.z|*1.1)
	float ModelMaxZAbs = FMath::Abs(ModelBox.Max.Z);
	float ModelMinZAbs = FMath::Abs(ModelBox.Min.Z);
	if (bCenterAxisHeight)
	{
		Location.Z = ModelMaxZAbs;
	}
	else
	{
		Location.Z = (ModelMaxZAbs + ModelMinZAbs);
	}
	// 叠加 Plan 的 Z
	Location.Z += OriginZ;
	TextActor->SetActorLocation(Location);

	float Scale = 1.0f;
	if (ForcedScale > 0.0f)
	{
		Scale = ForcedScale;
	}
	else
	{
		float TextRowFull = TextBoxExtent[RowAxisIdx] * 2.0f;
		if (TextRowFull > 0.01f)
			Scale = ModelRowFull / TextRowFull;
		else
			Scale = ModelRowFull / 50.0f;
	}
	TextActor->SetActorScale3D(FVector(Scale));
	return TextActor;
}

static AActor* SpawnTextActor(UWorld* World, const FVector& Location, const FString& Text, float WorldSize, const FColor& TextColor, bool bCenterAlign = false, float XScaleOverride = 0.0f, float ScaleMultiplier = 1.0f, bool bFolderName = false, int32 RowAxisIdx = 1)
{
	if (!World) return nullptr;

	// 文件夹名：X 为行轴时 Z 轴旋转 90°，否则 180°
	const FRotator& TextRotation = (bFolderName && RowAxisIdx == 0) ? TextZRotation90 : TextZRotation180;
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATextRenderActor* TextActor = World->SpawnActor<ATextRenderActor>(ATextRenderActor::StaticClass(), Location, TextRotation, SpawnParams);
	if (TextActor && TextActor->GetTextRender())
	{
		TextActor->GetTextRender()->SetText(FText::FromString(Text));
		TextActor->GetTextRender()->SetWorldSize(WorldSize);
		// 文件夹名旋转 180° 后 Horizontal 改为 Right，其他 Text 保持 Center 或 Left
		TextActor->GetTextRender()->SetHorizontalAlignment(bFolderName ? EHTA_Right : (bCenterAlign ? EHTA_Center : EHTA_Left));
		TextActor->GetTextRender()->SetVerticalAlignment(bCenterAlign ? EVRTA_TextCenter : EVRTA_TextBottom);
		TextActor->GetTextRender()->SetTextRenderColor(TextColor);
		if (XScaleOverride > 0.01f)
		{
			TextActor->GetTextRender()->SetXScale(XScaleOverride);
		}
		TextActor->SetActorScale3D(FVector(0.1f * ScaleMultiplier));
	}
	return TextActor;
}

static AActor* SpawnStaticMeshActor(UWorld* World, const FVector& Location, UStaticMesh* Mesh, float ScaleToHeightMeters = 0.0f, const FRotator& Rotation = FRotator::ZeroRotator)
{
	if (!World || !Mesh) return nullptr;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
	if (MeshActor && MeshActor->GetStaticMeshComponent())
	{
		MeshActor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
		if (ScaleToHeightMeters > 0.0f)
		{
			float CurrentHeightM = GetMeshHeightMeters(Mesh);
			if (CurrentHeightM > 0.0001f)
			{
				float Scale = ScaleToHeightMeters / CurrentHeightM;
				MeshActor->SetActorScale3D(FVector(Scale));
			}
		}
	}
	return MeshActor;
}

/** 获取唯一的总组名：若 Showcase 已存在则用 Showcase_v02, Showcase_v03 ... */
static FString GetUniqueShowcaseRootName(UWorld* World)
{
	static const FString BaseName = TEXT("Showcase");
	TSet<FString> UsedRoots;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		FName FolderPath = (*It)->GetFolderPath();
		if (FolderPath.IsNone()) continue;
		FString PathStr = FolderPath.ToString();
		int32 SlashPos;
		if (PathStr.FindChar(TEXT('/'), SlashPos))
		{
			PathStr = PathStr.Left(SlashPos);
		}
		if (PathStr.StartsWith(BaseName))
		{
			UsedRoots.Add(PathStr);
		}
	}

	FString Candidate = BaseName;
	if (!UsedRoots.Contains(Candidate)) return Candidate;

	for (int32 V = 2; V < 1000; V++)
	{
		Candidate = FString::Printf(TEXT("%s_v%02d"), *BaseName, V);
		if (!UsedRoots.Contains(Candidate)) return Candidate;
	}
	return BaseName + TEXT("_v99");
}

/** 每列（文件夹）的文字颜色 */
static const TArray<FLinearColor> ColumnColors = {
	FLinearColor(1.0f, 0.2f, 0.2f),   // 红
	FLinearColor(0.2f, 0.7f, 0.2f),   // 绿
	FLinearColor(0.2f, 0.4f, 1.0f),   // 蓝
	FLinearColor(1.0f, 0.6f, 0.0f),   // 橙
	FLinearColor(0.8f, 0.2f, 0.8f),   // 紫
	FLinearColor(0.0f, 0.8f, 0.8f),   // 青
	FLinearColor(1.0f, 0.8f, 0.2f),   // 黄
	FLinearColor(0.5f, 0.5f, 0.5f),   // 灰
};

#endif // WITH_EDITOR

int32 UModelShowcaseBPLibrary::CreateModelShowcaseSimple(
	const FString& ContentFolderPath,
	AActor* PlanActor)
{
	TArray<FString> Paths;
	if (!ContentFolderPath.IsEmpty())
	{
		Paths.Add(ContentFolderPath);
	}
	return CreateModelShowcase(Paths, PlanActor, EShowcaseRowAxis::Y, EShowcaseSortAxis::Auto);
}

int32 UModelShowcaseBPLibrary::CreateModelShowcase(
	const TArray<FString>& ContentFolderPaths,
	AActor* PlanActor,
	EShowcaseRowAxis RowAxis,
	EShowcaseSortAxis SortAxis)
{
#if WITH_EDITOR
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return 0;

	TArray<FString> FolderPaths = ContentFolderPaths;
	if (FolderPaths.Num() == 0)
	{
		FolderPaths = UEditorUtilityLibrary::GetSelectedFolderPaths();
	}
	if (FolderPaths.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ModelShowcase: 请先在 Content Browser 中选择一个或多个文件夹"));
		return 0;
	}

	if (!PlanActor)
	{
		USelection* Sel = GEditor->GetSelectedActors();
		TArray<AActor*> Actors;
		Sel->GetSelectedObjects<AActor>(Actors);
		if (Actors.Num() > 0) PlanActor = Actors[0];
	}
	if (!PlanActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("ModelShowcase: 请先在 World Outliner 中选择一个 Actor 作为排布原点"));
		return 0;
	}

	FVector Origin = PlanActor->GetActorLocation();

	// 行轴：0=Y, 1=X；列轴相反
	int32 RowAxisIdx = (RowAxis == EShowcaseRowAxis::Y) ? 1 : 0;  // Y=1, X=0
	int32 ColAxisIdx = 1 - RowAxisIdx;
	int32 SortAxisIdx = SortAxisToIndex(SortAxis);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TMap<FString, TArray<UStaticMesh*>> PathToMeshes;

	for (const FString& RawPath : FolderPaths)
	{
		FString FolderPath = NormalizeContentPath(RawPath);

		TArray<FAssetData> AssetDataList;
		AssetRegistry.GetAssetsByPath(FName(*FolderPath), AssetDataList, true);

		for (const FAssetData& Data : AssetDataList)
		{
			if (Data.AssetClassPath.GetAssetName() != FName("StaticMesh")) continue;

			UStaticMesh* Mesh = Cast<UStaticMesh>(Data.GetAsset());
			if (!Mesh) continue;

			FString PackagePath = Data.PackagePath.ToString();
			PathToMeshes.FindOrAdd(PackagePath).Add(Mesh);
		}
	}

	if (PathToMeshes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ModelShowcase: 在所选文件夹下未找到 StaticMesh"));
		return 0;
	}

	// 收集所有根路径（保持多选顺序）
	TArray<FString> RootPathsOrdered;
	TSet<FString> RootPathsSet;
	for (const FString& RawPath : FolderPaths)
	{
		FString Normalized = NormalizeContentPath(RawPath);
		if (!RootPathsSet.Contains(Normalized))
		{
			RootPathsSet.Add(Normalized);
			RootPathsOrdered.Add(Normalized);
		}
	}

	TArray<FShowcaseRow> Rows;
	TSet<FString> SeenPaths;
	for (const FString& RootPath : RootPathsOrdered)
	{
		TArray<FShowcaseRow> RowsFromRoot;
		GatherShowcaseRowsRecursive(RootPath, RootPath, PathToMeshes, RowsFromRoot);
		for (FShowcaseRow& R : RowsFromRoot)
		{
			if (!SeenPaths.Contains(R.FolderPath))
			{
				SeenPaths.Add(R.FolderPath);
				Rows.Add(MoveTemp(R));
			}
		}
	}

	if (Rows.Num() == 0) return 0;

	const bool bSortAxisAuto = (SortAxisIdx < 0);
	// 自动适配、X轴、Y轴 均使用旋转与 bounds 逻辑；仅 Z 轴不旋转
	const bool bUseRotationAndBounds = (SortAxisIdx <= 1);

	// 每行：自动适配时按最长轴排序；X/Y/Z 按固定轴排序。X/Y 也计算旋转信息用于旋转与间距
	TArray<TArray<FMeshAutoAdaptInfo>> RowAutoAdaptInfo;  // [RowIdx][MeshIdx]
	RowAutoAdaptInfo.SetNum(Rows.Num());
	for (int32 i = 0; i < Rows.Num(); i++)
	{
		FShowcaseRow& Row = Rows[i];
		TArray<FMeshAutoAdaptInfo>& Infos = RowAutoAdaptInfo[i];
		Infos.SetNum(Row.Meshes.Num());
		for (int32 j = 0; j < Row.Meshes.Num(); j++)
		{
			UStaticMesh* M = Row.Meshes[j];
			FMeshAutoAdaptInfo& Info = Infos[j];
			GetMeshLongestAxis(M, Info.LongestAxis, Info.LongestSize);
			Info.bNeedsRotation = (Info.LongestAxis != 2 && Info.LongestAxis != RowAxisIdx);
			Info.RowAxisBoundIdx = Info.bNeedsRotation ? Info.LongestAxis : RowAxisIdx;
			Info.ColAxisBoundIdx = Info.bNeedsRotation ? (1 - Info.LongestAxis) : ColAxisIdx;
			if (Info.bNeedsRotation)
			{
				Info.RotationYaw = (RowAxisIdx == 1) ? 90.0f : -90.0f;
			}
		}
		if (bSortAxisAuto)
		{
			TArray<TPair<UStaticMesh*, FMeshAutoAdaptInfo>> Pairs;
			for (int32 j = 0; j < Row.Meshes.Num(); j++)
				Pairs.Add({ Row.Meshes[j], Infos[j] });
			Pairs.Sort([](const TPair<UStaticMesh*, FMeshAutoAdaptInfo>& A, const TPair<UStaticMesh*, FMeshAutoAdaptInfo>& B)
				{ return A.Value.LongestSize < B.Value.LongestSize; });
			for (int32 j = 0; j < Row.Meshes.Num(); j++)
			{
				Row.Meshes[j] = Pairs[j].Key;
				Infos[j] = Pairs[j].Value;
			}
		}
		else
		{
			Row.Meshes.Sort([SortAxisIdx](UStaticMesh& A, UStaticMesh& B)
			{
				return GetMeshFullSizeOnAxis(&A, SortAxisIdx) < GetMeshFullSizeOnAxis(&B, SortAxisIdx);
			});
			// 排序后需重新计算 Infos 以保持一致（因 Mesh 顺序已变）
			for (int32 j = 0; j < Row.Meshes.Num(); j++)
			{
				UStaticMesh* M = Row.Meshes[j];
				FMeshAutoAdaptInfo& Info = Infos[j];
				GetMeshLongestAxis(M, Info.LongestAxis, Info.LongestSize);
				Info.bNeedsRotation = (Info.LongestAxis != 2 && Info.LongestAxis != RowAxisIdx);
				Info.RowAxisBoundIdx = Info.bNeedsRotation ? Info.LongestAxis : RowAxisIdx;
				Info.ColAxisBoundIdx = Info.bNeedsRotation ? (1 - Info.LongestAxis) : ColAxisIdx;
				if (Info.bNeedsRotation)
					Info.RotationYaw = (RowAxisIdx == 1) ? 90.0f : -90.0f;
			}
		}
	}

	// 预计算每列（文件夹）在列轴上的 BoundMin 和 BoundMax（用于列间距公式）
	TArray<float> RowBoundMin, RowBoundMax;
	RowBoundMin.SetNum(Rows.Num());
	RowBoundMax.SetNum(Rows.Num());
	for (int32 i = 0; i < Rows.Num(); i++)
	{
		float MaxExtent = 0.0f;
		for (int32 j = 0; j < Rows[i].Meshes.Num(); j++)
		{
			UStaticMesh* M = Rows[i].Meshes[j];
			if (!M) continue;
			if (bUseRotationAndBounds)
				MaxExtent = FMath::Max(MaxExtent, GetMeshExtentOnAxis(M, RowAutoAdaptInfo[i][j].ColAxisBoundIdx));
			else
				MaxExtent = FMath::Max(MaxExtent, GetMeshExtentOnAxis(M, ColAxisIdx));
		}
		RowBoundMin[i] = -MaxExtent;
		RowBoundMax[i] = MaxExtent;
	}

	// 每列间隔 = (第n-1行BoundMax + 第n行BoundMin) * 1.3；RowBoundMin 为负时取绝对值参与间距
	// 间距 = (RowBoundMax[n-1] + |RowBoundMin[n]|) * 1.3，即两列边缘距离的 1.3 倍
	TArray<float> ColAxisOffsets;
	ColAxisOffsets.SetNumZeroed(Rows.Num());
	ColAxisOffsets[0] = 0.0f;
	for (int32 i = 1; i < Rows.Num(); i++)
	{
		// 第n-1行右边缘到第n行左边缘的基准距离 = BoundMax[n-1] - BoundMin[n]（BoundMin 为负）
		float BaseGap = RowBoundMax[i - 1] - RowBoundMin[i];
		ColAxisOffsets[i] = ColAxisOffsets[i - 1] + BaseGap * 1.3f;
	}

	// 列轴最大半尺寸（用于 Plan 缩放与范围计算）
	TArray<float> RowMaxColExtent;
	RowMaxColExtent.SetNum(Rows.Num());
	for (int32 i = 0; i < Rows.Num(); i++)
	{
		float MaxExt = 0.0f;
		for (int32 j = 0; j < Rows[i].Meshes.Num(); j++)
		{
			UStaticMesh* M = Rows[i].Meshes[j];
			int32 ColB = bUseRotationAndBounds ? RowAutoAdaptInfo[i][j].ColAxisBoundIdx : ColAxisIdx;
			MaxExt = FMath::Max(MaxExt, GetMeshExtentOnAxis(M, ColB));
		}
		RowMaxColExtent[i] = MaxExt;
	}

	// 预计算每行文件夹名 Text 大小，取平均值作为本次生成所有文件夹名的统一大小
	TArray<float> RowNameSizes;
	RowNameSizes.SetNum(Rows.Num());
	for (int32 i = 0; i < Rows.Num(); i++)
	{
		float NameSize = 50.0f;
		if (Rows[i].Meshes.Num() > 0)
		{
			if (RowAxisIdx == 1 || bUseRotationAndBounds)
			{
				NameSize = FMath::Max(20.0f, RowBoundMax[i] - RowBoundMin[i]);
			}
			else
			{
				float Sum = 0.0f;
				for (UStaticMesh* M : Rows[i].Meshes)
				{
					Sum += GetMeshFullSizeOnAxis(M, SortAxisIdx);
				}
				NameSize = FMath::Max(20.0f, Sum / Rows[i].Meshes.Num());
			}
		}
		RowNameSizes[i] = NameSize;
	}
	float AvgNameSize = 50.0f;
	if (RowNameSizes.Num() > 0)
	{
		float SumName = 0.0f;
		for (float S : RowNameSizes) SumName += S;
		AvgNameSize = FMath::Max(20.0f, SumName / RowNameSizes.Num());
	}

	FString ShowcaseRoot = GetUniqueShowcaseRootName(World);

	GEditor->BeginTransaction(FText::FromString(TEXT("Create Model Showcase")));

	// 将 Plan 放入当前组
	if (PlanActor)
	{
		PlanActor->SetFolderPath(FName(*ShowcaseRoot));
	}

	int32 SpawnedCount = 0;

	float TotalRowAxisExtent = 0.0f;  // 所有行中最大的行轴长度
	float TotalColAxisExtent = 0.0f;  // 列轴总长度

	for (int32 RowIdx = 0; RowIdx < Rows.Num(); RowIdx++)
	{
		const FShowcaseRow& Row = Rows[RowIdx];

		// 列轴位置
		float ColPos = ColAxisOffsets[RowIdx];
		FVector BaseLoc = Origin;
		if (ColAxisIdx == 0)
		{
			BaseLoc.X += ColPos;
		}
		else
		{
			BaseLoc.Y += ColPos;
		}

		// 行轴：模型间间隔 = 上一模型行轴max + 当前模型行轴min（即 prev_extent + curr_extent）
		float RowAxisPos = 0.0f;
		float RowAxisPrevExtent = 80.0f;  // 文件夹名占位，与第一个模型留出间隙

		// 1. 文件夹名 Text（行首）
		FVector NameLoc = BaseLoc;
		if (RowAxisIdx == 0) NameLoc.X += RowAxisPos;
		else NameLoc.Y += RowAxisPos;

		FColor NameColor = ColumnColors[RowIdx % ColumnColors.Num()].ToFColor(true);
		// 文件夹名 Text 大小 = 本次生成所有文件夹名大小的平均值
		AActor* NameActor = SpawnTextActor(World, NameLoc, Row.FolderName, AvgNameSize, NameColor, false, 0.0f, 2.0f, true, RowAxisIdx);  // 文件夹名，X 为行时 90°，Y 为行时 180°
		if (NameActor)
		{
			NameActor->SetFolderPath(FName(*FString::Printf(TEXT("%s/%s"), *ShowcaseRoot, *Row.FolderName)));
			SpawnedCount++;
		}

		// 2. 每个模型 + 高度 Text（行内间隔最小1米含第一与第二模型；被限制时第一与第二模型的 Text scale=0.5；高度 Text 与本行模型同行轴）
		static const float MinModelGapCm = 100.0f;  // 1 米 = 100 cm
		static const float ClampedTextScale = 0.5f;
		float PrevBoundMaxRowAxis = 0.0f;
		AActor* PrevHeightActor = nullptr;  // 上一模型的高度 Text，用于第一与第二模型间隔被限制时回写第一个的缩放
		for (int32 MeshIdx = 0; MeshIdx < Row.Meshes.Num(); MeshIdx++)
		{
			UStaticMesh* Mesh = Row.Meshes[MeshIdx];
			FBox Box = Mesh->GetBoundingBox();
			int32 RowBoundIdx = bUseRotationAndBounds ? RowAutoAdaptInfo[RowIdx][MeshIdx].RowAxisBoundIdx : RowAxisIdx;
			float CurrBoundMin = Box.Min[RowBoundIdx];
			float CurrBoundMax = Box.Max[RowBoundIdx];
			const FMeshAutoAdaptInfo* AutoInfo = bUseRotationAndBounds ? &RowAutoAdaptInfo[RowIdx][MeshIdx] : nullptr;
			FRotator MeshRotation(0.0f, 0.0f, 0.0f);
			if (AutoInfo && AutoInfo->bNeedsRotation)
			{
				MeshRotation.Yaw = AutoInfo->RotationYaw;
			}
			bool bSpacingClamped = false;
			if (MeshIdx > 0)
			{
				float ComputedGap = (FMath::Abs(PrevBoundMaxRowAxis) + FMath::Abs(CurrBoundMin)) * 1.3f;
				float ActualGap = FMath::Max(ComputedGap, MinModelGapCm);
				bSpacingClamped = (ComputedGap < MinModelGapCm);
				RowAxisPos += ActualGap;
			}
			else
			{
				float CurrExtent = GetMeshExtentOnAxis(Mesh, RowBoundIdx);
				RowAxisPos += (RowAxisPrevExtent + CurrExtent) * 1.3f;
			}
			PrevBoundMaxRowAxis = CurrBoundMax;
			RowAxisPrevExtent = GetMeshExtentOnAxis(Mesh, RowBoundIdx);

			FVector MeshLoc = BaseLoc;
			if (RowAxisIdx == 0) MeshLoc.X += RowAxisPos;
			else MeshLoc.Y += RowAxisPos;
			if (Mesh)
			{
				MeshLoc.Z += FMath::Abs(Mesh->GetBoundingBox().Min.Z);
			}

			AActor* MeshActor = SpawnStaticMeshActor(World, MeshLoc, Mesh, 0.0f, MeshRotation);
			if (MeshActor)
			{
				MeshActor->SetFolderPath(FName(*FString::Printf(TEXT("%s/%s"), *ShowcaseRoot, *Row.FolderName)));
				SpawnedCount++;
			}

			float HeightM = GetMeshHeightMeters(Mesh);
			FString HeightStr = FString::Printf(TEXT("%.2f"), HeightM);

			// 高度 Text 与本行模型同一行轴（XY 与 MeshLoc 一致，Z 由 SpawnHeightTextActor 内部计算）
			FVector HeightTextLoc = MeshLoc;

			FColor HeightColor = ColumnColors[RowIdx % ColumnColors.Num()].ToFColor(true);
			float ForcedScale = bSpacingClamped ? ClampedTextScale : 0.0f;
			// 第一与第二模型间隔被限制时，第一个模型的高度 Text 也做同样缩放限制
			if (MeshIdx == 1 && bSpacingClamped && PrevHeightActor)
			{
				PrevHeightActor->SetActorScale3D(FVector(ClampedTextScale));
			}
			float ModelRowFullOverride = (bUseRotationAndBounds && AutoInfo) ? GetMeshFullSizeOnAxis(Mesh, RowBoundIdx) : -1.0f;
			AActor* HeightActor = SpawnHeightTextActor(World, HeightTextLoc, HeightStr, Mesh, HeightColor, RowAxisIdx, ForcedScale, false, Origin.Z, ModelRowFullOverride);
			if (HeightActor)
			{
				HeightActor->SetFolderPath(FName(*FString::Printf(TEXT("%s/%s"), *ShowcaseRoot, *Row.FolderName)));
				SpawnedCount++;
			}
			PrevHeightActor = HeightActor;
		}

		// 更新行轴最大长度（含文件夹名占位 80 + 本行模型总长）
		float RowLength = 80.0f + RowAxisPos + RowAxisPrevExtent;
		TotalRowAxisExtent = FMath::Max(TotalRowAxisExtent, RowLength);
	}

	// 列轴总长度与范围
	float ColAxisMin = -RowMaxColExtent[0];
	float ColAxisMax = ColAxisOffsets.Last() + RowMaxColExtent.Last();
	TotalColAxisExtent = ColAxisMax - ColAxisMin;

	// 将 Plan 移动到排列整体的中心，再缩放 XY 匹配整体范围
	if (PlanActor && TotalRowAxisExtent > 0 && TotalColAxisExtent > 0)
	{
		// 排列整体的中心（相对于 Origin）
		float RowAxisCenter = TotalRowAxisExtent * 0.5f;
		float ColAxisCenter = (ColAxisMin + ColAxisMax) * 0.5f;

		// 世界空间中心
		FVector WholeCenter = Origin;
		if (RowAxisIdx == 0) WholeCenter.X += RowAxisCenter;
		else WholeCenter.Y += RowAxisCenter;
		if (ColAxisIdx == 0) WholeCenter.X += ColAxisCenter;
		else WholeCenter.Y += ColAxisCenter;

		// 移动 Plan 到整体中心（Plan 中心在自身中间，故移动到排列整体的几何中心）
		PlanActor->SetActorLocation(WholeCenter);

		// 缩放 Plan 的 XY 以匹配整体的 XY 最大范围
		FVector PlanOrigin, PlanBoxExtent;
		PlanActor->GetActorBounds(false, PlanOrigin, PlanBoxExtent);
		float PlanSizeX = PlanBoxExtent.X * 2.0f;
		float PlanSizeY = PlanBoxExtent.Y * 2.0f;
		if (PlanSizeX > 0.01f && PlanSizeY > 0.01f)
		{
			float TargetX = (ColAxisIdx == 0) ? TotalColAxisExtent : TotalRowAxisExtent;
			float TargetY = (ColAxisIdx == 0) ? TotalRowAxisExtent : TotalColAxisExtent;
			FVector OldScale = PlanActor->GetActorScale3D();
			PlanActor->SetActorScale3D(FVector(
				OldScale.X * TargetX / PlanSizeX,
				OldScale.Y * TargetY / PlanSizeY,
				OldScale.Z
			));
		}
	}

	GEditor->EndTransaction();
	World->MarkPackageDirty();
	GEditor->RedrawLevelEditingViewports();

	return SpawnedCount;
#else
	return 0;
#endif
}
