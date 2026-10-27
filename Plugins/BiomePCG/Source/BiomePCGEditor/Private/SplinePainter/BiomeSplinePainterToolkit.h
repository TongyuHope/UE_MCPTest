#pragma once

#include "CoreMinimal.h"
#include "Toolkits/BaseToolkit.h"

class FBiomeSplinePainterEdMode;
class SVerticalBox;
class UBiomePaintedSplineComponent;

class FBiomeSplinePainterToolkit : public FModeToolkit
{
public:
    explicit FBiomeSplinePainterToolkit(FBiomeSplinePainterEdMode* InMode);
    virtual FName GetToolkitFName() const override { return TEXT("BiomeSplinePainter"); }
    virtual FText GetBaseToolkitName() const override;
    virtual FEdMode* GetEditorMode() const override;
    virtual TSharedPtr<SWidget> GetInlineContent() const override { return Content; }
    void UpdateSplineList();

private:
    FBiomeSplinePainterEdMode* Painter;
    TSharedPtr<SWidget> Content;
    TSharedPtr<SVerticalBox> SplineList;
    TArray<TWeakObjectPtr<UBiomePaintedSplineComponent>> CachedSplines;
};
