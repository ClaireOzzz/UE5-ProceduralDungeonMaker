// Copyright Chateau Pageot, Inc. All Rights Reserved.

#include "CityGen_RoomBase.h"
#include "CityGen_LogChannels.h"

#include "SimpleGridRuntime/Public/SG_GridComponent.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"

ACityGen_RoomBase::ACityGen_RoomBase()
{
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	//OffsetForGridAlignement = CreateDefaultSubobject<USceneComponent>(TEXT("GridAlignementCmpt"));
	GeometryFolder = CreateDefaultSubobject<USceneComponent>(TEXT("GeometryFolder"));
	OverlapFolder = CreateDefaultSubobject<USceneComponent>(TEXT("OverlapFolder"));
	ExitPointsFolder = CreateDefaultSubobject<USceneComponent>(TEXT("ExitPointsFolder"));
	BlockedExitFolder = CreateDefaultSubobject<USceneComponent>(TEXT("BlockedExitFolder"));
	BoundsBoxFolder = CreateDefaultSubobject<USceneComponent>(TEXT("BoundsBoxFolder"));

	//OffsetForGridAlignement->SetupAttachment(RootComponent);
	GeometryFolder->SetupAttachment(RootComponent);
	OverlapFolder->SetupAttachment(RootComponent);
	ExitPointsFolder->SetupAttachment(RootComponent);
	BlockedExitFolder->SetupAttachment(RootComponent);
	BoundsBoxFolder->SetupAttachment(RootComponent);
}


void ACityGen_RoomBase::OnConstruction(const FTransform& Transform) // getting all arrows on construction (wanted callineditor but that only works in instances?)
{
	Super::OnConstruction(Transform);

	RefreshCachedLocalGridCoord();
}

void ACityGen_RoomBase::RefreshCachedLocalGridCoord()
{
	// Clear old data for this instance
	CachedExitPointsData.Empty();
	CachedBlockedExitPointsData.Empty();

	// Cache local GridCoordinate for exits and bounds
	CacheAllArrowSubComponentToArray(ExitPointsFolder, CachedExitPointsData);
	CacheAllArrowSubComponentToArray(BlockedExitFolder, CachedBlockedExitPointsData);
	CacheBoundsLocalCoord();

#if 0 // Debug logging
	UE_LOG(LogCityGen, Log, TEXT("=== RefreshExitPoints() for %s ==="), *GetName());
	UE_LOG(LogCityGen, Log, TEXT("Exit Points: %d"), ExitPoints.Num());

	for (int32 i = 0; i < ExitPoints.Num(); i++)
	{
		const auto& Data = ExitPoints[i];
		UE_LOG(LogCityGen, Log, TEXT("[%d] Room: %s | Grid: (%d,%d,%d) | Rot: %s"),
			i,
			GetName(),
			Data.GridCoord.X,
			Data.GridCoord.Y,
			Data.GridCoord.Z,
			*Data.Rotation.ToCompactString());
	}

	UE_LOG(LogCityGen, Log, TEXT("Global Blocked Exit Points: %d"), BlockedExitPoints.Num());

	for (int32 i = 0; i < BlockedExitPoints.Num(); i++)
	{
		const auto& Data = BlockedExitPoints[i];
		UE_LOG(LogCityGen, Log, TEXT("[%d] Room: %s | Grid: (%d,%d,%d) | Rot: %s"),
			i,
			GetName(),
			Data.GridCoord.X,
			Data.GridCoord.Y,
			Data.GridCoord.Z,
			*Data.Rotation.ToCompactString());
	}
	UE_LOG(LogCityGen, Log, TEXT("================================"));
#endif
}

