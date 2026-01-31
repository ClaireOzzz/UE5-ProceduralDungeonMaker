// Copyright Chateau Pageot, Inc. All Rights Reserved.

#pragma once

#include "CityGen_NodeCoordinate.h"
#include "CityGen_ObstacleBase.h"
#include "GridBasedGeneratorBase.h"

#include "DungeonGenerator_GridBased.generated.h"

#define WITH_SORTED_EXIT_ARROW 0 // TODO : remove dead code

class UArrowComponent;
struct FExitArrowData;

// Do not use this class directly, use one of the sub classes
UCLASS()
class PROCEDURALCITYGENERATOR_API ADungeonGenerator_GridBased : public AGridBasedGeneratorBase
{
	GENERATED_BODY()

public:
	// Set a factor above 1 to limit Z corridor
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid Snapping")
	float DistanceFactorForZ = 5.0f;  // set to a high number to discourage unnecessary descents

	// Random seed
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	int32 RandomSeed = 12345;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Generation")
	bool bUseBlockedExit = true;

protected:
	UPROPERTY(VisibleAnywhere)
	TMap<FSG_GridCoordinate, ACityGen_RoomBase*> AllSpawnedCorridors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid");
	TObjectPtr<USG_GridComponent> DungeonGridCmpt;

	TArray<ACityGen_RoomBase*> AllRooms;

	TMap<FSG_GridCoordinate, FCellConnectionState> RequestedCorridors;

	TSet<FSG_GridCoordinate> BlockedGridTiles;

	TArray<FSG_GridCoordinate> TilesToIgnore;

	FRandomStream DungeonGenRandomStream;

public:
	// Sets default values for this actor's properties
	ADungeonGenerator_GridBased();

	UFUNCTION(CallInEditor)
	void ClearCorridorMeshes();

#if WITH_EDITOR
	// Function only here to be able to use in editor, in code directly use ConnectRoomsInOrder
	UFUNCTION(CallInEditor)
	void SpawnCorridorPath();

	UFUNCTION(CallInEditor)
	void DebugDrawConnectedPath();
#endif // WITH_EDITOR

	// This return an array without nullptr actors
	// The pair does not contains same actor
	virtual TArray<TPair<ACityGen_RoomBase*, ACityGen_RoomBase* >> GetRoomsToConnectArray() const;

	// This return an array without nullptr actors
	virtual TArray<ACityGen_RoomBase*> GetAllRoomsArray() const;

	virtual void InitFromSpawnedRooms(const TArray<ACityGen_RoomBase*>& SpawnedRooms);

	UFUNCTION(CallInEditor)
	bool ConnectRoomsInOrder();

#if WITH_EDITOR
	UFUNCTION(CallInEditor)
	void SnapRoomsToGridEd();

	UFUNCTION(CallInEditor)
	void DebugDrawRoomsCachedData();
#endif // WITH_EDITOR

protected:
	//void UpdateBlockedTiles_Obstacles();
	void UpdateBlockedTiles_ClosedExits();
	void UpdateBlockedTiles_RoomBounds();
	void UpdateBlockedTiles_TilesToIgnore();

	bool AddCorridorConnectingRooms(ACityGen_RoomBase* FromRoom, ACityGen_RoomBase* ToRoom);
#if WITH_SORTED_EXIT_ARROW
	// SORTING EXITS BASED ON CLOSEST TO FURTHEST from given location
	bool GetSortedExitArrows(ACityGen_RoomBase* FromRoom, const FSG_GridCoordinate& ToLocationGS, TArray<FExitArrowData*>& OutSortedExitData) const;
#endif // WITH_SORTED_EXIT_ARROW

	void SnapRoomsToGrid();

	// PATHFINDING

	// @param OutVector: Return location in world space of the first gridCoord of the path
	// @return: false if failed to find a path within the recursive loop hard coded limit
	bool FindPath(const FSG_GridCoordinate& StartDoorGridCoords, const FSG_GridCoordinate& StartRoomGridCoords, const FSG_GridCoordinate& EndDoorGridCoords, const FSG_GridCoordinate& EndRoomGridCoords);

	// @return: location in world space of the first gridCoord of the path
	void RetracePath(
		const FSG_GridCoordinate& StartGridCoord,
		const FSG_GridCoordinate& EndGridCoord,
		const TMap<FSG_GridCoordinate, FCityGen_NodeCoord>& NodesMap);

	// CORRIDOR SPAWNING

	void SpawnCorridors();

	ACityGen_RoomBase* SpawnDownLvlCorridor(const FSG_GridCoordinate& CurrentCoord, const FVector& Location) const;
	ACityGen_RoomBase* SpawnUpLvlCorridor(const FSG_GridCoordinate& CurrentCoord, const FVector& Location) const;
	ACityGen_RoomBase* SpawnUpAndDownLvlCorridor(const FSG_GridCoordinate& CurrentCoord, const FVector& Location) const;
	ACityGen_RoomBase* SpawnTCorridor(const FSG_GridCoordinate& CurrentCoord, const FVector& Location, const TArray<bool>& bDirections) const;
	ACityGen_RoomBase* SpawnCrossCorridor(const FSG_GridCoordinate& CurrentCoord, const FVector& Location) const;
	ACityGen_RoomBase* SpawnStraightCorridor(const FSG_GridCoordinate& CurrentCoord, const FVector& Location, const TArray<bool>& bDirections) const;

	void UpdateDoorStatus();

#if WITH_EDITOR
	// Check that there is no corridors overlapping rooms with an offset of half a cell
	// This is an old function that probably need to be re work in order to be relevant
	// We will not cleanup that function now as we run out of time
	void CheckNoOverlappingRooms(const TMap<FSG_GridCoordinate, ACityGen_RoomBase*>& AllCorridorActors);
#endif // WITH_EDITOR

	void OpenUsedExits();

	// HELPER FUNCTIONS

	bool IsGridTileBlocked(const FSG_GridCoordinate& GridCoord) const;

	float GetDistance(const FSG_GridCoordinate& A, const FSG_GridCoordinate& B) const;
};

