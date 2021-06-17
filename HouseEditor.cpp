// Fill out your copyright notice in the Description page of Project Settings.

#include "HouseEditor/HouseEditor.h"

#include "HouseEditor/HouseEditorFunctionLibrary.h"
#include "HouseEditor/BaseFireLadder.h"
#include "HouseEditor/BaseQuoin.h"
#include "HouseEditor/BasePilaster.h"
#include "HouseEditor/BaseCornice.h"
#include "HouseEditor/BaseRoof.h"

#include "SegmentEditor/BaseSegment.h"
#include "SegmentEditor/BaseSegmentDecoration.h"
#include "EdgeHouseConstructor/EdgeHouseConstructorSettings.h"
#include "RuntimeMesh/EDGEMeshUtility.h"

#include "MeshMergeModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Selection.h"
#include "Components/SceneComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "RuntimeMesh/RMCProviderManager.h"
#include "UObject/UObjectGlobals.h"

AHouseEditor::AHouseEditor()
{
	PrimaryActorTick.bCanEverTick = false;
	Walls.SetNum(4);
	TemplateLocal.WallPatterns.SetNum(4);
	TemplateLocal.FireLadderRates.SetNum(4);
	TemplateLocal.FireLadderOffsets.SetNum(4);
	TemplateLocal.PilasterRates.SetNum(4);
	TemplateLocal.PilasterOffsets.SetNum(4);
	TemplateLocal.HouseLength = 1;
	TemplateLocal.HouseWidth = 1;
	TemplateLocal.HouseHeight = 1;
	
	bFirstConstruction = true;

	DefaultRoofClass = GetDefault<UEdgeHouseConstructorSettings>()->DefaultRoofClass;
	DefaultWallMat = Cast<UMaterialInterface>(GetDefault<UEdgeHouseConstructorSettings>()->DefaultWallMaterial.ResolveObject());

	// Temp default values
	FillDecorationsWithDefaults(TemplateLocal.GlobalDecorationWeights);

	BaseRoot = CreateDefaultSubobject<USceneComponent>(FName("Root"));
	RootComponent = BaseRoot;

	HouseNavCollider = CreateDefaultSubobject<UBoxComponent>(FName("House Navigation Collider"));
	HouseNavCollider->SetCollisionProfileName("HouseNav");
	HouseNavCollider->SetCanEverAffectNavigation(true);
	HouseNavCollider->InitBoxExtent(FVector(0.f));
	HouseNavCollider->SetupAttachment(BaseRoot);
	
}

void AHouseEditor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (RuntimeMeshComponent == nullptr)
	{
		RuntimeMeshComponent = NewObject<URuntimeMeshComponent>(this, URuntimeMeshComponent::StaticClass(), NAME_None);
		RuntimeMeshComponent->CreationMethod = EComponentCreationMethod::Native;
		RuntimeMeshComponent->SetCanEverAffectNavigation(false);
		RuntimeMeshComponent->RegisterComponent();
		RuntimeMeshComponent->AttachToComponent(BaseRoot, FAttachmentTransformRules::SnapToTargetIncludingScale);
		bFirstConstruction = true;
	}
	
	if (bFirstConstruction)
	{
		bFirstConstruction = false;

		RMCProvider = NewObject<UEDGERuntimeMeshProvider>(this);

		if (ReadTemplate())
		{
			// --- New mesh merging
			if (!GetRMCProvider()->HaveMeshData())
			{
				//if (!(IsInGameThread() || IsAsyncLoading()))
				{
					GenerateMeshData();
					ClearHouse();
				}
			}
			// ---
		}

		// KOSTYL. remove links to elements that are not attached - possible when copying editor BP with them
		if (RoofActor != nullptr)
		{
			if (RoofActor->GetAttachParentActor() != this)
			{
				AllSegments.Empty();
				AllCustoms.Empty();
				RoofActor = nullptr;
			}
		}

		// --- Old mesh merging
		// if (TemplateLocal.MergedMesh != nullptr && SavedHouseMeshComponent == nullptr)
		// {
		// 	ConstructMergedMeshComponent();
		// }
		// ---
	}

	if (RMCProvider != nullptr)
	{
		if (GetRMCProvider()->HaveMeshData() && AllSegments.Num() == 0)
		{
			GetRuntimeMeshComponent()->Initialize(GetRMCProvider());
		}
	}
	else
	{
		RMCProvider = NewObject<UEDGERuntimeMeshProvider>(this);
	}
	
}

void AHouseEditor::ClearHouse(bool bRemoveInstanceMeshes)
{
	if (SavedHouseMeshComponent != nullptr)
	{
		SavedHouseMeshComponent->DestroyComponent();
	}
	GetRuntimeMeshComponent()->SetRuntimeMesh(nullptr);
	while (AllCustoms.Num() > 0) {
		AActor* Element = AllCustoms.Pop();
		if (Element != nullptr)
		{
			Element->Destroy();
		}
	}
	while (AllSegments.Num() > 0) {
		AActor* Element = AllSegments.Pop();
		if (Element != nullptr)
		{
			TArray<AActor*> Decorations;
			Element->GetAttachedActors(Decorations);
			for (AActor* Actor : Decorations)
			{
				Actor->Destroy();
			}
			Element->Destroy();
		}
	}
	while (WallAnchors.Num() > 0) {
		USceneComponent* Element = WallAnchors.Pop();
		if (Element != nullptr)
		{
			Element->DestroyComponent();
		}
	}
	if (RoofActor != nullptr) {
		RoofActor->Destroy();
	}
	if (bRemoveInstanceMeshes)
	{
		while (InstancedElements.Num() > 0)
		{
			InstancedElements.Pop()->DestroyComponent();
		}
	}
}

