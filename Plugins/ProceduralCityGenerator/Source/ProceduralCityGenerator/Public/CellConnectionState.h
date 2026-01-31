// Copyright Chateau Pageot, Inc. All Rights Reserved.

#pragma once

#include "SimpleGridRuntime/Public/SG_GridCoordinate.h"

#include "CoreMinimal.h"

struct FCellConnectionState
{
public:
	bool bConnectNorth = false;

	bool bNorthIsARoom = false;

	bool bConnectEast = false;

	bool bEastIsARoom = false;

	bool bConnectSouth = false;

	bool bSouthIsARoom = false;

	bool bConnectWest = false;

	bool bWestIsARoom = false;

	bool bConnectUp = false;

	bool bConnectDown = false;

public:
	void MakeConnection(const FSG_GridCoordinate& OurCoords, const FSG_GridCoordinate& OtherCoords, bool bIsARoom);
	
	int32 GetHorizontalConnectionCount() const;

	TArray<bool> GetDirectionArray() const;
};
