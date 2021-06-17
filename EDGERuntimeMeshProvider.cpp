// Fill out your copyright notice in the Description page of Project Settings.


#include "RuntimeMesh/EDGERuntimeMeshProvider.h"

void UEDGERuntimeMeshProvider::SetSectionsData(TArray<FRMCSectionData> SectionsData)
{
	FScopeLock Lock(&PropertySyncRoot);
	ClearSectionsData_Unsynced();		// We already locked scope, no reason to use normal version
	MeshSectionData = SectionsData;
	CalculateBoundsPoints();
	bHaveMeshData = true;
}

void UEDGERuntimeMeshProvider::AddSectionData(FRMCSectionData SectionData)
{
	FScopeLock Lock(&PropertySyncRoot);
	MeshSectionData.Add(SectionData);
}

void UEDGERuntimeMeshProvider::ClearSectionsData()
{
	FScopeLock Lock(&PropertySyncRoot);
	ClearSectionsData_Unsynced();
}

void UEDGERuntimeMeshProvider::ClearSectionsData_Unsynced()
{
	MeshSectionData.Empty();
	MinBoundPoint = FVector(0.f);
	MaxBoundPoint = FVector(0.f);
	bHaveMeshData = false;
}

bool UEDGERuntimeMeshProvider::GetSectionMeshForLOD_Unsynced(int32 LODIndex, int32 SectionIdx, FRuntimeMeshRenderableMeshData& MeshData)
{
	if (!MeshSectionData.IsValidIndex(SectionIdx))
	{
		return false;
	}
	
	for (int Idx = 0; Idx < MeshSectionData[SectionIdx].Vertices.Num(); Idx++)
	{
		MeshData.Positions.Add(MeshSectionData[SectionIdx].Vertices[Idx]);
		MeshData.Tangents.Add(MeshSectionData[SectionIdx].Normals[Idx], MeshSectionData[SectionIdx].Tangents[Idx]);
		MeshData.Colors.Add(FColor(0.f, 0.f, 0.f, 1.f));
		MeshData.TexCoords.Add(MeshSectionData[SectionIdx].UVs[Idx]);
	}

	for (int Tris : MeshSectionData[SectionIdx].Faces)
	{
		MeshData.Triangles.Add(Tris);
	}

	return true;
}

FVector UEDGERuntimeMeshProvider::GetBoxRadius() const
{
	FScopeLock Lock(&PropertySyncRoot);
	return (MaxBoundPoint - MinBoundPoint) / 2.f;
}

FVector UEDGERuntimeMeshProvider::GetBoxCenter() const
{
	FScopeLock Lock(&PropertySyncRoot);
	return MinBoundPoint + (MaxBoundPoint - MinBoundPoint) / 2.f;
}

bool UEDGERuntimeMeshProvider::HaveMeshData() const
{
	return bHaveMeshData
		   && MeshSectionData.Num() > 0
		   && Materials.Num() > 0;
}


void UEDGERuntimeMeshProvider::CalculateBoundsPoints()
{
	FScopeLock Lock(&PropertySyncRoot);

	FVector MinV = FVector(90000.f);
	FVector MaxV = FVector(-90000.f);

	for (FRMCSectionData Data : MeshSectionData)
	{
		for (FVector Vec : Data.Vertices)
		{
			MinV.X = FMath::Min(MinV.X, Vec.X);
			MinV.Y = FMath::Min(MinV.Y, Vec.Y);
			MinV.Z = FMath::Min(MinV.Z, Vec.Z);
		
			MaxV.X = FMath::Max(MaxV.X, Vec.X);
			MaxV.Y = FMath::Max(MaxV.Y, Vec.Y);
			MaxV.Z = FMath::Max(MaxV.Z, Vec.Z);
		}
	}

	MinBoundPoint = MinV;
	MaxBoundPoint = MaxV;

	MarkAllLODsDirty();
	MarkCollisionDirty();
}

TArray<FRMCSectionData> UEDGERuntimeMeshProvider::GetSectionData() const
{
	FScopeLock Lock(&PropertySyncRoot);
	return MeshSectionData;
}

TArray<UMaterialInterface*> UEDGERuntimeMeshProvider::GetMaterials() const
{
	FScopeLock Lock(&PropertySyncRoot);
	return Materials;
}


UMaterialInterface* UEDGERuntimeMeshProvider::GetMaterialFromSlot(int SlotIndex) const
{
	FScopeLock Lock(&PropertySyncRoot);

	if (!Materials.IsValidIndex(SlotIndex))
	{
		return nullptr;
	}
	
	return Materials[SlotIndex];
}

void UEDGERuntimeMeshProvider::SetMaterials(TArray<UMaterialInterface*> InMaterials)
{
	FScopeLock Lock(&PropertySyncRoot);

	Materials = InMaterials;
}

void UEDGERuntimeMeshProvider::AddMaterial(UMaterialInterface* InMaterial)
{
	FScopeLock Lock(&PropertySyncRoot);
	
	Materials.Add(InMaterial);

	// const int SlotIndex = Materials.Num() - 1;
	// const FName SlotName = *FString::Printf(TEXT("Slot_%i"), SlotIndex);
	// SetupMaterialSlot(SlotIndex, SlotName, Materials[SlotIndex]);
}




