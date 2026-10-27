#include "BiomePCG.h"
#include "ModelShowcaseBPLibrary.h"
#include "CollisionPresetBPLibrary.h"
#include "PCGToFoliageLib.h"
#include "LevelEditor.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Framework/Docking/WorkspaceItem.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ToolMenus.h"
#include "ToolMenu.h"
#include "ToolMenuEntry.h"
#include "ToolMenuSection.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Commands/UIAction.h"
#include "Modules/ModuleManager.h"
#include "EditorUtilityLibrary.h"
#include "Engine/StaticMesh.h"
#include "Selection.h"
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Editor.h"
#include "EditorUtilitySubsystem.h"
#include "Engine/StaticMesh.h"
#include "AssetRegistry/AssetRegistryModule.h"

class UEditorUtilityWidgetBlueprint;

#define LOCTEXT_NAMESPACE "FBiomePCGModule"

static const FName ModelShowcaseTabName("ModelShowcase");
static const FName CollisionPresetTabName("CollisionPreset");
static const FName PCGToFoliageTabName("PCGToFoliage");

struct FModelShowcasePanelState
{
	TArray<TSharedPtr<FString>> RowAxisOptions;
	TArray<TSharedPtr<FString>> SortAxisOptions;
	TSharedPtr<FString> RowAxisSelected;
	TSharedPtr<FString> SortAxisSelected;
};

struct FCollisionPresetPanelState
{
	TArray<TSharedPtr<FString>> PresetOptions;
	TSharedPtr<FString> SelectedPreset;
};

struct FPCGToFoliagePanelState
{
	bool bDestroySourceInstances = false;
	bool bCreateFolder = true;
	TSharedPtr<FString> FolderName;
};

static TSharedRef<SDockTab> SpawnModelShowcaseTab(const FSpawnTabArgs& SpawnTabArgs)
{
	TSharedRef<FModelShowcasePanelState> State = MakeShared<FModelShowcasePanelState>();
	State->RowAxisOptions.Add(MakeShared<FString>(TEXT("Y轴（行沿Y）")));
	State->RowAxisOptions.Add(MakeShared<FString>(TEXT("X轴（行沿X）")));
	State->RowAxisSelected = State->RowAxisOptions[0];
	State->SortAxisOptions.Add(MakeShared<FString>(TEXT("自动适配")));
	State->SortAxisOptions.Add(MakeShared<FString>(TEXT("Z轴")));
	State->SortAxisOptions.Add(MakeShared<FString>(TEXT("X轴")));
	State->SortAxisOptions.Add(MakeShared<FString>(TEXT("Y轴")));
	State->SortAxisSelected = State->SortAxisOptions[0];

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("ModelShowcaseTabTitle", "Model Showcase"))
		.ToolTipText(LOCTEXT("ModelShowcaseTabTooltip", "根据 Content 选中文件夹的模型结构排布到场景中"))
		[
			SNew(SBox)
			.Padding(20.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 10)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ModelShowcaseHint", "1. 在 Content Browser 中选择一个或多个文件夹\n2. 在世界大纲中选择一个 Actor 作为 Plan\n3. 调整下方参数后点击按钮生成展示"))
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 12)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0, 0, 12, 0)
					[
						SNew(STextBlock).Text(LOCTEXT("RowAxisLabel", "行轴："))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SComboBox<TSharedPtr<FString>>)
						.OptionsSource(&State->RowAxisOptions)
						.InitiallySelectedItem(State->RowAxisSelected)
						.OnSelectionChanged_Lambda([State](TSharedPtr<FString> NewValue, ESelectInfo::Type)
						{
							if (NewValue.IsValid()) State->RowAxisSelected = NewValue;
						})
						.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
						{
							return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : TEXT("")));
						})
						.Content()
						[
							SNew(STextBlock)
							.Text_Lambda([State]() { return FText::FromString(State->RowAxisSelected.IsValid() ? *State->RowAxisSelected : TEXT("")); })
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 8)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0, 0, 12, 0)
					[
						SNew(STextBlock).Text(LOCTEXT("SortAxisLabel", "排列顺序轴："))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SComboBox<TSharedPtr<FString>>)
						.OptionsSource(&State->SortAxisOptions)
						.InitiallySelectedItem(State->SortAxisSelected)
						.OnSelectionChanged_Lambda([State](TSharedPtr<FString> NewValue, ESelectInfo::Type)
						{
							if (NewValue.IsValid()) State->SortAxisSelected = NewValue;
						})
						.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
						{
							return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : TEXT("")));
						})
						.Content()
						[
							SNew(STextBlock)
							.Text_Lambda([State]() { return FText::FromString(State->SortAxisSelected.IsValid() ? *State->SortAxisSelected : TEXT("")); })
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 20)
				[
					SNew(SButton)
					.Text(LOCTEXT("CreateShowcaseButton", "创建 Model Showcase"))
					.OnClicked_Lambda([State]()
					{
#if WITH_EDITOR
						EShowcaseRowAxis RowAxis = EShowcaseRowAxis::Y;
						if (State->RowAxisSelected.IsValid())
						{
							int32 Idx = State->RowAxisOptions.IndexOfByPredicate([&](const TSharedPtr<FString>& P) { return P.IsValid() && *P == *State->RowAxisSelected; });
							if (Idx == 1) RowAxis = EShowcaseRowAxis::X;
						}
						EShowcaseSortAxis SortAxis = EShowcaseSortAxis::Auto;
						if (State->SortAxisSelected.IsValid())
						{
							int32 Idx = State->SortAxisOptions.IndexOfByPredicate([&](const TSharedPtr<FString>& P) { return P.IsValid() && *P == *State->SortAxisSelected; });
							if (Idx == 1) SortAxis = EShowcaseSortAxis::Z;
							else if (Idx == 2) SortAxis = EShowcaseSortAxis::X;
							else if (Idx == 3) SortAxis = EShowcaseSortAxis::Y;
						}
						UModelShowcaseBPLibrary::CreateModelShowcase(TArray<FString>(), nullptr, RowAxis, SortAxis);
#endif
						return FReply::Handled();
					})
				]
			]
		];
}

