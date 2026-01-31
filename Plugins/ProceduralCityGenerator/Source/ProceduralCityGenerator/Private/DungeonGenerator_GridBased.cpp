// Copyright Chateau Pageot, Inc. All Rights Reserved.

#include "DungeonGenerator_GridBased.h"

#include "CityGen_LogChannels.h"
#include "CityGen_NodeCoordinate.h"
#include "CityGen_ObstacleBase.h"
#include "CityGen_Roombase.h"

#include "SimpleGridRuntime/Public/SG_GridComponent.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include <Kismet/GameplayStatics.h>
#include "Runtime/Engine/Classes/Kismet/KismetSystemLibrary.h"
#include "Runtime/Engine/Classes/Kismet/KismetMathLibrary.h"

/*
Pathfinding logic (in FindPath):
1. Add Start node to Open set
2. While Open set is not empty, there are still paths to be checked:
3. Find node with lowest fCost in Open set
4. Move node from Open set to Closed set
5. If current node is Goal, reconstruct path
6. Find the neighbours of the current node
7. For each neighbor of current node:
	 8. If neighbor is not walkable or in Closed set, skip to next neighbor
	 9. Calculate cost to move to neighbor
	 10. If cost is lower than previously recorded cost or neighbor is not in Open set, record this path
	 11. If neighbor is not in Open set, add it to Open set
 12. Repeat from step 2 until path is found or Open set is empty

*/

// Sets default values
ADungeonGenerator_GridBased::ADungeonGenerator_GridBased()
{
	// Set up grid component
	DungeonGridCmpt = CreateDefaultSubobject<USG_GridComponent>(TEXT("DungeonGridCmpt"));
	DungeonGridCmpt->SetTileSize(TileSize);

	DungeonGenRandomStream = FRandomStream(RandomSeed);
}

void ADungeonGenerator_GridBased::ClearCorridorMeshes()
{
	for (const auto& Corridor : AllSpawnedCorridors)
	{
		if (Corridor.Value != nullptr)
		{
			Corridor.Value->Destroy();
		}
	}
	AllSpawnedCorridors.Empty();

	for (ACityGen_RoomBase* Room : AllRooms)
	{
		if (Room == nullptr)
		{
			continue;
		}

		Room->CloseAllDoors();
	}

	AllRooms.Empty();
	TilesToIgnore.Empty();
	BlockedGridTiles.Empty();
	RequestedCorridors.Empty();
}

#if WITH_EDITOR
// Function only here to be able to use in editor, in code directly use ConnectRoomsInOrder
void ADungeonGenerator_GridBased::SpawnCorridorPath()
{
	ConnectRoomsInOrder();
}

void ADungeonGenerator_GridBased::DebugDrawConnectedPath()
{
	const float DebugLifeTime = 5.0f;
	const FVector CellCenter = TileSize / 2.0;
	const FVector DebugDrawOffset_Center = CellCenter;
	const FVector DebugDrawOffset_Arrow = CellCenter + FVector(0,0,1) * DungeonGridCmpt->GetTileSize().Z * 0.75f;

	TArray<FSG_GridCoordinate> coords;
	RequestedCorridors.GetKeys(coords);
	for(const auto& coord : coords)
	{
		const FCellConnectionState& connectState = RequestedCorridors[coord];
		TArray<bool> DirArray = connectState.GetDirectionArray();

		FVector PositionBlock = DungeonGridCmpt->GridToWorld(coord);
		DrawDebugSphere(GetWorld(), PositionBlock + DebugDrawOffset_Center, 100.0, 8, FColor::Black, false, DebugLifeTime);
		for(uint32 rotIndex = 0; rotIndex < 4; ++rotIndex)
		{
			if(DirArray[rotIndex] == false)
			{
				continue;
			}
			FSG_GridCoordinate coordDest = coord + SG_GetDirectionForRotNorm(rotIndex);
			FVector PositionBlockDest = DungeonGridCmpt->GridToWorld(coordDest);

			DrawDebugDirectionalArrow(GetWorld(), PositionBlock + DebugDrawOffset_Center, PositionBlockDest + DebugDrawOffset_Arrow, 100.0f, FColor::Blue, false, DebugLifeTime);
		}
	}
}
#endif // WITH_EDITOR

// This return an array without nullptr actors
TArray<TPair<ACityGen_RoomBase*, ACityGen_RoomBase* >> ADungeonGenerator_GridBased::GetRoomsToConnectArray() const
{
	// No room to connect by default
	return TArray<TPair<ACityGen_RoomBase*, ACityGen_RoomBase* >>();
}

// This return an array without nullptr actors
TArray<ACityGen_RoomBase*> ADungeonGenerator_GridBased::GetAllRoomsArray() const
{
	// No room to connect by default
	return TArray<ACityGen_RoomBase*>();
}

void ADungeonGenerator_GridBased::InitFromSpawnedRooms(const TArray<ACityGen_RoomBase*>& SpawnedRooms)
{
	// Nothing by default
}

