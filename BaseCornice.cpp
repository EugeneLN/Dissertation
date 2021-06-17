// Fill out your copyright notice in the Description page of Project Settings.


#include "HouseEditor/BaseCornice.h"

#include "Components/StaticMeshComponent.h"

ABaseCornice::ABaseCornice()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ABaseCornice::OnConstruction(const FTransform& _transform) {
	Super::OnConstruction(_transform);

	while (allCornice.Num() > 0) {
		allCornice.Pop()->DestroyComponent();
	}

	FName compName;
	UStaticMeshComponent* newComp;
	for (int i = 1; i < corniceLength - 1; i++) {
		compName = *FString::Printf(TEXT("CorniceBase_%i"), i);
		newComp = NewObject<UStaticMeshComponent>(this, UStaticMeshComponent::StaticClass(), compName);
		newComp->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		newComp->SetupAttachment(RootComponent);
		newComp->SetRelativeLocation(FVector(segmentWidthInUnits * i, 0, 0));
		newComp->RegisterComponent();
		newComp->SetStaticMesh(corniceBase);
		allCornice.Add(newComp);
	}

	newComp = NewObject<UStaticMeshComponent>(this, UStaticMeshComponent::StaticClass(), FName("CorniceLeft"));
	newComp->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	newComp->SetupAttachment(RootComponent);
	newComp->SetRelativeLocation(FVector(0, 0, 0));
	newComp->RegisterComponent();
	newComp->SetStaticMesh(corniceLeft);
	allCornice.Add(newComp);

	newComp = NewObject<UStaticMeshComponent>(this, UStaticMeshComponent::StaticClass(), FName("CorniceRight"));
	newComp->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	newComp->SetupAttachment(RootComponent);
	newComp->SetRelativeLocation(FVector(segmentWidthInUnits * (corniceLength - 1), 0, 0));
	newComp->RegisterComponent();
	newComp->SetStaticMesh(corniceRight);
	allCornice.Add(newComp);
}