static TSharedRef<SDockTab> SpawnCollisionPresetTab(const FSpawnTabArgs& SpawnTabArgs)
{
	TSharedRef<FCollisionPresetPanelState> State = MakeShared<FCollisionPresetPanelState>();

	// 获取所有可用的 Collision Preset
	TArray<FString> Presets = UCollisionPresetBPLibrary::GetAvailableCollisionPresets();
	for (const FString& Preset : Presets)
	{
		State->PresetOptions.Add(MakeShared<FString>(Preset));
	}

	if (State->PresetOptions.Num() > 0)
	{
		// 默认选择 BlockAll
		int32 DefaultIdx = State->PresetOptions.IndexOfByPredicate([&](const TSharedPtr<FString>& P)
		{
			return P.IsValid() && *P == TEXT("BlockAll");
		});
		if (DefaultIdx == INDEX_NONE) DefaultIdx = 0;
		State->SelectedPreset = State->PresetOptions[DefaultIdx];
	}

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("CollisionPresetTabTitle", "Collision Preset Manager"))
		.ToolTipText(LOCTEXT("CollisionPresetTabTooltip", "批量修改 StaticMesh 资产的 Collision Presets 并保存"))
		[
			SNew(SBox)
			.Padding(20.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 10)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CollisionPresetHint", "1. 在 Content Browser 中选择文件夹（递归）或直接选择 StaticMesh 资产\n2. 从下拉菜单中选择目标 Collision Preset\n3. 点击按钮开始批量修改"))
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 12)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0, 0, 12, 0)
					[
						SNew(STextBlock).Text(LOCTEXT("CollisionPresetLabel", "Collision Preset："))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SComboBox<TSharedPtr<FString>>)
						.OptionsSource(&State->PresetOptions)
						.InitiallySelectedItem(State->SelectedPreset)
						.OnSelectionChanged_Lambda([State](TSharedPtr<FString> NewValue, ESelectInfo::Type)
						{
							if (NewValue.IsValid()) State->SelectedPreset = NewValue;
						})
						.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
						{
							return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : TEXT("")));
						})
						.Content()
						[
							SNew(STextBlock)
							.Text_Lambda([State]() { return FText::FromString(State->SelectedPreset.IsValid() ? *State->SelectedPreset : TEXT("")); })
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 20)
				[
					SNew(SButton)
					.Text(LOCTEXT("ApplyCollisionPresetButton", "应用 Collision Preset"))
					.OnClicked_Lambda([State]()
					{
#if WITH_EDITOR
						if (!State->SelectedPreset.IsValid())
						{
							UE_LOG(LogTemp, Warning, TEXT("CollisionPreset: 请先选择一个 Collision Preset"));
							return FReply::Handled();
						}

						// 获取 Content Browser 选中的文件夹
						TArray<FString> SelectedFolders = UEditorUtilityLibrary::GetSelectedFolderPaths();

						// 获取 Content Browser 选中的 StaticMesh 资产
						TArray<UStaticMesh*> SelectedMeshes;
						USelection* SelectedAssets = GEditor->GetSelectedObjects();
						if (SelectedAssets)
						{
							TArray<UObject*> Selected;
							SelectedAssets->GetSelectedObjects(UScriptStruct::StaticClass(), Selected);
							SelectedAssets->GetSelectedObjects(UStaticMesh::StaticClass(), Selected);
							for (UObject* Obj : Selected)
							{
								if (UStaticMesh* Mesh = Cast<UStaticMesh>(Obj))
								{
									SelectedMeshes.Add(Mesh);
								}
							}
						}

						if (SelectedFolders.Num() == 0 && SelectedMeshes.Num() == 0)
						{
							UE_LOG(LogTemp, Warning, TEXT("CollisionPreset: 请先在 Content Browser 中选择文件夹或 StaticMesh 资产"));
							return FReply::Handled();
						}

						int32 Count = UCollisionPresetBPLibrary::BatchModifyCollisionPresets(
							*State->SelectedPreset,
							SelectedFolders,
							SelectedMeshes
						);

						UE_LOG(LogTemp, Display, TEXT("CollisionPreset: 已处理 %d 个 StaticMesh"), Count);
#endif
						return FReply::Handled();
					})
				]
			]
		];
}