bool AHouseEditor::BuildHouse() {
	ClearHouse();

	// works for now - cause using only "box"-houses, must be reworked later
	for (int i = 0; i < 4; i++) {
		FVector CompRelLoc;
		switch (i) {
		case 0:
			CompRelLoc = FVector(0);
			break;
		case 1:
			CompRelLoc = FVector(SegmentWidthInUnits * TemplateLocal.HouseLength, 0, 0);
			break;
		case 2:
			CompRelLoc = FVector(SegmentWidthInUnits * TemplateLocal.HouseLength, SegmentWidthInUnits * -TemplateLocal.HouseWidth, 0);
			break;
		case 3:
			CompRelLoc = FVector(0, SegmentWidthInUnits * -TemplateLocal.HouseWidth, 0);
			break;
		default:
			break;
		}

		FName CompName = *FString::Printf(TEXT("Anchor_%i"), i);
		USceneComponent* NewComp = NewObject<USceneComponent>(this, USceneComponent::StaticClass(), CompName);
		NewComp->CreationMethod = EComponentCreationMethod::Instance;
		NewComp->SetupAttachment(RootComponent);
		NewComp->SetRelativeLocationAndRotation(CompRelLoc, FRotator(0, i * -90, 0));
		NewComp->RegisterComponent();
		WallAnchors.Add(NewComp);
	}

	// Random preparations (for decorations)
	if (TemplateLocal.RandomSeed <= 0)
	{
		TemplateLocal.RandomSeed = abs(rand());
	}
	srand(1);		// Reset any previous calls
	srand(static_cast<unsigned>(TemplateLocal.RandomSeed));		// MMMM?????

	// Walls

	UDataTable* PatternDataTable = UHouseEditorFunctionLibrary::GetPatternDataTable();
	if (PatternDataTable == NULL)
	{
		UE_LOG(LogTemp, Warning, TEXT("Data table is not valid. Can't build house. Construction aborted."));
		return false;
	}
	
	for (int WallIndex = 0; WallIndex < Walls.Num(); WallIndex++) {
		FPatternData* Data = PatternDataTable->FindRow<FPatternData>(TemplateLocal.WallPatterns[WallIndex], FString());
		if (Data == NULL) {
			UE_LOG(LogTemp, Warning, TEXT("Wall pattern #%i - '%s' was not found. Construction aborted."), WallIndex, *TemplateLocal.WallPatterns[WallIndex].ToString());
			ClearHouse();
			return false;
		}
		Walls[WallIndex] = *Data;

		int WallLength = (WallIndex % 2 == 0) ? TemplateLocal.HouseLength : TemplateLocal.HouseWidth; // works for now - couse using only "box"-houses

		// FireLadders

		TArray<int> LaddersHIndexes;
		if (TemplateLocal.HouseHeight > 1 && TemplateLocal.FireLadderClass != NULL && TemplateLocal.FireLadderRates[WallIndex] > 0 && WallLength >= 4) {
			ABaseFireLadder* NewLadder;
			if (TemplateLocal.FireLadderRates[WallIndex] == 1) {
				int SegmentIndex = FMath::Clamp(TemplateLocal.FireLadderOffsets[WallIndex], 0, WallLength - 4);
				NewLadder = GetWorld()->SpawnActorDeferred<ABaseFireLadder>(TemplateLocal.FireLadderClass, FTransform(), this);
				NewLadder->ladderHeight = TemplateLocal.HouseHeight - 1;
				NewLadder->FinishSpawning(FTransform(FVector(SegmentWidthInUnits * (1 + SegmentIndex), 0, GetRealHeight())));
				NewLadder->AttachToComponent(WallAnchors[WallIndex], FAttachmentTransformRules::KeepRelativeTransform);
				AllCustoms.Add(NewLadder);
				LaddersHIndexes.Add(SegmentIndex+1);
				LaddersHIndexes.Add(SegmentIndex+2);
			}
			else {
				for (int SegmentIndex = 1; SegmentIndex < WallLength - 2; SegmentIndex++) {
					if ((SegmentIndex - TemplateLocal.FireLadderOffsets[WallIndex]) % TemplateLocal.FireLadderRates[WallIndex] == 0) {
						NewLadder = GetWorld()->SpawnActorDeferred<ABaseFireLadder>(TemplateLocal.FireLadderClass, FTransform(), this);
						NewLadder->ladderHeight = TemplateLocal.HouseHeight - 1;
						NewLadder->FinishSpawning(FTransform(FVector(SegmentWidthInUnits * SegmentIndex, 0, GetRealHeight())));
						NewLadder->AttachToComponent(WallAnchors[WallIndex], FAttachmentTransformRules::KeepRelativeTransform);
						AllCustoms.Add(NewLadder);
						LaddersHIndexes.Add(SegmentIndex);
						LaddersHIndexes.Add(SegmentIndex+1);
					}
				}
			}
		}
		
		// Segments

		for (int vi = 0; vi < TemplateLocal.HouseHeight; vi++)
		{
			int OffsetH = 0;
			int LineIndex;
			if (Data->bRepeatPattern) {
				LineIndex = vi % Walls[WallIndex].Lines.Num();
			}
			else {
				LineIndex = (vi < Walls[WallIndex].Lines.Num()) ? vi : Walls[WallIndex].Lines.Num() - 1;
			}
			FPatternLine Line = Walls[WallIndex].Lines[LineIndex];
			
			for (int hi = 0; hi + OffsetH < WallLength; hi++) {
				int SegmentHIndex;
				if (Line.bRepeatLine) {
					SegmentHIndex = hi % Line.Segments.Num();
				}
				else {
					SegmentHIndex = (hi < Line.Segments.Num()) ? hi : Line.Segments.Num() - 1;
				}

				ABaseSegment* NewActor = GetWorld()->SpawnActor<ABaseSegment>(UHouseEditorFunctionLibrary::GetSegmentClassByFName(Line.Segments[SegmentHIndex].SegmentName), FVector(SegmentWidthInUnits * (hi + OffsetH), 0, GetRealHeight(vi)), FRotator(0));
				if (NewActor == NULL) {
					UE_LOG(LogTemp, Warning, TEXT("Actor was not created - aborting construction."));
					ClearHouse();
					return false;
				}
				NewActor->AttachToComponent(WallAnchors[WallIndex], FAttachmentTransformRules::KeepRelativeTransform);

				//TArray<TEnumAsByte<EMaterialSlot>> keys;
				//materialOverrides.GetKeys(keys);
				//for (int i = 0; i < keys.Num(); i++) {
				//	newActor->SetMaterialByName(keys[i], materialOverrides[keys[i]]);
				//}

				// temp single mat override ^
				if (TemplateLocal.WallMatOverride != nullptr) {
					NewActor->SetMaterialByName(Wall, TemplateLocal.WallMatOverride);
				}

				AllSegments.Add(NewActor);

				// Segment decorations

				if (LaddersHIndexes.Contains(hi + OffsetH) == false || vi == 0) {
					TArray<USceneComponent*> Sockets = NewActor->GetDecorationSockets();

					if (Sockets.Num() > 0)
					{
						for (USceneComponent* Socket : Sockets)
						{
							float RValue = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
							float Threshold = 0.f;

							TMap<FName, float> WeightsMap;
							if (vi == 0 && TemplateLocal.bGlobalDecorationsIgnoreGroundFloor)
							{
								const FSegmentDecorationsData EmptyData;
								WeightsMap = UHouseEditorFunctionLibrary::GetNormalizedDecorationWeights(Line.Segments[SegmentHIndex], Socket->GetFName(), EmptyData);
							}
							else
							{
								WeightsMap = UHouseEditorFunctionLibrary::GetNormalizedDecorationWeights(Line.Segments[SegmentHIndex], Socket->GetFName(), TemplateLocal.GlobalDecorationWeights[WallIndex]);
							}
					
							TSubclassOf<ABaseSegmentDecoration> DecorationClass;
							TArray<FName> Names;
							WeightsMap.GenerateKeyArray(Names);
		
							for (FName Name : Names)
							{
								Threshold += WeightsMap.FindRef(Name);
								if (RValue <= Threshold)
								{
									DecorationClass = UHouseEditorFunctionLibrary::GetSegmentDecorationClassByFName(Name);
									break;
								}
							}
							if (DecorationClass != NULL)
							{
								ABaseSegmentDecoration* NewDecoration = GetWorld()->SpawnActor<ABaseSegmentDecoration>(DecorationClass, FVector(0.f), FRotator(0.f));
								if (NewDecoration == NULL)
								{
									UE_LOG(LogTemp, Warning, TEXT("Decoration was not created! Process continued..."));
								}
								NewDecoration->AttachToComponent(Socket, FAttachmentTransformRules::KeepRelativeTransform);
							}
						}
					}
				}
				
				OffsetH += NewActor->GetSegmentSize() - 1;
			}
		}


		// Quoins

		if (TemplateLocal.QuoinClass != NULL) {
			ABaseQuoin* NewQuoin = GetWorld()->SpawnActorDeferred<ABaseQuoin>(TemplateLocal.QuoinClass, FTransform(), this);
			NewQuoin->quoinHeight = TemplateLocal.HouseHeight;
			if (TemplateLocal.BottomCorniceClass != NULL) {
				NewQuoin->corniceOffset = CorniceHeightOffset;
			}
			NewQuoin->FinishSpawning(FTransform());
			NewQuoin->AttachToComponent(WallAnchors[WallIndex], FAttachmentTransformRules::KeepRelativeTransform);
			AllCustoms.Add(NewQuoin);
		}

		// Pilasters

		if (TemplateLocal.PilasterClass != NULL && TemplateLocal.PilasterRates[WallIndex] > 0) {
			for (int SegmentNum = 1; SegmentNum < WallLength; SegmentNum++) {
				if ((SegmentNum - TemplateLocal.PilasterOffsets[WallIndex]) % TemplateLocal.PilasterRates[WallIndex] == 0) {
					ABasePilaster*  NewPilaster = GetWorld()->SpawnActorDeferred<ABasePilaster>(TemplateLocal.PilasterClass, FTransform(), this);
					NewPilaster->pilasterHeight = (TemplateLocal.bPilasterIgnoreGroundFloor) ? TemplateLocal.HouseHeight - 1 : TemplateLocal.HouseHeight;
					if (TemplateLocal.BottomCorniceClass != NULL && !TemplateLocal.bPilasterIgnoreGroundFloor) {
						NewPilaster->corniceOffset = CorniceHeightOffset;
					}
					NewPilaster->FinishSpawning(FTransform(FVector(SegmentWidthInUnits * SegmentNum, 0, (TemplateLocal.bPilasterIgnoreGroundFloor) ? GetRealHeight() : 0)));
					NewPilaster->AttachToComponent(WallAnchors[WallIndex], FAttachmentTransformRules::KeepRelativeTransform);
					AllCustoms.Add(NewPilaster);
				}
			}
		}

		// Cornices

		if (TemplateLocal.BottomCorniceClass != NULL) {
			ABaseCornice* NewCornice = GetWorld()->SpawnActorDeferred<ABaseCornice>(TemplateLocal.BottomCorniceClass, FTransform(), this);
			NewCornice->corniceLength = WallLength;
			NewCornice->FinishSpawning(FTransform(FVector(0, 0, SegmentHeightInUnits)));
			NewCornice->AttachToComponent(WallAnchors[WallIndex], FAttachmentTransformRules::KeepRelativeTransform);
			AllCustoms.Add(NewCornice);
		}
		if (TemplateLocal.TopCorniceClass != NULL) {
			ABaseCornice* NewCornice = GetWorld()->SpawnActorDeferred<ABaseCornice>(TemplateLocal.TopCorniceClass, FTransform(), this);
			NewCornice->corniceLength = WallLength;
			NewCornice->FinishSpawning(FTransform(FVector(0, 0, GetRealHeight(TemplateLocal.HouseHeight))));
			NewCornice->AttachToComponent(WallAnchors[WallIndex], FAttachmentTransformRules::KeepRelativeTransform);
			AllCustoms.Add(NewCornice);
		}
	}

	// Roof

	if (TemplateLocal.RoofClass == nullptr) {
		if (DefaultRoofClass == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("No Default Room Class found. Please check Edge House Constructor plugin settings."))
		}
		TemplateLocal.RoofClass = DefaultRoofClass;
	}
	float ExtraHeight = (TemplateLocal.TopCorniceClass != NULL) ? CorniceHeightOffset - 0.3f : 0;
	RoofActor = GetWorld()->SpawnActorDeferred<ABaseRoof>(TemplateLocal.RoofClass, FTransform(), this);
	RoofActor->RoofLength = TemplateLocal.HouseLength;
	RoofActor->RoofWidth = TemplateLocal.HouseWidth;
	RoofActor->FinishSpawning(FTransform(FVector(0, 0, GetRealHeight(TemplateLocal.HouseHeight) + ExtraHeight)));
	RoofActor->AttachToComponent(WallAnchors[0], FAttachmentTransformRules::KeepRelativeTransform);

	if (TemplateLocal.WallMatOverride != nullptr)
	{
		RoofActor->SetMaterialByName(Wall, TemplateLocal.WallMatOverride );
	}

	return true;
}