void ACityGen_RoomBase::CacheAllArrowSubComponentToArray(USceneComponent* Folder, TArray<FExitArrowData>& OutTargetArray)
{
	TArray<USceneComponent*> ArrowChildren;
	Folder->GetChildrenComponents(false, ArrowChildren);

#if WITH_EDITOR
	if ((Folder->GetRelativeLocation() != FVector(0, 0, 0)) ||
		(Folder->GetRelativeRotation() != FRotator(0, 0, 0)))
	{
		UE_LOG(LogCityGen, Warning, TEXT("Arrow folder should not have relative location or rotation %s"), *GetName());
	}
#endif // WITH_EDITOR

	int32 LocalRotation = 0;
	FVector RoomCenterOffset = GetRoomCenterOffset(LocalRotation);

	for (USceneComponent* ArrowChild : ArrowChildren)
	{
		if (UArrowComponent* Arrow = Cast<UArrowComponent>(ArrowChild))
		{
			// Skip arrows created or modified at the instance level
			if (Arrow->CreationMethod == EComponentCreationMethod::Instance)
			{
				UE_LOG(LogCityGen, Verbose, TEXT("Skipping instance arrow: %s"), *Arrow->GetName());
				continue;
			}

			const FVector ArrowRelativeLocation = Arrow->GetRelativeLocation();
			const FRotator ArrowRelativeRotation = Arrow->GetRelativeRotation();

			FExitArrowData Data;
			Data.LocalGridCoord = USG_GridComponent::WorldToGridFloat(ArrowRelativeLocation + FVector(0, 0, TileSize.Z / 2.0), ArrowRelativeRotation.Yaw, FTransform(), TileSize);
			Data.DungeonGridCoord = FSG_GridCoordinateWithRotation(0, 0, 0, 0); // Will be updated later
			Data.DungeonDoorGridCoord = FSG_GridCoordinate(0, 0, 0); // Will be updated later
			Data.bIsUsed = false; // default to unused
			Data.ArrowCmpt = Arrow;
			OutTargetArray.Add(Data);
		}
	}
}

void ACityGen_RoomBase::CacheBoundsLocalCoord()
{
	CachedBoundsLocalGridCoord.Empty();

	TArray<FBox> roomBounds = GetRoomBounds();
	for(const auto& roomBound : roomBounds)
	{
		FVector Min = roomBound.Min + TileSize * BoundSafeZonePercent;
		FVector Max = roomBound.Max - TileSize * BoundSafeZonePercent;

		FSG_GridCoordinateFloat CachedBoundMinLocal = USG_GridComponent::WorldToGridFloat(Min, FTransform(), TileSize);
		FSG_GridCoordinateFloat CachedBoundMaxLocal = USG_GridComponent::WorldToGridFloat(Max, FTransform(), TileSize);
		CachedBoundsLocalGridCoord.Add(FBoundCoords(CachedBoundMinLocal, CachedBoundMaxLocal));
	}
}

#if WITH_EDITOR
void ACityGen_RoomBase::CheckForArrowAtSameGridLocation(TArray<FExitArrowData>& OutArray) const
{
	TSet<FSG_GridCoordinate> SeenKeys;
	for(const FExitArrowData& Data : OutArray)
	{
		if (SeenKeys.Contains(Data.DungeonGridCoord.position))
		{
			UE_LOG(LogCityGen, Warning, TEXT("Arro at same grid location detected %s: %s"), *GetName(), *Data.ArrowCmpt->GetName());
		}
		SeenKeys.Add(Data.DungeonGridCoord.position);
	}
}
#endif // WITH_EDITOR

// Should be called after UpdateGridCoordCaches
TArray<FSG_GridCoordinate> ACityGen_RoomBase::BuildListOfOverlappingCoordsFromBounds() const
{
	TArray<FSG_GridCoordinate> OverlappingTiles;

	for(const auto& roomBound : CachedBoundsDungeonGridCoord)
	{
		for (int32 X = roomBound.Min.X; X <= roomBound.Max.X; ++X)
		{
			for (int32 Y = roomBound.Min.Y; Y <= roomBound.Max.Y; ++Y)
			{
				for (int32 Z = roomBound.Min.Z; Z <= roomBound.Max.Z; ++Z)
				{
					OverlappingTiles.Add(FSG_GridCoordinate(X, Y, Z));
				}
			}
		}
	}
	return OverlappingTiles;
}

void ACityGen_RoomBase::CloseAllDoors()
{
	for (FExitArrowData& ExitPoint : CachedExitPointsData)
	{
		SetExitState(false, ExitPoint);
	}
}

