#include "SplinePainter/BiomeSplinePainterEdMode.h"

#include "BiomePaintedSplineComponent.h"
#include "BiomeSplinePainterActor.h"
#include "SplinePainter/BiomeSplinePainterSettings.h"
#include "SplinePainter/BiomeSplinePainterToolkit.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "EditorViewportClient.h"
#include "Engine/Blueprint.h"
#include "Engine/HitResult.h"
#include "Engine/Level.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "Helpers/PCGHelpers.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "LevelUtils.h"
#include "Misc/PackageName.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "ScopedTransaction.h"
#include "Toolkits/ToolkitManager.h"
#include "UObject/Package.h"
#include "UnrealClient.h"

#define LOCTEXT_NAMESPACE "BiomeSplinePainterEdMode"

const FEditorModeID FBiomeSplinePainterEdMode::ModeId = TEXT("EM_BiomeSplinePainter");

FBiomeSplinePainterEdMode::FBiomeSplinePainterEdMode()
    : Settings(NewObject<UBiomeSplinePainterSettings>(GetTransientPackage()))
{
}

FBiomeSplinePainterEdMode::~FBiomeSplinePainterEdMode() = default;

UBiomeSplinePainterSettings* FBiomeSplinePainterEdMode::GetSettings() const
{
    return Settings.Get();
}

void FBiomeSplinePainterEdMode::Enter()
{
    FEdMode::Enter();
    Status = LOCTEXT("PlaceHint", "请单击地表放置 Actor，或选择已有绘制 Actor。 ");
    UseSelection();
    BeginPIEHandle = FEditorDelegates::PreBeginPIE.AddRaw(this, &FBiomeSplinePainterEdMode::HandleBeginPIE);
    Toolkit = MakeShared<FBiomeSplinePainterToolkit>(this);
    Toolkit->Init(GetModeManager()->GetToolkitHost());
}

void FBiomeSplinePainterEdMode::Exit()
{
    CancelStroke();
    bLeftSequence = false;
    FEditorDelegates::PreBeginPIE.Remove(BeginPIEHandle);
    BeginPIEHandle.Reset();
    if (Toolkit.IsValid())
    {
        FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
        Toolkit.Reset();
    }
    FEdMode::Exit();
}

FText FBiomeSplinePainterEdMode::GetStatus() const
{
    FString Text = Status.ToString();
    if (const ABiomeSplinePainterActor* Target = Settings->TargetActor.Get())
    {
        Text += FString::Printf(TEXT("\n目标：%s  |  已保存 %d 条线"), *Target->GetActorLabel(), Target->GetPaintedSplines().Num());
        if (!Target->PCGComponent || !Target->PCGComponent->GetGraph())
        {
            Text += TEXT("\n未指定 PCG 图；仍可绘制和保存样条。");
        }
        else if (Target->PCGComponent->IsGenerating())
        {
            Text += TEXT("\nPCG 正在生成；后续刷新将合并等待。");
        }
    }
    if (bDrawing)
    {
        Text += FString::Printf(TEXT("\n当前笔画：%d 个控制点，松开左键提交。"), StrokePoints.Num());
    }
    return FText::FromString(Text);
}

void FBiomeSplinePainterEdMode::UseSelection()
{
    CancelStroke();
    if (!GEditor)
    {
        return;
    }
    for (FSelectionIterator It(*GEditor->GetSelectedActors()); It; ++It)
    {
        ABiomeSplinePainterActor* Target = Cast<ABiomeSplinePainterActor>(*It);
        if (IsValid(Target) && Target->GetWorld() && Target->GetWorld()->WorldType == EWorldType::Editor)
        {
            Settings->TargetActor = Target;
            Settings->Graph = Target->PCGComponent ? Target->PCGComponent->GetGraph() : nullptr;
            Settings->bAutoRefreshPCG = Target->bAutoRefreshPCG;
            SelectedSpline.Reset();
            Status = LOCTEXT("Ready", "按住左键绘制新样条，松开提交。");
            return;
        }
    }
    Status = Settings->TargetActor.IsValid()
        ? LOCTEXT("KeepTarget", "未选中兼容 Actor，当前目标保留；新建请点击“放置新的 Actor”。")
        : LOCTEXT("NoSelection", "未选中绘制 Actor；单击地表可以放置新 Actor。");
}