bool ADungeonGenerator_GridBased::ConnectRoomsInOrder()
{
	AllRooms = GetAllRoomsArray();

	if (AllRooms.Num() < 2)
	{
		UE_LOG(LogCityGen, Warning, TEXT("Not enough rooms to connect."));
		return false;
	}
	if(AllSpawnedCorridors.Num() > 0)
	{
		UE_LOG(LogCityGen, Warning, TEXT("There is still corridors left from previous generation, clearing them."));
		ClearCorridorMeshes();
	}

	check(TilesToIgnore.Num() == 0);
	check(AllSpawnedCorridors.Num() == 0);
	check(RequestedCorridors.Num() == 0);

	// Clear previously setup blocked gridTiles
	BlockedGridTiles.Empty();

	//UpdateBlockedTiles_Obstacles();
	SnapRoomsToGrid();
	UpdateBlockedTiles_RoomBounds();
	UpdateBlockedTiles_ClosedExits();
	UpdateBlockedTiles_TilesToIgnore();

	bool bFoundPathBetweenAllRooms = true;
	TArray<TPair<ACityGen_RoomBase*, ACityGen_RoomBase* >> RoomsToConnect = GetRoomsToConnectArray();
	for(const auto& RoomToConnect : RoomsToConnect)
	{
		if (AddCorridorConnectingRooms(RoomToConnect.Key, RoomToConnect.Value) == false)
		{
			bFoundPathBetweenAllRooms = false;
		}
	}

	if (bFoundPathBetweenAllRooms)
	{
		SpawnCorridors();
		UpdateDoorStatus();
		return true;
	}

	UE_LOG(LogCityGen, Error, TEXT("No path found"));
	return false; // Failed to connect rooms
}

// This return an array without nullptr actors
// The pair does not contains same actor
bool ADungeonGenerator_GridBased::AddCorridorConnectingRooms(ACityGen_RoomBase* FromRoom, ACityGen_RoomBase* ToRoom)
{
	check (FromRoom != ToRoom);

#if !WITH_SORTED_EXIT_ARROW
	TArray<FExitArrowData>& FromExitPoints = FromRoom->GetCachedExitPointsDataRef();
	TArray<FExitArrowData>& ToExitPoints = ToRoom->GetCachedExitPointsDataRef();
	if ((FromExitPoints.Num() == 0) || (ToExitPoints.Num() == 0))
	{
		return false;
	}

	bool bFoundExactMatchingDoors = false;
	FExitArrowData* FromExitWithMinDistance = nullptr;
	FExitArrowData* ToExitWithMinDistance = nullptr;

	// Special case: if the "FromRoom" and "ToRoom" both have a door matching and touching
	for (auto& FromExit : FromExitPoints)
	{
		for (auto& ToExit : ToExitPoints)
		{
			// Need to test both way as the door maybe on a corner pointing to a different direction
			if((FromExit.DungeonGridCoord.position == ToExit.DungeonDoorGridCoord) &&
				(FromExit.DungeonDoorGridCoord == ToExit.DungeonGridCoord.position))
			{
				bFoundExactMatchingDoors = true;
				FromExitWithMinDistance = &FromExit;
				ToExitWithMinDistance = &ToExit;
				break;
			}
		}
		if(bFoundExactMatchingDoors)
		{
			break;
		}
	}

	// In that case no need to run pathfinding
	if(bFoundExactMatchingDoors)
	{
		// Marks the exit as used (useful to open doors)
		FromExitWithMinDistance->bIsUsed = true;
		ToExitWithMinDistance->bIsUsed = true;
		return true;
	}

	float MinDistance = FLT_MAX;
	for(auto& FromExit : FromExitPoints)
	{
		// Skip exit blocked
		if (IsGridTileBlocked(FromExit.DungeonGridCoord.position))
		{
			continue;
		}

		for (auto& ToExit : ToExitPoints)
		{
			// Skip exit blocked
			if(IsGridTileBlocked(ToExit.DungeonGridCoord.position))
			{
				continue;
			}

			float currentDistance = GetDistance(FromExit.DungeonGridCoord.position, ToExit.DungeonGridCoord.position);

			if(currentDistance < MinDistance)
			{
				MinDistance = currentDistance;
				FromExitWithMinDistance = &FromExit;
				ToExitWithMinDistance = &ToExit;
			}
		}
	}

	if((FromExitWithMinDistance == nullptr) || (ToExitWithMinDistance == nullptr))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to find a door combinaison for the room, maybe one room have all its exit blocked?"));
		return false;
	}

	bool bFindPathBetweenRooms = FindPath(FromExitWithMinDistance->DungeonGridCoord.position, FromExitWithMinDistance->DungeonDoorGridCoord, ToExitWithMinDistance->DungeonGridCoord.position, ToExitWithMinDistance->DungeonDoorGridCoord);
	if (bFindPathBetweenRooms == false)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to find a path within the recursive loop hard coded limit for this exit pair, move to next."));
		return false;
	}
	else
	{
		// Marks the exit as used (useful to open doors)
		FromExitWithMinDistance->bIsUsed = true;
		ToExitWithMinDistance->bIsUsed = true;
		return true;
	}
