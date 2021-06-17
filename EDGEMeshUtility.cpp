// Fill out your copyright notice in the Description page of Project Settings.


#include "RuntimeMesh/EDGEMeshUtility.h"

#include "EdgeHouseConstructor/EdgeHouseConstructorSettings.h"
#include "HouseEditor/HouseEditorFunctionLibrary.h"

bool UEDGEMeshUtility::ReadMeshDataAsRaw(const UStaticMeshComponent* MeshComp, const FVertexOffsetParams& OffsetParams,  vector<EDGEMeshSectionData>& OutRawData)
{

	const auto& Mesh = MeshComp->GetStaticMesh();
	// This will throw assert?
	check(Mesh != nullptr);

	UDataTable* MaterialsTable = Cast<UDataTable>(GetDefault<UEdgeHouseConstructorSettings>()->MaterialsDataTable.ResolveObject());
	TArray<FMaterialsTableRow*> MatRows;
	if (MaterialsTable == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cant read materials data table for RMC building. Materials wont be set."));
	}
	else
	{
		MaterialsTable->GetAllRows<FMaterialsTableRow>(FString(), MatRows);
	}

	bool bDataTableWasUpdated = false;
	
	OutRawData.clear();
	
	//const int LODCount = Mesh->RenderData->LODResources.Num();			// TODO: Don't forget to implement LODs
	//for (int LODIdx = 0; LODIdx < LODCount; LODIdx++)
	const int LODIdx = 0;
	{
		auto& LOD = Mesh->RenderData->LODResources[LODIdx];
		for (int SectionIdx = 0; SectionIdx < LOD.Sections.Num(); SectionIdx++)
		{
			auto& Section = LOD.Sections[SectionIdx];
			OutRawData.push_back(EDGEMeshSectionData());

			// Base section info
			OutRawData[SectionIdx].SectionData.push_back(Section.MinVertexIndex);
			OutRawData[SectionIdx].SectionData.push_back(Section.MaxVertexIndex);
			OutRawData[SectionIdx].SectionData.push_back(Section.FirstIndex);
			OutRawData[SectionIdx].SectionData.push_back(Section.NumTriangles);

			// Finding material name
			OutRawData[SectionIdx].MaterialName = "NONE";
			bool bMatWasFound = false;
			if (MaterialsTable != nullptr)
			{
				const auto& MatInterface = MeshComp->GetMaterial(Section.MaterialIndex);
				for (auto& RowRef : MatRows)
				{
					if (RowRef->Material == MatInterface)
					{
						OutRawData[SectionIdx].MaterialName = string(TCHAR_TO_UTF8(*RowRef->MatName));
						bMatWasFound = true;
					}
				}
				if (!bMatWasFound)
				{
					FMaterialsTableRow NewRow;
					NewRow.Material = MatInterface;
					NewRow.MatName = MatInterface->GetName();
					const FName NewRowName = *FString::Printf(TEXT("%s"), *NewRow.MatName);
					MaterialsTable->AddRow(NewRowName, NewRow);
					OutRawData[SectionIdx].MaterialName = string(TCHAR_TO_UTF8(*NewRow.MatName));
					bDataTableWasUpdated = true;
				}
			}
			
			// Vertices
			for (uint32 VertIdx = Section.MinVertexIndex; VertIdx <= Section.MaxVertexIndex; VertIdx++)
			{
				FVector Vector;
				
				// Positions
				Vector = LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(VertIdx);
				Vector = OffsetParams.MeshRotation.RotateVector(Vector);
				Vector += OffsetParams.PivotOffset;
				OutRawData[SectionIdx].Vertices.push_back(Vector.X);
				OutRawData[SectionIdx].Vertices.push_back(Vector.Y);
				OutRawData[SectionIdx].Vertices.push_back(Vector.Z);

				// Normals
				Vector = LOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(VertIdx);
				Vector = OffsetParams.MeshRotation.RotateVector(Vector);
				OutRawData[SectionIdx].Normals.push_back(Vector.X);
				OutRawData[SectionIdx].Normals.push_back(Vector.Y);
				OutRawData[SectionIdx].Normals.push_back(Vector.Z);

				// Tangents
				Vector = LOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentX(VertIdx);
				Vector = OffsetParams.MeshRotation.RotateVector(Vector);
				OutRawData[SectionIdx].Tangents.push_back(Vector.X);
				OutRawData[SectionIdx].Tangents.push_back(Vector.Y);
				OutRawData[SectionIdx].Tangents.push_back(Vector.Z);

				// UVs
				OutRawData[SectionIdx].UVs.push_back(LOD.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(VertIdx, 0).X);
				OutRawData[SectionIdx].UVs.push_back(LOD.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(VertIdx, 0).Y);
			}

			// Faces indices
			for (uint32 Idx = Section.FirstIndex; Idx < Section.FirstIndex + Section.NumTriangles * 3; Idx++)
			{
				OutRawData[SectionIdx].Indices.push_back(LOD.IndexBuffer.GetIndex(Idx) - Section.MinVertexIndex);
			}
		} // End Sections generating

	}

	if (bDataTableWasUpdated)
	{
		UHouseEditorFunctionLibrary::CheckOutAndSave(MaterialsTable);
	}

	
	return true;
}

