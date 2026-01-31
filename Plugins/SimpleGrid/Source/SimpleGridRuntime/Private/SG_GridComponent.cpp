// Copyright Chateau Pageot, Inc. All Rights Reserved.

#include "SimpleGridRuntime/Public/SG_GridComponent.h"

#include "Net/UnrealNetwork.h"



USG_GridComponent::USG_GridComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if GRIDCOMPONENT_DRAWDEBUG
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetComponentTickEnabled(true);
#endif // GRIDCOMPONENT_DRAWDEBUG
}

// @GridIndex: index in the grid
// @return cell corner in world space after applying cellSize
FVector USG_GridComponent::GridToWorld(const FSG_GridCoordinate& GridIndex) const
{
	return GridToWorld_Float(FSG_GridCoordinateFloat(GridIndex.X, GridIndex.Y, GridIndex.Z));
}

// @GridIndex: index in the grid
// @return cell corner in relative space after applying cellSize
FVector USG_GridComponent::GridToRelative(const FSG_GridCoordinate& GridIndex) const
{
	return GridToRelative_Float(FSG_GridCoordinateFloat(GridIndex.X, GridIndex.Y, GridIndex.Z));
}

// @GridIndex: index in the grid
// @return cell corner in world space after applying cellSize
// Allow floating coordinate
FVector USG_GridComponent::GridToWorld_Float(const FSG_GridCoordinateFloat& GridIndexFloat) const
{
	const int32 rotationUnnorm = FMath::RoundToInt(GetComponentRotation().Yaw / 90.0);
	const FVector relativeLocationWS = GridToRelative_Float(GridIndexFloat.RotateBy(rotationUnnorm));
	return GetComponentLocation() + relativeLocationWS;
}

// @GridIndex: index in the grid
// @return cell center in world space after applying cellSize
// Allow floating coordinate
FVector USG_GridComponent::GridToWorld_3DCenter(const FSG_GridCoordinate& GridIndex) const
{
	return GridToWorld_Float(FSG_GridCoordinateFloat(GridIndex.X, GridIndex.Y, GridIndex.Z) + FSG_GridCoordinateFloat(0.5f, 0.5f, 0.5f));
}

// @GridIndex: index in the grid
// @return cell corner in relative space after applying cellSize
// Allow floating coordinate
FVector USG_GridComponent::GridToRelative_Float(const FSG_GridCoordinateFloat& GridIndexFloat) const
{
	return FVector(GridIndexFloat.X * TileSize.X, GridIndexFloat.Y * TileSize.Y, GridIndexFloat.Z * TileSize.Z);
}

// @GridIndex: index in the grid
// @return cell center(XY axis) in relative space after applying cellSize
FVector USG_GridComponent::GridToWorld_2DCenter(const FSG_GridCoordinate& GridIndex) const
{
	return GridToWorld_Float(FSG_GridCoordinateFloat(GridIndex.X, GridIndex.Y, GridIndex.Z) + FSG_GridCoordinateFloat(0.5f, 0.5f, 0.0f));
}

// This does not check if inside the grid or not
// Transform from WorldSpace to GridSpace
FSG_GridCoordinate USG_GridComponent::WorldToGrid(const FVector& positionWS) const
{
	return WorldToGrid(positionWS, GetComponentTransform().Inverse(), TileSize);
}

// This does not check if inside the grid or not
// Transform from WorldSpace to GridSpace
FSG_GridCoordinate USG_GridComponent::WorldToGrid(const FVector& positionWS, const FTransform& GridInverseTransformWS, const FVector& TileSize)
{
	FVector RelativePosition = GridInverseTransformWS.TransformPosition(positionWS);
	FSG_GridCoordinate GridLocation = FSG_GridCoordinate(
		FMath::FloorToInt(RelativePosition.X / TileSize.X),
		FMath::FloorToInt(RelativePosition.Y / TileSize.Y),
		FMath::FloorToInt(RelativePosition.Z / TileSize.Z));
	return GridLocation;
}

// This does not check if inside the grid or not
// Transform from WorldSpace to GridSpace
FSG_GridCoordinateWithRotation USG_GridComponent::WorldToGrid(const FVector& positionWS, float YawWS) const
{
	return WorldToGrid(positionWS, YawWS, GetComponentTransform().Inverse(), TileSize);
}
FSG_GridCoordinateFloatWithRotation USG_GridComponent::WorldToGridFloat(const FVector& positionWS, float YawWS) const
{
	return WorldToGridFloat(positionWS, YawWS, GetComponentTransform().Inverse(), TileSize);
}

