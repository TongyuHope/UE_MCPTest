#pragma once

#include "CoreMinimal.h"
#include "EdMode.h"
#include "UObject/StrongObjectPtr.h"

class ABiomeSplinePainterActor;
class UBiomePaintedSplineComponent;
class UBiomeSplinePainterSettings;
class UWorld;
struct FHitResult;

/** Viewport authoring mode. Strokes stay transient until mouse release commits a single transaction. */
class FBiomeSplinePainterEdMode : public FEdMode
{
public:
    static const FEditorModeID ModeId;

    FBiomeSplinePainterEdMode();
    virtual ~FBiomeSplinePainterEdMode() override;
    virtual void Enter() override;
    virtual void Exit() override;
    virtual bool UsesToolkits() const override { return true; }
    virtual bool UsesTransformWidget() const override { return false; }
    virtual bool UsesTransformWidget(UE::Widget::EWidgetMode) const override { return false; }
    virtual bool InputKey(FEditorViewportClient* Client, FViewport* Viewport, FKey Key, EInputEvent Event) override;
    virtual bool MouseMove(FEditorViewportClient* Client, FViewport* Viewport, int32 X, int32 Y) override;
    virtual bool CapturedMouseMove(FEditorViewportClient* Client, FViewport* Viewport, int32 X, int32 Y) override;
    virtual bool LostFocus(FEditorViewportClient* Client, FViewport* Viewport) override;
    virtual bool StartTracking(FEditorViewportClient* Client, FViewport* Viewport) override;
    virtual bool EndTracking(FEditorViewportClient* Client, FViewport* Viewport) override;
    virtual bool InputDelta(FEditorViewportClient* Client, FViewport* Viewport, FVector& Drag, FRotator& Rot, FVector& Scale) override;
    virtual bool HandleClick(FEditorViewportClient* Client, HHitProxy* HitProxy, const FViewportClick& Click) override;
    virtual void Tick(FEditorViewportClient* Client, float DeltaTime) override;
    virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
    virtual void PostUndo() override;
    virtual void MapChangeNotify() override;

    UBiomeSplinePainterSettings* GetSettings() const;
    FText GetStatus() const;
    void UseSelection();
    void NewTarget();
    void ApplyGraph();
    void RefreshPCG();
    void DeleteSelectedSpline();
    void SelectSpline(UBiomePaintedSplineComponent* Spline);
    void Finish();

private:
    bool TraceSurface(FEditorViewportClient* Client, FViewport* Viewport, int32 X, int32 Y, FHitResult& Hit) const;
    void UpdatePointer(FEditorViewportClient* Client, FViewport* Viewport, int32 X, int32 Y);
    bool PlaceActor(UWorld* World, const FVector& Location);
    UClass* ResolveActorClass();
    void CommitStroke();
    void CancelStroke();
    void HandleBeginPIE(bool bSimulating);

    TStrongObjectPtr<UBiomeSplinePainterSettings> Settings;
    TWeakObjectPtr<ABiomeSplinePainterActor> StrokeTarget;
    TWeakObjectPtr<UBiomePaintedSplineComponent> SelectedSpline;
    TArray<FVector> StrokePoints;
    FVector HoverPosition = FVector::ZeroVector;
    bool bHasHover = false;
    bool bDrawing = false;
    bool bLeftSequence = false;
    bool bHadSurfaceGap = false;
    FViewport* StrokeViewport = nullptr;
    FDelegateHandle BeginPIEHandle;
    FText Status;
};
