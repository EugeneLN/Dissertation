// Fill out your copyright notice in the Description page of Project Settings.


#include "SegmentEditor/PatternEditor.h"
#include "SegmentEditor/BaseSegment.h"
#include "SegmentEditor/BaseSegmentDecoration.h"

#include "HouseEditor/HouseEditorFunctionLibrary.h"

#include "EdgeHouseConstructor/EdgeHouseConstructorSettings.h"

#include "UObject/Package.h"
#include "Kismet/KismetSystemLibrary.h" 
#include "AssetRegistry/AssetRegistryModule.h"
#include "Runtime/Engine/Classes/Engine/DataTable.h"


APatternEditor::APatternEditor()
{
	PrimaryActorTick.bCanEverTick = false;
	OffsetsV.Add(0);
	OffsetsH.Add(0);
}


void APatternEditor::AutoGeneratePattern()
{
	ClearAll(true);

	for (int vi = 0; vi < AutoLinesCount; vi++) {
		OffsetsH.Empty();
		OffsetsH.Add(0);

		FPatternLine newLine;
		for (int hi = 0; hi < AutoSegmentsCount; hi++) {
			FSegmentData NewClass;
			NewClass.SegmentName = AutoFillerClass;
			newLine.Segments.Add(NewClass);
			BuildElement(hi, vi, NewClass);
		}
		newLine.LineNumber = vi;
		newLine.LineSize = OffsetsH.Last() / SegmentWidthInUnits;
		SegmentsClassArray.Add(newLine);

		OffsetsV.Add((vi + 1) * SegmentHeightInUnits);
	}
}

void APatternEditor::RebuildPattern()
{
	ClearAll(false);

	for (int vi = 0; vi < SegmentsClassArray.Num(); vi++) {
		OffsetsH.Empty();
		OffsetsH.Add(0);

		TArray<FSegmentData> currLine = SegmentsClassArray[vi].Segments;
		for (int hi = 0; hi < currLine.Num(); hi++) {
			BuildElement(hi, vi, currLine[hi]);
		}
		OffsetsV.Add((vi + 1)* SegmentHeightInUnits);

		SegmentsClassArray[vi].LineNumber = vi;
		SegmentsClassArray[vi].LineSize = OffsetsH.Last() / SegmentWidthInUnits;
	}
}

void APatternEditor::ClearSegments()
{
	ClearAll(true);
}

void APatternEditor::ClearAll(bool bClearClasses)
{
	while (SegmentsActors.Num() > 0) {
		AActor* Element = SegmentsActors.Pop();
		TArray<AActor*> Decorations;
		Element->GetAttachedActors(Decorations);
		for (AActor* Actor : Decorations)
		{
			Actor->Destroy();
		}
		Element->Destroy();
	}
	OffsetsV.Empty();
	OffsetsV.Add(0);
	OffsetsH.Empty();
	OffsetsH.Add(0);
	if (bClearClasses) {
		SegmentsClassArray.Empty();
	}
}

void APatternEditor::AddLineAt(FPatternLine NewLine, int LineIndex)
{
	if (LineIndex < 0 || LineIndex > SegmentsClassArray.Num()) {
		UE_LOG(LogTemp, Warning, TEXT("Not valid index to add line: %i"), LineIndex);
		return;
	}
	SegmentsClassArray.Insert(NewLine, LineIndex);
	for (int i = LineIndex; i < SegmentsClassArray.Num(); i++) {
		SegmentsClassArray[i].LineNumber = i;
	}
}

void APatternEditor::AddLine(FPatternLine NewLine)
{
	AddLineAt(NewLine, SegmentsClassArray.Num());
}

void APatternEditor::RemoveLineAt(int LineIndex)
{
	if (!SegmentsClassArray.IsValidIndex(LineIndex)) {
		UE_LOG(LogTemp, Warning, TEXT("Not valid index to remove line: %i"), LineIndex);
		return;
	}
	SegmentsClassArray.RemoveAt(LineIndex);
	int i = LineIndex;
	while (i < SegmentsClassArray.Num()) {
		SegmentsClassArray[i].LineNumber = i;
		i++;
	}
}