bool AHouseEditor::ReadTemplate()
{
	if (HouseTemplateName != NAME_None)
	{
		UDataTable* HouseTemplateTable = UHouseEditorFunctionLibrary::GetHouseParamsTemplateDataTable();
		if (HouseTemplateTable == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("No House Params Template Data Table availavle. Aborting parameters transfer."))
			return false;
		}
		FHouseParamsTemplate* MainTemplate = HouseTemplateTable->FindRow<FHouseParamsTemplate>(HouseTemplateName, FString());
		if (MainTemplate == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("Aborting parameters transfer."))
			return false;
		}

		TemplateLocal.HouseHeight = (TemplateOverride.bOverHouseHeight) ? TemplateOverride.HouseHeight : MainTemplate->HouseHeight;
		TemplateLocal.HouseLength = (TemplateOverride.bOverHouseLength) ? TemplateOverride.HouseLength : MainTemplate->HouseLength;
		TemplateLocal.HouseWidth = (TemplateOverride.bOverHouseWidth) ? TemplateOverride.HouseWidth : MainTemplate->HouseWidth;
		TemplateLocal.PilasterClass = (TemplateOverride.bOverPilasterClass) ? TemplateOverride.PilasterClass : MainTemplate->PilasterClass;
		TemplateLocal.PilasterOffsets = (TemplateOverride.bOverPilasterOffsets) ? TemplateOverride.PilasterOffsets : MainTemplate->PilasterOffsets;
		TemplateLocal.PilasterRates = (TemplateOverride.bOverPilasterRates) ? TemplateOverride.PilasterRates : MainTemplate->PilasterRates;
		TemplateLocal.QuoinClass = (TemplateOverride.bOverQuoinClass) ? TemplateOverride.QuoinClass : MainTemplate->QuoinClass;
		TemplateLocal.RoofClass = (TemplateOverride.bOverRoofClass) ? TemplateOverride.RoofClass : MainTemplate->RoofClass;
		TemplateLocal.WallPatterns = (TemplateOverride.bOverWallPatterns) ? TemplateOverride.WallPatterns : MainTemplate->WallPatterns;
		TemplateLocal.FireLadderClass = (TemplateOverride.bOverFireLadderClass) ? TemplateOverride.FireLadderClass : MainTemplate->FireLadderClass;
		TemplateLocal.FireLadderOffsets = (TemplateOverride.bOverFireLadderOffsets) ? TemplateOverride.FireLadderOffsets : MainTemplate->FireLadderOffsets;
		TemplateLocal.FireLadderRates = (TemplateOverride.bOverFireLadderRates) ? TemplateOverride.FireLadderRates : MainTemplate->FireLadderRates;
		TemplateLocal.BottomCorniceClass = (TemplateOverride.bOverBottomCorniceClass) ? TemplateOverride.BottomCorniceClass : MainTemplate->BottomCorniceClass;
		TemplateLocal.TopCorniceClass = (TemplateOverride.bOverTopCorniceClass) ? TemplateOverride.TopCorniceClass : MainTemplate->TopCorniceClass;
		TemplateLocal.WallMatOverride = (TemplateOverride.bOverWallMatOverride) ? TemplateOverride.WallMatOverride : MainTemplate->WallMatOverride;
		TemplateLocal.bPilasterIgnoreGroundFloor = (TemplateOverride.bOverPilasterIgnoreGroundFloor)
												? TemplateOverride.bPilasterIgnoreGroundFloor : MainTemplate->bPilasterIgnoreGroundFloor;
		TemplateLocal.GlobalDecorationWeights = (TemplateOverride.bOverGlobalDecorationWeights)
												? TemplateOverride.GlobalDecorationWeights : MainTemplate->GlobalDecorationWeights;
		TemplateLocal.bGlobalDecorationsIgnoreGroundFloor = (TemplateOverride.bOverGlobalDecorationsIgnoreGroundFloor)
												? TemplateOverride.bGlobalDecorationsIgnoreGroundFloor : MainTemplate->bGlobalDecorationsIgnoreGroundFloor;
		TemplateLocal.MergedMesh = (TemplateOverride.bOverMergedMesh) ? TemplateOverride.MergedMesh : MainTemplate->MergedMesh;

		TemplateLocal.RandomSeed = MainTemplate->RandomSeed;
		
		// Check for empty input - can occur from old data
		if (TemplateLocal.GlobalDecorationWeights.Num() == 0)
		{
			FillDecorationsWithDefaults(TemplateLocal.GlobalDecorationWeights);
		}

		// --- Don't think this must be here after all --- Old mesh merging
		// if(TemplateLocal.MergedMesh != nullptr)
		// {
		// 	ConstructMergedMeshComponent();
		// }
		// else
		// {
		// 	BuildHouse();
		// }
		// ---
		return true;
	}
	return false;
}

