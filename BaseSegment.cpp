// Fill out your copyright notice in the Description page of Project Settings.


#include "SegmentEditor/BaseSegment.h"
#include "SegmentEditor/SegmentsData.h"

ABaseSegment::ABaseSegment()
{
	PrimaryActorTick.bCanEverTick = false;
	
}

void ABaseSegment::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	TArray<USceneComponent*> AllComps;
	GetComponents<USceneComponent>(AllComps);
	for (auto& Comp : AllComps)
	{
		FName CompName = Comp->GetFName();
		if (GetDecorationSocketsNames().Contains(CompName))
		{
			AddDecorationSocket(CompName, Comp);
		}
		Comp->SetCanEverAffectNavigation(false);
	}
}

int ABaseSegment::GetSegmentSize() const
{
	return segmentSize;
}

TArray<FName> ABaseSegment::GetSegmentTags() const
{
	return segmentTags;
}

TArray<USceneComponent*> ABaseSegment::GetDecorationSockets() const
{
	TArray<USceneComponent*> SocketComponents;
	for (FDecorationSocketData SocketData : DecorationSockets)
	{
		SocketComponents.Add(SocketData.SocketComponent);
	}
	return SocketComponents;
}

TArray<FName> ABaseSegment::GetDecorationSocketsNames() const
{
	TArray<FName> SocketNames;
	for (FDecorationSocketData SocketData : DecorationSockets)
	{
		SocketNames.Add(SocketData.SocketName);
	}
	return SocketNames;
}

// Returns "None" if tag wasn't found at specified index.
FName ABaseSegment::GetSegmentTag(int Index) const
{
	if (segmentTags.IsValidIndex(Index)) {
		return segmentTags[Index];
	}
	else {
		return FName();
	}
}

bool ABaseSegment::HasSegmentTag(FName Tag)
{
	for (int i = 0; i < segmentTags.Num(); i++ )
	{
		if (segmentTags[i] == Tag)
			return true;
	}
	return false;
}

void ABaseSegment::SetMaterialByName(TEnumAsByte<EMaterialSlot> SlotName, UMaterialInterface* Material)
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

void ABaseSegment::AddDecorationSocket(FName SocketName, USceneComponent* SocketComponent)
{
	for (FDecorationSocketData& SocketData : DecorationSockets)
	{
		if (SocketData.SocketName == SocketName)
		{
			SocketData.SocketComponent = SocketComponent;
			return;
		}
	}
	
	FDecorationSocketData NewSocketData;
	NewSocketData.SocketName = SocketName;
	NewSocketData.SocketComponent = SocketComponent;
	DecorationSockets.Add(NewSocketData);
}