void ACityGen_RoomBase::SetExitState(bool bUsed, FExitArrowData& InOutExitPoint)
{
	bool bVisible = !bUsed;

	// The arrow component pointer may not be set yet
	if(InOutExitPoint.ArrowCmpt != nullptr)
	{
		InOutExitPoint.ArrowCmpt->SetVisibility(bVisible);
		ECollisionEnabled::Type collisionType = bUsed ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics;

		TArray<USceneComponent*> ArrowChildren;
		InOutExitPoint.ArrowCmpt->GetChildrenComponents(false, ArrowChildren);
		for (USceneComponent* ChildComp : ArrowChildren)
		{
			ChildComp->SetVisibility(bVisible);
			if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(ChildComp))
			{
				StaticMeshComp->SetCollisionEnabled(collisionType);
			}
		}
	}

	InOutExitPoint.bIsUsed = bUsed;
}

void ACityGen_RoomBase::OpenUsedExits()
{
	for (FExitArrowData& ExitPoint : CachedExitPointsData) 
	{
		if(ExitPoint.bIsUsed)
		{
			SetExitState(true, ExitPoint);
		}
	}
}

void ACityGen_RoomBase::SetExitsUsed(const FCellConnectionState& state)
{
	// Here we don't have to manage up and down, as the style of the corridor should have already take care of it
	// We really just need to open side door for the elevator/stair corridors

	// We need to adjust for the actor rotation
	FRotator RoomRot = GetActorRotation();
	int32 rotationNorm = FSG_GridCoordinateWithRotation::RotationWorldToGrid(RoomRot.Yaw);

	// Compute list of local space coord to open
	TArray<FSG_GridCoordinate> DoorsToOpen;
	if (state.bConnectNorth)
	{
		DoorsToOpen.Add(FSG_GridCoordinate(1, 0, 0).RotateBy(rotationNorm));
	}
	if (state.bConnectEast)
	{
		DoorsToOpen.Add(FSG_GridCoordinate(0, 1, 0).RotateBy(rotationNorm));
	}
	if (state.bConnectSouth)
	{
		DoorsToOpen.Add(FSG_GridCoordinate(-1, 0, 0).RotateBy(rotationNorm));
	}
	if (state.bConnectWest)
	{
		DoorsToOpen.Add(FSG_GridCoordinate(0, -1, 0).RotateBy(rotationNorm));
	}

	FSG_GridCoordinateFloat OffsetGridFloatLocal = GetRoomCenterRoundingOffsetGridFloat();
	
	for(const auto&DoorToOpen : DoorsToOpen)
	{
		bool bFoundDoor = false;
		for (FExitArrowData& ExitPoint : CachedExitPointsData)
		{
			FSG_GridCoordinateFloat ExitPointLocalWithOffsetFloat = OffsetGridFloatLocal + ExitPoint.LocalGridCoord.position;
			FSG_GridCoordinate ExitPointLocalWithOffset = ExitPointLocalWithOffsetFloat.SnapToGrid();
			ExitPointLocalWithOffset.Z = 0;// special case for corridor exit matching

			if(ExitPointLocalWithOffset == DoorToOpen)
			{
				ExitPoint.bIsUsed = true;
				bFoundDoor = true;
				break;
			}
		}
		if(!bFoundDoor)
		{
			UE_LOG(LogCityGen, Warning, TEXT("Failed to find door for connection."));
		}
	}
}

// Return the offset of the actor location compare to the grid coord cell center
// @rotation: normalize rotation
FVector ACityGen_RoomBase::GetRoomCenterOffset(int32 rotation) const
{
	FSG_GridCoordinateFloat OffsetGridFloatLocal = GetRoomCenterOffsetGridFloat();

	// Here we only want "positive" offset, as it will mirror for "2,3" rotation
	if ((rotation % 2) == 0)
	{
		return FVector(OffsetGridFloatLocal.X, OffsetGridFloatLocal.Y, OffsetGridFloatLocal.Z) * TileSize;
	}
	else
	{
		return FVector(OffsetGridFloatLocal.Y, OffsetGridFloatLocal.X, OffsetGridFloatLocal.Z) * TileSize;
	}
}

