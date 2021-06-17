// Fill out your copyright notice in the Description page of Project Settings.


#include "HouseEditor/BaseFireLadder.h"

#include "Components/StaticMeshComponent.h"

ABaseFireLadder::ABaseFireLadder()
{
	PrimaryActorTick.bCanEverTick = false;

}

void ABaseFireLadder::OnConstruction(const FTransform& _transform) {
	Super::OnConstruction(_transform);

	while (allStairs.Num() > 0) {
		allStairs.Pop()->DestroyComponent();
	}

	// UInstancedStaticMeshComponent ??? for the later

	UStaticMeshComponent* ladderBase = NewObject<UStaticMeshComponent>(this, UStaticMeshComponent::StaticClass(), FName("LadderBase"));
	ladderBase->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	ladderBase->SetupAttachment(RootComponent);
	ladderBase->SetRelativeLocation(FVector(0, 0, 0));
	ladderBase->RegisterComponent();
	ladderBase->SetStaticMesh(baseMesh);
	allStairs.Add(ladderBase);

	FName compName;
	UStaticMeshComponent* newComp;
	for (int i = 1; i < ladderHeight; i++) {
		compName = *FString::Printf(TEXT("Stairs_%i"), i);
		newComp = NewObject<UStaticMeshComponent>(this, UStaticMeshComponent::StaticClass(), compName);
		newComp->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		newComp->SetupAttachment(ladderBase);
		newComp->SetRelativeLocation(FVector(0, 0, segmentHeightInUnits * (i - 1)));
		newComp->RegisterComponent();
		newComp->SetStaticMesh(stairsMesh);
		allStairs.Add(newComp);

		compName = *FString::Printf(TEXT("Floor_%i"), i);
		newComp = NewObject<UStaticMeshComponent>(this, UStaticMeshComponent::StaticClass(), compName);
		newComp->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		newComp->SetupAttachment(ladderBase);
		newComp->SetRelativeLocation(FVector(0, 0, segmentHeightInUnits * i));
		newComp->RegisterComponent();
		newComp->SetStaticMesh(floorMesh);
		allStairs.Add(newComp);
	}
}