#else
	FSG_GridCoordinate FromSnappedLocationGS = FromRoom->GetRoomGridCoord().position;
	FSG_GridCoordinate ToSnappedLocationGS = ToRoom->GetRoomGridCoord().position;

	TArray<FExitArrowData*> SortedExits_FromRoom;
	bool bGetExitFromRoom = GetSortedExitArrows(FromRoom, ToSnappedLocationGS, SortedExits_FromRoom);
	if (bGetExitFromRoom == false)
	{
		return false;
	}
	TArray<FExitArrowData*> SortedExits_ToRoom;
	bool bGetExitToRoom = GetSortedExitArrows(ToRoom, FromSnappedLocationGS, SortedExits_ToRoom);
	if (bGetExitToRoom == false)
	{
		return false;
	}

	// Loop through SortedExits_FromRoom and SortedExits_ToRoom to find the best exit pair
	bool bFindPathBetweenRooms = false;
	for (int32 j = 0; j < SortedExits_FromRoom.Num(); ++j)
	{
		FExitArrowData* pFromExit = SortedExits_FromRoom[j];
		if (!pFromExit->ArrowCmpt)
		{
			continue;
		}
		FSG_GridCoordinate FromDoorCoordinate = pFromExit->DungeonDoorGridCoord;

		// TODO : Maybe we could have a first selection of exit<>exit to: 1) remove invalid 2) select the one with closest manhatan distance
		for (int32 k = 0; k < SortedExits_ToRoom.Num(); ++k)
		{
			FExitArrowData* pToExit = SortedExits_ToRoom[k];
			if (!pToExit->ArrowCmpt)
			{
				continue;
			}
			FSG_GridCoordinate ToDoorCoordinate = pToExit->DungeonDoorGridCoord;

			TilesToIgnore.Add(FromSnappedLocationGS);
			TilesToIgnore.Add(ToSnappedLocationGS);
			UpdateBlockedTiles_TilesToIgnore();

#if 0
			// Optional: Visualize blocked tiles
			for (const FSG_GridCoordinate& Tile : BlockedGridTiles)
			{
				FVector Location = DungeonGridCmpt->GridToWorld(Tile) + TileSize / 2.0; // Centered
				UKismetSystemLibrary::DrawDebugBox(this, Location, TileSize / 2.0, FLinearColor::Black, FRotator::ZeroRotator, 10.f);
			}
#endif

			bFindPathBetweenRooms = FindPath(pFromExit->DungeonGridCoord.position, FromDoorCoordinate, pToExit->DungeonGridCoord.position, ToDoorCoordinate);
			if (bFindPathBetweenRooms == false)
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to find a path within the recursive loop hard coded limit for this exit pair, move to next."));
			}
			else
			{
				// Marks the exit as used (useful to open doors)
				pFromExit->bIsUsed = true;
				pToExit->bIsUsed = true;
				break; // path found
			}
		}
		if (bFindPathBetweenRooms)
		{
			return true;
		}
	}
	UE_LOG(LogCityGen, Error, TEXT("No path found"));
	return false;
#endif
}

#if WITH_SORTED_EXIT_ARROW

// SORTING EXITS BASED ON CLOSEST TO FURTHEST from given location
bool ADungeonGenerator_GridBased::GetSortedExitArrows(ACityGen_RoomBase* FromRoom, const FSG_GridCoordinate& ToLocationGS, TArray<FExitArrowData*>& OutSortedExitData) const
{
	TMap<float, FExitArrowData*> DistanceMap; // Use int32 for distance keys

	if (!FromRoom)
	{
		UE_LOG(LogCityGen, Warning, TEXT("GetSortedExitArrows: Null room pointers"));
		return false;
	}

	// Loop over all exit points on FromRoom
	TArray<FExitArrowData>& FromRoomExitPoints = FromRoom->GetCachedExitPointsDataRef();
	for (FExitArrowData& ExitData : FromRoomExitPoints)
	{
		// Calculate local grid distance directly using GridCoords (no world conversions)
		float Distance = GetDistance(ExitData.DungeonGridCoord.position, ToLocationGS);

		// Use Distance as the key for sorting
		// To handle potential duplicates, add a small offset (like index) or use TMultiMap if necessary
		while (DistanceMap.Contains(Distance))
		{
			Distance++;
		}

		DistanceMap.Add(Distance, &ExitData);
	}

	// Sort by distance ascending
	DistanceMap.KeySort(TLess<float>());

	// Copy to output array in sorted order
	for (auto& Pair : DistanceMap)
	{
		OutSortedExitData.Add(Pair.Value);
	}

	return true;
}
#endif // WITH_SORTED_EXIT_ARROW

#if WITH_EDITOR
void ADungeonGenerator_GridBased::SnapRoomsToGridEd()
{
	TArray<ACityGen_RoomBase*> Rooms = GetAllRoomsArray(); // Need to call this function as 'AllRooms' may not be set yet if we wll from editor button
	for (ACityGen_RoomBase* Room : Rooms)
	{
		check(Room);
		Room->SnapRoomToGrid(DungeonGridCmpt);
		Room->UpdateGridCoordCaches(DungeonGridCmpt);
	}
}
#endif // WITH_EDITOR

void ADungeonGenerator_GridBased::SnapRoomsToGrid()
{
	for (ACityGen_RoomBase* Room : AllRooms)
	{
		Room->SnapRoomToGrid(DungeonGridCmpt);
		Room->UpdateGridCoordCaches(DungeonGridCmpt);
	}
}

#if WITH_EDITOR
void ADungeonGenerator_GridBased::DebugDrawRoomsCachedData()
{
	TArray<ACityGen_RoomBase*> Rooms = GetAllRoomsArray(); // Need to call this function as 'AllRooms' may not be set yet if we wll from editor button
	for (ACityGen_RoomBase* Room : Rooms)
	{
		check(Room);
		Room->DebugDrawRoomCachedData(DungeonGridCmpt);
	}
}
#endif // WITH_EDITOR

