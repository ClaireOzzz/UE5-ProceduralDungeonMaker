// Copyright Chateau Pageot, Inc. All Rights Reserved.

#include "SimpleGridRuntime/Public/SG_GridComponentWithActorTracking.h"

//#include "SnowVania/MovableResource/SVInteractableMovableResource.h"
//#include "SnowVania/MovableResource/SVInteractablePackedModule.h"
//#include "SnowVania/SVLogChannels.h"
//#include "SnowVania/Vehicle/SVVehicleState.h"
//#include "SnowVania/VehicleFunction/SVOperatableVFWithGrid.h"

#include "Engine/OverlapResult.h"
#include "Kismet/GameplayStatics.h"

USG_GridComponentWithActorTracking::USG_GridComponentWithActorTracking(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);

#if SG_GRIDCOMPONENTWITHACTORTRACK_DRAWDEBUG
	bDebugDrawGrid = false;
#endif // SG_GRIDCOMPONENTWITHACTORTRACK_DRAWDEBUG
}

void USG_GridComponentWithActorTracking::BuildGrid()
{
	if (CellStateGrid.IsEmpty())
	{
		CellStateGrid.Init(FCellState(), Width * Height);
	}
}

bool USG_GridComponentWithActorTracking::CellIsEmpty(const FSG_GridCoordinate& GridIndex) const
{
	return CellStateGrid[GridIndex.X + GridIndex.Y * Width].bIsCellEmpty; // Module space location
}

AActor* USG_GridComponentWithActorTracking::GetItemAtCoord(const FSG_GridCoordinate& GridIndex) const
{
	return CellStateGrid[GridIndex.X + GridIndex.Y * Width].ActorRef; // Module space location
}

bool USG_GridComponentWithActorTracking::WorldPositionIsEmpty(const FVector& position) const
{
	FVector RelativePosition = position - GetOwner()->GetActorLocation().RotateAngleAxis(-GetOwner()->GetActorRotation().Yaw, FVector(0,0,1)); // Actor Relative space location
	FSG_GridCoordinate PositionInGrid = RelativeToGrid(RelativePosition); // Module space location
	if (!IsCoordinateValid(PositionInGrid))
	{
		return false;
	}
	return CellIsEmpty(PositionInGrid);
}

bool USG_GridComponentWithActorTracking::PlaceItemOnCoordinate(const FSG_GridCoordinate& GridIndex, AActor* item)
{
	// Module space location
	if (!CellIsEmpty(GridIndex))
	{
		return false;
	}
	
	CellStateGrid[GridIndex.X + GridIndex.Y * Width].bIsCellEmpty = false;
	CellStateGrid[GridIndex.X + GridIndex.Y * Width].ActorRef = item;
	return true;
}

bool USG_GridComponentWithActorTracking::PlaceItemOnWorldPosition(const FVector& position, AActor* item)
{
	FSG_GridCoordinate GridIndex = WorldToGrid(position); // Module space location
	if (!IsCoordinateValid(GridIndex))
	{
		return false;
	}
	return PlaceItemOnCoordinate(GridIndex, item);
}

void USG_GridComponentWithActorTracking::RemoveItemOnCoordinate(const FSG_GridCoordinate& GridIndex)
{
	// Module space location
	CellStateGrid[GridIndex.X + GridIndex.Y * Width].bIsCellEmpty = true;
	CellStateGrid[GridIndex.X + GridIndex.Y * Width].ActorRef = nullptr;
}

bool USG_GridComponentWithActorTracking::RemoveItemOnWorldPosition(const FVector& position)
{
	FSG_GridCoordinate GridIndex = WorldToGrid(position); // Module space location
	if (!IsCoordinateValid(GridIndex))
	{
		return false;
	}
	RemoveItemOnCoordinate(GridIndex);
	return true;
}

bool USG_GridComponentWithActorTracking::RemoveItemFromGrid(AActor *const &item)
{
	for (int32 i = 0; i < Width * Height; ++i)
	{
		if(CellStateGrid[i].ActorRef == item)
		{
			CellStateGrid[i].bIsCellEmpty = true;
			CellStateGrid[i].ActorRef = nullptr;
			return true;
		}
	}

	return false;
}

//// Warning; assume coordinate inside the grid
//void USG_GridComponentWithActorTracking::SetIsEmptyAtGridLocation(const FSG_GridCoordinate& GridIndex, bool empty)
//{
//	// Module space location
//	check(GridIndex.X >= 0 && GridIndex.X < Width);
//	check(GridIndex.Y >= 0 && GridIndex.Y < Height);
//	IsEmpty[GridIndex.X + GridIndex.Y * Width] = empty;
//}

