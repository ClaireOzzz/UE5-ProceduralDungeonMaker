// Copyright Chateau Pageot, Inc. All Rights Reserved.

#pragma once

#include "SimpleGridRuntime/Public/SG_GridComponent.h"

#include "CityGen_NodeCoordinate.generated.h"

USTRUCT(BlueprintType)
struct FCityGen_NodeCoord
{
	GENERATED_BODY()

public:
	// A* algorithm variables
	int32 GCost = TNumericLimits< int32 >::Max(); // Cost so far, set as highest for the first round of pathfinding
	int32 HCost = TNumericLimits< int32 >::Max(); // Heuristic

	bool bHaveParent = false;
	FSG_GridCoordinate ParentCoordinate = {};
	FSG_GridCoordinate NodeCoordinate = {};

public:
	FCityGen_NodeCoord() = default;

	// Total cost
	FORCEINLINE float ComputeFCost() const
	{
		return HCost + GCost;
	}

	// accessors (getters setters)
	FORCEINLINE const FSG_GridCoordinate& GetNodeCoordinate() const
	{
		return NodeCoordinate;
	}

	FORCEINLINE const FSG_GridCoordinate& GetParentNodeCoordinate() const // USED IN PATHFINDING
	{
		return ParentCoordinate;
	}

	FORCEINLINE FString ToString()
	{
		return FString::Printf(TEXT("GCost:%d HCost:%d FCost:%f"), GCost, HCost, ComputeFCost());
	}
};