#if 0
void ADungeonGenerator_GridBased::UpdateBlockedTiles_Obstacles()
{
	for (AActor* Obstacle : Obstacles)
	{
		if (Obstacle == nullptr)
		{
			continue;
		}

		ACityGen_ObstacleBase* ObstacleBase = Cast<ACityGen_ObstacleBase>(Obstacle);
		if (!ObstacleBase)
		{
			continue;
		}

		// Get world grid coords directly
		TArray<FSG_GridCoordinate> WorldAffectedTiles =
			ObstacleBase->GetWorldTilesOverlappingBounds(ObstacleBase->GetObstacleBounds(), 1.0f);

		for (const FSG_GridCoordinate& Tile : WorldAffectedTiles)
		{
			BlockedGridTiles.Add(Tile); // Already in world coords
		}
	}
}
#endif

void ADungeonGenerator_GridBased::UpdateBlockedTiles_ClosedExits()
{
	if(bUseBlockedExit == false)
	{
		return;
	}

	for (ACityGen_RoomBase* Room : AllRooms)
	{
		const TArray<FExitArrowData>& CachedBlockedExitPoints = Room->GetCachedBlockedExitPointsData();
		for (const FExitArrowData& BlockedExitData : CachedBlockedExitPoints)
		{
			BlockedGridTiles.Add(BlockedExitData.DungeonGridCoord.position);
		}
	}
}

void ADungeonGenerator_GridBased::UpdateBlockedTiles_RoomBounds()
{
	for (ACityGen_RoomBase* RoomBase : AllRooms)
	{
		if (RoomBase == nullptr)
		{
			continue;
		}

		TArray<FSG_GridCoordinate> AffectedTiles = RoomBase->BuildListOfOverlappingCoordsFromBounds();
		for(const auto& affectTile : AffectedTiles)
		{
			BlockedGridTiles.Add(affectTile);
		}
	}
}

void ADungeonGenerator_GridBased::UpdateBlockedTiles_TilesToIgnore()
{
	for (FSG_GridCoordinate Tiles : TilesToIgnore)
	{
		BlockedGridTiles.Remove(Tiles);
	}
}

