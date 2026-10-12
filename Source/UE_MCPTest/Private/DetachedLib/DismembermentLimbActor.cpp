#include "DetachedLib/DismembermentLimbActor.h"

#include "DetachedLib/DismembermentLog.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

bool FDismembermentCleanupSettings::Validate(FString& OutError) const
{
	OutError.Reset();
	if (Mode != ELimbCleanupMode::Time && Mode != ELimbCleanupMode::Distance)
	{
		OutError = TEXT("Invalid DetachedLimbCleanupMode.");
		return false;
	}
	if (!FMath::IsFinite(MaxLifeSpan) || MaxLifeSpan < 0)
	{
		OutError = TEXT("DetachedLimbLifeSpan must be finite and non-negative; 0 disables the lifetime limit.");
		return false;
	}
	if (Mode == ELimbCleanupMode::Distance &&
		(!FMath::IsFinite(Distance) || Distance < 0 || !FMath::IsFinite(CheckInterval) || CheckInterval <= 0 ||
		 !FMath::IsFinite(OutOfRangeDuration) || OutOfRangeDuration < 0))
	{
		OutError = TEXT("Distance cleanup requires a finite non-negative distance/duration and a finite positive check interval.");
		return false;
	}
	return true;
}

ADismembermentLimbActor::ADismembermentLimbActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	LimbMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LimbMesh"));
	LimbSkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("LimbSkeletalMesh"));
	LimbMesh->SetupAttachment(RootComponent);
	LimbSkeletalMesh->SetupAttachment(RootComponent);
	LimbMesh->SetVisibility(false);
	LimbMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LimbSkeletalMesh->SetVisibility(false);
	LimbSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADismembermentLimbActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StopPhysicsTimerHandle);
		World->GetTimerManager().ClearTimer(PhysicalSimulationAvoidanceTimerHandle);
		World->GetTimerManager().ClearTimer(DistanceCleanupTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void ADismembermentLimbActor::ReInitializeLimb(const FTransform& NewTransform, EDismembermentLimbMeshType MeshType,
	UStaticMesh* NewStaticMesh, USkeletalMesh* NewSkeletalMesh, FName NewCollisionProfileName)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StopPhysicsTimerHandle);
		World->GetTimerManager().ClearTimer(PhysicalSimulationAvoidanceTimerHandle);
		World->GetTimerManager().ClearTimer(DistanceCleanupTimerHandle);
	}
	SetLifeSpan(0.0f);
	OutOfRangeStartTime = -1.0;
	LastDistanceCheckPlayer.Reset();
	ActiveLimbComponent = nullptr;
	for (UPrimitiveComponent* Mesh : {static_cast<UPrimitiveComponent*>(LimbMesh.Get()), static_cast<UPrimitiveComponent*>(LimbSkeletalMesh.Get())})
	{
		if (!IsValid(Mesh)) continue;
		if (Mesh->IsSimulatingPhysics())
		{
			Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		}
		Mesh->SetSimulatePhysics(false);
		Mesh->SetVisibility(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// Static physics can detach a child component from the scene root.
		Mesh->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetIncludingScale);
		Mesh->SetRelativeTransform(FTransform::Identity);
	}
	SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
	CurrentCollisionProfileName = NewCollisionProfileName;
	if (MeshType == EDismembermentLimbMeshType::StaticMesh && IsValid(LimbMesh))
	{
		LimbMesh->SetStaticMesh(NewStaticMesh);
		if (IsValid(LimbSkeletalMesh)) LimbSkeletalMesh->SetSkeletalMesh(nullptr);
		ActiveLimbComponent = LimbMesh;
	}
	else if (MeshType == EDismembermentLimbMeshType::SkeletalMesh && IsValid(LimbSkeletalMesh))
	{
		LimbSkeletalMesh->SetSkeletalMesh(NewSkeletalMesh);
		if (IsValid(LimbMesh)) LimbMesh->SetStaticMesh(nullptr);
		ActiveLimbComponent = LimbSkeletalMesh;
	}
	if (IsValid(ActiveLimbComponent))
	{
		ActiveLimbComponent->SetVisibility(true);
		ActiveLimbComponent->SetCollisionProfileName(NewCollisionProfileName);
	}
}

bool ADismembermentLimbActor::AddImpulse(const FVector& ImpulseDirection, float ImpactForce, const FVector& AngularImpulseDegrees)
{
	if (!StartLimbPhysicsSimulation()) return false;
	ActiveLimbComponent->WakeAllRigidBodies();
	ActiveLimbComponent->AddImpulse(ImpulseDirection.GetSafeNormal() * ImpactForce, NAME_None, true);
	ActiveLimbComponent->AddAngularImpulseInDegrees(AngularImpulseDegrees, NAME_None, true);
	return true;
}

void ADismembermentLimbActor::SetPhysicalSimulationAvoidance(float InAvoidanceTime, FName InAvoidanceCollisionProfileName)
{
	PhysicalSimulationAvoidanceTime = FMath::Max(0.0f, InAvoidanceTime);
	PhysicalSimulationAvoidanceCollisionProfileName = InAvoidanceCollisionProfileName;
}