// Return the offset of the actor location compare to the grid coord cell center (local)
FSG_GridCoordinateFloat ACityGen_RoomBase::GetRoomCenterOffsetGridFloat() const
{
	FVector BoundsSize = GetRoomGlobalSize();
	int32 RoomTileWidth = FMath::FloorToInt(BoundsSize.X / TileSize.X);
	int32 RoomTileHeight = FMath::FloorToInt(BoundsSize.Y / TileSize.Y);
	float CenterOffsetX = (RoomTileWidth % 2 == 0) ? 0 : 0.5;
	float CenterOffsetY = (RoomTileHeight % 2 == 0) ? 0 : 0.5;

	FSG_GridCoordinateFloat baseOffset(CenterOffsetX, CenterOffsetY, 0);
	return baseOffset;
}

// Return the offset to apply to the actor location in order to 
// have accurate snapping to the grid
// @rotation: normalize rotation
FVector ACityGen_RoomBase::GetRoomCenterRoundingOffset(int32 rotation) const
{
	FSG_GridCoordinateFloat OffsetGridFloatLocal = GetRoomCenterRoundingOffsetGridFloat();

	// Here we only want "positive" offset, as it will mirror for "2,3" rotation
	if ((rotation % 2) == 0)
	{
		return FVector(OffsetGridFloatLocal.X, OffsetGridFloatLocal.Y, OffsetGridFloatLocal.Z) * TileSize;
	}
	else
	{
		return FVector(OffsetGridFloatLocal.Y, OffsetGridFloatLocal.X, OffsetGridFloatLocal.Z) * TileSize;
	}
}

// Return the offset to apply to the actor location in order to 
// have accurate snapping to the grid
FSG_GridCoordinateFloat ACityGen_RoomBase::GetRoomCenterRoundingOffsetGridFloat() const
{
	FVector BoundsSize = GetRoomGlobalSize();
	int32 RoomTileWidth = FMath::FloorToInt(BoundsSize.X / TileSize.X);
	int32 RoomTileHeight = FMath::FloorToInt(BoundsSize.Y / TileSize.Y);
	float CenterOffsetX = (RoomTileWidth % 2 == 0) ? 0.5 : 0.0;
	float CenterOffsetY = (RoomTileHeight % 2 == 0) ? 0.5 : 0.0;

	FSG_GridCoordinateFloat baseOffset(CenterOffsetX, CenterOffsetY, 0.5);
	return baseOffset;
}

void ACityGen_RoomBase::SnapRoomToGrid(USG_GridComponent* GridComponent)
{
	// Snap to 90 degree yaw, no pitch or roll allowed
	FRotator RoomRot = GetActorRotation();
	int32 rotationNorm = FSG_GridCoordinateWithRotation::RotationWorldToGrid(RoomRot.Yaw);

	// Based on bounds, we compute if the actor center is in middle of grid cell, or at a corner
	FVector RoomCenterOffset = GetRoomCenterOffset(rotationNorm);
	FVector RoomCenterRoundingOffset = GetRoomCenterRoundingOffset(rotationNorm);

	// Compute grid location
	FVector RoomPos = GetActorLocation();
	CachedRoomGridCoord.position = GridComponent->WorldToGrid(RoomPos + RoomCenterRoundingOffset);
	CachedRoomGridCoord.rotation = rotationNorm;

	// Compute world location
	FVector SnapLocation = GridComponent->GridToWorld(CachedRoomGridCoord.position);
	FRotator SnapRotation = FSG_GridCoordinateWithRotation::RotationGridToWorld(rotationNorm);

	// Snap to grid aligned location/rotation
	SetActorLocationAndRotation(SnapLocation + RoomCenterOffset, SnapRotation.Quaternion());
}