// @param OutVector: Return location in world space of the first gridCoord of the path
// @return: false if failed to find a path within the recursive loop hard coded limit
bool ADungeonGenerator_GridBased::FindPath(const FSG_GridCoordinate& StartDoorGridCoords, const FSG_GridCoordinate& StartRoomGridCoords, const FSG_GridCoordinate& EndDoorGridCoords, const FSG_GridCoordinate& EndRoomGridCoords)
{
	TMap<FSG_GridCoordinate, FCityGen_NodeCoord> OpenNodesMap;
	TMap<FSG_GridCoordinate, FCityGen_NodeCoord> ClosedNodesMap;
	TArray<FSG_GridCoordinate> OpenSet;
	TArray<FSG_GridCoordinate> ClosedSet;

	FCityGen_NodeCoord StartNode;

	StartNode.GCost = 0;
	StartNode.HCost = 0;
	StartNode.NodeCoordinate = StartDoorGridCoords;
	StartNode.bHaveParent = false;
	StartNode.ParentCoordinate = StartDoorGridCoords;

	FCityGen_NodeCoord EndNode;

	EndNode.NodeCoordinate = EndDoorGridCoords;
	EndNode.bHaveParent = false;
	EndNode.ParentCoordinate = EndDoorGridCoords;

	OpenSet.Add(StartDoorGridCoords); // Add Start node to Open set
	OpenNodesMap.Add(StartDoorGridCoords, StartNode);

	int32 numIterations = 0;
	const int32 maxIterations = 800; // To avoid infinite loop in case of setting mistake
	while (OpenSet.Num() && numIterations <= maxIterations)
	{
		numIterations = numIterations + 1;
		if (numIterations > maxIterations)
		{
			UE_LOG(LogCityGen, Warning, TEXT("MAX ITERATIONS REACHED"));
			return false;
		}
		
		FSG_GridCoordinate CurrentCoords = OpenSet[0];
		FCityGen_NodeCoord CurrentNode = OpenNodesMap[CurrentCoords];

		for (int32 i = 0; i < OpenSet.Num(); i++) // Find node with lowest fCost in Open set
		{
			// check distance logic
			if (OpenNodesMap[OpenSet[i]].ComputeFCost() <= OpenNodesMap[CurrentCoords].ComputeFCost())
			{
				CurrentCoords = OpenSet[i];
				CurrentNode = OpenNodesMap[CurrentCoords];
			}
		}

		// Move node from Open set to Closed set
		OpenSet.Remove(CurrentCoords);
		OpenNodesMap.Remove(CurrentCoords);
		ClosedSet.Add(CurrentCoords);
		ClosedNodesMap.Add(CurrentCoords, CurrentNode);

		#if 0
			// DEBUG
			UE_LOG(LogCityGen, Log, TEXT("OpenSet Size: %d"), OpenSet.Num());
			
			UKismetSystemLibrary::DrawDebugBox(
				this,
				DungeonGridCmpt->GridToWorld(StartDoorGridCoords) + TileSize / 2.0,
				TileSize / 2.0,
				FLinearColor::Red,
				FRotator::ZeroRotator,
				10.1f
			);

			UKismetSystemLibrary::DrawDebugBox(
				this,
				DungeonGridCmpt->GridToWorld(EndDoorGridCoords) + TileSize / 2.0,
				TileSize / 2.0,
				FLinearColor::Yellow,
				FRotator::ZeroRotator,
				10.1f
			);
		#endif

		if (CurrentCoords == EndDoorGridCoords)
		{
			// We need to add the room location in order that the corridors spawning take them in account
			RequestedCorridors.FindOrAdd(StartDoorGridCoords).MakeConnection(StartDoorGridCoords, StartRoomGridCoords, true);
			RequestedCorridors.FindOrAdd(EndDoorGridCoords).MakeConnection(EndDoorGridCoords, EndRoomGridCoords, true);

			UE_LOG(LogCityGen, Log, TEXT("PATH FOUND"));
			RetracePath(StartDoorGridCoords, EndDoorGridCoords, ClosedNodesMap);
			return true;
		}

		// For each neighbor of current node:
		TArray<FSG_GridCoordinate> Neighbours = DungeonGridCmpt->GetNeighbourNodes3D(CurrentCoords);

		for (int32 i = 0; i < Neighbours.Num(); i++)
		{
			// Adding nodes
			const FSG_GridCoordinate& CurrentNeighbourCoordinate = Neighbours[i];

			if (IsGridTileBlocked(CurrentNeighbourCoordinate))
			{
				continue;
			}

			float MovementCost = GetDistance(CurrentCoords, Neighbours[i]);

			// check if this is already used in a previous path, and lower the cost if it is 
			if (RequestedCorridors.Contains(CurrentNeighbourCoordinate))
			{
				const float MovementReductionFactorForCellWithCorridor = 0.5f;
				MovementCost *= MovementReductionFactorForCellWithCorridor;
			}

			float CurrentNeighbour_NewGCost = CurrentNode.GCost + MovementCost;
			float CurrentNeighbour_NewHCost = GetDistance(Neighbours[i], EndDoorGridCoords);
			float CurrentNeighbour_NewFCost = CurrentNeighbour_NewHCost + CurrentNeighbour_NewGCost;

			if (ClosedNodesMap.Contains(CurrentNeighbourCoordinate))
			{
				const FCityGen_NodeCoord& AlreadyVisitedNode = ClosedNodesMap[CurrentNeighbourCoordinate];
				if (AlreadyVisitedNode.ComputeFCost() < CurrentNeighbour_NewFCost)
				{
					continue;
				}
			}

			if (OpenNodesMap.Contains(CurrentNeighbourCoordinate))
			{
				FCityGen_NodeCoord& AlreadyVisitedNode = OpenNodesMap[CurrentNeighbourCoordinate];
				if (AlreadyVisitedNode.ComputeFCost() < CurrentNeighbour_NewFCost)
				{
					continue;
				}
				
				AlreadyVisitedNode.GCost = CurrentNeighbour_NewGCost;
				AlreadyVisitedNode.HCost = CurrentNeighbour_NewHCost;
				AlreadyVisitedNode.bHaveParent = true;
				AlreadyVisitedNode.ParentCoordinate = CurrentCoords;
			}
			else
			{
				FCityGen_NodeCoord NewNeighbourNode;
				NewNeighbourNode.GCost = CurrentNeighbour_NewGCost;
				NewNeighbourNode.HCost = CurrentNeighbour_NewHCost;
				NewNeighbourNode.bHaveParent = true;
				NewNeighbourNode.ParentCoordinate = CurrentCoords;
				NewNeighbourNode.NodeCoordinate = CurrentNeighbourCoordinate;

				OpenSet.Add(CurrentNeighbourCoordinate);
				OpenNodesMap.Add(CurrentNeighbourCoordinate, NewNeighbourNode);
			}
		}
	}

	UE_LOG(LogCityGen, Warning, TEXT("No path found after %d iterations"), numIterations);
	return false;
}

// @return: location in world space of the first gridCoord of the path
void ADungeonGenerator_GridBased::RetracePath(
	const FSG_GridCoordinate& StartGridCoords,
	const FSG_GridCoordinate& EndGridCoords,
	const TMap<FSG_GridCoordinate, FCityGen_NodeCoord>& NodesMap) 
{
	if (StartGridCoords == EndGridCoords)
	{
		return;
	}

	const FCityGen_NodeCoord* CurrentNode = &(NodesMap[EndGridCoords]);
	const FCityGen_NodeCoord& StartNode = NodesMap[StartGridCoords];

	while(CurrentNode->bHaveParent)
	{
		const FSG_GridCoordinate& CurrentNodeCoord = CurrentNode->GetNodeCoordinate();
		const FSG_GridCoordinate& ParentNodeCoord = CurrentNode->GetParentNodeCoordinate();
		RequestedCorridors.FindOrAdd(CurrentNodeCoord).MakeConnection(CurrentNodeCoord, ParentNodeCoord, false);
		RequestedCorridors.FindOrAdd(ParentNodeCoord).MakeConnection(ParentNodeCoord, CurrentNodeCoord, false);

#if 0
		UE_LOG(LogCityGen, Log, TEXT("Path Grid Coord X: %d  Y: %d   Z: %d"),
			CurrentNodeCoord.X, CurrentNodeCoord.Y, CurrentNodeCoord.Z);

		UKismetSystemLibrary::DrawDebugBox(
			this,
			DungeonGridCmpt->GridToWorld(CurrentNodeCoord) + TileSize / 2.0,
			TileSize / 2.0,
			FLinearColor::Green,
			FRotator::ZeroRotator,
			10.1f
		);
#endif
		CurrentNode = &(NodesMap[CurrentNode->GetParentNodeCoordinate()]);
	}
}