// This does not check if inside the grid or not
// Transform from WorldSpace to GridSpace
FSG_GridCoordinateWithRotation USG_GridComponent::WorldToGrid(const FVector& positionWS, float YawWS, const FTransform& GridInverseTransformWS, const FVector& TileSize)
{
	FVector RelativePosition = GridInverseTransformWS.TransformPosition(positionWS);

	int32 rotationNorm = FSG_GridCoordinateWithRotation::RotationWorldToGrid(YawWS);

	FSG_GridCoordinateWithRotation GridLocation = FSG_GridCoordinateWithRotation(
		FMath::FloorToInt(RelativePosition.X / TileSize.X),
		FMath::FloorToInt(RelativePosition.Y / TileSize.Y),
		FMath::FloorToInt(RelativePosition.Z / TileSize.Z),
		rotationNorm);
	return GridLocation;
}

FSG_GridCoordinateFloatWithRotation USG_GridComponent::WorldToGridFloat(const FVector& positionWS, float YawWS, const FTransform& GridInverseTransformWS, const FVector& TileSize)
{
	// For rotation we round in order to avoid precision issue when going from world->Grid-<World
	// (as world also have rotator->Quaternion) that result in yaw being really small negative value
	int32 rotationUnnorm = FMath::RoundToInt(YawWS / 90.0);
	int32 rotationNorm = FSG_GridCoordinateWithRotation::NormalizeCardinalRotation(rotationUnnorm);

	FSG_GridCoordinateFloat positionFloat = WorldToGridFloat(positionWS, GridInverseTransformWS, TileSize);

	FSG_GridCoordinateFloatWithRotation GridLocation = FSG_GridCoordinateFloatWithRotation(
		positionFloat,
		rotationNorm);
	return GridLocation;
}

FSG_GridCoordinateFloat USG_GridComponent::WorldToGridFloat(const FVector& positionWS, const FTransform& GridInverseTransformWS, const FVector& TileSize)
{
	FVector RelativePosition = GridInverseTransformWS.TransformPosition(positionWS);

	FSG_GridCoordinateFloat GridLocation = FSG_GridCoordinateFloat(
		(RelativePosition.X / TileSize.X),
		(RelativePosition.Y / TileSize.Y),
		(RelativePosition.Z / TileSize.Z));
	return GridLocation;
}

// Return adjacent cell, direction need to be normalized. Return coordinate not guarantee to be inside grid
// Warning: only support 2D
FSG_GridCoordinate USG_GridComponent::AdjacentCellFromDirectionGS(const FSG_GridCoordinate& GridIndex, const FVector& DirectionGridSpace)
{
	const float sqHalf = 0.7071067811;
	// X is Up, Y is Right
	const FVector adjacents[8] = {
		FVector(sqHalf,-sqHalf,0),	FVector( 1,0,0),	FVector(sqHalf,sqHalf,0),
		FVector(0, -1,0),								FVector(0, 1, 0),
		FVector(-sqHalf,-sqHalf,0),	FVector(-1,0,0),	FVector(-sqHalf,sqHalf,0),
	};
	const FSG_GridCoordinate adjacentsGrid[8] = {
		FSG_GridCoordinate( 1, -1, 0),	FSG_GridCoordinate( 1, 0, 0),	FSG_GridCoordinate(1, 1, 0),
		FSG_GridCoordinate(0, -1, 0),								FSG_GridCoordinate(0, 1, 0),
		FSG_GridCoordinate(-1,-1, 0),	FSG_GridCoordinate(-1, 0, 0),	FSG_GridCoordinate(-1,1, 0),
	};

	uint32 bestCell = 0;
	float bestDot = DirectionGridSpace.Dot(adjacents[0]);
	for (uint32 i = 1; i < 8; ++i)
	{
		float currentDot = DirectionGridSpace.Dot(adjacents[i]);
		if (currentDot > bestDot)
		{
			bestDot = currentDot;
			bestCell = i;
		}
	}

	return GridIndex + adjacentsGrid[bestCell];
}