void APatternEditor::UpdateSegment(FSegmentData NewSegment, int LineIndex, int SegmentIndex)
{
	if (!SegmentsClassArray.IsValidIndex(LineIndex)) {
		UE_LOG(LogTemp, Warning, TEXT("Not valid line index: %i"), LineIndex);
		return;
	}
	if (!SegmentsClassArray[LineIndex].Segments.IsValidIndex(SegmentIndex)) {
		UE_LOG(LogTemp, Warning, TEXT("Not valid segment index: %i"), SegmentIndex);
		return;
	}

	int OldSize = UHouseEditorFunctionLibrary::GetSegmentSizeByFName(SegmentsClassArray[LineIndex].Segments[SegmentIndex].SegmentName);
	int NewSize = UHouseEditorFunctionLibrary::GetSegmentSizeByFName(NewSegment.SegmentName);
	SegmentsClassArray[LineIndex].Segments[SegmentIndex] = NewSegment;
	if (NewSize != OldSize) {
		SegmentsClassArray[LineIndex].LineSize += NewSize - OldSize;
	}
}

void APatternEditor::AddSegmentAt(FSegmentData NewSegment, int LineIndex, int SegmentIndex)
{
	if (!SegmentsClassArray.IsValidIndex(LineIndex)) {
		UE_LOG(LogTemp, Warning, TEXT("Not valid line index: %i"), LineIndex);
		return;
	}
	if (SegmentIndex < 0 || SegmentIndex > SegmentsClassArray[LineIndex].Segments.Num()) {
		UE_LOG(LogTemp, Warning, TEXT("Not valid segment index: %i"), SegmentIndex);
		return;
	}

	SegmentsClassArray[LineIndex].Segments.Insert(NewSegment, SegmentIndex);
	SegmentsClassArray[LineIndex].LineSize += UHouseEditorFunctionLibrary::GetSegmentSizeByFName(NewSegment.SegmentName);
}

void APatternEditor::AddSegment(FSegmentData NewSegment, int LineIndex)
{
	if (!SegmentsClassArray.IsValidIndex(LineIndex)) {
		UE_LOG(LogTemp, Warning, TEXT("Not valid line index: %i"), LineIndex);
		return;
	}
	AddSegmentAt(NewSegment, LineIndex, SegmentsClassArray[LineIndex].Segments.Num());
}

void APatternEditor::RemoveSegmentAt(int LineIndex, int SegmentIndex)
{
	if (!SegmentsClassArray.IsValidIndex(LineIndex)) {
		UE_LOG(LogTemp, Warning, TEXT("Not valid line index: %i"), LineIndex);
return;
	}
	if (!SegmentsClassArray[LineIndex].Segments.IsValidIndex(SegmentIndex)) {
		UE_LOG(LogTemp, Warning, TEXT("Not valid segment index: %i"), SegmentIndex);
		return;
	}

	SegmentsClassArray[LineIndex].LineSize -= UHouseEditorFunctionLibrary::GetSegmentSizeByFName(SegmentsClassArray[LineIndex].Segments[SegmentIndex].SegmentName);
	SegmentsClassArray[LineIndex].Segments.RemoveAt(SegmentIndex);
	
	if (SegmentsClassArray[LineIndex].Segments.Num() == 0) {
		RemoveLineAt(LineIndex);
	}
}


// Customs

void APatternEditor::PreviewMaterials()
{
	TArray<TEnumAsByte<EMaterialSlot>> Keys;
	MaterialPreview.GetKeys(Keys);
	if (Keys.Num() == 0) {
		return;
	}

	for (int i = 0; i < SegmentsActors.Num(); i++) {
		for (int k = 0; k < Keys.Num(); k++) {
			SegmentsActors[i]->SetMaterialByName(Keys[k], MaterialPreview[Keys[k]]);
		}
	}
}