static TSharedRef<SDockTab> SpawnPCGToFoliageTab(const FSpawnTabArgs& SpawnTabArgs)
{
	TSharedRef<FPCGToFoliagePanelState> State = MakeShared<FPCGToFoliagePanelState>();
	State->FolderName = MakeShared<FString>(TEXT("FoliageType"));

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("PCGToFoliageTabTitle", "PCG To Foliage"))
		.ToolTipText(LOCTEXT("PCGToFoliageTabTooltip", "将 PCG Instanced StaticMesh 烘焙转换为 Foliage Actor"))
		[
			SNew(SBox)
			.Padding(20.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 10)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PCGToFoliageHint", "1. 在世界大纲中选择包含 PCG 生成的 Instanced StaticMesh 的 Actor（可多选）\n2. 勾选是否销毁原 ISM 实例\n3. 勾选是否创建子文件夹保存 FoliageType\n4. 点击按钮开始转换"))
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 12)
				[
					SNew(SCheckBox)
					.OnCheckStateChanged_Lambda([State](ECheckBoxState NewState)
					{
						State->bDestroySourceInstances = (NewState == ECheckBoxState::Checked);
					})
					.Content()
					[
						SNew(STextBlock).Text(LOCTEXT("DestroySourceInstances", "销毁原 ISM 实例"))
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 8)
				[
					SNew(SCheckBox)
					.IsChecked(true)
					.OnCheckStateChanged_Lambda([State](ECheckBoxState NewState)
					{
						State->bCreateFolder = (NewState == ECheckBoxState::Checked);
					})
					.Content()
					[
						SNew(STextBlock).Text(LOCTEXT("CreateFolder", "在 StaticMesh 同级目录创建子文件夹保存 FoliageType"))
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 20)
				[
					SNew(SButton)
					.Text(LOCTEXT("ConvertToFoliageButton", "转换为 Foliage"))
					.OnClicked_Lambda([State]()
					{
#if WITH_EDITOR
						// 获取世界大纲中选中的所有 Actor
						TArray<AActor*> SelectedActors;
						GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors);

						if (SelectedActors.Num() == 0)
						{
							UE_LOG(LogTemp, Warning, TEXT("PCGToFoliage: 请先在世界大纲中选择包含 PCG ISM 的 Actor"));
							return FReply::Handled();
						}

						// 使用多 Actor 版本的函数
						UPCGToFoliageLib::ConvertMultipleActorsToFoliage(
							SelectedActors[0],  // WorldContextObject
							SelectedActors,      // 所有选中的 Actor
							State->bDestroySourceInstances,
							State->bCreateFolder,
							*State->FolderName
						);

						UE_LOG(LogTemp, Display, TEXT("PCGToFoliage: 转换完成"));
#endif
						return FReply::Handled();
					})
				]
			]
		];
}