// CORRIDOR TYPE SPAWNING
void ADungeonGenerator_GridBased::SpawnCorridors()
{
	TSet<FSG_GridCoordinate> ProcessedCoords;

	TArray<FSG_GridCoordinate> CorridorCoords;
	RequestedCorridors.GetKeys(CorridorCoords);
	for (int32 i = 0; i < CorridorCoords.Num(); ++i)
	{
		const FSG_GridCoordinate& CurrentCoord = CorridorCoords[i];
		const FCellConnectionState& CurrentCoordConnection = RequestedCorridors[CurrentCoord];

		// Here we should not process coord twice
		check(!ProcessedCoords.Contains(CurrentCoord));
		ProcessedCoords.Add(CurrentCoord);

		int32 UpCount = CurrentCoordConnection.bConnectUp ? 1 : 0;
		int32 DownCount = CurrentCoordConnection.bConnectDown ? 1 : 0;
		int32 HorizontalCount = CurrentCoordConnection.GetHorizontalConnectionCount();
		UE_LOG(LogCityGen, Log, TEXT("HorizontalCount: %d"), HorizontalCount);
		TArray<bool> bHasDirections = CurrentCoordConnection.GetDirectionArray();

		FVector LocationWS = DungeonGridCmpt->GridToWorld(CurrentCoord) + FVector(TileSize.X, TileSize.Y, 0) / 2.0;

		if (DownCount > 0 && UpCount == 0)
		{
			ACityGen_RoomBase* pCorridor = SpawnDownLvlCorridor(CurrentCoord, LocationWS);
			if (pCorridor != nullptr)
			{
				AllSpawnedCorridors.Add(CurrentCoord, pCorridor);
			}
			continue;
		}

		if (UpCount > 0 && DownCount == 0)
		{ 
			ACityGen_RoomBase* pCorridor = SpawnUpLvlCorridor(CurrentCoord, LocationWS);
			if (pCorridor != nullptr)
			{
				AllSpawnedCorridors.Add(CurrentCoord, pCorridor);
			}
			continue;
		}

		if (UpCount > 0 && DownCount > 0)
		{
			ACityGen_RoomBase* pCorridor = SpawnUpAndDownLvlCorridor(CurrentCoord, LocationWS);
			if (pCorridor != nullptr)
			{
				AllSpawnedCorridors.Add(CurrentCoord, pCorridor);
			}
			continue;
		}

		if (HorizontalCount == 3)
		{
			ACityGen_RoomBase* pCorridor = SpawnTCorridor(CurrentCoord, LocationWS, bHasDirections);
			if (pCorridor != nullptr)
			{
				AllSpawnedCorridors.Add(CurrentCoord, pCorridor);
			}
			continue;
		}

		if (HorizontalCount == 4)
		{
			ACityGen_RoomBase* pCorridor = SpawnCrossCorridor(CurrentCoord, LocationWS);
			if (pCorridor != nullptr)
			{
				AllSpawnedCorridors.Add(CurrentCoord, pCorridor);
			}
			continue;
		}

		if (HorizontalCount == 2)
		{
			ACityGen_RoomBase* pCorridor = SpawnStraightCorridor(CurrentCoord, LocationWS, bHasDirections); //can spawn straight or corner, depending on neighbour positions
			if (pCorridor != nullptr)
			{
				AllSpawnedCorridors.Add(CurrentCoord, pCorridor);
			}
			continue;
		}
	}
	
#if WITH_EDITOR
	CheckNoOverlappingRooms(AllSpawnedCorridors);
#endif // WITH_EDITOR
}

ACityGen_RoomBase* ADungeonGenerator_GridBased::SpawnDownLvlCorridor(const FSG_GridCoordinate& CurrentCoord, const FVector& Location) const
{
	if (ElevatorCorridorRoom_Down == nullptr)
	{
		UE_LOG(LogCityGen, Warning, TEXT("ElevatorCorridorRoom_Down is nullptr"));
		return nullptr;
	}

	FVector LocationWSLvl2 = DungeonGridCmpt->GridToWorld(CurrentCoord) + FVector(TileSize.X, TileSize.Y, 0) / 2.0;
	ACityGen_RoomBase* SpawnElevatorCorridorRoom = GetWorld()->SpawnActor<ACityGen_RoomBase>(ElevatorCorridorRoom_Down, Location, FRotator::ZeroRotator);
	if (SpawnElevatorCorridorRoom == nullptr)
	{
		UE_LOG(LogCityGen, Warning, TEXT("Failed to spawn ElevatorCorridorRoom_Down actor"));
		return nullptr;
	}

	return SpawnElevatorCorridorRoom;
}

