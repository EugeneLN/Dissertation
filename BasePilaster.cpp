// Fill out your copyright notice in the Description page of Project Settings.


#include "HouseEditor/BasePilaster.h"

#include "Components/StaticMeshComponent.h"

// Sets default values
ABasePilaster::ABasePilaster()
{
	PrimaryActorTick.bCanEverTick = false;

}

void ABasePilaster::OnConstruction(const FTransform& _transform) {
	Super::OnConstruction(_transform);

	while (allPilasters.Num() > 0) {
		allPilasters.Pop()->DestroyComponent();
	}

	UStaticMeshComponent* newComp;
	newComp = NewObject<UStaticMeshComponent>(this, UStaticMeshComponent::StaticClass(), FName("PilasterSegment_0"));
	newComp->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	newComp->SetupAttachment(RootComponent);
	newComp->SetRelativeLocation(FVector(0));
	newComp->RegisterComponent();
	newComp->SetStaticMesh(pilasterMesh);
	allPilasters.Add(newComp);

	for (int i = 1; i < pilasterHeight; i++) {
		FName compName = *FString::Printf(TEXT("PilasterSegment_%i"), i);
		newComp = NewObject<UStaticMeshComponent>(this, UStaticMeshComponent::StaticClass(), compName);
		newComp->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		newComp->SetupAttachment(RootComponent);
		newComp->SetRelativeLocation(FVector(0, 0, segmentHeightInUnits * i + corniceOffset));
		newComp->RegisterComponent();
		newComp->SetStaticMesh(pilasterMesh);
		allPilasters.Add(newComp);
	}
}