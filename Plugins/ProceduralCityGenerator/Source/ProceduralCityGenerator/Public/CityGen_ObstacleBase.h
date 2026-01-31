// Copyright Chateau Pageot, Inc. All Rights Reserved.

#pragma once

#include "SimpleGridRuntime/Public/SG_GridComponent.h"
#include "SimpleGridRuntime/Public/SG_GridCoordinate.h"

#include "GameFramework/Actor.h"

#include "CityGen_ObstacleBase.generated.h"


UCLASS(Deprecated)
class PROCEDURALCITYGENERATOR_API ADEPRECATED_CityGen_ObstacleBase : public AActor
{
	GENERATED_BODY()

public:
	ADEPRECATED_CityGen_ObstacleBase();

	TArray<FSG_GridCoordinate> GetWorldTilesOverlappingBounds(const FBox& Bounds, float BoundExpandSizeWS) const;

	// BOUNDS
	FBox GetObstacleBounds() const;

#if WITH_EDITOR
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Exits")
	void VisualiseObstacleBounds() const;
#endif // WITH_EDITOR

	UPROPERTY();
	TObjectPtr<USG_GridComponent> GridCmpt;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* GeometryFolder;

protected:
	FVector TileSize = FVector(500, 500, 250);
};



