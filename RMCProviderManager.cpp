// Fill out your copyright notice in the Description page of Project Settings.


#include "RuntimeMesh/RMCProviderManager.h"
#include "RuntimeMesh/EDGERuntimeMeshProvider.h"

TMap<FName, UEDGERuntimeMeshProvider*> EDGERuntimeProviderManager::Providers = TMap<FName, UEDGERuntimeMeshProvider*>();

bool EDGERuntimeProviderManager::GetProvider(UObject* Context, const FString& FileName, UEDGERuntimeMeshProvider*& OutProvider)
{
	const FName Name = *FString::Printf(TEXT("%s"), *FileName);
	UEDGERuntimeMeshProvider** Ref = Providers.Find(Name);
	UEDGERuntimeMeshProvider* Item = nullptr;
	bool bCreateNew = true;
	if (Ref != nullptr)
	{
		Item = *Ref;
		if (Item->IsValidLowLevel())
		{
			if (Item->GetTemplateName() == Name)
			{
				bCreateNew = false;
			}
		}
	}
	
	if (bCreateNew)
	{
		TArray<FRMCSectionData> AllSections;
		TArray<UMaterialInterface*> AllMaterials;
		// Try to read file
		if (UEDGEMeshUtility::ReadMeshDataFromFile(FileName, AllSections, AllMaterials))
		{
			UE_LOG(LogTemp, Display, TEXT("~~ Provider <%s> found in file."), *FileName);
			OutProvider->SetTemplateName(Name);
			OutProvider->SetSectionsData(AllSections);
			OutProvider->SetMaterials(AllMaterials);
			AddProvider(Context, Name, OutProvider);
			return true;
		}
		UE_LOG(LogTemp, Display, TEXT("~~ Provider <%s> not found."), *FileName);
		return false;
	}

	UE_LOG(LogTemp, Display, TEXT("~~ Provider <%s> found in memory."), *FileName);
	OutProvider->SetTemplateName(Item->GetTemplateName());
	OutProvider->SetSectionsData(Item->GetSectionData());
	OutProvider->SetMaterials(Item->GetMaterials());
	return true;
}

void EDGERuntimeProviderManager::AddProvider(UObject* Context, const FName Name, UEDGERuntimeMeshProvider* Provider)
{
	UEDGERuntimeMeshProvider* NewProvider = NewObject<UEDGERuntimeMeshProvider>(Context->GetWorld());
	NewProvider->SetTemplateName(Provider->GetTemplateName());
	NewProvider->SetSectionsData(Provider->GetSectionData());
	NewProvider->SetMaterials(Provider->GetMaterials());
	Providers.Add(Name, NewProvider);
}

void EDGERuntimeProviderManager::ResetManager()
{
	Providers.Empty();
}