ACityGen_RoomBase* ADungeonGenerator_GridBased::SpawnUpLvlCorridor(const FSG_GridCoordinate& CurrentCoord, const FVector& Location) const
{
	if (ElevatorCorridorRoom_Up == nullptr)
	{
		UE_LOG(LogCityGen, Warning, TEXT("ElevatorCorridorRoom_Up is nullptr"));
		return nullptr;
	}

	FVector LocationWSLvl2 = DungeonGridCmpt->GridToWorld(CurrentCoord) + FVector(TileSize.X, TileSize.Y, 0) / 2.0;
	ACityGen_RoomBase* SpawnElevatorCorridorRoom = GetWorld()->SpawnActor<ACityGen_RoomBase>(ElevatorCorridorRoom_Up, Location, FRotator::ZeroRotator);
	if (SpawnElevatorCorridorRoom == nullptr)
	{
		UE_LOG(LogCityGen, Warning, TEXT("Failed to spawn ElevatorCorridorRoom_Up actor"));
		return nullptr;
	}

	return SpawnElevatorCorridorRoom;
}

ACityGen_RoomBase* ADungeonGenerator_GridBased::SpawnUpAndDownLvlCorridor(const FSG_GridCoordinate& CurrentCoord, const FVector& Location) const
{
	if (ElevatorCorridorRoom_UpAndDown == nullptr)
	{
		UE_LOG(LogCityGen, Warning, TEXT("ElevatorCorridorRoom_UpAndDown is nullptr"));
		return nullptr;
	}

	FVector LocationWSLvl2 = DungeonGridCmpt->GridToWorld(CurrentCoord) + FVector(TileSize.X, TileSize.Y, 0) / 2.0;
	ACityGen_RoomBase* SpawnElevatorCorridorRoom = GetWorld()->SpawnActor<ACityGen_RoomBase>(ElevatorCorridorRoom_UpAndDown, Location, FRotator::ZeroRotator);
	if (SpawnElevatorCorridorRoom == nullptr)
	{
		UE_LOG(LogCityGen, Warning, TEXT("Failed to spawn ElevatorCorridorRoom_UpAndDown actor"));
		return nullptr;
	}

	return SpawnElevatorCorridorRoom;
}

ACityGen_RoomBase* ADungeonGenerator_GridBased::SpawnTCorridor(const FSG_GridCoordinate& CurrentCoord, const FVector& Location, const TArray<bool>& bDirections) const
{
	if (TJunctionCorridorRoom == nullptr)
	{
		UE_LOG(LogCityGen, Warning, TEXT("TJunctionCorridorRoom is nullptr"));
		return nullptr;
	}

	FRotator TjunctionRotation = FRotator::ZeroRotator;

	if (!bDirections[0]) //NORTH
	{
		TjunctionRotation = FRotator(0.f, 90.f, 0.f);
	}
	else if (!bDirections[1]) //EAST
	{
		TjunctionRotation = FRotator(0.f, 180.f, 0.f);
	}
	else if (!bDirections[2]) //SOUTH
	{
		TjunctionRotation = FRotator(0.f, -90.0f, 0.f);
	}
	else if (!bDirections[3]) //WEST
	{
		TjunctionRotation = FRotator(0.f, 0.f, 0.f);
	}
	else
	{
		check(false); // Should never happens
	}

	ACityGen_RoomBase* SpawnTCorridorRoom = GetWorld()->SpawnActor<ACityGen_RoomBase>(TJunctionCorridorRoom, Location, TjunctionRotation);
	return SpawnTCorridorRoom;
}

ACityGen_RoomBase* ADungeonGenerator_GridBased::SpawnCrossCorridor(const FSG_GridCoordinate& CurrentCoord, const FVector& Location) const
{
	if (CrossJunctionCorridorRoom == nullptr)
	{
		UE_LOG(LogCityGen, Warning, TEXT("CrossJunctionCorridorRoom is nullptr"));
		return nullptr;
	}

	// Random rotation in 90° increments
	int32 RandomIndex = DungeonGenRandomStream.RandRange(0, 3); // 0, 1, 2, or 3
	float Yaw = RandomIndex * 90.f;

	FRotator RandomRotation(0.f, Yaw, 0.f);

	ACityGen_RoomBase* SpawnCrossCorridorRoom = GetWorld()->SpawnActor<ACityGen_RoomBase>(CrossJunctionCorridorRoom, Location, RandomRotation);
	return SpawnCrossCorridorRoom;
}

ACityGen_RoomBase* ADungeonGenerator_GridBased::SpawnStraightCorridor(const FSG_GridCoordinate& CurrentCoord, const FVector& Location, const TArray<bool>& bDirections) const
{
	TSubclassOf<ACityGen_RoomBase> RoomToSpawn = nullptr;
	FRotator RoomRotation = FRotator::ZeroRotator;
	bool bNorth = bDirections[0];
	bool bEast = bDirections[1];
	bool bSouth = bDirections[2];
	bool bWest = bDirections[3];

	// Check for straight corridor
	if ((bNorth && bSouth) || (bEast && bWest))
	{
		if (CorridorRoom == nullptr)
		{
			UE_LOG(LogCityGen, Warning, TEXT("CorridorRoom is nullptr"));
			return nullptr;
		}
		
		RoomToSpawn = CorridorRoom;
		if (bNorth && bSouth)
		{
			float Yaw = FMath::RandBool() ? 0.f : 180.f;
			RoomRotation = FRotator(0.f, Yaw, 0.f); // North/south 
		}
		else
		{
			float Yaw = FMath::RandBool() ? 90.f : -90.f;
			RoomRotation = FRotator(0.f, Yaw, 0.f); // East/west 
		}
	}
	// Check for adjacent (corner)
	else
	{
		if (CorridorCornerRoom == nullptr)
		{
			UE_LOG(LogCityGen, Warning, TEXT("CorridorCornerRoom is nullptr"));
			return nullptr;
		}

		RoomToSpawn = CorridorCornerRoom;

		if (bNorth && bEast)
		{
			RoomRotation = FRotator(0.f, 0.f, 0.f);
		}
		else if (bSouth && bEast)
		{
			RoomRotation = FRotator(0.f, 90.f, 0.f);
		}
		else if (bSouth && bWest)
		{
			RoomRotation = FRotator(0.f, 180.f, 0.f);
		}
		else if (bNorth && bWest)
		{
			RoomRotation = FRotator(0.f, -90.f, 0.f);
		}
		else
		{
			check(false); // Should never happens
		}
	}

	ACityGen_RoomBase* SpawnCorridorRoom = GetWorld()->SpawnActor<ACityGen_RoomBase>(RoomToSpawn, Location, RoomRotation);
	return SpawnCorridorRoom;
}