void FBiomeSplinePainterEdMode::NewTarget()
{
    CancelStroke();
    Settings->TargetActor.Reset();
    SelectedSpline.Reset();
    Status = LOCTEXT("NewHint", "下一次单击地表放置新 Actor，此次点击不画线。");
}

UClass* FBiomeSplinePainterEdMode::ResolveActorClass()
{
    if (Settings->ActorClass)
    {
        UClass* Class = Settings->ActorClass.Get();
        if (!Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
        {
            return Class;
        }
        Status = LOCTEXT("InvalidClass", "选定的 Actor 类不可实例化，请选择有效蓝图类。");
        return nullptr;
    }

    const FString PackageName = TEXT("/Game/PCG/BP_PCG/BP_BiomeSplinePainter");
    const FString AssetName = TEXT("BP_BiomeSplinePainter");
    const FString AssetPath = PackageName + TEXT(".") + AssetName;
    const bool bPackageOnDisk = FPackageName::DoesPackageExist(PackageName);
    UObject* Existing = FindObject<UObject>(nullptr, *AssetPath);
    if (!Existing && bPackageOnDisk)
    {
        Existing = LoadObject<UObject>(nullptr, *AssetPath, nullptr, LOAD_NoWarn);
    }
    if (Existing)
    {
        UBlueprint* Blueprint = Cast<UBlueprint>(Existing);
        if (Blueprint && Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(ABiomeSplinePainterActor::StaticClass())
            && !Blueprint->GeneratedClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
        {
            Settings->ActorClass = Blueprint->GeneratedClass;
            return Blueprint->GeneratedClass;
        }
        Status = LOCTEXT("AssetConflict", "默认蓝图路径已被不兼容资产占用，请在面板选择其他 Actor 类。");
        return nullptr;
    }
    if (bPackageOnDisk || FindPackage(nullptr, *PackageName))
    {
        Status = LOCTEXT("PackageConflict", "默认蓝图包已存在但无法作为绘制 Actor 使用，请选择其他 Actor 类。");
        return nullptr;
    }

    UPackage* Package = CreatePackage(*PackageName);
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        ABiomeSplinePainterActor::StaticClass(), Package, FName(*AssetName), BPTYPE_Normal, TEXT("BiomeSplinePainter"));
    if (!Blueprint)
    {
        Status = LOCTEXT("CreateBlueprintFailed", "无法创建绘制蓝图，请查看输出日志。");
        return nullptr;
    }
    FAssetRegistryModule::AssetCreated(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    Package->MarkPackageDirty();
    if (!Blueprint->GeneratedClass || Blueprint->Status == BS_Error)
    {
        Status = LOCTEXT("BlueprintClassFailed", "蓝图已创建但未产生有效类，请检查并编译该蓝图。");
        return nullptr;
    }
    Settings->ActorClass = Blueprint->GeneratedClass;
    return Blueprint->GeneratedClass;
}

bool FBiomeSplinePainterEdMode::PlaceActor(UWorld* World, const FVector& Location)
{
    if (!World || World->WorldType != EWorldType::Editor || !World->GetCurrentLevel()
        || FLevelUtils::IsLevelLocked(World->GetCurrentLevel()))
    {
        Status = LOCTEXT("LockedLevel", "当前关卡不可编辑，无法放置 Actor。");
        return false;
    }
    UClass* Class = ResolveActorClass();
    if (!Class)
    {
        return false;
    }

    const FScopedTransaction Transaction(LOCTEXT("PlaceTransaction", "放置 Biome 样条绘制 Actor"));
    World->GetCurrentLevel()->Modify();
    FActorSpawnParameters Params;
    Params.OverrideLevel = World->GetCurrentLevel();
    Params.ObjectFlags |= RF_Transactional;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ABiomeSplinePainterActor* Target = World->SpawnActor<ABiomeSplinePainterActor>(Class, FTransform(Location), Params);
    if (!Target)
    {
        Status = LOCTEXT("SpawnFailed", "无法放置绘制 Actor。");
        return false;
    }
    Target->SetActorLabel(TEXT("BiomeSplinePainter"));
    Target->bAutoRefreshPCG = Settings->bAutoRefreshPCG;
    if (Settings->Graph)
    {
        Target->PCGComponent->SetGraphLocal(Settings->Graph);
    }
    Target->MarkPackageDirty();
    Settings->TargetActor = Target;
    GEditor->SelectNone(false, true, false);
    GEditor->SelectActor(Target, true, true);
    Status = LOCTEXT("Placed", "Actor 已放置。再次按住左键开始一条新线；请保存新蓝图和关卡。");
    return true;
}

bool FBiomeSplinePainterEdMode::TraceSurface(
    FEditorViewportClient* Client, FViewport* Viewport, int32 X, int32 Y, FHitResult& Hit) const
{
    if (!Client || !Viewport || !Client->IsLevelEditorClient() || !Client->GetWorld()
        || Client->GetWorld()->WorldType != EWorldType::Editor || (GEditor && GEditor->PlayWorld))
    {
        return false;
    }
    const FIntPoint Size = Viewport->GetSizeXY();
    if (X < 0 || Y < 0 || X >= Size.X || Y >= Size.Y)
    {
        return false;
    }
    FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(
        Viewport, Client->GetScene(), Client->EngineShowFlags).SetRealtimeUpdate(Client->IsRealtime()));
    const FSceneView* View = Client->CalcSceneView(&Family);
    if (!View)
    {
        return false;
    }
    const FViewportCursorLocation Cursor(View, Client, X, Y);
    const FVector Start = Cursor.GetOrigin();
    const FVector End = Start + Cursor.GetDirection() * 10000000.0;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(BiomeSplinePainter), true);
    if (ABiomeSplinePainterActor* Target = Settings->TargetActor.Get())
    {
        Query.AddIgnoredActor(Target);
    }

    // Retrace through ignored generated surfaces: filtering a single blocking hit afterwards
    // would otherwise prevent us from reaching the landscape behind a tree canopy.
    for (int32 Attempt = 0; Attempt < 128; ++Attempt)
    {
        if (!Client->GetWorld()->LineTraceSingleByChannel(Hit, Start, End, Settings->TraceChannel, Query))
        {
            return false;
        }
        const UPrimitiveComponent* Component = Hit.GetComponent();
        const AActor* Actor = Hit.GetActor();
        const bool bGeneratedActor = Actor && Actor->ActorHasTag(PCGHelpers::DefaultPCGActorTag);
        const bool bGeneratedComponent = Component && (Component->ComponentHasTag(PCGHelpers::DefaultPCGTag)
            || Component->ComponentHasTag(PCGHelpers::DefaultPCGDebugTag));
        if (!Settings->bIgnorePCGGenerated || (!bGeneratedActor && !bGeneratedComponent))
        {
            return Hit.bBlockingHit;
        }
        if (bGeneratedActor)
        {
            Query.AddIgnoredActor(Actor);
        }
        else if (Component)
        {
            Query.AddIgnoredComponent(Component);
        }
        else
        {
            return false;
        }
    }
    return false;
}