static TSharedRef<FWorkspaceItem> GetOrCreateTAToolsCategory()
{
	static TSharedPtr<FWorkspaceItem> TAToolsCategory;
	if (!TAToolsCategory.IsValid())
	{
		TAToolsCategory = WorkspaceMenu::GetMenuStructure().GetStructureRoot()->AddGroup(
			FName("TATools"),
			LOCTEXT("TAToolsCategory", "TA Tools"),
			LOCTEXT("TAToolsCategoryTooltip", "TA 工具"),
			FSlateIcon(),
			true);
	}
	return TAToolsCategory.ToSharedRef();
}

static void RegisterModelShowcaseTab(TSharedPtr<FTabManager> InTabManager)
{
	if (!InTabManager.IsValid()) return;

	InTabManager->RegisterTabSpawner(ModelShowcaseTabName, FOnSpawnTab::CreateStatic(&SpawnModelShowcaseTab))
		.SetDisplayName(LOCTEXT("ModelShowcase", "Model Showcase"))
		.SetTooltipText(LOCTEXT("ModelShowcaseTooltip", "根据 Content 选中文件夹的模型结构排布到场景中（需先选文件夹和 Plan Actor）"))
		.SetGroup(GetOrCreateTAToolsCategory());
}

static void RegisterCollisionPresetTab(TSharedPtr<FTabManager> InTabManager)
{
	if (!InTabManager.IsValid()) return;

	InTabManager->RegisterTabSpawner(CollisionPresetTabName, FOnSpawnTab::CreateStatic(&SpawnCollisionPresetTab))
		.SetDisplayName(LOCTEXT("CollisionPresetManager", "Collision Preset Manager"))
		.SetTooltipText(LOCTEXT("CollisionPresetManagerTooltip", "批量修改 StaticMesh 资产的 Collision Presets 并保存"))
		.SetGroup(GetOrCreateTAToolsCategory());
}

static void RegisterPCGToFoliageTab(TSharedPtr<FTabManager> InTabManager)
{
	if (!InTabManager.IsValid()) return;

	InTabManager->RegisterTabSpawner(PCGToFoliageTabName, FOnSpawnTab::CreateStatic(&SpawnPCGToFoliageTab))
		.SetDisplayName(LOCTEXT("PCGToFoliage", "PCG To Foliage"))
		.SetTooltipText(LOCTEXT("PCGToFoliageTooltip", "将 PCG Instanced StaticMesh 烘焙转换为 Foliage Actor"))
		.SetGroup(GetOrCreateTAToolsCategory());
}

static void MakeTAToolsSubMenu(UToolMenu* InMenu)
{
	FToolMenuSection& Section = InMenu->AddSection("TAToolsSection", LOCTEXT("TAToolsSection", "TA 工具"));

	Section.AddMenuEntry(
		"ModelShowcase",
		LOCTEXT("ModelShowcase", "资产排列"),
		LOCTEXT("ModelShowcaseTooltip", "根据 Content 选中文件夹的模型结构排布到场景中（需先选文件夹和 Plan Actor）"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			if (FLevelEditorModule* LevelEditorModule = FModuleManager::Get().GetModulePtr<FLevelEditorModule>("LevelEditor"))
			{
				if (TSharedPtr<FTabManager> TabManager = LevelEditorModule->GetLevelEditorTabManager())
				{
					TabManager->TryInvokeTab(ModelShowcaseTabName);
				}
			}
		}))
	);


	Section.AddMenuEntry(
		"PCGToFoliage",
		LOCTEXT("PCGToFoliage", "烘焙Foliage"),
		LOCTEXT("PCGToFoliageMenuTooltip", "将 PCG Instanced StaticMesh 烘焙转换为 Foliage Actor"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			if (FLevelEditorModule* LevelEditorModule = FModuleManager::Get().GetModulePtr<FLevelEditorModule>("LevelEditor"))
			{
				if (TSharedPtr<FTabManager> TabManager = LevelEditorModule->GetLevelEditorTabManager())
				{
					TabManager->TryInvokeTab(PCGToFoliageTabName);
				}
			}
		}))
	);


}

