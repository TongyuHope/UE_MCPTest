#pragma once

#include "CoreMinimal.h"  // <--- 必须是第一个
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BiomePCGBPLibrary.generated.h" // <--- 必须是 #include 列表的最后一个！

UCLASS()
class BIOMEPCG_API UBiomePCGBPLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "BiomePCG", meta = (WorldContext = "WorldContextObject"))
    static void ConvertSelectionToFoliage(UObject* WorldContextObject, bool bRemoveOriginal = true);
};