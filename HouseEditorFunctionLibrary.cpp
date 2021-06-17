// Fill out your copyright notice in the Description page of Project Settings.


#include "HouseEditor/HouseEditorFunctionLibrary.h"

#include "ImageUtils.h"
#include "ObjectTools.h"
#include "SourceControlHelpers.h"
#include "Components/Image.h"

#include "EdgeHouseConstructor/EdgeHouseConstructorSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "RuntimeMesh/EDGEMeshUtility.h"
#include "RuntimeMesh/RMCProviderManager.h"

#include "SegmentEditor/BaseSegment.h"
#include "SegmentEditor/BaseSegmentDecoration.h"
#include "ThumbnailRendering/ThumbnailManager.h"


UDataTable* UHouseEditorFunctionLibrary::GetPatternDataTable()
{
	UDataTable* PatternDataTable = GetDataTableByType(PatternData);
	if (PatternDataTable == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Pattern Data Table was not found. Please, check Edge House Constructor plugin settings."));
		return NULL;
	}
	if (PatternDataTable->GetRowStruct() != FPatternData::StaticStruct())
	{
		UE_LOG(LogTemp, Error, TEXT("Pattern Data Table has wrong row struct, must be <PatternData>. Please, check Edge House Constructor plugin settings."));
		return NULL;
	}
	return PatternDataTable;
}

UDataTable* UHouseEditorFunctionLibrary::GetSegmentDataTable()
{
	UDataTable* SegmentDataTable = GetDataTableByType(SegmentData);
	if (SegmentDataTable == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Segment Data Table was not found. Please, check Edge House Constructor plugin settings."));
		return NULL;
	}
	if (SegmentDataTable->GetRowStruct() != FSegmentClassesData::StaticStruct())
	{
		UE_LOG(LogTemp, Error, TEXT("Segment Data Table has wrong row struct, must be <SegmentClassesData>. Please, check Edge House Constructor plugin settings."));
		return NULL;
	}
	return SegmentDataTable;
}

UDataTable* UHouseEditorFunctionLibrary::GetSegmentDecorationDataTable()
{
	UDataTable* SegmentDecorationDataTable = GetDataTableByType(SegmentDecorationData);
	if (SegmentDecorationDataTable == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Segment Decoration Data Table was not found. Please, check Edge House Constructor plugin settings."));
		return NULL;
	}
	if (SegmentDecorationDataTable->GetRowStruct() != FSegmentDecorationClassesData::StaticStruct())
	{
		UE_LOG(LogTemp, Error, TEXT("Segment Decoration Data Table has wrong row struct, must be <SegmentDecorationClassesData>. Please, check Edge House Constructor plugin settings."));
		return NULL;
	}

	return SegmentDecorationDataTable;
}

UDataTable* UHouseEditorFunctionLibrary::GetHouseParamsTemplateDataTable()
{
	UDataTable* HouseParamsTemplateDataTable = GetDataTableByType(HouseTemplate);
	if (HouseParamsTemplateDataTable == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("House Params Template Data Table was not found. Please, check Edge House Constructor plugin settings."));
		return NULL;
	}
	if (HouseParamsTemplateDataTable->GetRowStruct() != FHouseParamsTemplate::StaticStruct())
	{
		UE_LOG(LogTemp, Error, TEXT("House Params Template Data Table has wrong row struct, must be <HouseParamsTemplate>. Please, check Edge House Constructor plugin settings."));
		return NULL;
	}

	return HouseParamsTemplateDataTable;
}

