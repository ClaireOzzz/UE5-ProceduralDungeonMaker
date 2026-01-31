// Copyright Chateau Pageot, Inc. All Rights Reserved.

#include "DungeonGenerator_Star.h"

#include "CityGen_Roombase.h"

// This return an array without nullptr actors
// The pair does not contains same actor
TArray<TPair<ACityGen_RoomBase*, ACityGen_RoomBase* >> ADungeonGenerator_Star::GetRoomsToConnectArray() const
{
	TArray<TPair<ACityGen_RoomBase*, ACityGen_RoomBase* >> result;
	if (!CentralRoom)
	{
		// Prevent crash if the user did not fill a entry in the room array
		return result;
	}

	for (int32 i = 0; i < RoomsToConnect.Num(); ++i) // FOR EACH ROOM PAIR LOOP
	{
		ACityGen_RoomBase* ToRoom = RoomsToConnect[i];
		if (!ToRoom)
		{
			// Prevent crash if the user did not fill a entry in the room array
			continue;
		}
		if (CentralRoom == ToRoom)
		{
			// Prevent assert if the user fill the room array with same room twice in a row
			continue;
		}
		result.Add(TPair<ACityGen_RoomBase*, ACityGen_RoomBase* >(CentralRoom, ToRoom));
	}
	return result;
}

// This return an array without nullptr actors
TArray<ACityGen_RoomBase*> ADungeonGenerator_Star::GetAllRoomsArray() const
{
	TArray<ACityGen_RoomBase*> result;
	for(ACityGen_RoomBase* Room : RoomsToConnect)
	{
		if(Room != nullptr)
		{
			result.Add(Room);
		}
	}
	if (CentralRoom != nullptr)
	{
		result.Add(CentralRoom);
	}
	for (ACityGen_RoomBase* Obstacle : Obstacles)
	{
		if (Obstacle != nullptr)
		{
			result.Add(Obstacle);
		}
	}
	return result;
}

void ADungeonGenerator_Star::InitFromSpawnedRooms(const TArray<ACityGen_RoomBase*>& SpawnedRooms)
{
	RoomsToConnect = SpawnedRooms;
}