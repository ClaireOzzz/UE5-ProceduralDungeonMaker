// Copyright Chateau Pageot, Inc. All Rights Reserved.

// PART OF OLD GENERATION METHOD, NOT USED ANYMORE

#include "DungeonGenerator.h"

#include "CityGen_Roombase.h"
#include "ClosingWall.h"
#include "RB_DungeonRoom_01.h"
#include "RB_DungeonStairs_Room_01.h"

#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"


// Sets default values
ADungeonGenerator::ADungeonGenerator()
{
}

// Called when the game starts or when spawned
void ADungeonGenerator::BeginPlay()
{
	Super::BeginPlay();

	SetSeed();

	SpawnStarterRoom();

	SpawnNextRoom();
}

void ADungeonGenerator::SetSeed()
{
	int32 Results;
	if (Seed == -1)
	{
		Results = FMath::Rand();
	}
	else
	{
		Results = Seed;
	}

	RandomStream.Initialize(Results);
}

void ADungeonGenerator::SpawnStarterRoom()
{
	ARB_DungeonRoom_01* SpawnedStarterRoom = this->GetWorld()->SpawnActor<ARB_DungeonRoom_01>(StarterRoomClass);

	SpawnedStarterRoom->SetActorLocation(this->GetActorLocation());

	SpawnedStarterRoom->ExitPointsFolder->GetChildrenComponents(false, Exits);
}

void ADungeonGenerator::SpawnNextRoom()
{
	bCanSpawn = true;

	int32 RoomIndex = RandomStream.RandRange(0, RoomsToBeSpawned.Num() - 1);
	LatestSpawnedRoom = this->GetWorld()->SpawnActor<ACityGen_RoomBase>(RoomsToBeSpawned[RoomIndex]);

	int32 ExitIndex = RandomStream.RandRange(0, Exits.Num() - 1);
	USceneComponent* SelectedExitPoint = Exits[ExitIndex];

	FVector AdjustedLocation = SelectedExitPoint->GetComponentLocation() + SelectedExitPoint->GetForwardVector() * 250.0f; 
	//250 adjustment to center it, has to be half the room floor size

	LatestSpawnedRoom->SetActorLocation(AdjustedLocation);
	LatestSpawnedRoom->SetActorRotation(SelectedExitPoint->GetComponentRotation());

	RemoveOverlappingRooms();
	//RemoveExtraLevels();

	if (bCanSpawn)
	{
		Exits.Remove(SelectedExitPoint);
		TArray<USceneComponent*> LatestRoomExitPoints;
		LatestSpawnedRoom->ExitPointsFolder->GetChildrenComponents(false, LatestRoomExitPoints);
		Exits.Append(LatestRoomExitPoints);
		RoomAmount = RoomAmount - 1;
	}

	int32 numIterations = 0;
	const int32 maxIterations = 300; // To avoid infinite loop in case of setting mistake
	if (RoomAmount > 0 && numIterations <= maxIterations)
	{
		numIterations = numIterations + 1;
		if (numIterations > maxIterations)
		{
			UE_LOG(LogTemp, Warning, TEXT("MAX ITERATIONS REACHED"));
		}
		Counter += 1;
		UE_LOG(LogTemp, Log, TEXT("Room Amount: %d"), RoomAmount);
		SpawnNextRoom();
	}
	else
	{
		CloseUnusedExits();
	}
}

void ADungeonGenerator::RemoveOverlappingRooms()
{
	TArray<USceneComponent*> OverlappedRooms;
	LatestSpawnedRoom->OverlapFolder->GetChildrenComponents(false, OverlappedRooms);

	TArray<UPrimitiveComponent*> OverlappingComponents;

	for (USceneComponent* Element : OverlappedRooms)
	{
		Cast<UBoxComponent>(Element)->GetOverlappingComponents(OverlappingComponents);
	}

	for (UPrimitiveComponent* Element : OverlappingComponents)
	{
		bCanSpawn = false;
		LatestSpawnedRoom->Destroy();
	}
}

void ADungeonGenerator::RemoveExtraLevels()
{
	//ARB_DungeonStairs_Room_01* StairsRoom = Cast<ARB_DungeonStairs_Room_01>(LatestSpawnedRoom);

	/*if (StairsRoom)
	{
		int32 FloorHeight = StairsRoom->DistanceBetweenFloors;
		float MaxHeight = MaxLevels * FloorHeight;

		TArray<USceneComponent*> ExitPoints;
		StairsRoom->ExitPointsFolder->GetChildrenComponents(false, ExitPoints);

		for (USceneComponent* ExitPoint : ExitPoints)
		{
			if (!ExitPoint) continue;

			FVector WorldLocation = ExitPoint->GetComponentLocation();
			if (WorldLocation.Z >= MaxHeight)
			{
				bCanSpawn = false;
				LatestSpawnedRoom->Destroy();
			}
		}
	}*/
	return; 
}

void ADungeonGenerator::CloseUnusedExits()
{
	for (USceneComponent* Element : Exits)
	{
		AClosingWall* LatestClosingWallSpawned = GetWorld()->SpawnActor<AClosingWall>(ClosingWall); // AClosingWall should not be an actor, but a function of the room \ module \ exit

		FVector RelativeOffset(0.0f, -100.0f, 100.0f);
		FVector WorldOffset = Element->GetComponentRotation().RotateVector(RelativeOffset);

		LatestClosingWallSpawned->SetActorLocation(Element->GetComponentLocation() + WorldOffset);
		LatestClosingWallSpawned->SetActorRotation(Element->GetComponentRotation() + FRotator(0.0f, 90.0f, 0.0f));
	}
}