void FBiomeSplinePainterEdMode::UpdatePointer(FEditorViewportClient* Client, FViewport* Viewport, int32 X, int32 Y)
{
    FHitResult Hit;
    bHasHover = TraceSurface(Client, Viewport, X, Y, Hit);
    if (!bHasHover)
    {
        bHadSurfaceGap |= bDrawing;
        return;
    }
    HoverPosition = Hit.ImpactPoint;
    if (!bDrawing || StrokeViewport != Viewport || StrokePoints.IsEmpty())
    {
        return;
    }
    const double Distance = FVector::Distance(StrokePoints.Last(), HoverPosition);
    const double MaxGap = FMath::Max(Settings->MaxGapDistance, Settings->MinPointSpacing * 2.0);
    if (Distance > MaxGap)
    {
        bHasHover = false; // Never append the far re-entry point as the stroke endpoint.
        CommitStroke();
        bHasHover = true;
        Status = LOCTEXT("Gap", "跨越距离过大，当前有效笔画已结束；松开左键后再画新线。");
        return;
    }
    bHadSurfaceGap = false;
    if (Distance >= FMath::Max(1.0, Settings->MinPointSpacing))
    {
        StrokePoints.Add(HoverPosition);
    }
}

bool FBiomeSplinePainterEdMode::InputKey(FEditorViewportClient* Client, FViewport* Viewport, FKey Key, EInputEvent Event)
{
    if (!Client || !Viewport || (GEditor && GEditor->PlayWorld))
    {
        return false;
    }
    if (Key == EKeys::Escape && Event == IE_Pressed)
    {
        if (bDrawing)
        {
            CancelStroke();
            Status = LOCTEXT("Cancelled", "当前笔画已取消，已保存样条保留。");
        }
        else
        {
            Finish();
        }
        return true;
    }
    if (bDrawing && Event == IE_Pressed && (Key == EKeys::Z || Key == EKeys::Y) && Client->IsCtrlPressed())
    {
        CancelStroke();
        return true;
    }
    if (Key != EKeys::LeftMouseButton)
    {
        return false;
    }
    if (Event == IE_Released && bLeftSequence)
    {
        if (bDrawing && StrokeViewport == Viewport)
        {
            FIntPoint Mouse;
            Viewport->GetMousePos(Mouse);
            UpdatePointer(Client, Viewport, Mouse.X, Mouse.Y);
            CommitStroke();
        }
        bLeftSequence = false;
        return true;
    }
    if (Client->IsAltPressed() || Client->IsCtrlPressed() || Viewport->KeyState(EKeys::RightMouseButton))
    {
        return false;
    }
    if (Event != IE_Pressed && Event != IE_DoubleClick)
    {
        return bLeftSequence;
    }
    if (bLeftSequence)
    {
        return true;
    }
    bLeftSequence = true;
    FIntPoint Mouse;
    Viewport->GetMousePos(Mouse);
    UpdatePointer(Client, Viewport, Mouse.X, Mouse.Y);
    if (!bHasHover)
    {
        Status = LOCTEXT("NoSurface", "没有可绘制的地表碰撞；请检查地表和碰撞通道。");
        return true;
    }
    ABiomeSplinePainterActor* Target = Settings->TargetActor.Get();
    if (!Target)
    {
        PlaceActor(Client->GetWorld(), HoverPosition);
        return true;
    }
    if (Target->GetWorld() != Client->GetWorld() || FLevelUtils::IsLevelLocked(Target))
    {
        Status = LOCTEXT("InvalidTarget", "目标不在当前可编辑世界，或其关卡已锁定。");
        return true;
    }
    StrokePoints = { HoverPosition };
    StrokeTarget = Target;
    StrokeViewport = Viewport;
    bDrawing = true;
    bHadSurfaceGap = false;
    return true;
}