void ADismembermentLimbActor::SetLimbPhysicsSimulationTime(float InPhysicsSimulationTime)
{
	LimbPhysicsSimulationTime = FMath::Max(0.0f, InPhysicsSimulationTime);
}

bool ADismembermentLimbActor::ConfigureCleanup(const FDismembermentCleanupSettings& Settings)
{
	FString Error;
	if (!Settings.Validate(Error) || !GetWorld() || !IsValid(ActiveLimbComponent))
	{
		UE_LOG(LogDismemberment, Warning, TEXT("Limb=%s: cleanup initialization failed. %s"), *GetName(), *Error);
		return false;
	}
	GetWorld()->GetTimerManager().ClearTimer(DistanceCleanupTimerHandle);
	CleanupSettings = Settings;
	OutOfRangeStartTime = -1.0;
	LastDistanceCheckPlayer.Reset();
	// Zero explicitly removes any previous lifetime limit, including in Distance mode.
	SetLifeSpan(Settings.MaxLifeSpan);
	if (Settings.Mode == ELimbCleanupMode::Distance)
	{
		GetWorld()->GetTimerManager().SetTimer(DistanceCleanupTimerHandle, this,
			&ThisClass::CheckDistanceCleanup, Settings.CheckInterval, true);
	}
	return true;
}

void ADismembermentLimbActor::CheckDistanceCleanup()
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(ActiveLimbComponent)) return;
	// Query the current pawn every time: the limb's Owner is the dismembered host,
	// and a player pawn can be replaced during death/respawn/possession.
	const APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
	APawn* Player = IsValid(Controller) ? Controller->GetPawn() : nullptr;
	if (!IsValid(Player))
	{
		OutOfRangeStartTime = -1.0;
		LastDistanceCheckPlayer.Reset();
		return;
	}
	if (LastDistanceCheckPlayer.Get() != Player)
	{
		OutOfRangeStartTime = -1.0;
		LastDistanceCheckPlayer = Player;
	}
	// Static physics may detach the active mesh; the Actor's scene root can stay at
	// the spawn point. Measure from the active model, not from GetActorLocation().
	const FVector LimbLocation = ActiveLimbComponent->GetComponentLocation();
	const FVector PlayerLocation = Player->GetActorLocation();
	if (LimbLocation.ContainsNaN() || PlayerLocation.ContainsNaN())
	{
		OutOfRangeStartTime = -1.0;
		return;
	}
	const double DistanceSquared = FVector::DistSquared(LimbLocation, PlayerLocation);
	const double ThresholdSquared = FMath::Square(static_cast<double>(CleanupSettings.Distance));
	if (DistanceSquared <= ThresholdSquared)
	{
		OutOfRangeStartTime = -1.0;
		return;
	}
	const double Now = World->GetTimeSeconds();
	if (OutOfRangeStartTime < 0.0) OutOfRangeStartTime = Now;
	if (Now - OutOfRangeStartTime >= CleanupSettings.OutOfRangeDuration) Destroy();
}

bool ADismembermentLimbActor::StartLimbPhysicsSimulation()
{
	if (!IsValid(ActiveLimbComponent) || !GetWorld())
	{
		UE_LOG(LogDismemberment, Warning, TEXT("Limb=%s: active component or World is invalid."), *GetName());
		return false;
	}
	DisableLimbCollision();
	ActiveLimbComponent->SetSimulatePhysics(true);
	if (!ActiveLimbComponent->IsSimulatingPhysics())
	{
		UE_LOG(LogDismemberment, Warning, TEXT("Limb=%s Component=%s: physics simulation could not start."), *GetName(), *ActiveLimbComponent->GetName());
		return false;
	}
	FTimerManager& Timers = GetWorld()->GetTimerManager();
	if (PhysicalSimulationAvoidanceTime > 0)
		Timers.SetTimer(PhysicalSimulationAvoidanceTimerHandle, this, &ThisClass::EnableLimbCollision, PhysicalSimulationAvoidanceTime, false);
	else
		PhysicalSimulationAvoidanceTimerHandle = Timers.SetTimerForNextTick(this, &ThisClass::EnableLimbCollision);
	const float Delay = LimbPhysicsSimulationTime + PhysicalSimulationAvoidanceTime;
	if (Delay > 0)
		Timers.SetTimer(StopPhysicsTimerHandle, this, &ThisClass::StopLimbPhysicsSimulation, Delay, false);
	else
		StopPhysicsTimerHandle = Timers.SetTimerForNextTick(this, &ThisClass::StopLimbPhysicsSimulation);
	return true;
}

void ADismembermentLimbActor::StopLimbPhysicsSimulation() const
{
	if (IsValid(ActiveLimbComponent)) ActiveLimbComponent->SetSimulatePhysics(false);
}

void ADismembermentLimbActor::EnableLimbCollision() const
{
	if (IsValid(ActiveLimbComponent)) ActiveLimbComponent->SetCollisionProfileName(CurrentCollisionProfileName);
}

void ADismembermentLimbActor::DisableLimbCollision() const
{
	if (IsValid(ActiveLimbComponent)) ActiveLimbComponent->SetCollisionProfileName(PhysicalSimulationAvoidanceCollisionProfileName);
}