// Warning; assume coordinate inside the grid
bool USG_GridComponentWithActorTracking::SetIsEmptyAtRectangle(const FSG_GridCoordinate& RectangleGridCornerIndex, const FIntVector& RectangleSize, bool empty, const FString& debugEmptyID)
{
	// Module space location
	if(RectangleGridCornerIndex.X + RectangleSize.X > Width)
	{
		return false;
	}
	if(RectangleGridCornerIndex.Y + RectangleSize.Y > Height)
	{
		return false;
	}

	for (int32 i = 0; i < RectangleSize.X; ++i)
	{
		for (int32 j = 0; j < RectangleSize.Y; ++j)
		{
			CellStateGrid[RectangleGridCornerIndex.X + i + (RectangleGridCornerIndex.Y + j) * Width].bIsCellEmpty = empty;
#if CP_WITH_CELL_FULL_NAME
			if(empty == false)
			{
				CellStateGrid[RectangleGridCornerIndex.X + i + (RectangleGridCornerIndex.Y + j) * Width].FullName = debugEmptyID;
			}
#endif // CP_WITH_CELL_FULL_NAME
		}
	}
	return true;
}

/**
 * Verifies the emptiness of each cell in the grid by performing line traces from the grid center to the upwards direction
 * The IsEmpty array is updated based on the presence of certain types of actors in the traced cells
 * 
 * 2024-07-01: Note at here we do a single line trace at center, oppose to USG_GridComponentWithActorTracking::GetItemsOnPosition that use a box overlap intersect
 */
void USG_GridComponentWithActorTracking::UpdateCellsIsEmptyUsingTrace()
{
	const float cTraceOffsetUp = 190.0f;
	const float cTraceOffsetDown = 0.0f;
	const FVector upVector = GetUpVector();

	// Set up collision query parameters, ignoring the owner of the grid component
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = true;
	//QueryParams.AddIgnoredActor(GetOwner());

	// Iterate through each cell in the grid
	for (int32 i = 0; i < Width; ++i)
	{
		for (int32 j = 0; j < Height; ++j)
		{
			if(CellStateGrid[i + j * Width].bIsCellEmpty == false)
			{
				// No need to test cell that were already marked by the walls setup
				continue;
			}

			// Calculate the world coordinates of the center of the current grid cell (Include GridHeight = 10 offset in Z axis)
			FVector TraceEnd = GridToWorld_2DCenter( FSG_GridCoordinate(i, j, 0));

			// Define the trace end point as above the grid cell
			FVector TraceStart = TraceEnd + upVector * cTraceOffsetUp;
			TraceEnd = TraceEnd - upVector * cTraceOffsetDown;

			// Perform a line trace to check for collision with dynamic world objects
			FHitResult Hit;
			//ECollisionChannel TraceChannelProperty = ECC_WorldDynamic;
			ECollisionChannel TraceChannelProperty = ECC_Visibility; // 2025-02-12: change to visiblity in order that repairSpot which as WorldDynamic overlap does not set cell non-empty
			if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, TraceChannelProperty, QueryParams))
			{
				// Check if the hit actor is a VF or a Resource
				//if (Hit.GetActor()->IsA(ASVVehicleFunction::StaticClass()) || Hit.GetActor()->IsA(ASVInteractableMovableResource::StaticClass()))
				{
					// Update the IsEmpty array for the current grid cell to indicate it is not empty
					CellStateGrid[i + j * Width].bIsCellEmpty = false;
#if CP_WITH_CELL_FULL_NAME
					CellStateGrid[i + j * Width].FullName = Hit.GetComponent()->GetFullName();
#endif // CP_WITH_CELL_FULL_NAME
				}
			}
			/*else // 2024-07-01: Commented out, we only want to make cell NOT empty on collision
			{
				// Update the IsEmpty array for the current grid cell to indicate it is empty
				IsEmpty[i + j * Width] = true;
			}*/
		}
	}
}

TArray<AActor*> USG_GridComponentWithActorTracking::GetAllActorsOnGrid() const
{
	TArray<AActor*> AllActors = TArray<AActor*>();
	for (auto&& CellState : CellStateGrid)
	{
		if (CellState.ActorRef != nullptr)
		{
			AllActors.Add(CellState.ActorRef);
		}
	}
	return AllActors;
}

bool USG_GridComponentWithActorTracking::FindEmptyCell(FSG_GridCoordinate& OutPosition) const
{
	for (int32 i = 0; i < Width; ++i)
	{
		for (int32 j = 0; j < Height; ++j)
		{
			FSG_GridCoordinate gridIndex = FSG_GridCoordinate(i, j, 0);
			if (CellIsEmpty(gridIndex))
			{
				OutPosition = gridIndex;
				return true;
			}
		}
	}
	return false;
}

// Find an empty cell that is also not overlapping with ObjectTypes
bool USG_GridComponentWithActorTracking::FindEmptyCellFilterByOverlapCheck(const TArray<TEnumAsByte<EObjectTypeQuery> >& ObjectTypes, FSG_GridCoordinate& OutPositionGS) const
{
	TArray<AActor*> IgnoreActors;
	for (int32 i = 0; i < Width; ++i)
	{
		for (int32 j = 0; j < Height; ++j)
		{
			FSG_GridCoordinate gridIndex = FSG_GridCoordinate(i, j, 0);
			if (CellIsEmpty(gridIndex))
			{
				if (BoxOverlapActorsForCell(gridIndex, ObjectTypes, IgnoreActors) == false)
				{
					OutPositionGS = gridIndex;
					return true;
				}
			}
		}
	}
	return false;
}