void UEDGEMeshUtility::MergeSections(vector<EDGEMeshSectionData>& RawData)
{
	TArray<int> SectionsThatMoved;
	TArray<string> MatNames;
	TArray<int> RemappedIndices;

	if (RawData.size() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Attempt to merge mesh data without data (array of size 0). Process canceled."))
		return;
	}
	
	for (int SectionIdx = 0; SectionIdx < RawData.size(); SectionIdx++)
	{
		int SectionIdxWithMat = 0;MatNames.Find(RawData[SectionIdx].MaterialName);
		if (MatNames.Find(RawData[SectionIdx].MaterialName, SectionIdxWithMat))
		{
			auto& BaseSection = RawData[RemappedIndices[SectionIdxWithMat]];
			auto& MovedSection = RawData[SectionIdx];
			
			SectionsThatMoved.Insert(SectionIdx, 0);
			for (auto& Ind : MovedSection.Indices)
			{
				Ind += BaseSection.Vertices.size() / 3;
			}
			
			BaseSection.Vertices.insert(end(BaseSection.Vertices), begin(MovedSection.Vertices), end(MovedSection.Vertices));
			BaseSection.Normals.insert(end(BaseSection.Normals), begin(MovedSection.Normals), end(MovedSection.Normals));
			BaseSection.Tangents.insert(end(BaseSection.Tangents), begin(MovedSection.Tangents), end(MovedSection.Tangents));
			BaseSection.UVs.insert(end(BaseSection.UVs), begin(MovedSection.UVs), end(MovedSection.UVs));
			BaseSection.Indices.insert(end(BaseSection.Indices), begin(MovedSection.Indices), end(MovedSection.Indices));
		}
		else
		{
			MatNames.Add(RawData[SectionIdx].MaterialName);
			RemappedIndices.Add(SectionIdx);
		}
	}

	// Remove moved sections
	for (const auto& Num : SectionsThatMoved)
	{
		RawData.erase(RawData.begin() + Num);
	}

	// Recalculate sections data after merging
	RawData[0].SectionData[0] = 0;										// MinVertIndex
	RawData[0].SectionData[1] = RawData[0].Vertices.size() / 3 - 1;		// MaxVertIndex
	RawData[0].SectionData[2] = 0;										// FirstTriIndex
	RawData[0].SectionData[3] = RawData[0].Indices.size() / 3;			// NumTriangles
	
	for (int SectionIdx = 1; SectionIdx < RawData.size(); SectionIdx++)
	{
		RawData[SectionIdx].SectionData[0] = RawData[SectionIdx - 1].SectionData[1] + 1;
		RawData[SectionIdx].SectionData[1] = RawData[SectionIdx].SectionData[0] + RawData[SectionIdx].Vertices.size() / 3 - 1;
		RawData[SectionIdx].SectionData[2] = RawData[SectionIdx - 1].SectionData[2] + RawData[SectionIdx - 1].SectionData[3] * 3;
		RawData[SectionIdx].SectionData[3] = RawData[SectionIdx].Indices.size() / 3;
	}
}