void ACityGen_RoomBase::UpdateGridCoordCaches(USG_GridComponent* GridComponent)
{
	FVector RoomPos = GetActorLocation();
	FRotator RoomRot = GetActorRotation();

	// We need to re-update this value after actor snap to get the actual actor grid float position
	FSG_GridCoordinateFloatWithRotation RoomGridCoordFloat = GridComponent->WorldToGridFloat(RoomPos, RoomRot.Yaw);

	// Update dungeon coord
	for(FExitArrowData& ExitPoint : CachedExitPointsData)
	{
		FVector ArrowLocation = ExitPoint.ArrowCmpt->GetComponentLocation();
		int32 ArrowRotation = ExitPoint.ArrowCmpt->GetComponentRotation().Yaw;

		FSG_GridCoordinateFloatWithRotation DungeonGridCoordFloat = RoomGridCoordFloat.GetPositionAndRotationParentSpace(ExitPoint.LocalGridCoord);
		ExitPoint.DungeonGridCoord = DungeonGridCoordFloat.SnapToGrid();
		ExitPoint.DungeonDoorGridCoord = ExitPoint.DungeonGridCoord.position - SG_GetDirectionForRotNorm(ExitPoint.DungeonGridCoord.rotation);
	}
	for (FExitArrowData& BlockedExitPoint : CachedBlockedExitPointsData)
	{
		FVector ArrowLocation = BlockedExitPoint.ArrowCmpt->GetComponentLocation();
		int32 ArrowRotation = BlockedExitPoint.ArrowCmpt->GetComponentRotation().Yaw;

		FSG_GridCoordinateFloatWithRotation DungeonGridCoordFloat = RoomGridCoordFloat.GetPositionAndRotationParentSpace(BlockedExitPoint.LocalGridCoord);
		BlockedExitPoint.DungeonGridCoord = DungeonGridCoordFloat.SnapToGrid();
		BlockedExitPoint.DungeonDoorGridCoord = BlockedExitPoint.DungeonGridCoord.position - SG_GetDirectionForRotNorm(BlockedExitPoint.DungeonGridCoord.rotation);
	}

#if WITH_EDITOR
	CheckForArrowAtSameGridLocation(CachedExitPointsData);
	CheckForArrowAtSameGridLocation(CachedBlockedExitPointsData);
#endif // WITH_EDITOR

	uint32 index = 0;
	CachedBoundsDungeonGridCoord.SetNum(CachedBoundsLocalGridCoord.Num());
	for(const auto& boundLocal : CachedBoundsLocalGridCoord)
	{
		FSG_GridCoordinateFloat MinCoordFloat = RoomGridCoordFloat.GetPositionParentSpace(boundLocal.Min);
		FSG_GridCoordinate MinCoord = MinCoordFloat.SnapToGrid();
		FSG_GridCoordinateFloat MaxCoordFloat = RoomGridCoordFloat.GetPositionParentSpace(boundLocal.Max);
		FSG_GridCoordinate MaxCoord = MaxCoordFloat.SnapToGrid();

		// Due to room rotation, the min/max may be reversed
		auto& boundDungeon = CachedBoundsDungeonGridCoord[index];
		boundDungeon.Min.X = FMath::Min(MinCoord.X, MaxCoord.X);
		boundDungeon.Min.Y = FMath::Min(MinCoord.Y, MaxCoord.Y);
		boundDungeon.Min.Z = FMath::Min(MinCoord.Z, MaxCoord.Z);
		boundDungeon.Max.X = FMath::Max(MinCoord.X, MaxCoord.X);
		boundDungeon.Max.Y = FMath::Max(MinCoord.Y, MaxCoord.Y);
		boundDungeon.Max.Z = FMath::Max(MinCoord.Z, MaxCoord.Z);
		++index;
	}
}

