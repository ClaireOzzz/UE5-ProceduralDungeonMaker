// Copyright Chateau Pageot, Inc. All Rights Reserved.

#include "CellConnectionState.h"

#include "SimpleGridRuntime/Public/SG_GridComponent.h"

void FCellConnectionState::MakeConnection(const FSG_GridCoordinate& OurCoords, const FSG_GridCoordinate& OtherCoords, bool bIsARoom)
{
	if(OurCoords.Z != OtherCoords.Z)
	{
		// Special case for stairs / elevator
		check(bIsARoom == false); // currently we do not support elevator/stairs from room (but should not be difficult to add)
		if(OtherCoords.Z > OurCoords.Z)
		{
			bConnectUp = true;
		}
		else
		{
			bConnectDown = true;
		}
		return;
	}
	int32 rotation = SG_GetRotNormForDirection(OtherCoords - OurCoords);

	switch(rotation)
	{
	case 0:
		bConnectNorth = true;
		bNorthIsARoom |= bIsARoom; // IsARoom should not be revert back to false, but just to be sure
		break;
	case 1:
		bConnectEast = true;
		bEastIsARoom |= bIsARoom; // IsARoom should not be revert back to false, but just to be sure
		break;
	case 2:
		bConnectSouth = true;
		bSouthIsARoom |= bIsARoom; // IsARoom should not be revert back to false, but just to be sure
		break;
	case 3:
		bConnectWest = true;
		bWestIsARoom |= bIsARoom; // IsARoom should not be revert back to false, but just to be sure
		break;
	default:
		check(false); // If this trigger, check that the coordinate are direct neighbors
	}
}

int32 FCellConnectionState::GetHorizontalConnectionCount() const
{
	int32 result = (bConnectNorth? 1 : 0);
	result += (bConnectEast ? 1 : 0);
	result += (bConnectSouth ? 1 : 0);
	result += (bConnectWest ? 1 : 0);
	return result;
}

TArray<bool> FCellConnectionState::GetDirectionArray() const
{
	TArray<bool> result;
	result.Reserve(4);
	result.Add(bConnectNorth);
	result.Add(bConnectEast);
	result.Add(bConnectSouth);
	result.Add(bConnectWest);
	return result;
}