void UEDGERuntimeMeshProvider::Initialize()
{
	int LODIdx = 0;	// TODO: Waiting LODs implementation
	FRuntimeMeshLODProperties LODProperties;
	LODProperties.ScreenSize = 0.0f;

	ConfigureLODs({ LODProperties });

	
	FRuntimeMeshSectionProperties Properties;
	Properties.bCastsShadow = true;
	Properties.bIsVisible = true;
	Properties.UpdateFrequency = ERuntimeMeshUpdateFrequency::Infrequent;

	for (int SectionIdx = 0; SectionIdx < MeshSectionData.Num(); SectionIdx++)
	{
		const int SlotIdx = (Materials.IsValidIndex(MeshSectionData[SectionIdx].MaterialSlot)) ? MeshSectionData[SectionIdx].MaterialSlot : 0;
		const FName SlotName = *FString::Printf(TEXT("Slot_%i"), SlotIdx);
		
		SetupMaterialSlot(SlotIdx, SlotName, Materials[SlotIdx]);
		
		Properties.MaterialSlot = SlotIdx;
		CreateSection(LODIdx, SectionIdx, Properties);
	}

	MarkCollisionDirty();
}

FBoxSphereBounds UEDGERuntimeMeshProvider::GetBounds()
{
	return FBoxSphereBounds(FBox(MinBoundPoint, MaxBoundPoint));
}

bool UEDGERuntimeMeshProvider::GetSectionMeshForLOD(int32 LODIndex, int32 SectionIdx, FRuntimeMeshRenderableMeshData& MeshData)
{	// We should only ever be queried for section 0 and lod 0
	// check(SectionId == 0 && LODIndex == 0);			// Are we really need this???

	FScopeLock Lock(&PropertySyncRoot);

	return GetSectionMeshForLOD_Unsynced(LODIndex, SectionIdx, MeshData);
}

bool UEDGERuntimeMeshProvider::GetAllSectionsMeshForLOD(int32 LODIndex, TMap<int32, FRuntimeMeshSectionData>& MeshDatas)
{
	return false;
	
	FScopeLock Lock(&PropertySyncRoot);

	// TODO: Need more exploring...
	for (int SectionIdx = 0; SectionIdx < MeshSectionData.Num(); SectionIdx++)
	{
		FRuntimeMeshSectionData NewData;
		if (GetSectionMeshForLOD_Unsynced(LODIndex, SectionIdx, NewData.MeshData) == false)
		{
			return false;
		}
		MeshDatas.Add(SectionIdx, NewData);
	}
	
	return true;
}

FRuntimeMeshCollisionSettings UEDGERuntimeMeshProvider::GetCollisionSettings()
{
	FRuntimeMeshCollisionSettings Settings;
	Settings.bUseAsyncCooking = true;
	Settings.bUseComplexAsSimple = false;

	FVector Extents = FVector(MaxBoundPoint.X - MinBoundPoint.X, MaxBoundPoint.Y - MinBoundPoint.Y, MaxBoundPoint.Z - MinBoundPoint.Z);
	FVector Center = MinBoundPoint + (MaxBoundPoint - MinBoundPoint) / 2.f;
	Settings.Boxes.Emplace(Center, FRotator(0.f), Extents.X, Extents.Y, Extents.Z);
	
	return Settings;
}

bool UEDGERuntimeMeshProvider::HasCollisionMesh()
{
	return true;
}

bool UEDGERuntimeMeshProvider::GetCollisionMesh(FRuntimeMeshCollisionData& CollisionData)
{
	// Add the single collision section
	CollisionData.CollisionSources.Emplace(0, 5, this, 0, ERuntimeMeshCollisionFaceSourceType::Collision);

	FRuntimeMeshCollisionVertexStream& CollisionVertices = CollisionData.Vertices;
	FRuntimeMeshCollisionTriangleStream& CollisionTriangles = CollisionData.Triangles;


	// Generate verts
	CollisionVertices.Add(FVector(MinBoundPoint.X, MaxBoundPoint.Y, MaxBoundPoint.Z));
	CollisionVertices.Add(FVector(MaxBoundPoint.X, MaxBoundPoint.Y, MaxBoundPoint.Z));
	CollisionVertices.Add(FVector(MaxBoundPoint.X, MinBoundPoint.Y, MaxBoundPoint.Z));
	CollisionVertices.Add(FVector(MaxBoundPoint.X, MinBoundPoint.Y, MaxBoundPoint.Z));

	CollisionVertices.Add(FVector(MinBoundPoint.X, MaxBoundPoint.Y, MinBoundPoint.Z));
	CollisionVertices.Add(FVector(MaxBoundPoint.X, MaxBoundPoint.Y, MinBoundPoint.Z));
	CollisionVertices.Add(FVector(MaxBoundPoint.X, MinBoundPoint.Y, MinBoundPoint.Z));
	CollisionVertices.Add(FVector(MinBoundPoint.X, MinBoundPoint.Y, MinBoundPoint.Z));

	// Pos Z
	CollisionTriangles.Add(0, 1, 3);
	CollisionTriangles.Add(1, 2, 3);
	// Neg X
	CollisionTriangles.Add(4, 0, 7);
	CollisionTriangles.Add(0, 3, 7);
	// Pos Y
	CollisionTriangles.Add(5, 1, 4);
	CollisionTriangles.Add(1, 0, 4);
	// Pos X
	CollisionTriangles.Add(6, 2, 5);
	CollisionTriangles.Add(2, 1, 5);
	// Neg Y
	CollisionTriangles.Add(7, 3, 6);
	CollisionTriangles.Add(3, 2, 6);
	// Neg Z
	CollisionTriangles.Add(7, 6, 4);
	CollisionTriangles.Add(6, 5, 4);

	return true;
}

bool UEDGERuntimeMeshProvider::IsThreadSafe()
{
	return true;
}