#if WITH_EDITOR
void ACityGen_RoomBase::DebugDrawRoomCachedData(USG_GridComponent* GridComponent)
{
	float DebugLifeTime = 5.0f;

	FVector CellCenter = TileSize / 2.0;

	float ConeAngleRad = FMath::DegreesToRadians(30.0f);

	const float ConeHeight = 250.0f;

	FVector Position = GridComponent->GridToWorld(CachedRoomGridCoord.position);
	FVector Direction = FSG_GridCoordinateWithRotation::RotationGridToWorld(CachedRoomGridCoord.rotation).RotateVector(FVector(1, 0, 0));
	DrawDebugCone(GetWorld(), Position + CellCenter + Direction * ConeHeight, -Direction, ConeHeight, ConeAngleRad, ConeAngleRad, 8, FColor::Blue, false, DebugLifeTime);

	//DrawDebugCone(GetWorld(), FVector(0,0,0) + FVector(1, 0, 0) * ConeHeight, FVector(-1,0,0), ConeHeight, ConeAngleRad, ConeAngleRad, 8, FColor::Orange, false, DebugLifeTime);

	for (const FExitArrowData& ExitPoint : CachedExitPointsData)
	{
		FVector PositionExit = GridComponent->GridToWorld(ExitPoint.DungeonGridCoord.position);
		FVector DirectionExit = FSG_GridCoordinateWithRotation::RotationGridToWorld(ExitPoint.DungeonGridCoord.rotation).RotateVector(FVector(1, 0, 0));
		DrawDebugCone(GetWorld(), PositionExit + CellCenter + DirectionExit * ConeHeight, -DirectionExit, ConeHeight, ConeAngleRad, ConeAngleRad, 8, FColor::Green, false, DebugLifeTime);
	}

	for (const FExitArrowData& ExitPoint : CachedBlockedExitPointsData)
	{
		FVector PositionExit = GridComponent->GridToWorld(ExitPoint.DungeonGridCoord.position);
		const float BlockedSizePercent = 0.8;
		DrawDebugBox(GetWorld(), PositionExit + CellCenter, TileSize / 2 * BlockedSizePercent, FQuat::Identity, FColor::Red, false, DebugLifeTime);
	}

	TArray<FSG_GridCoordinate> blockTiles = BuildListOfOverlappingCoordsFromBounds();
	for (const FSG_GridCoordinate& blockTile : blockTiles)
	{
		FVector PositionBlock = GridComponent->GridToWorld(blockTile);
		DrawDebugSphere(GetWorld(), PositionBlock + CellCenter, 100.0, 8, FColor::Black, false, DebugLifeTime);
	}
}
#endif // WITH_EDITOR

// Return local bounds
TArray<FBox> ACityGen_RoomBase::GetRoomBounds() const
{
	TArray<FBox> result;
#if WITH_EDITOR
	if((BoundsBoxFolder->GetRelativeLocation() != FVector(0, 0, 0)) ||
		(BoundsBoxFolder->GetRelativeRotation() != FRotator(0, 0, 0)))
	{
		UE_LOG(LogCityGen, Warning, TEXT("BoundsBoxFolder should not have relative location or rotation %s"), *GetName());
	}
#endif // WITH_EDITOR

	TArray<USceneComponent*> ChildComponents;
	BoundsBoxFolder->GetChildrenComponents(true, ChildComponents);
	for (USceneComponent* Component : ChildComponents)
	{
		if (!Component)
		{
			continue;
		}

		UBoxComponent* BoxComp = Cast<UBoxComponent>(Component);
		if (!BoxComp)
		{
			continue;
		}
#if WITH_EDITOR
		if (BoundsBoxFolder->GetRelativeRotation() != FRotator(0, 0, 0))
		{
			UE_LOG(LogCityGen, Warning, TEXT("Box component should not have relative rotation %s"), *GetName());
		}
#endif // WITH_EDITOR

		const FVector boxExtentScaled = BoxComp->GetScaledBoxExtent();
		FBox localBounds(BoxComp->GetRelativeLocation() - boxExtentScaled, BoxComp->GetRelativeLocation() + boxExtentScaled);

		result.Add(localBounds);
	}

	return result;
}

FVector ACityGen_RoomBase::GetRoomGlobalSize() const
{
	TArray<FBox> roomBounds = GetRoomBounds();
	if(roomBounds.Num() < 1)
	{
		return FVector(0, 0, 0);
	}
	FBox localBounds = roomBounds[0];
	for(int32 i=1; i < roomBounds.Num(); ++i)
	{
		localBounds += roomBounds[i];
	}
	return localBounds.GetExtent() * 2.0;
}
