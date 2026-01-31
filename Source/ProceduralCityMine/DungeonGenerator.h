// Copyright Chateau Pageot, Inc. All Rights Reserved.

// PART OF OLD GENERATION METHOD, NOT USED ANYMORE

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DungeonGenerator.generated.h"

class ARB_DungeonRoom_01;
class ACityGen_RoomBase;
class AClosingWall;
class ARB_DungeonStairs_Room_01;

UCLASS()
class PROCEDURALCITYMINE_API ADungeonGenerator : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ADungeonGenerator();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	ACityGen_RoomBase* LatestSpawnedRoom;

	UPROPERTY(EditAnywhere, Category = "Dungeon Settings")
	TSubclassOf<ARB_DungeonRoom_01> StarterRoomClass; 

	UPROPERTY(EditAnywhere, Category = "Dungeon Settings")
	TArray<TSubclassOf<ACityGen_RoomBase>> RoomsToBeSpawned;

	UPROPERTY(EditAnywhere, Category = "Dungeon Settings")
	int32 RoomAmount;

	UPROPERTY(EditAnywhere, Category = "Closing Wall")
	TSubclassOf<AClosingWall> ClosingWall;

	bool bCanSpawn = true;

	TArray<USceneComponent*> Exits;

	FRandomStream RandomStream;

	UPROPERTY(EditAnywhere, Category = "Dungeon Settings")
	int32 Seed;

	UPROPERTY(EditAnywhere, Category = "Dungeon Settings")
	int32 MaxLevels;

	UPROPERTY(EditAnywhere, Category = "Dungeon Settings")
	int32 Counter = 0;

	void SetSeed();

	void SpawnStarterRoom();

	void SpawnNextRoom();

	void RemoveOverlappingRooms();

	void RemoveExtraLevels();

	void CloseUnusedExits();

};
