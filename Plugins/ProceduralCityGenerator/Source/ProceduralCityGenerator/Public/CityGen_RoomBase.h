// Copyright Chateau Pageot, Inc. All Rights Reserved.

#pragma once

#include "CellConnectionState.h"

#include "SimpleGridRuntime/Public/SG_GridComponentWithSize.h"
#include "SimpleGridRuntime/Public/SG_GridCoordinateFloatWithRotation.h"
#include "SimpleGridRuntime/Public/SG_GridCoordinateWithRotation.h"

#include "GameFramework/Actor.h"

#include "CityGen_RoomBase.generated.h"

class UBoxComponent;
class UArrowComponent;
class USG_GridComponentWithSize;
class EditorUtils;

USTRUCT(BlueprintType)
struct FBoundCoords
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FSG_GridCoordinateFloat Min;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FSG_GridCoordinateFloat Max;
public:
	FBoundCoords() = default;

	FBoundCoords(const FSG_GridCoordinateFloat& min, const FSG_GridCoordinateFloat& max)
		: Min(min)
		, Max(max)
	{
	}
};

USTRUCT(BlueprintType)
struct FExitArrowData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UArrowComponent* ArrowCmpt;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FSG_GridCoordinateFloatWithRotation LocalGridCoord; // Exit coord, grid offset in actor space
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FSG_GridCoordinateWithRotation DungeonGridCoord; // Exit coord, In Dungeon space

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FSG_GridCoordinate DungeonDoorGridCoord; // Cell with door coord, In Dungeon space

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsUsed = false; // If true, this exit is connected to a corridor

	bool operator==(const FExitArrowData& Other) const // to check if two exit arrows are the same
	{
		return ArrowCmpt == Other.ArrowCmpt;
	}
};

// Room should be placed in level with Pitch=0 and Roll=0
UCLASS(BlueprintType)
class PROCEDURALCITYGENERATOR_API ACityGen_RoomBase : public AActor
{
GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exits")
	USceneComponent* ExitPointsFolder = nullptr; // Add Arrow components as child of this component to mark 'exit points'

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* OverlapFolder = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* BlockedExitFolder = nullptr; // All unavailable spots which are not exits

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* GeometryFolder = nullptr;

	// Will consider all Box components under this scene component as bounds to block the grid
	// This component and the box components Should not be rotated at all
	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* BoundsBoxFolder = nullptr;

protected:
	// First updated at OnConstruction
	// Then dungeon coord is updated at SnapRoomToGridAndUpdateCaches
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cached Data")
	TArray<FExitArrowData> CachedExitPointsData;

	// First updated at OnConstruction
	// Then dungeon coord is updated at SnapRoomToGridAndUpdateCaches
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cached Data")
	TArray<FExitArrowData> CachedBlockedExitPointsData;

	// Updated at SnapRoomToGridAndUpdateCaches
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cached Data")
	FSG_GridCoordinateWithRotation CachedRoomGridCoord;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<FBoundCoords> CachedBoundsLocalGridCoord;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<FBoundCoords> CachedBoundsDungeonGridCoord;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cached Data")
	FVector TileSize = FVector(500, 500, 250);

	float BoundSafeZonePercent = 0.1f; // Percent of grid Cell remove from bounds to compute list of blocked cell

public:
	ACityGen_RoomBase();

	const TArray<FExitArrowData>& GetCachedExitPointsData() const
	{
		return CachedExitPointsData;
	}

	TArray<FExitArrowData>& GetCachedExitPointsDataRef()
	{
		return CachedExitPointsData;
	}

	const TArray<FExitArrowData>& GetCachedBlockedExitPointsData() const
	{
		return CachedBlockedExitPointsData;
	}

	// WARNING: Only valid after call SnapRoomToGrid
	// Space is linked to the USG_GridComponent used when calling SnapRoomToGrid
	const FSG_GridCoordinateWithRotation& GetRoomGridCoord() const
	{
		return CachedRoomGridCoord;
	}

	virtual void OnConstruction(const FTransform& Transform) override;

	void SnapRoomToGrid(USG_GridComponent* GridComponent);

	// Should be called after SnapRoomToGrid, as it rely on CachedRoomGridCoord
	// Convert cached Local grid coord, to dungeon grid coord
	void UpdateGridCoordCaches(USG_GridComponent* GridComponent);

	// Should be called after UpdateGridCoordCaches
	TArray<FSG_GridCoordinate> BuildListOfOverlappingCoordsFromBounds() const;

	void CloseAllDoors();

	void OpenUsedExits();

	void SetExitsUsed(const FCellConnectionState& state);

#if WITH_EDITOR
	void DebugDrawRoomCachedData(USG_GridComponent* GridComponent);
#endif // WITH_EDITOR

protected:
	void RefreshCachedLocalGridCoord();

private:
	void SetExitState(bool bUsed, FExitArrowData& InOutExitPoint);

	// Return local bounds
	TArray<FBox> GetRoomBounds() const;

	// Return local size
	FVector GetRoomGlobalSize() const;

	// Return the offset of the actor location compare to the grid coord cell center
	// @rotation: normalize rotation
	FVector GetRoomCenterOffset(int32 rotation) const;

	// Return the offset of the actor location compare to the grid coord (local)
	FSG_GridCoordinateFloat GetRoomCenterOffsetGridFloat() const;

	// Return the offset of the actor location compare to the grid coord cell center
	// have accurate snapping to the grid
	// @rotation: normalize rotation
	FVector GetRoomCenterRoundingOffset(int32 rotation) const;

	FSG_GridCoordinateFloat GetRoomCenterRoundingOffsetGridFloat() const;

	// Return the offset to apply to the Arrow location in order to 
	// have accurate snapping to the grid
	//FVector GetArrowCenterRoundingOffset() const;
	void CacheAllArrowSubComponentToArray(USceneComponent* Folder, TArray<FExitArrowData>& OutTargetArray);

#if WITH_EDITOR
	void CheckForArrowAtSameGridLocation(TArray<FExitArrowData>& OutArray) const;
#endif // WITH_EDITOR

	void CacheBoundsLocalCoord();
};
