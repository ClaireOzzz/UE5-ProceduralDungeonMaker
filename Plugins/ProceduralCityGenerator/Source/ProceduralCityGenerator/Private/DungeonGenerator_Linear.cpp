// Copyright Chateau Pageot, Inc. All Rights Reserved.

#include "DungeonGenerator_Linear.h"

// This return an array without nullptr actors
// The pair does not contains same actor
TArray<TPair<ACityGen_RoomBase*, ACityGen_RoomBase* >> ADungeonGenerator_Linear::GetRoomsToConnectArray() const
{
	TArray<TPair<ACityGen_RoomBase*, ACityGen_RoomBase* >> result;
	for (int32 i = 0; i < RoomsToConnect.Num() - 1; ++i) // FOR EACH ROOM PAIR LOOP
	{
		ACityGen_RoomBase* FromRoom = RoomsToConnect[i];
		ACityGen_RoomBase* ToRoom = RoomsToConnect[i + 1];
		if (!FromRoom || !ToRoom)
		{
			// Prevent crash if the user did not fill a entry in the room array
			continue;
		}
		if (FromRoom == ToRoom)
		{
			// Prevent assert if the user fill the room array with same room twice in a row
			continue;
		}
		result.Add(TPair<ACityGen_RoomBase*, ACityGen_RoomBase* >(FromRoom, ToRoom));
	}
	return result;
}

// This return an array without nullptr actors
TArray<ACityGen_RoomBase*> ADungeonGenerator_Linear::GetAllRoomsArray() const
{
	TArray<ACityGen_RoomBase*> result;
	for (ACityGen_RoomBase* Room : RoomsToConnect)
	{
		if (Room != nullptr)
		{
			result.Add(Room);
		}
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

void ADungeonGenerator_Linear::InitFromSpawnedRooms(const TArray<ACityGen_RoomBase*>& SpawnedRooms)
{
	RoomsToConnect = SpawnedRooms;
}