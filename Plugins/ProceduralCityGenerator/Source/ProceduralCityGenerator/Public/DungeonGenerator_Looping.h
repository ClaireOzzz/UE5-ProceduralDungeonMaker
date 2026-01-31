// Copyright Chateau Pageot, Inc. All Rights Reserved.

#pragma once

#include "DungeonGenerator_GridBased.h"

#include "DungeonGenerator_Looping.generated.h"

/**
 * 
 */
UCLASS()
class PROCEDURALCITYGENERATOR_API ADungeonGenerator_Looping : public ADungeonGenerator_GridBased
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Dungeon Settings")
	TArray<ACityGen_RoomBase*> RoomsToConnect;

	// Obstacle are just room we don't try to connect
	UPROPERTY(EditAnywhere, Category = "Dungeon Settings")
	TArray<ACityGen_RoomBase*> Obstacles;

public:
	// This return an array without nullptr actors
	// The pair does not contains same actor
	virtual TArray<TPair<ACityGen_RoomBase*, ACityGen_RoomBase* >> GetRoomsToConnectArray() const override;

	// This return an array without nullptr actors
	virtual TArray<ACityGen_RoomBase*> GetAllRoomsArray() const override;

	virtual void InitFromSpawnedRooms(const TArray<ACityGen_RoomBase*>& SpawnedRooms) override;
};