bool UEDGEMeshUtility::WriteMeshDataToFile(const FString& FileName, const vector<EDGEMeshSectionData>& RawData)
{
	FString UnrealFullFileName = FPlatformProcess::UserTempDir() + FString("EDGE/SavedMeshData/") + FileName + ".txt";
	UE_LOG(LogTemp, Display, TEXT("~~ Write FileName: %s"), *UnrealFullFileName);
	
	string FullFileName = string(TCHAR_TO_UTF8(*UnrealFullFileName));

	string ErrorString = string();
	if (EDGEMeshDataProvider::WriteToFile(FullFileName, RawData, ErrorString))
	{
		return true;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *FString(ErrorString.c_str()));
		return false;
	}
}

bool UEDGEMeshUtility::ReadMeshDataFromFile(const FString& FileName, TArray<FRMCSectionData>& OutUnrealData, TArray<UMaterialInterface*>& Materials)
{
	FString UnrealFullFileName = FPlatformProcess::UserTempDir() + FString("EDGE/SavedMeshData/") + FileName + ".txt";
	UE_LOG(LogTemp, Display, TEXT("~~ Read FileName: %s"), *UnrealFullFileName);
	
	string FullFileName = string(TCHAR_TO_UTF8(*UnrealFullFileName));
	vector<EDGEMeshSectionData> RawData;

	string ErrorString = string();
	if (EDGEMeshDataProvider::ReadFromFile(FullFileName, RawData, ErrorString))
	{
		ConvertSectionDataToUnreal(RawData, OutUnrealData, Materials);
		return true;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *FString(ErrorString.c_str()));
		return false;
	}
}

bool UEDGEMeshUtility::RemoveFile(const FString& FileName)
{
	FString UnrealFullFileName = FPlatformProcess::UserTempDir() + FString("EDGE/SavedMeshData/") + FileName + ".txt";
	UE_LOG(LogTemp, Display, TEXT("~~ Delete FileName: %s"), *UnrealFullFileName);
	
	string FullFileName = string(TCHAR_TO_UTF8(*UnrealFullFileName));
	string ErrorString = string();
	
	if (EDGEMeshDataProvider::RemoveFile(FullFileName, ErrorString))
	{
		return true;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *FString(ErrorString.c_str()));
		return false;
	}
}