void AHouseEditor::SaveTemplate()
{
	UDataTable* HouseTemplateTable = UHouseEditorFunctionLibrary::GetHouseParamsTemplateDataTable();
	if (HouseTemplateTable == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("No House Params Template Data Table availavle. Aborting template saving."))
		return;
	}

	// --- Old mesh merging
	//CreateMergedMesh(HouseTemplateName.ToString());
	// ---
	
	bMeshIsDirty = true;
	
	HouseTemplateTable->AddRow(HouseTemplateName, TemplateLocal);

	UHouseEditorFunctionLibrary::CheckOutAndSave(HouseTemplateTable);
}

void AHouseEditor::SaveTemplateOverride()
{
	if (HouseTemplateName == NAME_None)
	{
		UE_LOG(LogTemp, Warning, TEXT("No base template for override generation. Specify one or save as new template instead of overrides."));
		return;
	}
	UDataTable* HouseTemplateTable = UHouseEditorFunctionLibrary::GetHouseParamsTemplateDataTable();
	if (HouseTemplateTable == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("No House Params Template Data Table availavle. Aborting parameters transfer."))
		return;
	}
	FHouseParamsTemplate* MainTemplate = HouseTemplateTable->FindRow<FHouseParamsTemplate>(HouseTemplateName, FString());
	if (MainTemplate == nullptr)
	{
		return;
	}

	TemplateOverride = *(new FHouseParamsOverrides());
	
	if (TemplateLocal.HouseHeight != MainTemplate->HouseHeight)
	{
		TemplateOverride.bOverHouseHeight = true;
		TemplateOverride.HouseHeight = TemplateLocal.HouseHeight;
	}
	if (TemplateLocal.HouseLength != MainTemplate->HouseLength)
	{
		TemplateOverride.bOverHouseLength = true;
		TemplateOverride.HouseLength = TemplateLocal.HouseLength;
	}
	if (TemplateLocal.HouseWidth != MainTemplate->HouseWidth)
	{
		TemplateOverride.bOverHouseWidth = true;
		TemplateOverride.HouseWidth = TemplateLocal.HouseWidth;
	}
	if (TemplateLocal.PilasterClass != MainTemplate->PilasterClass)
	{
		TemplateOverride.bOverPilasterClass = true;
		TemplateOverride.PilasterClass = TemplateLocal.PilasterClass;
	}
	if (TemplateLocal.QuoinClass != MainTemplate->QuoinClass)
	{
		TemplateOverride.bOverQuoinClass = true;
		TemplateOverride.QuoinClass = TemplateLocal.QuoinClass;
	}
	if (TemplateLocal.RoofClass != MainTemplate->RoofClass)
	{
		TemplateOverride.bOverRoofClass = true;
		TemplateOverride.RoofClass = TemplateLocal.RoofClass;
	}
	if (TemplateLocal.BottomCorniceClass != MainTemplate->BottomCorniceClass)
	{
		TemplateOverride.bOverBottomCorniceClass = true;
		TemplateOverride.BottomCorniceClass = TemplateLocal.BottomCorniceClass;
	}
	if (TemplateLocal.TopCorniceClass != MainTemplate->TopCorniceClass)
	{
		TemplateOverride.bOverTopCorniceClass = true;
		TemplateOverride.TopCorniceClass = TemplateLocal.TopCorniceClass;
	}
	if (TemplateLocal.FireLadderClass != MainTemplate->FireLadderClass)
	{
		TemplateOverride.bOverFireLadderClass = true;
		TemplateOverride.FireLadderClass = TemplateLocal.FireLadderClass;
	}
	if (TemplateLocal.WallMatOverride != MainTemplate->WallMatOverride)
	{
		TemplateOverride.bOverWallMatOverride = true;
		TemplateOverride.WallMatOverride = TemplateLocal.WallMatOverride;
	}
	if (TemplateLocal.bPilasterIgnoreGroundFloor != MainTemplate->bPilasterIgnoreGroundFloor)
	{
		TemplateOverride.bOverPilasterIgnoreGroundFloor = true;
		TemplateOverride.bPilasterIgnoreGroundFloor = TemplateLocal.bPilasterIgnoreGroundFloor;
	}
	if (TemplateLocal.PilasterOffsets != MainTemplate->PilasterOffsets)
	{
		TemplateOverride.bOverPilasterOffsets = true;
		TemplateOverride.PilasterOffsets = TemplateLocal.PilasterOffsets;
	}
	if (TemplateLocal.PilasterRates != MainTemplate->PilasterRates)
	{
		TemplateOverride.bOverPilasterRates = true;
		TemplateOverride.PilasterRates = TemplateLocal.PilasterRates;
	}
	if (TemplateLocal.FireLadderOffsets != MainTemplate->FireLadderOffsets)
	{
		TemplateOverride.bOverFireLadderOffsets = true;
		TemplateOverride.FireLadderOffsets = TemplateLocal.FireLadderOffsets;
	}
	if (TemplateLocal.FireLadderRates != MainTemplate->FireLadderRates)
	{
		TemplateOverride.bOverFireLadderRates = true;
		TemplateOverride.FireLadderRates = TemplateLocal.FireLadderRates;
	}
	if (TemplateLocal.WallPatterns != MainTemplate->WallPatterns)
	{
		TemplateOverride.bOverWallPatterns = true;
		TemplateOverride.WallPatterns = TemplateLocal.WallPatterns;
	}
	if (TemplateLocal.GlobalDecorationWeights != MainTemplate->GlobalDecorationWeights)
	{
		TemplateOverride.bOverGlobalDecorationWeights = true;
		TemplateOverride.GlobalDecorationWeights = TemplateLocal.GlobalDecorationWeights;
	}
	if (TemplateLocal.bGlobalDecorationsIgnoreGroundFloor != MainTemplate->bGlobalDecorationsIgnoreGroundFloor)
	{
		TemplateOverride.bOverGlobalDecorationsIgnoreGroundFloor = true;
		TemplateOverride.bGlobalDecorationsIgnoreGroundFloor = TemplateLocal.bGlobalDecorationsIgnoreGroundFloor;
	}
	
	TemplateOverride.bOverMergedMesh = TemplateOverride.bOverHouseHeight || TemplateOverride.bOverHouseLength || TemplateOverride.bOverHouseWidth
									|| TemplateOverride.bOverPilasterClass || TemplateOverride.bOverPilasterOffsets || TemplateOverride.bOverPilasterRates
									|| TemplateOverride.bOverQuoinClass || TemplateOverride.bOverRoofClass || TemplateOverride.bOverWallPatterns
									|| TemplateOverride.bOverFireLadderClass || TemplateOverride.bOverFireLadderOffsets || TemplateOverride.bOverFireLadderRates
									|| TemplateOverride.bOverBottomCorniceClass || TemplateOverride.bOverTopCorniceClass || TemplateOverride.bOverWallMatOverride
									|| TemplateOverride.bOverPilasterIgnoreGroundFloor || TemplateOverride.bOverGlobalDecorationWeights || TemplateOverride.bOverGlobalDecorationsIgnoreGroundFloor;

	bMeshIsDirty = true;
	
	if (TemplateOverride.bOverMergedMesh == true)
	{
		const FString MeshName = GetLevel()->GetName() + "_" + GetName();
		// --- Old mesh merging
		// CreateMergedMesh(MeshName);
		// TemplateOverride.MergedMesh = TemplateLocal.MergedMesh;
		// ---
	}
	
}