void FBiomeSplinePainterEdMode::CommitStroke()
{
    if (!bDrawing)
    {
        return;
    }
    ABiomeSplinePainterActor* Target = StrokeTarget.Get();
    if (bHasHover && !StrokePoints.IsEmpty() && !bHadSurfaceGap
        && FVector::DistSquared(StrokePoints.Last(), HoverPosition) > 1.0)
    {
        StrokePoints.Add(HoverPosition);
    }
    double Length = 0.0;
    for (int32 Index = 1; Index < StrokePoints.Num(); ++Index)
    {
        Length += FVector::Distance(StrokePoints[Index - 1], StrokePoints[Index]);
    }
    if (IsValid(Target) && Target == Settings->TargetActor.Get() && !FLevelUtils::IsLevelLocked(Target)
        && StrokePoints.Num() >= 2 && Length >= FMath::Max(1.0, Settings->MinStrokeLength))
    {
        const FScopedTransaction Transaction(LOCTEXT("StrokeTransaction", "绘制一条 Biome 样条"));
        SelectSpline(Target->AddPaintedSpline(StrokePoints, Settings->bSmooth));
        Status = LOCTEXT("StrokeSaved", "样条已提交。再次按住左键绘制另一条独立样条。");
    }
    else
    {
        Status = LOCTEXT("StrokeTooShort", "笔画过短或目标不可编辑，未保存本条线。");
    }
    CancelStroke();
}

