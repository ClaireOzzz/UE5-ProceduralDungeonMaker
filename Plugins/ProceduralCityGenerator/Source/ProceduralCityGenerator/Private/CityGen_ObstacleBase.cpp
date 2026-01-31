// Copyright Chateau Pageot, Inc. All Rights Reserved.

#include "CityGen_ObstacleBase.h"
#include "CityGen_LogChannels.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include <Kismet/KismetSystemLibrary.h>

// Sets default values
ADEPRECATED_CityGen_ObstacleBase::ADEPRECATED_CityGen_ObstacleBase()
{
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	GeometryFolder = CreateDefaultSubobject<USceneComponent>(TEXT("GeometryFolder"));
	GeometryFolder->SetupAttachment(RootComponent);

	GridCmpt = CreateDefaultSubobject<USG_GridComponent>(TEXT("GridCmpt"));
	GridCmpt->SetTileSize(TileSize);
}

TArray<FSG_GridCoordinate> ADEPRECATED_CityGen_ObstacleBase::GetWorldTilesOverlappingBounds(const FBox& Bounds, float BoundExpandSizeWS) const
{
	TArray<FSG_GridCoordinate> OverlappingTiles;

	FVector Min = Bounds.ExpandBy(BoundExpandSizeWS).Min;
	FVector Max = Bounds.ExpandBy(BoundExpandSizeWS).Max;

	// Convert bounds directly to world grid coordinates
	FSG_GridCoordinate MinGrid = GridCmpt->WorldToGrid(Min);
	FSG_GridCoordinate MaxGrid = GridCmpt->WorldToGrid(Max);

	for (int32 X = MinGrid.X; X <= MaxGrid.X; ++X)
	{
		for (int32 Y = MinGrid.Y; Y <= MaxGrid.Y; ++Y)
		{
			for (int32 Z = MinGrid.Z; Z <= MaxGrid.Z; ++Z)
			{
				OverlappingTiles.Add(FSG_GridCoordinate(X, Y, Z));
			}
		}
	}

	return OverlappingTiles;
}

FBox ADEPRECATED_CityGen_ObstacleBase::GetObstacleBounds() const
{
	if (!GeometryFolder)
	{
		UE_LOG(LogCityGen, Warning, TEXT("GeometryFolder is not set in ObstacleBase."));
		return FBox(EForceInit::ForceInitToZero); // Return empty box
	}

	TArray<USceneComponent*> ChildComponents;
	GeometryFolder->GetChildrenComponents(true, ChildComponents);

	FBox CombinedBox(EForceInit::ForceInitToZero);

	for (USceneComponent* Component : ChildComponents)
	{
		if (!Component)
		{
			continue;
		}

		UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component);
		if (!PrimComp)
		{
			continue;
		}

		FBox ComponentBounds;

		// if static mesh
		if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(PrimComp))
		{
			if (StaticMeshComp->GetStaticMesh())
			{
				ComponentBounds = StaticMeshComp->Bounds.GetBox();
			}
		}
		// if box component
		else if (UBoxComponent* BoxComp = Cast<UBoxComponent>(PrimComp))
		{
			ComponentBounds = BoxComp->CalcBounds(BoxComp->GetComponentTransform()).GetBox();
		}
		// if primitive
		else
		{
			ComponentBounds = PrimComp->Bounds.GetBox();
		}

		if (ComponentBounds.IsValid)
		{
			CombinedBox += ComponentBounds; // Union of bounding boxes
		}
	}

	return CombinedBox;
}

#if WITH_EDITOR
void ADEPRECATED_CityGen_ObstacleBase::VisualiseObstacleBounds() const
{
	FBox Bounds = GetObstacleBounds();
	if (!Bounds.IsValid)
	{
		UE_LOG(LogCityGen, Warning, TEXT("Obstacle bounds are not valid."));
		return;
	}

	// Now these are world grid coords
	TArray<FSG_GridCoordinate> WorldAffectedTiles = GetWorldTilesOverlappingBounds(Bounds, 1.0f);

	UE_LOG(LogCityGen, Log, TEXT("Obstacle at world location: %s"), *GetActorLocation().ToString());
	UE_LOG(LogCityGen, Log, TEXT("Number of affected WORLD tiles: %d"), WorldAffectedTiles.Num());

	for (const FSG_GridCoordinate& Tile : WorldAffectedTiles)
	{
		FVector TileWorldPos = GridCmpt->GridToWorld(Tile);
		UKismetSystemLibrary::DrawDebugBox(this, TileWorldPos, FVector(250, 250, 125), FLinearColor::Blue, FRotator::ZeroRotator, 10.f);
	}
}
#endif // WITH_EDITOR