void AHouseEditor::ClearTemplateOverride()
{
	TemplateOverride = *(new FHouseParamsOverrides());
	ReadTemplate();
}

void AHouseEditor::UpdateMaterials()
{
	//TArray<TEnumAsByte<EMaterialSlot>> keys;
	//materialOverrides.GetKeys(keys);
	//if (keys.Num() == 0) {
	//	return;
	//}
	// temp single mat override
	UMaterialInterface* newMat = (TemplateLocal.WallMatOverride != nullptr) ? TemplateLocal.WallMatOverride : DefaultWallMat;
	
	for (int i = 0; i < AllSegments.Num(); i++) {
		//for (int k = 0; k < keys.Num(); k++) {
		//	allSegments[i]->SetMaterialByName(keys[k], materialOverrides[keys[k]]);
		//}
		AllSegments[i]->SetMaterialByName(Wall, newMat);
	}
	if (IsValid(RoofActor))
	{
		RoofActor->SetMaterialByName(Wall, newMat);
	}
}

void AHouseEditor::ConstructMergedMeshComponent()
{
	ClearHouse();
	SavedHouseMeshComponent = NewObject<UStaticMeshComponent>(this, FName("MergedMesh"));
	SavedHouseMeshComponent->CreationMethod = EComponentCreationMethod::Instance;
	SavedHouseMeshComponent->SetupAttachment(GetRootComponent());
	SavedHouseMeshComponent->RegisterComponent();
	SavedHouseMeshComponent->SetStaticMesh(TemplateLocal.MergedMesh);
}