void FBiomeSplinePainterEdMode::CancelStroke()
{
    bDrawing = false;
    StrokePoints.Reset();
    StrokeTarget.Reset();
    StrokeViewport = nullptr;
    bHadSurfaceGap = false;
}

bool FBiomeSplinePainterEdMode::MouseMove(FEditorViewportClient* Client, FViewport* Viewport, int32 X, int32 Y)
{
    UpdatePointer(Client, Viewport, X, Y);
    Viewport->InvalidateDisplay();
    return bLeftSequence;
}

bool FBiomeSplinePainterEdMode::CapturedMouseMove(FEditorViewportClient* Client, FViewport* Viewport, int32 X, int32 Y)
{
    return MouseMove(Client, Viewport, X, Y);
}

bool FBiomeSplinePainterEdMode::StartTracking(FEditorViewportClient* Client, FViewport* Viewport)
{
    return Viewport->KeyState(EKeys::LeftMouseButton) && !Client->IsAltPressed()
        && !Client->IsCtrlPressed() && !Viewport->KeyState(EKeys::RightMouseButton);
}

bool FBiomeSplinePainterEdMode::EndTracking(FEditorViewportClient* Client, FViewport* Viewport)
{
    const bool bHandled = bLeftSequence;
    if (bDrawing)
    {
        if (StrokeViewport == Viewport && !Viewport->KeyState(EKeys::LeftMouseButton))
        {
            FIntPoint Mouse;
            Viewport->GetMousePos(Mouse);
            UpdatePointer(Client, Viewport, Mouse.X, Mouse.Y);
            CommitStroke();
        }
        else
        {
            // Tracking can also end because capture/focus was lost while the button is held.
            CancelStroke();
        }
    }
    bLeftSequence = false;
    return bHandled;
}

bool FBiomeSplinePainterEdMode::InputDelta(FEditorViewportClient*, FViewport*, FVector&, FRotator&, FVector&)
{
    return bLeftSequence;
}

bool FBiomeSplinePainterEdMode::HandleClick(FEditorViewportClient*, HHitProxy*, const FViewportClick& Click)
{
    return Click.GetKey() == EKeys::LeftMouseButton && !Click.IsAltDown() && !Click.IsControlDown();
}

bool FBiomeSplinePainterEdMode::LostFocus(FEditorViewportClient* Client, FViewport* Viewport)
{
    CancelStroke();
    bLeftSequence = false;
    bHasHover = false;
    return FEdMode::LostFocus(Client, Viewport);
}

void FBiomeSplinePainterEdMode::Tick(FEditorViewportClient* Client, float DeltaTime)
{
    FEdMode::Tick(Client, DeltaTime);
    if (bDrawing && (!StrokeTarget.IsValid() || StrokeTarget != Settings->TargetActor))
    {
        CancelStroke();
    }
    if (SelectedSpline.IsValid() && SelectedSpline->GetOwner() != Settings->TargetActor.Get())
    {
        SelectedSpline.Reset();
    }
    if (Toolkit.IsValid())
    {
        StaticCastSharedPtr<FBiomeSplinePainterToolkit>(Toolkit)->UpdateSplineList();
    }
}

void FBiomeSplinePainterEdMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
    FEdMode::Render(View, Viewport, PDI);
    if (bHasHover)
    {
        const FLinearColor Color = bDrawing ? FLinearColor::Yellow : FLinearColor::Green;
        PDI->DrawPoint(HoverPosition, Color, 12.0f, SDPG_Foreground);
        PDI->DrawLine(HoverPosition - FVector(20, 0, 0), HoverPosition + FVector(20, 0, 0), Color, SDPG_Foreground);
        PDI->DrawLine(HoverPosition - FVector(0, 20, 0), HoverPosition + FVector(0, 20, 0), Color, SDPG_Foreground);
    }
    for (int32 Index = 1; Index < StrokePoints.Num(); ++Index)
    {
        PDI->DrawLine(StrokePoints[Index - 1], StrokePoints[Index], FLinearColor::Yellow, SDPG_Foreground, 2.0f);
    }
    if (bDrawing && bHasHover && !StrokePoints.IsEmpty())
    {
        PDI->DrawLine(StrokePoints.Last(), HoverPosition, FLinearColor::Yellow, SDPG_Foreground, 2.0f);
    }
    if (UBiomePaintedSplineComponent* Spline = SelectedSpline.Get())
    {
        const int32 Steps = FMath::Clamp(FMath::CeilToInt(Spline->GetSplineLength() / 50.0f), 1, 512);
        FVector Previous = Spline->GetLocationAtDistanceAlongSpline(0, ESplineCoordinateSpace::World);
        for (int32 Index = 1; Index <= Steps; ++Index)
        {
            const FVector Next = Spline->GetLocationAtDistanceAlongSpline(
                Spline->GetSplineLength() * Index / Steps, ESplineCoordinateSpace::World);
            PDI->DrawLine(Previous, Next, FLinearColor(0.0f, 0.8f, 1.0f), SDPG_Foreground, 3.0f);
            Previous = Next;
        }
    }
}

void FBiomeSplinePainterEdMode::SelectSpline(UBiomePaintedSplineComponent* Spline)
{
    SelectedSpline = Spline;
    if (GEditor)
    {
        GEditor->RedrawLevelEditingViewports();
    }
}

void FBiomeSplinePainterEdMode::DeleteSelectedSpline()
{
    CancelStroke();
    ABiomeSplinePainterActor* Target = Settings->TargetActor.Get();
    if (!Target || !SelectedSpline.IsValid() || FLevelUtils::IsLevelLocked(Target))
    {
        Status = LOCTEXT("SelectSpline", "请先选择当前目标的一条可编辑样条。");
        return;
    }
    const FScopedTransaction Transaction(LOCTEXT("DeleteTransaction", "删除一条 Biome 样条"));
    if (Target->RemovePaintedSpline(SelectedSpline.Get()))
    {
        SelectedSpline.Reset();
        Status = LOCTEXT("Deleted", "样条已删除，可撤销。");
    }
}

void FBiomeSplinePainterEdMode::ApplyGraph()
{
    ABiomeSplinePainterActor* Target = Settings->TargetActor.Get();
    if (!Target || FLevelUtils::IsLevelLocked(Target))
    {
        return;
    }
    const FScopedTransaction Transaction(LOCTEXT("GraphTransaction", "设置 Biome 样条 PCG 图"));
    Target->Modify();
    Target->PCGComponent->Modify();
    Target->bAutoRefreshPCG = Settings->bAutoRefreshPCG;
    Target->PCGComponent->SetGraphLocal(Settings->Graph);
    Target->MarkPackageDirty();
    Target->RequestPCGRefresh();
}

void FBiomeSplinePainterEdMode::RefreshPCG()
{
    if (ABiomeSplinePainterActor* Target = Settings->TargetActor.Get())
    {
        Target->RefreshPCG();
    }
}

void FBiomeSplinePainterEdMode::PostUndo()
{
    CancelStroke();
    if (ABiomeSplinePainterActor* Target = Settings->TargetActor.Get())
    {
        Target->RequestPCGRefresh();
    }
}

void FBiomeSplinePainterEdMode::MapChangeNotify()
{
    CancelStroke();
    bLeftSequence = false;
    Settings->TargetActor.Reset();
    SelectedSpline.Reset();
    bHasHover = false;
}

void FBiomeSplinePainterEdMode::HandleBeginPIE(bool)
{
    Finish();
}

void FBiomeSplinePainterEdMode::Finish()
{
    GLevelEditorModeTools().DeactivateMode(ModeId);
}

#undef LOCTEXT_NAMESPACE
