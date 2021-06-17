// Fill out your copyright notice in the Description page of Project Settings.


#include "EDGEDistrictEditor/DistrictBuilder.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "PP_EDGE/EDGESettings.h"


ADistrictBuilderActor::ADistrictBuilderActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ItemClassTable = Cast<UDataTable>(GetDefault<UEDGESettings>()->DistrictItemClassesDataTable.ResolveObject());
	TileSize = 200.f;

	BaseRoot = CreateDefaultSubobject<USceneComponent>("BaseRoot");
	SetRootComponent(BaseRoot);
	BaseRoot->SetMobility(EComponentMobility::Static);
}

void ADistrictBuilderActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (DistrictDataAsset != nullptr)
	{
		DistrictData = DistrictDataAsset->ReadData();
	}
}

void ADistrictBuilderActor::BuildDistrict()
{
	if (ItemClassTable == nullptr)
	{
		return;
	}
	
	ClearDistrict();

	TMap<FName, AActor*> TiledActors;
	
	for (auto& TileData : DistrictData)
	{
		FDistrictItemClass* RowRef = ItemClassTable->FindRow<FDistrictItemClass>(TileData.ItemName, FString());
		if (RowRef == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("Couldn't find row <%s> in <%s> data table."), *TileData.ItemName.ToString(), *ItemClassTable->GetName());
			continue;
		}

		TSubclassOf<AActor> ItemClass;
		ItemClass = RowRef->ItemClass;
		AActor* NewActor;
		
		switch (RowRef->ItemType)
		{
			case DIT_Single:
			{
				FVector RelativeLocation = FVector(TileData.TileCoords.X * TileSize, TileData.TileCoords.Y * -TileSize, 0.f);
				NewActor = GetWorld()->SpawnActor<AActor>(ItemClass, GetActorLocation() + RelativeLocation, FRotator(0.f));
				NewActor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
				IDistrictItem::Execute_SetTileData(NewActor, TileData);
				AllActors.Add(NewActor);
				break;
			}
			case DIT_Tiled:
			{
				AActor** Ref = TiledActors.Find(TileData.ItemName);
				if (Ref == nullptr)
				{
					NewActor = GetWorld()->SpawnActor<AActor>(ItemClass, GetActorLocation(), FRotator(0.f));
					NewActor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
					TiledActors.Add(TileData.ItemName, NewActor);
					AllActors.Add(NewActor);
				}
				else
				{
					NewActor = *Ref;
				}
				IDistrictItem::Execute_SetTileData(NewActor, TileData);
				break;
			}
		}
	}

	for (auto& Item : AllActors)
	{
		IDistrictItem::Execute_BuildDistrictItem(Item);
	}
	
}

void ADistrictBuilderActor::ClearDistrict()
{
	while (AllActors.Num() > 0)
	{
		AllActors.Pop()->Destroy();
	}
}

void ADistrictBuilderActor::SetDistrictData(TArray<FDistrictTileData> NewDistrictData, bool bWriteToAsset)
{
	DistrictData = NewDistrictData;
	
	if (!bWriteToAsset)
	{
		DistrictDataAsset = nullptr;
	}
	else
	{
		if (DistrictDataAsset != nullptr)
		{
			DistrictDataAsset->WriteData(DistrictData);
		}
		else
		{
			FString ActualName = GetName();
			FString PackageName = GetDefault<UEDGESettings>()->DistrictDataAssetsDirectory.Path + "/" + GetName();
			FString SavePath = FPaths::ProjectContentDir() + PackageName.RightChop(6) + ".uasset";
			
			if (FPaths::FileExists(SavePath))
			{
				int32 Idx = FMath::Rand();
				UE_LOG(LogTemp, Warning, TEXT("Asset <%s> already exists. New one will be called <%s_%i>."), *PackageName, *PackageName, Idx);
				ActualName += "_" + FString::FromInt(Idx);
				PackageName = GetDefault<UEDGESettings>()->DistrictDataAssetsDirectory.Path + "/" + GetName();
				SavePath = FPaths::ProjectContentDir() + PackageName.RightChop(6) + ".uasset";
			}
			UPackage* Package = CreatePackage(*PackageName);

			DistrictDataAsset = NewObject<UDistrictDataAsset>(Package, UDistrictDataAsset::StaticClass(), FName(*ActualName), RF_Public);

			FAssetRegistryModule::AssetCreated(DistrictDataAsset);
			UPackage::Save(Package, DistrictDataAsset, RF_NoFlags, *SavePath, GError,0,false,true);
			
			DistrictDataAsset->WriteData(DistrictData);
		}
	}
}

// Called when the game starts or when spawned
void ADistrictBuilderActor::BeginPlay()
{
	Super::BeginPlay();

	if (DistrictData.Num() > 0)
	{
		BuildDistrict();
	}
}