void AHouseEditor::RebuildWithRMC()
{
	if (!GetRMCProvider())
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] No RMCProvider to build mesh."), *GetName());
		return;
	}

	if (!GetRMCProvider()->HaveMeshData() || bMeshIsDirty)
	{
		GenerateMeshData();
	}

	ClearHouse();

	// This can be FALSE if house couldn't be built for some reasons
	if (GetRMCProvider()->HaveMeshData())
	{
		GetRuntimeMeshComponent()->Initialize(GetRMCProvider());
	}
}

void AHouseEditor::InstantiateElements()
{
	TArray<FName> MeshNames;
	TArray<UStaticMeshComponent*> SMComps;

	if (AllSegments.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Can't instantiate meshes - there are no segments!"));
		return;
	}
	
	while (InstancedElements.Num() > 0)
	{
		InstancedElements.Pop()->DestroyComponent();
	}

	// Func to add instance to existing instance or create new one
	auto AddInstancedElement = [&](UStaticMeshComponent*& Comp)
	{
		const FName MeshName = Comp->GetStaticMesh()->GetFName();
		const int Idx = MeshNames.Find(MeshName);
		if (Idx >= 0)
		{
			InstancedElements[Idx]->AddInstanceWorldSpace(Comp->GetComponentTransform());
		}
		else
		{
			UInstancedStaticMeshComponent* NewISMComp = NewObject<UInstancedStaticMeshComponent>(this, UInstancedStaticMeshComponent::StaticClass(), MeshName);
			NewISMComp->RegisterComponent();
			this->AddInstanceComponent(NewISMComp);
			NewISMComp->SetStaticMesh(Comp->GetStaticMesh());
			for (int MatIdx = 0; MatIdx < Comp->GetNumMaterials(); MatIdx++)
			{
				NewISMComp->SetMaterial(MatIdx, Comp->GetMaterial(MatIdx));
			}
			NewISMComp->AddInstanceWorldSpace(Comp->GetComponentTransform());
			InstancedElements.Add(NewISMComp);
			MeshNames.Add(MeshName);
		}
	};

	// Copy segments and decorations
	for (auto& Segment : AllSegments)
	{
		TArray<AActor*> Attachements;
		Segment->GetAttachedActors(Attachements);
		for (auto& Attachement : Attachements)
		{
			Attachement->GetComponents<UStaticMeshComponent>(SMComps);
			for (auto& SMComp : SMComps)
			{
				AddInstancedElement(SMComp);
			}
		}

		Segment->GetComponents<UStaticMeshComponent>(SMComps);
		for (auto& SMComp : SMComps)
		{
			AddInstancedElement(SMComp);
		}
	}

	// Copy custom elements
	for (auto& Custom : AllCustoms)
	{
		Custom->GetComponents<UStaticMeshComponent>(SMComps);
		for (auto& SMComp : SMComps)
		{
			AddInstancedElement(SMComp);
		}
	}

	// Copy roof
	RoofActor->GetComponents<UStaticMeshComponent>(SMComps);		// MB make elements (such as cornices/pillars/roof) to create ISMs by default???
	for (auto& SMComp : SMComps)
	{
		AddInstancedElement(SMComp);
	}

	ClearHouse(false);
	
	for (auto& ISM : InstancedElements)
	{
		ISM->MarkRenderStateDirty();
	}

}