TMap<FName, float> UHouseEditorFunctionLibrary::GetNormalizedDecorationWeights(FSegmentData SegmentData, FName SocketName, FSegmentDecorationsData GlobalWeights)
{
	// Create new weights map from "SocketName" and "Any" (None) maps. Same key values are added.
	TMap<FName, float> WeightsMap;
	for (FSegmentDecorationsData SegmentDecorationsData : SegmentData.SegmentDecorationWeights)
	{
		if (SegmentDecorationsData.SocketName == SocketName || SegmentDecorationsData.SocketName == NAME_None)
		{
			TArray<FName> Keys;
			SegmentDecorationsData.SocketDecorationWeights.GenerateKeyArray(Keys);
			for (FName Key : Keys)
			{
				if (WeightsMap.Contains(Key))
				{
					WeightsMap.Add(Key, WeightsMap.FindRef(Key) + SegmentDecorationsData.SocketDecorationWeights.FindRef(Key));
				}
				else
				{
					WeightsMap.Add(Key, SegmentDecorationsData.SocketDecorationWeights.FindRef(Key));
				}
			}
		}
	}

	// If not ignoring Global Weights - adding them to WeightsMap
	if (SegmentData.bIgnoreGlobalDecorations == false)
	{
		TArray<FName> Keys;
		GlobalWeights.SocketDecorationWeights.GenerateKeyArray(Keys);
		for (FName Key : Keys)
		{
			if (WeightsMap.Contains(Key))
			{
				WeightsMap.Add(Key, WeightsMap.FindRef(Key) + GlobalWeights.SocketDecorationWeights.FindRef(Key));
			}
			else
			{
				WeightsMap.Add(Key, GlobalWeights.SocketDecorationWeights.FindRef(Key));
			}
		}
	}

	// Calculating sum of all weights, so we can normalize it later
	TArray<float> Values;
	WeightsMap.GenerateValueArray(Values);
	float TotalValue = 0.f;
	for (float Value : Values)
	{
		TotalValue += Value;
	}

	// Normalize values if needed
	if (TotalValue > 1.f)
	{
		TArray<FName> Keys;
		WeightsMap.GenerateKeyArray(Keys);
		for (FName Key : Keys)
		{
			WeightsMap.Add(Key, WeightsMap.FindRef(Key) / TotalValue);
		}
	}

	return WeightsMap;
}

TSubclassOf<ABaseSegment> UHouseEditorFunctionLibrary::GetSegmentClassByFName(FName ClassName)
{
	UDataTable* SegmentDataTable = GetSegmentDataTable();
	if (SegmentDataTable->IsValidLowLevel() == false || SegmentDataTable->GetRowStruct() != FSegmentClassesData::StaticStruct())
	{
		UE_LOG(LogTemp, Warning, TEXT("Data table is not valid or has wrong RowStrut"));
		return NULL;
	}

	FSegmentClassesData* DataRow = SegmentDataTable->FindRow<FSegmentClassesData>(ClassName, FString());
	if (DataRow == nullptr)
	{
		return NULL;
	}
	return DataRow->SegmentClass;
}

int UHouseEditorFunctionLibrary::GetSegmentSizeByFName(FName ClassName)
{
	TSubclassOf<ABaseSegment> SegmentClass = GetSegmentClassByFName(ClassName);
	if (SegmentClass == NULL)
	{
		return 0;
	}

	return SegmentClass.GetDefaultObject()->GetSegmentSize();;
}

TSubclassOf<ABaseSegmentDecoration> UHouseEditorFunctionLibrary::GetSegmentDecorationClassByFName(FName DecorationClassName)
{
	UDataTable* SegmentDecorationDataTable = GetSegmentDecorationDataTable();
	if (SegmentDecorationDataTable->IsValidLowLevel() == false) {
		return NULL;
	}

	FSegmentDecorationClassesData* DataRow = SegmentDecorationDataTable->FindRow<FSegmentDecorationClassesData>(DecorationClassName, FString());
	if (DataRow == nullptr)
	{
		return NULL;
	}
	return DataRow->SegmentDecorationClass;
}

void UHouseEditorFunctionLibrary::FindSegmentDecorationDataBySocket(FSegmentDecorationsData& OutDecorationData, FSegmentData& InSegmentData, FName SocketName)
{
	for (FSegmentDecorationsData& DecorationData : InSegmentData.SegmentDecorationWeights)
	{
		if (DecorationData.SocketName == SocketName)
		{
			OutDecorationData = DecorationData;
			return;
		}
	}
}

