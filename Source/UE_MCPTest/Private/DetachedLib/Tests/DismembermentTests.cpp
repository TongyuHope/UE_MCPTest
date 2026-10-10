#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "DetachedLib/AnimNotify_ApplyDismemberment.h"
#include "DetachedLib/DismembermentLimbActor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDismembermentResolutionTest, "UE_MCPTest.Dismemberment.RequestResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDismembermentResolutionTest::RunTest(const FString& Parameters)
{
	UDismembermentConfigData* Data = NewObject<UDismembermentConfigData>();
	FSKMDismembermentConfig& Part = Data->DismembermentConfigs.Add(TEXT("upperarm_r"));
	Part.DefaultConfig.ImpactForce = 100;
	Part.DefaultConfig.TargetSkeletalMeshComponentName = TEXT("ArmMesh");
	FDismembermentImpactPreset& Preset = Part.ImpactPresets.Add(TEXT("Strong"));
	Preset.ImpactForce = 500;
	Preset.LimbSpace = ELimbSpace::CharacterSpace;
	Preset.AngularImpulseSpace = ELimbSpace::BoneSpace;
	FDismembermentRequest Request;
	Request.BoneName = TEXT("upperarm_r");
	FDismembermentConfig Resolved;
	FString Error;
	TestTrue(TEXT("Resolve default"), Data->ResolveRequest(Request, Resolved, Error));
	TestEqual(TEXT("Default force"), Resolved.ImpactForce, 100.0f);
	Request.PresetName = TEXT("Strong");
	TestTrue(TEXT("Resolve preset"), Data->ResolveRequest(Request, Resolved, Error));
	TestEqual(TEXT("Preset wins over defaults"), Resolved.ImpactForce, 500.0f);
	TestEqual(TEXT("Preset does not replace target"), Resolved.TargetSkeletalMeshComponentName, FName(TEXT("ArmMesh")));
	TestEqual(TEXT("Preset does not replace removal mode"), Resolved.GetRemovalMode(), ELimbRemovalMode::HideBone);
	TestEqual(TEXT("Angular space copied independently"), Resolved.AngularImpulseSpace, ELimbSpace::BoneSpace);
	Request.bOverrideImpact = true;
	Request.OverrideConfig.ImpactForce = 0;
	TestTrue(TEXT("Resolve explicit zero"), Data->ResolveRequest(Request, Resolved, Error));
	TestEqual(TEXT("Zero is a value, not an unset marker"), Resolved.ImpactForce, 0.0f);
	TestEqual(TEXT("Asset was not mutated"), Part.DefaultConfig.ImpactForce, 100.0f);
	Request.PresetName = TEXT("Missing");
	TestFalse(TEXT("Unknown preset fails even with explicit impact"), Data->ResolveRequest(Request, Resolved, Error));
	TestTrue(TEXT("Preset diagnostic contains its name"), Error.Contains(TEXT("Missing")));
	Request.BoneName = TEXT("MissingBone");
	TestFalse(TEXT("Unknown part fails"), Data->ResolveRequest(Request, Resolved, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDismembermentMeshSelectionTest, "UE_MCPTest.Dismemberment.MeshTypeIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDismembermentMeshSelectionTest::RunTest(const FString& Parameters)
{
	UDismembermentConfigData* Data = NewObject<UDismembermentConfigData>();
	Data->LimbMeshType = EDismembermentLimbMeshType::SkeletalMesh;
	FSKMDismembermentConfig& Part = Data->DismembermentConfigs.Add(TEXT("upperarm_r"));
	USkeletalMesh* Candidate = NewObject<USkeletalMesh>();
	Part.DismembermentSkeletalMeshes = {nullptr, Candidate, nullptr};
	UDismembermentComponent* Low = NewObject<UDismembermentComponent>();
	UDismembermentComponent* High = NewObject<UDismembermentComponent>();
	Low->DismembermentConfigData = Data;
	High->DismembermentConfigData = Data;
	TestEqual(TEXT("Asset default retained"), High->GetResolvedLimbMeshType(), EDismembermentLimbMeshType::SkeletalMesh);
	Low->LimbMeshTypeOverride = EDismembermentLimbMeshTypeOverride::StaticMesh;
	TestEqual(TEXT("Instance chooses static"), Low->GetResolvedLimbMeshType(), EDismembermentLimbMeshType::StaticMesh);
	TestEqual(TEXT("Other instance unaffected"), High->GetResolvedLimbMeshType(), EDismembermentLimbMeshType::SkeletalMesh);
	TestEqual(TEXT("Shared asset not mutated"), Data->LimbMeshType, EDismembermentLimbMeshType::SkeletalMesh);
	TestFalse(TEXT("Missing static does not fall back"), Part.ValidForMeshType(Low->GetResolvedLimbMeshType()));
	TestTrue(TEXT("Skeletal candidate exists"), Part.ValidForMeshType(High->GetResolvedLimbMeshType()));
	for (int32 Index = 0; Index < 10; ++Index)
		TestEqual(TEXT("Null holes never selected"), Part.GetRandomDismembermentSkeletalMesh(), Candidate);
	FString Error;
	TestFalse(TEXT("Asset existence alone is not sufficient physics"), UDismembermentConfigData::ValidateMesh(Candidate, Error));
	TestTrue(TEXT("Missing Physics Asset diagnostic"), Error.Contains(TEXT("Physics Asset")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDismembermentMigrationTest, "UE_MCPTest.Dismemberment.LegacyNotificationMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDismembermentMigrationTest::RunTest(const FString& Parameters)
{
	UAnimNotify_ApplyDismemberment* Legacy = NewObject<UAnimNotify_ApplyDismemberment>();
	FDismembermentConfig Config;
	Config.TargetSkeletalMeshComponentName = TEXT("ArmMesh");
	Config.ImpactForce = 0;
	Config.ImpactDirection = FVector(1, 2, 3);
	Config.AngularImpulseDegrees = FVector(10, 20, 30);
	Config.bUseWorldScale = false; // Ignored by the original implementation.
	Legacy->ApplyDismembermentConfigs.Add(TEXT("upperarm_r"), Config);
	Legacy->bHideWholeTargetMesh = true;
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FObjectAndNameAsStringProxyArchive Save(Writer, false);
	Legacy->Serialize(Save);
	UAnimNotify_ApplyDismemberment* Loaded = NewObject<UAnimNotify_ApplyDismemberment>();
	FMemoryReader Reader(Bytes);
	FObjectAndNameAsStringProxyArchive Load(Reader, true);
	Loaded->Serialize(Load);
	Loaded->MigrateLegacyConfigurations();
	TestEqual(TEXT("Serialized legacy map converted"), Loaded->DismembermentRequests.Num(), 1);
	if (Loaded->DismembermentRequests.Num() != 1) return false;
	const FDismembermentRequest& Request = Loaded->DismembermentRequests[0];
	TestTrue(TEXT("All old parameters explicit"), Request.bOverrideImpact && Request.bOverrideTargetMesh && Request.bOverrideRemovalMode && Request.bOverrideWorldScale);
	TestEqual(TEXT("Notify OR switch retained"), Request.OverrideConfig.GetRemovalMode(), ELimbRemovalMode::DestroyWholeMeshComponent);
	TestEqual(TEXT("Zero force retained"), Request.OverrideConfig.ImpactForce, 0.0f);
	TestEqual(TEXT("Direction retained"), Request.OverrideConfig.ImpactDirection, Config.ImpactDirection);
	TestEqual(TEXT("Angular impulse retained"), Request.OverrideConfig.AngularImpulseDegrees, Config.AngularImpulseDegrees);
	TestEqual(TEXT("Target retained"), Request.OverrideConfig.TargetSkeletalMeshComponentName, Config.TargetSkeletalMeshComponentName);
	TestTrue(TEXT("Old ignored switch preserves inherited model scale"), Request.OverrideConfig.bUseWorldScale);
	TestEqual(TEXT("Old angular space remains world"), Request.OverrideConfig.AngularImpulseSpace, ELimbSpace::WorldSpace);
	Loaded->MigrateLegacyConfigurations();
	TestEqual(TEXT("Migration is idempotent"), Loaded->DismembermentRequests.Num(), 1);
	TestTrue(TEXT("Legacy map consumed"), Loaded->ApplyDismembermentConfigs.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDismembermentAuthoringTest, "UE_MCPTest.Dismemberment.AuthoringOptionsAndValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDismembermentAuthoringTest::RunTest(const FString& Parameters)
{
	UDismembermentConfigData* Data = NewObject<UDismembermentConfigData>();
	Data->DismembermentConfigs.Add(TEXT("upperarm_r")).ImpactPresets.Add(TEXT("Strong"));
	Data->DismembermentConfigs.Add(TEXT("hand_l")).ImpactPresets.Add(TEXT("Strong"));
	UAnimNotify_ApplyDismemberment* Notify = NewObject<UAnimNotify_ApplyDismemberment>();
	Notify->AuthoringConfigData = Data;
	TestEqual(TEXT("Part dropdown uses authoring asset"), Notify->GetBoneNameOptions().Num(), 2);
	TestEqual(TEXT("Presets deduplicated plus None"), Notify->GetPresetNameOptions().Num(), 2);
	TestEqual(TEXT("Default preset available"), Notify->GetPresetNameOptions()[0], FString(TEXT("None")));
	FDismembermentConfig Config;
	Config.RemovalMode = ELimbRemovalMode::DestroyWholeMeshComponent;
	FString Error;
	TestFalse(TEXT("Whole removal needs explicit target"), UDismembermentConfigData::ValidateConfig(Config, Error));
	Config.TargetSkeletalMeshComponentName = TEXT("ArmMesh");
	TestTrue(TEXT("Zero impulse with explicit target is valid"), UDismembermentConfigData::ValidateConfig(Config, Error));
	TestFalse(TEXT("Unknown collision profile fails"), UDismembermentConfigData::ValidateCollisionProfile(TEXT("DismembermentMissingProfile"), Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDismembermentTemplateValidationTest, "UE_MCPTest.Dismemberment.BlueprintTemplateValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDismembermentTemplateValidationTest::RunTest(const FString& Parameters)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("Static candidate loads"), Cube)) return false;
	UDismembermentConfigData* Data = NewObject<UDismembermentConfigData>();
	Data->LimbMeshType = EDismembermentLimbMeshType::StaticMesh;
	FSKMDismembermentConfig& Part = Data->DismembermentConfigs.Add(TEXT("upperarm_r"));
	Part.DismembermentMeshes.Add(Cube);
	UDismembermentComponent* Template = NewObject<UDismembermentComponent>(GetTransientPackage(), NAME_None, RF_ArchetypeObject);
	Template->DismembermentConfigData = Data;
	TestTrue(TEXT("Blueprint component archetype context"), Template->IsTemplate());
	TestNull(TEXT("SCS template has no constructed owner"), Template->GetOwner());
	FDataValidationContext ValidContext;
	TestEqual(TEXT("Valid template does not require instance meshes"), Template->IsDataValid(ValidContext), EDataValidationResult::Valid);
	TestEqual(TEXT("No false host errors"), ValidContext.GetNumErrors(), uint32(0));

	Part.DefaultConfig.RemovalMode = ELimbRemovalMode::DestroyWholeMeshComponent;
	FDataValidationContext InvalidModeContext;
	TestEqual(TEXT("Template still checks removal parameters"), Template->IsDataValid(InvalidModeContext), EDataValidationResult::Invalid);
	Part.DefaultConfig.RemovalMode = ELimbRemovalMode::HideBone;
	Part.DismembermentMeshes.Reset();
	FDataValidationContext InvalidResourceContext;
	TestEqual(TEXT("Template still checks selected resources"), Template->IsDataValid(InvalidResourceContext), EDataValidationResult::Invalid);
	Part.DismembermentMeshes.Add(Cube);
	Template->PhysicalSimulationAvoidanceCollisionProfileName.Name = TEXT("DismembermentMissingProfile");
	FDataValidationContext InvalidProfileContext;
	TestEqual(TEXT("Template still checks collision profiles"), Template->IsDataValid(InvalidProfileContext), EDataValidationResult::Invalid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDismembermentPhysicsPathsTest, "UE_MCPTest.Dismemberment.PhysicsPathsAndHostProtection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDismembermentPhysicsPathsTest::RunTest(const FString& Parameters)
{
	UDismembermentConfigData* ExistingData = LoadObject<UDismembermentConfigData>(nullptr,
		TEXT("/Game/Environment/LimbCharacter/DA_Dismemberment.DA_Dismemberment"));
	if (!TestNotNull(TEXT("Existing user test asset loads"), ExistingData)) return false;
	const FSKMDismembermentConfig* ExistingPart = ExistingData->DismembermentConfigs.Find(TEXT("upperarm_r"));
	if (!TestNotNull(TEXT("upperarm_r still registered"), ExistingPart)) return false;
	USkeletalMesh* Skeletal = ExistingPart->GetRandomDismembermentSkeletalMesh();
	UStaticMesh* Static = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("Skeletal candidate loads"), Skeletal) || !TestNotNull(TEXT("Static test candidate loads"), Static)) return false;
	FString Error;
	TestTrue(TEXT("Existing SkeletalMesh physical resources"), UDismembermentConfigData::ValidateMesh(Skeletal, Error));
	if (!Error.IsEmpty()) AddError(Error);
	const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Physics test world"), World)) return false;
	UDismembermentConfigData* Data = NewObject<UDismembermentConfigData>();
	Data->LimbMeshType = EDismembermentLimbMeshType::SkeletalMesh;
	FSKMDismembermentConfig& Part = Data->DismembermentConfigs.Add(TEXT("upperarm_r"));
	Part.DismembermentSkeletalMeshes.Add(Skeletal);
	Part.DismembermentMeshes.Add(Static);
	for (EDismembermentLimbMeshTypeOverride Type : {EDismembermentLimbMeshTypeOverride::SkeletalMesh, EDismembermentLimbMeshTypeOverride::StaticMesh})
	{
		ACharacter* Host = World->SpawnActor<ACharacter>();
		if (!TestNotNull(TEXT("Host spawned"), Host)) continue;
		Host->GetMesh()->SetSkeletalMesh(Skeletal);
		UDismembermentComponent* Component = NewObject<UDismembermentComponent>(Host);
		Host->AddInstanceComponent(Component);
		Component->RegisterComponent();
		Component->DismembermentConfigData = Data;
		Component->LimbMeshTypeOverride = Type;
		FDismembermentConfig Whole;
		Whole.RemovalMode = ELimbRemovalMode::DestroyWholeMeshComponent;
		Whole.TargetSkeletalMeshComponentName = Host->GetMesh()->GetFName();
		AddExpectedError(TEXT("the character's main mesh is protected"), EAutomationExpectedErrorFlags::Contains, 1);
		TestFalse(TEXT("Main mesh removal rejected"), Component->ApplyDismemberment(TEXT("upperarm_r"), Whole));
		TestTrue(TEXT("Main mesh survives"), IsValid(Host->GetMesh()) && Host->GetMesh()->IsRegistered());
		Component->DrivenSkeletalMeshComponentName = TEXT("MissingDrivenMesh");
		FDataValidationContext MissingDriverContext;
		TestEqual(TEXT("Concrete owner still validates host meshes"), Component->IsDataValid(MissingDriverContext), EDataValidationResult::Invalid);
		AddExpectedError(TEXT("the character's main mesh is protected"), EAutomationExpectedErrorFlags::Contains, 1);
		TestFalse(TEXT("Missing cache cannot bypass main mesh protection"), Component->ApplyDismemberment(TEXT("upperarm_r"), Whole));
		Component->DrivenSkeletalMeshComponentName = Host->GetMesh()->GetFName();
		if (Type == EDismembermentLimbMeshTypeOverride::StaticMesh)
		{
			Part.DismembermentMeshes.Reset();
			AddExpectedError(TEXT("No valid StaticMesh candidates"), EAutomationExpectedErrorFlags::Contains, 1);
			FDismembermentRequest MissingStatic;
			MissingStatic.BoneName = TEXT("upperarm_r");
			TestFalse(TEXT("Static selection never falls back to skeletal"), Component->ApplyRegisteredDismemberment(MissingStatic));
			TestFalse(TEXT("Missing static candidate does not hide host"), Host->GetMesh()->IsBoneHiddenByName(TEXT("upperarm_r")));
			Part.DismembermentMeshes.Add(Static);
		}
		FDismembermentRequest Request;
		Request.BoneName = TEXT("upperarm_r");
		TestTrue(TEXT("Registered physics path succeeds"), Component->ApplyRegisteredDismemberment(Request));
		TestTrue(TEXT("Host bone hidden after successful physics"), Host->GetMesh()->IsBoneHiddenByName(TEXT("upperarm_r")));
	}
	int32 LimbCount = 0;
	for (TActorIterator<ADismembermentLimbActor> It(World); It; ++It) ++LimbCount;
	TestEqual(TEXT("Both paths generated exactly one limb each"), LimbCount, 2);
	World->DestroyWorld(false);
	return true;
}

#endif