// Return adjacent cell, direction need to be normalized. Return coordinate not guarantee to be inside grid
// Warning: only support 2D
FSG_GridCoordinate USG_GridComponent::AdjacentCellFromDirectionWS(const FSG_GridCoordinate& GridIndex, const FVector& DirectionWS) const
{
	FVector DirectionGridSpace = GetComponentTransform().Inverse().TransformVector(DirectionWS);
	return AdjacentCellFromDirectionGS(GridIndex, DirectionGridSpace);
}

// Get neighboor nodes in 2D space
TArray<FSG_GridCoordinate> USG_GridComponent::GetNeighbourNodes2D(const FSG_GridCoordinate& Node)
{
	TArray<FSG_GridCoordinate> NeighbourNodes;
	NeighbourNodes.Reserve(4);
	NeighbourNodes.Add(FSG_GridCoordinate(Node + FSG_GridCoordinate(1, 0, 0))); // west
	NeighbourNodes.Add(FSG_GridCoordinate(Node + FSG_GridCoordinate(0, 1, 0))); // North
	NeighbourNodes.Add(FSG_GridCoordinate(Node + FSG_GridCoordinate(-1, 0, 0))); // East
	NeighbourNodes.Add(FSG_GridCoordinate(Node + FSG_GridCoordinate(0, -1, 0))); // South

	return NeighbourNodes;
}

// Get neighboor nodes in 3D space
TArray<FSG_GridCoordinate> USG_GridComponent::GetNeighbourNodes3D(const FSG_GridCoordinate& Node)
{
	TArray<FSG_GridCoordinate> NeighbourNodes;
	NeighbourNodes.Reserve(6);
	NeighbourNodes.Add(FSG_GridCoordinate(Node + FSG_GridCoordinate(1, 0, 0))); // west
	NeighbourNodes.Add(FSG_GridCoordinate(Node + FSG_GridCoordinate(0, 1, 0))); // North
	NeighbourNodes.Add(FSG_GridCoordinate(Node + FSG_GridCoordinate(-1, 0, 0))); // East
	NeighbourNodes.Add(FSG_GridCoordinate(Node + FSG_GridCoordinate(0, -1, 0))); // South
	NeighbourNodes.Add(FSG_GridCoordinate(Node + FSG_GridCoordinate(0, 0, 1))); // Up
	NeighbourNodes.Add(FSG_GridCoordinate(Node + FSG_GridCoordinate(0, 0, -1))); // Down

	return NeighbourNodes;
}

void USG_GridComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, TileSize);
	DOREPLIFETIME(ThisClass, DebugTrace);
	DOREPLIFETIME(ThisClass, DebugGridCoordinateSystem);
}

#if GRIDCOMPONENT_DRAWDEBUG
void USG_GridComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (DebugTrace)
	{
		DrawDebug();
	}
	if (DebugGridCoordinateSystem)
	{
		DrawDebugGridCoordinateSystem();
	}
}

void USG_GridComponent::DrawDebug()
{
	for (int32 i = -2; i < 3; i++)
	{
		for (int32 j = -2; j < 3; j++)
		{
			FVector Pos = GridToWorld_2DCenter(FSG_GridCoordinate(i, j, 0));
			FVector Extent = FVector(1.0f, 1.0f, 0) * TileSize * 0.4f;
			DrawDebugSolidBox(GetWorld(), Pos, Extent, FColor::Green, false, 0.0f);
		}
	}
}

void USG_GridComponent::DrawDebugGridCoordinateSystem()
{
	const FVector Location = GetComponentLocation();
	const FRotator Rotation = GetComponentRotation();
	const FVector ActorLocation = GetOwner()->GetActorLocation();
	const FRotator ActorRotation = GetOwner()->GetActorRotation();
	const float Scale = 100.0f;
	const float Duration = -1.0f;
	const uint8 DepthPriority = SDPG_Foreground;
	const float Thickness = 5.0f;
	DrawDebugCoordinateSystem(GetWorld(), Location, Rotation, Scale, false, Duration, DepthPriority, Thickness);
	DrawDebugCoordinateSystem(GetWorld(), ActorLocation, ActorRotation, Scale, false, Duration, DepthPriority, Thickness);
}

#endif // GRIDCOMPONENT_DRAWDEBUG