void UEDGEMeshUtility::ClearMeshDataFiles()
{
	FString UnrealDir = FPlatformProcess::UserTempDir() + FString("EDGE/SavedMeshData");
	UE_LOG(LogTemp, Display, TEXT("~~ Clear Dir: %s"), *UnrealDir);

	IPlatformFile& FileManager = FPlatformFileManager::Get().GetPlatformFile();

	// Delete folder with all its content
	if(FileManager.DeleteDirectoryRecursively(*UnrealDir))
	{
		UE_LOG(LogTemp, Warning, TEXT("FilePaths: Directory and sub-files and folders are Deleted"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("FilePaths: Directory and sub-files and folders are not Deleted"));
	}
}


void UEDGEMeshUtility::ConvertSectionDataToRaw(const TArray<FRMCSectionData>& UnrealData, const TArray<UMaterialInterface*>& Materials, vector<EDGEMeshSectionData>& OutRawData)
{
	UDataTable* MaterialsTable = Cast<UDataTable>(GetDefault<UEdgeHouseConstructorSettings>()->MaterialsDataTable.ResolveObject());
	TArray<FMaterialsTableRow*> MatRows;
	if (MaterialsTable == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cant read materials data table for RMC building. Materials wont be set."));
	}
	else
	{
		MaterialsTable->GetAllRows<FMaterialsTableRow>(FString(), MatRows);
	}
	
	OutRawData.clear();

	int VertIdxCounter = 0;
	int IndicesCounter = 0;
	
	for (const auto& UnrealSection : UnrealData)
	{
		OutRawData.push_back(EDGEMeshSectionData());
		auto& RawSection = OutRawData.back();

		RawSection.MaterialName = "NONE";
		if (MaterialsTable != nullptr)
		{
			for (auto& RowRef : MatRows)
			{
				if (RowRef->Material == Materials[UnrealSection.MaterialSlot])
				{
					RawSection.MaterialName = string(TCHAR_TO_UTF8(*RowRef->MatName));
				}
			}
		}
		
		RawSection.SectionData[0] = VertIdxCounter;		// MinVertIndex
		RawSection.SectionData[2] = IndicesCounter;		// FirstTriIndex
		
		for (FVector Vec : UnrealSection.Vertices)
		{
			RawSection.Vertices.push_back(Vec.X);
			RawSection.Vertices.push_back(Vec.Y);
			RawSection.Vertices.push_back(Vec.Z);
			VertIdxCounter++;
		}
		for (FVector Vec : UnrealSection.Normals)
		{
			RawSection.Normals.push_back(Vec.X);
			RawSection.Normals.push_back(Vec.Y);
			RawSection.Normals.push_back(Vec.Z);
		}
		for (FVector Vec : UnrealSection.Tangents)
		{
			RawSection.Tangents.push_back(Vec.X);
			RawSection.Tangents.push_back(Vec.Y);
			RawSection.Tangents.push_back(Vec.Z);
		}
		for (FVector2D Vec : UnrealSection.UVs)
		{
			RawSection.UVs.push_back(Vec.X);
			RawSection.UVs.push_back(Vec.Y);
		}
		for (int Ind : UnrealSection.Faces)
		{
			RawSection.Indices.push_back(Ind);
			IndicesCounter++;
		}

		RawSection.SectionData[1] = VertIdxCounter - 1;		// MaxVertIndex
		RawSection.SectionData[3] = IndicesCounter / 3;		// NumTriangles
	}
}

void UEDGEMeshUtility::ConvertSectionDataToUnreal(const vector<EDGEMeshSectionData>& RawData, TArray<FRMCSectionData>& OutUnrealData, TArray<UMaterialInterface*>& Materials)
{
	UDataTable* MaterialsTable = Cast<UDataTable>(GetDefault<UEdgeHouseConstructorSettings>()->MaterialsDataTable.ResolveObject());
	if (MaterialsTable == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cant read materials data table for RMC building. Array will be filled with nullptrs."));
	}
	
	OutUnrealData.Empty();
	Materials.Empty();
	
	for (const auto& RawSection : RawData)
	{
		FRMCSectionData& UnrealSection = OutUnrealData.AddDefaulted_GetRef();
		if (MaterialsTable != nullptr)
		{
			const FName RowName = *FString::Printf(TEXT("%s"), *FString(RawSection.MaterialName.c_str()));
			FMaterialsTableRow* RowRef = MaterialsTable->FindRow<FMaterialsTableRow>(RowName, FString());
			if (RowRef != nullptr)
			{
				int MatIdx = Materials.AddUnique(RowRef->Material);
				UnrealSection.MaterialSlot = MatIdx;
			}
			else
			{
				Materials.Add(nullptr);
				UE_LOG(LogTemp, Warning, TEXT("Cant find material with name <%s>. Replaced with nullptr."), *RowName.ToString());
			}
		}
		else
		{
			Materials.Add(nullptr);
		}
		
		for (int Idx = 0; Idx < RawSection.Vertices.size(); Idx += 3)
		{
			UnrealSection.Vertices.Add(FVector(RawSection.Vertices[Idx], RawSection.Vertices[Idx+1], RawSection.Vertices[Idx+2]));
			UnrealSection.Normals.Add(FVector(RawSection.Normals[Idx], RawSection.Normals[Idx+1], RawSection.Normals[Idx+2]));
			UnrealSection.Tangents.Add(FVector(RawSection.Tangents[Idx], RawSection.Tangents[Idx+1], RawSection.Tangents[Idx+2]));
		}
		for (int Idx = 0; Idx < RawSection.UVs.size(); Idx += 2)
		{
			UnrealSection.UVs.Add(FVector2D(RawSection.UVs[Idx], RawSection.UVs[Idx+1]));
		}
		for (auto Ind : RawSection.Indices)
		{
			UnrealSection.Faces.Add(Ind);
		}
	}
}