void ADungeonGenerator_GridBased::UpdateDoorStatus()
{
	TSet<FSG_GridCoordinate> ProcessedCoords;

	TArray<FSG_GridCoordinate> CorridorCoords;
	RequestedCorridors.GetKeys(CorridorCoords);
	for (int32 i = 0; i < CorridorCoords.Num(); ++i)
	{
		const FSG_GridCoordinate& CurrentCoord = CorridorCoords[i];
		const FCellConnectionState& CurrentCoordConnection = RequestedCorridors[CurrentCoord];

		// 1) We only have to check the corridor, as the room exit state are set when the path are found
		// 2) We only have to check for elevator/stairs, as other case are manage by the type of corridor
		if(!CurrentCoordConnection.bConnectUp && !CurrentCoordConnection.bConnectDown)
		{
			continue;
		}

		ACityGen_RoomBase** pCorridorActorPtr = AllSpawnedCorridors.Find(CurrentCoord);
		if(pCorridorActorPtr != nullptr)
		{
			ACityGen_RoomBase* pCorridorActor = *pCorridorActorPtr;
			//pCorridorActor->UpdateExitPoint(DungeonGridCmpt); // For corridor we only need exit point local
			pCorridorActor->SetExitsUsed(CurrentCoordConnection);
			continue;
		}
	}

	OpenUsedExits();
}

#if WITH_EDITOR
// Check that there is no corridors overlapping rooms with an offset of half a cell
// This is an old function that probably need to be re work in order to be relevant
// We will not cleanup that function now as we run out of time
void ADungeonGenerator_GridBased::CheckNoOverlappingRooms(const TMap<FSG_GridCoordinate, ACityGen_RoomBase*>& AllCorridorActors)
{
	for(const auto& Corridor : AllCorridorActors)
	{
		ACityGen_RoomBase* CorridorMeshRoom = Corridor.Value;
		check (CorridorRoom != nullptr);

		FBox CorridorBounds = CorridorMeshRoom->GetComponentsBoundingBox().ExpandBy(FVector(-(TileSize.X)/2, -(TileSize.Y) / 2, 0.f));

		for (int32 j = 0; j < AllRooms.Num(); ++j)
		{
			ACityGen_RoomBase* MainRoom = AllRooms[j];
			if ((MainRoom == nullptr) || (CorridorMeshRoom == MainRoom))
			{
				continue;
			}
			FBox RoomBounds = MainRoom->GetComponentsBoundingBox().ExpandBy(FVector(FVector(-(TileSize.X) / 2, -(TileSize.Y) / 2, 0.f)));

			if (CorridorBounds.Intersect(RoomBounds))
			{
				DrawDebugBox(GetWorld(), RoomBounds.GetCenter(), RoomBounds.GetExtent(), FColor::Red, false, 10.0f);
				DrawDebugBox(GetWorld(), CorridorBounds.GetCenter(), CorridorBounds.GetExtent(), FColor::Blue, false, 10.0f);
				break; // No need to keep checking this corridor
			}
		}
	}
}
#endif // WITH_EDITOR

// Why this function is overlap based:
// - The corridor may connect to a door that was not the inital chosen door
// - To reach a tile, the algorithim doesn't know from which direction to go, and may cross into rooms to get there. In that case, we want the door to be open.
void ADungeonGenerator_GridBased::OpenUsedExits()
{
	// Gather all arrow boxes in the level to compare collisions later
	TArray<FBox> AllArrowBoxes;
	TArray<UArrowComponent*> AllArrowComponents;
	for (const auto& Corridor : AllSpawnedCorridors)
	{
		check (Corridor.Value != nullptr);

		Corridor.Value->OpenUsedExits();
	}
	for (ACityGen_RoomBase* Room : AllRooms)
	{
		Room->OpenUsedExits();
	}
}

float ADungeonGenerator_GridBased::GetDistance(const FSG_GridCoordinate& A, const FSG_GridCoordinate& B) const
{
	int32 DX = FMath::Abs(A.X - B.X);
	int32 DY = FMath::Abs(A.Y - B.Y);
	int32 DZ = FMath::Abs(A.Z - B.Z);

	// Using Manhattan distance for grid-based pathfinding
	return DX + DY + (DZ * DistanceFactorForZ);
}

bool ADungeonGenerator_GridBased::IsGridTileBlocked(const FSG_GridCoordinate& GridCoord) const
{
	return BlockedGridTiles.Contains(GridCoord);
}