bool USG_GridComponentWithActorTracking::BoxOverlapActorsForCell(const FSG_GridCoordinate& gridIndex, const TArray<TEnumAsByte<EObjectTypeQuery> >& ObjectTypes, const TArray<AActor*>& IgnoreActors) const
{
	bool bTraceComplex = false;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RadiusTargetingOverlap), bTraceComplex);
	Params.bReturnPhysicalMaterial = false;
	Params.AddIgnoredActors(IgnoreActors);

	TArray<FOverlapResult> Overlaps;

	FCollisionObjectQueryParams ObjectParams;
	for (auto Iter = ObjectTypes.CreateConstIterator(); Iter; ++Iter)
	{
		const ECollisionChannel& Channel = UCollisionProfile::Get()->ConvertToCollisionChannel(false, *Iter);
		ObjectParams.AddObjectTypesToQuery(Channel);
	}

	UWorld* World = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::LogAndReturnNull);
	if (World != nullptr)
	{
		World->OverlapMultiByObjectType(Overlaps, GridToWorld_2DCenter(gridIndex), FQuat::Identity, ObjectParams, FCollisionShape::MakeBox(TileSize / 2.0f), Params);
	}
	return (Overlaps.Num() > 0);
}

// Rotate in 2d (Yaw)
// @param GridRotatedNormVS in [0; 3]
void USG_GridComponentWithActorTracking::RotateGridContents(int32 rotationNormalize)
{
	if (rotationNormalize == 0)
	{
		return;
	}

	int32 newWidth = Width;
	int32 newWHeight = Height;
	if ((rotationNormalize == 1)||(rotationNormalize == 3))
	{
		newWidth = Height;
		newWHeight = Width;
	}

	TArray<FCellState> NewCellState;
	NewCellState.SetNum(Height * Width);

	for (int32 j = 0; j < Height; j++)
	{
		for (int32 i = 0; i < Width; i++)
		{
			FSG_GridCoordinate newcood = RotateCoord2D(FSG_GridCoordinate(i, j, 0), rotationNormalize);
			NewCellState[newcood.Y * newWidth + newcood.X] = CellStateGrid[j * Width + i];
		}
	}

	CellStateGrid = NewCellState;
	Width = newWidth;
	Height = newWHeight;
}

#if SG_GRIDCOMPONENTWITHACTORTRACK_DRAWDEBUG
void USG_GridComponentWithActorTracking::DebugDrawGrid()
{
	UWorld * pWorld = GetOwner()->GetWorld();

	FPlane plane(0, 0, 0, GetComponentLocation().Z);

	for (int32 j = 0; j < Height; j++)
	{
		for (int32 i = 0; i < Width; i++)
		{
			const FSG_GridCoordinate coord(i, j, 0);
			const FVector locationWS = GridToWorld_2DCenter(coord);
			const bool bCellEmpty = CellIsEmpty(coord);
			const bool bItemValid = IsValid(GetItemAtCoord(coord));

			float planeSize = TileSize.X / 2 * 0.9; // "* 0.9" to shrink a bit for visibility"

			FLinearColor color = FLinearColor::Green;
			if (bCellEmpty == false)
			{
				if (bItemValid)
				{
					color = FLinearColor::Blue;
				}
				else
				{
					color = FLinearColor::Yellow;
				}
			}

			DrawDebugSolidPlane(pWorld, plane, locationWS, planeSize, color.ToFColor(false));
			DrawDebugString(pWorld, locationWS, FString::Printf(TEXT("%i, %i"), i, j));
		}
	}
}
#endif // SG_GRIDCOMPONENTWITHACTORTRACK_DRAWDEBUG

// Server only
AActor* USG_GridComponentWithActorTracking::SpawnActorAndPlaceOnGrid(const FSG_GridCoordinate& GridLocation, TSubclassOf<AActor> ActorClass)
{
	FTransform SpawnTransform(GridToWorld_2DCenter(GridLocation));

	if (ActorClass == nullptr)
	{
		return nullptr;
	}

	AActor* pNewActor =
		GetWorld()->SpawnActorDeferred<AActor>(ActorClass, SpawnTransform);

	if (pNewActor == nullptr)
	{
		return nullptr;
	}

	pNewActor->FinishSpawning(SpawnTransform);

	if (PlaceItemOnCoordinate(GridLocation, pNewActor) == false)
	{
		return nullptr;
	}
	if (pNewActor->AttachToActor(GetOwner(), FAttachmentTransformRules::KeepWorldTransform) == false)
	{
		return nullptr;
	}

	check(pNewActor);
	return pNewActor;
}