// Generate and save texture based on thumbnail of selected object (if it can have any)
UTexture2D* UHouseEditorFunctionLibrary::MakeTextureBasedOnThumbnail(UObject* Object)
{
	if (Object == NULL)
	{
		//UE_LOG(LogTemp, Display, TEXT("RealObject = NULL"));
		return NULL;
	}
	FObjectThumbnail* Thumbnail = ThumbnailTools::GenerateThumbnailForObjectToSaveToDisk(Object);

	if (Thumbnail == NULL)
	{
		UE_LOG(LogTemp, Display, TEXT("Object can't have thumbnail"));
		return NULL;
	}
	if (Thumbnail->IsEmpty())
	{
		//UE_LOG(LogTemp, Display, TEXT("Thumbnail == Empty"));
		return NULL;
	}

	TArray<uint8> ByteArray = Thumbnail->GetUncompressedImageData();
	FString Text = "";
	TArray<FColor> ColorArray;
	FColor Color;

	for (int i = 0; i < ByteArray.Num(); i++)
	{
		int K = ByteArray[i];
		Text.Append(" ").Append(FString::FromInt(K));
		if (i % 4 == 0)
		{
			Color.B = K;
		}
		if (i % 4 == 1)
		{
			Color.G = K;
		}
		if (i % 4 == 2)
		{
			Color.R = K;
		}
		if (i % 4 == 3)
		{
			Color.A = K;
			//UE_LOG(LogTemp, Display, TEXT("%i, %s --- %s"), i, *Text, *Color.ToString());
			Text.Empty();
			ColorArray.Add(Color);
		}
	}

	const int32 SrcWidth = Thumbnail->GetImageWidth();
	const int32 SrcHeight = Thumbnail->GetImageHeight();
	const FString TextureName = "ThumbnailTexture_" + Object->GetName();
	
	FCreateTexture2DParameters Params;
	Params.MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;

	// really cant get why this is working like that... others with seemingly same code dont need .uasset extension in names
	const FString SavePath = FPaths::ProjectContentDir() + GetDefault<UEdgeHouseConstructorSettings>()->SegmentImagesDirectory.Path.RightChop(6) + "/" + TextureName + ".uasset";
	const FString PackageName = GetDefault<UEdgeHouseConstructorSettings>()->SegmentImagesDirectory.Path + "/" + TextureName;
	UPackage* TexturePackage = CreatePackage(*PackageName);
	check(TexturePackage);
	TexturePackage->FullyLoad();
	TexturePackage->Modify();
	
	UTexture2D* Texture = FImageUtils::CreateTexture2D(SrcWidth, SrcHeight, ColorArray, TexturePackage, TextureName, EObjectFlags::RF_Public, Params);
	
	CheckOutAndSave(Texture, SavePath, TexturePackage);
	
	return Texture;
}

void UHouseEditorFunctionLibrary::ResetProviderManager()
{
	EDGERuntimeProviderManager::ResetManager();
}

void UHouseEditorFunctionLibrary::ClearAllMeshDataFiles()
{
	UEDGEMeshUtility::ClearMeshDataFiles();
}


UDataTable* UHouseEditorFunctionLibrary::GetDataTableByType(EDataTableType Type)
{
	FSoftObjectPath DataTablePath;
	switch (Type)
	{
		case PatternData:
			DataTablePath = GetDefault<UEdgeHouseConstructorSettings>()->PatternDataTable;
			break;
		case SegmentData:
			DataTablePath = GetDefault<UEdgeHouseConstructorSettings>()->SegmentClassesDataTable;
			break;
		case SegmentDecorationData:
			DataTablePath = GetDefault<UEdgeHouseConstructorSettings>()->SegmentDecorationClassesDataTable;
			break;
		case HouseTemplate:
			DataTablePath = GetDefault<UEdgeHouseConstructorSettings>()->HouseParamsTemplateDataTable;
	}
	if (DataTablePath.IsValid())
	{
		UDataTable* DataTable = Cast<UDataTable>(DataTablePath.ResolveObject());
		if (DataTable != nullptr)
		{
			return DataTable;
		}
	}

	return nullptr;
}

void UHouseEditorFunctionLibrary::CheckOutAndSave(UObject* Asset, FString SavePath, UPackage* Package)
{
	if (SavePath.IsEmpty())
	{
		SavePath = UKismetSystemLibrary::GetSystemPath(Asset);
		if (SavePath.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("[EdgeHouseConstructor] Can't create auto-path. Aborting save..."));
			return;
		}
	}
	
	if (Package == nullptr)
	{
		Package = Asset->GetPackage();
	}
	Package->MarkPackageDirty();

	bool bMarkToAddLater = false;
	
	if (USourceControlHelpers::IsAvailable())
	{
		if (FPaths::FileExists(SavePath))
		{
			USourceControlHelpers::CheckOutOrAddFile(SavePath);
		}
		else
		{
			bMarkToAddLater = true;
		}
	}
	
	// Not sure if this SAVE_NoError flag is a good idea, but it doesn't allow engine to crush if file can't be saved (f.e. if it's ReadOnly)
	// But we only will see errors in logs in that case...
	UPackage::Save(Package, Asset, EObjectFlags::RF_NoFlags, *SavePath, GError,0,false,true,SAVE_NoError);
	
	if (bMarkToAddLater)
	{
		USourceControlHelpers::MarkFileForAdd(SavePath);
	}
}