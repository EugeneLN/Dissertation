// Fill out your copyright notice in the Description page of Project Settings.


#include "EDGECityEditor/CityBuilder.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "PP_EDGE/EDGESettings.h"


ACityBuilderActor::ACityBuilderActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ItemClassTable = Cast<UDataTable>(GetDefault<UEDGESettings>()->CityItemClassesDataTable.ResolveObject());
	TileSize = 200.f;

	BaseRoot = CreateDefaultSubobject<USceneComponent>("BaseRoot");
	SetRootComponent(BaseRoot);
	BaseRoot->SetMobility(EComponentMobility::Static);
}

void ACityBuilderActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (CityDataAsset != nullptr)
	{
		CityData = CityDataAsset->ReadData();
	}
}

void ACityBuilderActor::BuildCity()
{
	if (ItemClassTable == nullptr)
	{
		return;
	}
	
	ClearCity();

	TMap<FName, AActor*> TiledActors;
	
	for (auto& TileData : CityData)
	{
		FCityItemClass* RowRef = ItemClassTable->FindRow<FCityItemClass>(TileData.ItemName, FString());
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
				ICityItem::Execute_SetTileData(NewActor, TileData);
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
				ICityItem::Execute_SetTileData(NewActor, TileData);
				break;
			}
		}
	}

	for (auto& Item : AllActors)
	{
		ICityItem::Execute_BuildCityItem(Item);
	}
	
}

void ACityBuilderActor::ClearCity()
{
	while (AllActors.Num() > 0)
	{
		AllActors.Pop()->Destroy();
	}
}

void ACityBuilderActor::SetCityData(TArray<FCityTileData> NewCityData, bool bWriteToAsset)
{
	CityData = NewCityData;
	
	if (!bWriteToAsset)
	{
		CityDataAsset = nullptr;
	}
	else
	{
		if (CityDataAsset != nullptr)
		{
			CityDataAsset->WriteData(CityData);
		}
		else
		{
			FString ActualName = GetName();
			FString PackageName = GetDefault<UEDGESettings>()->CityDataAssetsDirectory.Path + "/" + GetName();
			FString SavePath = FPaths::ProjectContentDir() + PackageName.RightChop(6) + ".uasset";
			
			if (FPaths::FileExists(SavePath))
			{
				int32 Idx = FMath::Rand();
				UE_LOG(LogTemp, Warning, TEXT("Asset <%s> already exists. New one will be called <%s_%i>."), *PackageName, *PackageName, Idx);
				ActualName += "_" + FString::FromInt(Idx);
				PackageName = GetDefault<UEDGESettings>()->CityDataAssetsDirectory.Path + "/" + GetName();
				SavePath = FPaths::ProjectContentDir() + PackageName.RightChop(6) + ".uasset";
			}
			UPackage* Package = CreatePackage(*PackageName);

			CityDataAsset = NewObject<UCityDataAsset>(Package, UCityDataAsset::StaticClass(), FName(*ActualName), RF_Public);

			FAssetRegistryModule::AssetCreated(CityDataAsset);
			UPackage::Save(Package, CityDataAsset, RF_NoFlags, *SavePath, GError,0,false,true);
			
			CityDataAsset->WriteData(CityData);
		}
	}
}

// Called when the game starts or when spawned
void ACityBuilderActor::BeginPlay()
{
	Super::BeginPlay();

	if (CityData.Num() > 0)
	{
		BuildCity();
	}
}