void AHouseEditor::GenerateMeshData()
{
	TArray<FRMCSectionData> AllSections;
	vector<EDGEMeshSectionData> AllRawSections;
	
	TArray<UMaterialInterface*> AllMaterials;
	bool bDataFound = false;

	const FString FileName = TemplateOverride.bOverMergedMesh ? GetLevel()->GetName() + "_" + GetName() : HouseTemplateName.ToString();
	
	if (!bMeshIsDirty)
	{
		bDataFound = EDGERuntimeProviderManager::GetProvider(this, FileName, RMCProvider);
	}
	
	if (!bDataFound)
	{
		// If no file found - generate new one
		vector<EDGEMeshSectionData> RawSections;
		TArray<UStaticMeshComponent*> MeshComponents;

		// Build house segments, if they are not present
		if (AllSegments.Num() == 0)
		{
			if (!BuildHouse())
			{
				UE_LOG(LogTemp, Error, TEXT("House couldn't be rebuilt. Aborting mesh generation."));
				return;
			}
		}

		// Start with roof meshes
		if (RoofActor != nullptr)
		{
			RoofActor->GetComponents<UStaticMeshComponent>(MeshComponents);
		}
	
		// Collect meshes from segment and decorations
		for (auto& Segment : AllSegments)
		{
			TArray<UStaticMeshComponent*> ThisComponents;
			Segment->GetComponents<UStaticMeshComponent>(ThisComponents);
			MeshComponents.Append(ThisComponents);

			// --- For now skip decorations
			// TArray<AActor*> Attachements;
			// Segment->GetAttachedActors(Attachements);
			// for (auto& Actor : Attachements)
			// {
			// 	ABaseSegmentDecoration* Decor = Cast<ABaseSegmentDecoration>(Actor);
			// 	if (Decor != nullptr)
			// 	{
			// 		Decor->GetComponents<UStaticMeshComponent>(ThisComponents);
			// 	}
			// 	MeshComponents.Append(ThisComponents);
			// }
		}

		// Collect meshes from house elements
		for (auto& Element : AllCustoms)
		{
			TArray<UStaticMeshComponent*> ThisComponents;
			Element->GetComponents<UStaticMeshComponent>(ThisComponents);
			MeshComponents.Append(ThisComponents);
		}

		FVertexOffsetParams OffsetParams;
	
		for (auto& MeshComponent : MeshComponents)
		{
			USceneComponent* HousePoint = MeshComponent->GetAttachParent();
			while (HousePoint->GetOwner() != this)
			{
				HousePoint = HousePoint->GetAttachParent();
			}
			OffsetParams.MeshRotation = MeshComponent->GetComponentRotation() - GetActorRotation();
			OffsetParams.PivotOffset = GetActorRotation().UnrotateVector(MeshComponent->GetComponentLocation() - HousePoint->GetComponentLocation()) + HousePoint->GetRelativeLocation();
			if (UEDGEMeshUtility::ReadMeshDataAsRaw(MeshComponent, OffsetParams, RawSections))
			{
				// Add sections to All array
				for (auto& RawSection : RawSections)
				{
					AllRawSections.push_back(RawSection);
				}
			}
		}

		UEDGEMeshUtility::MergeSections(AllRawSections);
		UEDGEMeshUtility::WriteMeshDataToFile(FileName, AllRawSections);
		UEDGEMeshUtility::ConvertSectionDataToUnreal(AllRawSections, AllSections, AllMaterials);

		const FName Name = *FString::Printf(TEXT("%s"), *FileName);
		
		RMCProvider = NewObject<UEDGERuntimeMeshProvider>(this);
		RMCProvider->SetTemplateName(Name);
		RMCProvider->SetSectionsData(AllSections);
		RMCProvider->SetMaterials(AllMaterials);

		EDGERuntimeProviderManager::AddProvider(this, Name, RMCProvider);
	}

	GetHouseNavCollider()->SetRelativeLocation(GetRMCProvider()->GetBoxCenter());
	GetHouseNavCollider()->SetBoxExtent(GetRMCProvider()->GetBoxRadius());
	
	bMeshIsDirty = false;

}

