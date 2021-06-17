// Fill out your copyright notice in the Description page of Project Settings.


#include "HouseEditor/BaseRoof.h"

#include "Components/StaticMeshComponent.h"

ABaseRoof::ABaseRoof()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ABaseRoof::OnConstruction(const FTransform& Transform) {
	Super::OnConstruction(Transform);

	while (AllElements.Num() > 0)
	{
		AllElements.Pop()->DestroyComponent();
	}
	while (LineAnchors.Num() > 0)
	{
		LineAnchors.Pop()->DestroyComponent();
	}

	// Anchors created manually, while we using "box" houses.
	for (int Index = 0; Index < 4; Index++)
	{
		FVector CompRelLoc;
		switch (Index) {
		case 0:
			CompRelLoc = FVector(0);
			break;
		case 1:
			CompRelLoc = FVector(SegmentWidthInUnits * RoofLength, 0, 0);
			break;
		case 2:
			CompRelLoc = FVector(SegmentWidthInUnits * RoofLength, SegmentWidthInUnits * -RoofWidth, 0);
			break;
		case 3:
			CompRelLoc = FVector(0, SegmentWidthInUnits * -RoofWidth, 0);
			break;
		default:
			break;
		}
		
		FName CompName = *FString::Printf(TEXT("RoofAnchor_%i"), Index);
		USceneComponent* NewComp = NewObject<USceneComponent>(this, USceneComponent::StaticClass(), CompName);
		NewComp->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		NewComp->SetupAttachment(RootComponent);
		NewComp->SetRelativeLocationAndRotation(CompRelLoc, FRotator(0, Index * -90, 0));
		NewComp->RegisterComponent();
		LineAnchors.Add(NewComp);
	}

	// Outer elements
	for (int SideIndex = 0; SideIndex < RoofLines.Num(); SideIndex++)
	{
		int SideLenght = (SideIndex % 2 == 0) ? RoofLength : RoofWidth;		// Temp one, cause using "box" houses
		FName SMCompName = *FString::Printf(TEXT("Corner_%i"), SideIndex);
		UStaticMeshComponent* NewSMComp = NewObject<UStaticMeshComponent>(this, UStaticMeshComponent::StaticClass(), SMCompName);
		NewSMComp->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		NewSMComp->SetupAttachment(LineAnchors[SideIndex]);
		NewSMComp->RegisterComponent();
		NewSMComp->SetStaticMesh(GetMesh(SideIndex, -1));
		AllElements.Add(NewSMComp);
		
		for (int ElemIndex = 0; ElemIndex < SideLenght - 2; ElemIndex++)
		{
			SMCompName = *FString::Printf(TEXT("SideElement_%i_%i"), SideIndex, ElemIndex);
			NewSMComp = NewObject<UStaticMeshComponent>(this, UStaticMeshComponent::StaticClass(), SMCompName);
			NewSMComp->CreationMethod = EComponentCreationMethod::UserConstructionScript;
			NewSMComp->SetupAttachment(LineAnchors[SideIndex]);
			NewSMComp->SetRelativeLocation(FVector(SegmentWidthInUnits * (ElemIndex + 1), 0, 0));
			NewSMComp->RegisterComponent();
			NewSMComp->SetStaticMesh(GetMesh(SideIndex, ElemIndex));
			AllElements.Add(NewSMComp);
		}
	}

	// Inner elements (filler)
	for (int HIndex = 1; HIndex < RoofLength - 1; HIndex++)
	{
		for (int VIndex = 1; VIndex < RoofWidth -1; VIndex++)
		{
			FName FillerCompName = *FString::Printf(TEXT("FillerElement_%i_%i"), HIndex, VIndex);
			UStaticMeshComponent* FillerComp = NewObject<UStaticMeshComponent>(this, UStaticMeshComponent::StaticClass(), FillerCompName);
			FillerComp->CreationMethod = EComponentCreationMethod::UserConstructionScript;
			FillerComp->SetupAttachment(RootComponent);
			FillerComp->SetRelativeLocation(FVector(SegmentWidthInUnits * HIndex, SegmentWidthInUnits * -VIndex, FillerHeightOffset));
			FillerComp->RegisterComponent();
			FillerComp->SetStaticMesh(FillerMesh);
			AllElements.Add(FillerComp);
		}
	}

}

void ABaseRoof::SetMaterialByName(TEnumAsByte<EMaterialSlot> SlotName, UMaterialInterface* Material)
{
	if (Material == nullptr) {
		return;
	}

	FName SlotFName;
	switch (SlotName)
	{
	case Wall:
		SlotFName = FName("Wall");
		break;
	case Window:
		SlotFName = FName("Window");
		break;
	default:
		break;
	}

	TArray<UActorComponent*> Comps;
	GetComponents(Comps, true);
	for (int i = 0; i < Comps.Num(); i++) {
		UStaticMeshComponent* CurrComp = Cast<UStaticMeshComponent>(Comps[i]);
		if (CurrComp != NULL) {
			CurrComp->SetMaterialByName(SlotFName, Material);
		}
	}
}

UStaticMesh* ABaseRoof::GetMesh(int SideIndex, int ElemIndex)
{
	if (RoofLines.IsValidIndex(SideIndex) == false)
	{
		return nullptr;
	}
	
	// ElemIndex < 0 equals to Corner Mesh
	if (ElemIndex < 0)
	{
		if (RoofLines[SideIndex].CornerMesh == nullptr)
		{
			if (RoofLines.IsValidIndex(RoofLines[SideIndex].CopyLine) && RoofLines[SideIndex].CopyLine != SideIndex)
			{
				return GetMesh(RoofLines[SideIndex].CopyLine, ElemIndex);
			}
			return nullptr;
		}
		return RoofLines[SideIndex].CornerMesh;
	}

	// If LineMeshes is empty - bRepeatLine will be ignored
	if (RoofLines[SideIndex].LineMeshes.Num() == 0)
	{
		if (RoofLines.IsValidIndex(RoofLines[SideIndex].CopyLine) && RoofLines[SideIndex].CopyLine != SideIndex)
		{
			return GetMesh(RoofLines[SideIndex].CopyLine, ElemIndex);
		}
		return nullptr;
	}

	// Get new ElemIndex based on bRepeatLine
	ElemIndex = (RoofLines[SideIndex].bRepeatLine) ? (ElemIndex % RoofLines[SideIndex].LineMeshes.Num()) : FMath::Clamp(ElemIndex, 0, RoofLines[SideIndex].LineMeshes.Num() - 1);
	if (RoofLines[SideIndex].LineMeshes.IsValidIndex(ElemIndex) == false)
	{
		return nullptr;
	}

	// Zero-filled elements of LineMeshes will be taken from CopyLine, with bRepeatLine combined from both lines
	if (RoofLines[SideIndex].LineMeshes[ElemIndex] == nullptr)
	{
		if (RoofLines.IsValidIndex(RoofLines[SideIndex].CopyLine))
		{
			return GetMesh(RoofLines[SideIndex].CopyLine, ElemIndex);
		}
		return nullptr;
	}
	return RoofLines[SideIndex].LineMeshes[ElemIndex];
}
