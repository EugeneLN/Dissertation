// Fill out your copyright notice in the Description page of Project Settings.


#include "SegmentEditor/BigBaseSegment.h"

TArray<TSubclassOf<ABaseSegment>> ABigBaseSegment::GetSmallSegments() {
	return smallSegments;
}

// Return NULL if segment wasn't found at specified index.
TSubclassOf<ABaseSegment> ABigBaseSegment::GetSmallSegment(int _index) {
	if (smallSegments.IsValidIndex(_index)) {
		return smallSegments[_index];
	}
	else {
		return NULL;
	}
}