// Internal functions

void APatternEditor::BuildElement(int SegmentHPos, int SegmentVPos, FSegmentData NewSegment)
{
	ABaseSegment* NewActor;
	TSubclassOf<ABaseSegment> NewActorClass = UHouseEditorFunctionLibrary::GetSegmentClassByFName(NewSegment.SegmentName);
	if (NewActorClass == NULL) {
		return;
	}

	NewActor = GetWorld()->SpawnActor<ABaseSegment>(NewActorClass, FVector(OffsetsH[SegmentHPos], 0, OffsetsV[SegmentVPos]), FRotator(0));
	if (NewActor == NULL) {
		UE_LOG(LogTemp, Warning, TEXT("Actor was not created!"));
		return;
	}
	NewActor->AttachToActor(this, FAttachmentTransformRules::KeepRelativeTransform);			// TODO: remake offests calculation, couse its BS for now

	TArray<TEnumAsByte<EMaterialSlot>> Keys;
	MaterialPreview.GetKeys(Keys);
	for (int i = 0; i < Keys.Num(); i++) {
		NewActor->SetMaterialByName(Keys[i], MaterialPreview[Keys[i]]);
	}

	SegmentsActors.Add(NewActor);
	OffsetsH.Add(OffsetsH[SegmentHPos] + NewActor->GetSegmentSize() * SegmentWidthInUnits);
	
	TArray<USceneComponent*> Sockets = NewActor->GetDecorationSockets();
	
	if (NewSegment.SegmentDecorationWeights.Num() > 0 && Sockets.Num() > 0)
	{
		for (USceneComponent* Socket : Sockets)
		{
			float RValue = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);			// TODO: make random with seed that may (or may not) be saved
			float Threshold = 0.f;

			const FSegmentDecorationsData EmptyData;
			TMap<FName, float> WeightsMap = UHouseEditorFunctionLibrary::GetNormalizedDecorationWeights(NewSegment, Socket->GetFName(), EmptyData);
			
			TSubclassOf<ABaseSegmentDecoration> DecorationClass;
			TArray<FName> Names;
			WeightsMap.GenerateKeyArray(Names);
			
			for (FName Name : Names)
			{
				Threshold += WeightsMap.FindRef(Name);
				if (RValue <= Threshold)
				{
					DecorationClass = UHouseEditorFunctionLibrary::GetSegmentDecorationClassByFName(Name);
					break;
				}
			}
			if (DecorationClass != NULL)
			{
				ABaseSegmentDecoration* NewDecoration = GetWorld()->SpawnActor<ABaseSegmentDecoration>(DecorationClass, FVector(0.f), FRotator(0.f));
				if (NewDecoration == NULL)
				{
					UE_LOG(LogTemp, Warning, TEXT("Decoration was not created!"));
					return;
				}
				NewDecoration->AttachToComponent(Socket, FAttachmentTransformRules::KeepRelativeTransform);
			}
		}
	}
	
}

void APatternEditor::SavePattern()
{
	FPatternData Data;
	UDataTable* PatternsDataTable = UHouseEditorFunctionLibrary::GetPatternDataTable();

	if (PatternsDataTable == NULL)
	{
		UE_LOG(LogTemp, Warning, TEXT("Data table is not valid. Pattern was not saved."));
		return;
	}

	Data.PatternName = PatternName;
	Data.bRepeatPattern = bRepeatPattern;
	Data.Lines = SegmentsClassArray;

	PatternsDataTable->AddRow(PatternName, Data);

	UHouseEditorFunctionLibrary::CheckOutAndSave(PatternsDataTable);
}