static void RegisterTAToolsMenu()
{
	FToolMenuOwnerScoped OwnerScoped(TEXT("BiomePCG"));
	// 注册 TA Tools 子菜单内容（仅此一处，避免与 AddSubMenu 的 delegate 重复）
	UToolMenu* TAToolsMenu = UToolMenus::Get()->RegisterMenu("LevelEditor.MainMenu.TATools", NAME_None, EMultiBoxType::Menu);
	TAToolsMenu->AddDynamicSection("TAToolsSection", FNewToolMenuDelegate::CreateStatic(&MakeTAToolsSubMenu));
	// 在顶栏添加 TA Tools 下拉，使用空 FNewToolMenuChoice 从已注册的 LevelEditor.MainMenu.TATools 拉取内容（避免重复）
	UToolMenu* MainMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu");
	FToolMenuSection& Section = MainMenu->FindOrAddSection(NAME_None);
	FToolMenuEntry& TAToolsEntry = Section.AddSubMenu(
		"TATools",
		LOCTEXT("TAToolsMenu", "TA Tools"),
		LOCTEXT("TAToolsMenuTooltip", "TA 工具"),
		FNewToolMenuChoice()
	);
	TAToolsEntry.InsertPosition = FToolMenuInsert("Help", EToolMenuInsertType::Before);
}

void FBiomePCGModule::StartupModule()
{
#if WITH_EDITOR
	// 注册到 Level Editor 的 TabManager，这样会出现在 Window 菜单中
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	OnRegisterTabsHandle = LevelEditorModule.OnRegisterTabs().AddStatic(&RegisterModelShowcaseTab);
	OnRegisterTabsHandle2 = LevelEditorModule.OnRegisterTabs().AddStatic(&RegisterCollisionPresetTab);
	OnRegisterTabsHandle3 = LevelEditorModule.OnRegisterTabs().AddStatic(&RegisterPCGToFoliageTab);

	// 若 Level Editor 已创建，立即注册（例如插件热重载后）
	if (TSharedPtr<FTabManager> TabManager = LevelEditorModule.GetLevelEditorTabManager())
	{
		RegisterModelShowcaseTab(TabManager);
		RegisterCollisionPresetTab(TabManager);
		RegisterPCGToFoliageTab(TabManager);
	}

	// 在顶栏添加 TA Tools 下拉菜单（与 File、Edit、Window、Tools 同级）
	MenuCallbackHandle = UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&RegisterTAToolsMenu));
#endif
}

void FBiomePCGModule::ShutdownModule()
{
#if WITH_EDITOR
	UToolMenus::UnRegisterStartupCallback(MenuCallbackHandle);
	UToolMenus::UnregisterOwner(TEXT("BiomePCG"));

	if (FLevelEditorModule* LevelEditorModule = FModuleManager::Get().GetModulePtr<FLevelEditorModule>("LevelEditor"))
	{
		LevelEditorModule->OnRegisterTabs().Remove(OnRegisterTabsHandle);
		LevelEditorModule->OnRegisterTabs().Remove(OnRegisterTabsHandle2);
		LevelEditorModule->OnRegisterTabs().Remove(OnRegisterTabsHandle3);
		if (TSharedPtr<FTabManager> TabManager = LevelEditorModule->GetLevelEditorTabManager())
		{
			TabManager->UnregisterTabSpawner(ModelShowcaseTabName);
			TabManager->UnregisterTabSpawner(CollisionPresetTabName);
			TabManager->UnregisterTabSpawner(PCGToFoliageTabName);
		}
	}
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBiomePCGModule, BiomePCG)