// Floor = 0 means ground floor, where cornice offset never used
int AHouseEditor::GetRealHeight(int Floor) const
{
	bool corniceOffsetMP = (TemplateLocal.BottomCorniceClass != NULL && Floor > 0) ? 1 : 0;
	return (SegmentHeightInUnits * Floor) + (CorniceHeightOffset * corniceOffsetMP);
}

void AHouseEditor::CreateMergedMesh(FString MeshName)
{
	if (AllSegments.Num() > 0)
	{
		UWorld* World = GetWorld();
		FMeshMergingSettings MergingSettings = FMeshMergingSettings();
		MergingSettings.bPivotPointAtZero = true;					// We moving actor to Zero before merge. Look below.
		MergingSettings.bMergePhysicsData = true;
		const float ScreenSize = TNumericLimits<float>::Max();
		const FString PackageName = GetDefault<UEdgeHouseConstructorSettings>()->HouseMeshesDirectory.Path + "/HouseMesh_" + MeshName;

		TArray<UPrimitiveComponent*> ComponentsToMerge;

		TArray<AActor*> ActorsToMerge;
		ActorsToMerge.Append(AllSegments);
		for (AActor* Segment : AllSegments)
		{
			TArray<AActor*> Decorations;						//TODO: Can be made better. In case of some unexpected attachments.
			Segment->GetAttachedActors(Decorations);
			ActorsToMerge.Append(Decorations);
		}
		ActorsToMerge.Append(AllCustoms);
		ActorsToMerge.Add(RoofActor);
		for (AActor* Actor : ActorsToMerge)
		{
			TArray<UPrimitiveComponent*> ActorComps;
			Actor->GetComponents<UPrimitiveComponent>(ActorComps);
			ComponentsToMerge.Append(ActorComps);
		}

		TArray<UObject*> OutAssetsToSync;
		FVector OutMergedActorLocation;

		// When merging it's necessary to counter merge Rotation and Location. Using Zero transform
		// makes life easier and we have "normalized" merged mesh.
		const FTransform OldTransform = GetActorTransform();
		SetActorTransform(FTransform());
		
		const IMeshMergeUtilities& MergeModule = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();
		MergeModule.MergeComponentsToStaticMesh(ComponentsToMerge, World, MergingSettings,					// mb InOuter can be made from SavedHouseMesh ?
                                                nullptr, nullptr, PackageName,
                                                OutAssetsToSync, OutMergedActorLocation,
                                                ScreenSize, true);

		SetActorTransform(OldTransform);
		
		UStaticMesh* SavedHouseMesh;
		OutAssetsToSync.FindItemByClass(&SavedHouseMesh);
		
	
		const FString SavePath = FPaths::ProjectContentDir() + PackageName.RightChop(6) + ".uasset";
		
		UHouseEditorFunctionLibrary::CheckOutAndSave(SavedHouseMesh, SavePath);

		TemplateLocal.MergedMesh = SavedHouseMesh;
	}
	
	if (TemplateLocal.MergedMesh != nullptr)
	{
		ConstructMergedMeshComponent();
	}
	
}

void AHouseEditor::FillDecorationsWithDefaults(TArray<FSegmentDecorationsData>& Data)
{
	Data.Empty();
	FSegmentDecorationsData DecorationsData;
	DecorationsData.SocketName = "Wall F";
	Data.Add(DecorationsData);
	DecorationsData.SocketName = "Wall R";
	Data.Add(DecorationsData);
	DecorationsData.SocketName = "Wall B";
	Data.Add(DecorationsData);
	DecorationsData.SocketName = "Wall L";
	Data.Add(DecorationsData);
}