void APatternEditor::LoadPattern()
{
	FPatternData* Data;
	UDataTable* PatternsDataTable = UHouseEditorFunctionLibrary::GetPatternDataTable();

	if (PatternsDataTable == NULL)
	{
		UE_LOG(LogTemp, Warning, TEXT("Data table is not valid. Can't load patterns."));
		return;
	}
	
	Data = PatternsDataTable->FindRow<FPatternData>(PatternName, FString());
	if (Data != NULL) {
		SegmentsClassArray = Data->Lines;
		bRepeatPattern = Data->bRepeatPattern;
		RebuildPattern();
	}
}

// Segment Classes Data Table

void APatternEditor::UpdateAvailableSegmentClasses()
{
	UDataTable* SegmentClassesDataTable = UHouseEditorFunctionLibrary::GetSegmentDataTable();
	UDataTable* SegmentDecorationClassesDataTable = UHouseEditorFunctionLibrary::GetSegmentDecorationDataTable();
	const FString PackagePath = GetDefault<UEdgeHouseConstructorSettings>()->ElementsBlueprintsDirectory.Path;

	if (SegmentClassesDataTable == NULL)
	{
		UE_LOG(LogTemp, Warning, TEXT("Data table is not valid. Can't update segments. Update aborted."))
		return;
	}
	if (SegmentDecorationClassesDataTable == NULL)
	{
		UE_LOG(LogTemp, Warning, TEXT("Data table is not valid. Can't update decorations. Update aborted."))
		return;
	}
	
	TArray<FAssetData> AssetData;
	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FARFilter Filter;
	
	if (AssetRegistry.Get().IsLoadingAssets()) {
		UE_LOG(LogTemp, Display, TEXT("Data update paused untill all assets will be loaded."));
		AssetRegistry.Get().WaitForCompletion();
		UE_LOG(LogTemp, Display, TEXT("Assets loaded. Resuming data update."));
	}

	Filter.ClassNames.Add("Blueprint");
	Filter.PackagePaths.Add(*PackagePath);
	Filter.bRecursivePaths = true;
	AssetRegistry.Get().GetAssets(Filter, AssetData);

	SegmentClassesDataTable->EmptyTable();
	SegmentDecorationClassesDataTable->EmptyTable();
	for (int i = 0; i < AssetData.Num(); i++) {
		FAssetData Data = AssetData[i];

		UClass* DataClass = Cast<UBlueprint>(Data.GetAsset())->GeneratedClass;
		if (DataClass->IsChildOf<ABaseSegment>()) {
			FSegmentClassesData RowData;
			RowData.SegmentClass = DataClass;
			RowData.SegmentImage = UHouseEditorFunctionLibrary::MakeTextureBasedOnThumbnail(Data.GetAsset());
			SegmentClassesDataTable->AddRow(Data.AssetName, RowData);
			UE_LOG(LogTemp, Display, TEXT("Found segment class: %s"), *Data.AssetName.ToString());
		}
		else if (DataClass->IsChildOf<ABaseSegmentDecoration>()) {
			FSegmentDecorationClassesData RowData;
			RowData.SegmentDecorationClass = DataClass;
			SegmentDecorationClassesDataTable->AddRow(Data.AssetName, RowData);
			UE_LOG(LogTemp, Display, TEXT("Found segment decoration class: %s"), *Data.AssetName.ToString());
		}
	}

	SaveSegmentClassesDataTable();
	SaveSegmentDecorationClassesDataTable();
}

void APatternEditor::SaveSegmentClassesDataTable() const
{
	UDataTable* SegmentClassesDataTable = UHouseEditorFunctionLibrary::GetSegmentDataTable();
	if (SegmentClassesDataTable == NULL)
	{
		return;
	}
	
	UHouseEditorFunctionLibrary::CheckOutAndSave(SegmentClassesDataTable);
}


void APatternEditor::SaveSegmentDecorationClassesDataTable() const
{
	UDataTable* SegmentDecorationClassesDataTable = UHouseEditorFunctionLibrary::GetSegmentDecorationDataTable();
	if (SegmentDecorationClassesDataTable == NULL)
	{
		return;
	}

	UHouseEditorFunctionLibrary::CheckOutAndSave(SegmentDecorationClassesDataTable);
}
