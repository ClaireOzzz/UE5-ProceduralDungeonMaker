// Copyright Chateau Pageot, Inc. All Rights Reserved.

#pragma once

#include "CellConnectionState.h"
#include "CityGen_RoomBase.h"

#include "GameFramework/Actor.h"

#include "GridBasedGeneratorBase.generated.h"

class ACityGen_RoomBase;

UCLASS()
class PROCEDURALCITYGENERATOR_API AGridBasedGeneratorBase : public AActor
{
	GENERATED_BODY()

public:
	// CORRIDOR TYPES

	UPROPERTY(EditAnywhere, Category = "Corridor Types")
	TSubclassOf<ACityGen_RoomBase> CorridorRoom; // Corridor, open X positive(North) and X negative(South)

	UPROPERTY(EditAnywhere, Category = "Corridor Types")
	TSubclassOf<ACityGen_RoomBase> CorridorCornerRoom; // L shape Corridor, open X positive(North) and Y positive(East)

	UPROPERTY(EditAnywhere, Category = "Corridor Types")
	TSubclassOf<ACityGen_RoomBase> TJunctionCorridorRoom; // T intersection, open X positive(North), Y positive(East) and X negative(South)

	UPROPERTY(EditAnywhere, Category = "Corridor Types")
	TSubclassOf<ACityGen_RoomBase> CrossJunctionCorridorRoom; // Cross intersection, open all direction

	UPROPERTY(EditAnywhere, Category = "Corridor Types")
	TSubclassOf<ACityGen_RoomBase> ElevatorCorridorRoom_Up;

	UPROPERTY(EditAnywhere, Category = "Corridor Types")
	TSubclassOf<ACityGen_RoomBase> ElevatorCorridorRoom_Down;

	UPROPERTY(EditAnywhere, Category = "Corridor Types")
	TSubclassOf<ACityGen_RoomBase> ElevatorCorridorRoom_UpAndDown;

protected:
	FVector TileSize = FVector(500, 500, 250);

};

