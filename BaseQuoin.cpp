// Fill out your copyright notice in the Description page of Project Settings.


#include "HouseEditor/BaseQuoin.h"

#include "Components/StaticMeshComponent.h"

// Sets default values
ABaseQuoin::ABaseQuoin()
{
	PrimaryActorTick.bCanEverTick = false;

}

void ABaseQuoin::OnConstruction(const FTransform& _transform) {
	Super::OnConstruction(_transform);

	while (allQuoins.Num() > 0) {
		allQuoins.Pop()->DestroyComponent();
	}

	UStaticMeshComponent* newComp;
	newComp = NewObject<UStaticMeshComponent>(this, UStaticMeshComponent::StaticClass(), FName("QuoinSegment_0"));
	newComp->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	newComp->SetupAttachment(RootComponent);
	newComp->SetRelativeLocation(FVector(0));
	newComp->RegisterComponent();
	newComp->SetStaticMesh(quoinMesh);
	allQuoins.Add(newComp);

	for (int i = 1; i < quoinHeight; i++) {
		FName compName = *FString::Printf(TEXT("QuoinSegment_%i"), i);
		newComp = NewObject<UStaticMeshComponent>(this, UStaticMeshComponent::StaticClass(), compName);
		newComp->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		newComp->SetupAttachment(RootComponent);
		newComp->SetRelativeLocation(FVector(0, 0, segmentHeightInUnits * i + corniceOffset));
		newComp->RegisterComponent();
		newComp->SetStaticMesh(quoinMesh);
		allQuoins.Add(newComp);
	}
}