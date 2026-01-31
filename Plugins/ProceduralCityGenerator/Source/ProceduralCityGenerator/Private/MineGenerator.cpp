// Copyright Chateau Pageot, Inc. All Rights Reserved.

#include "MineGenerator.h"

#include "CityGen_LogChannels.h"
#include "CityGen_Roombase.h"
#include "DungeonGenerator_GridBased.h"
#include "DungeonGenerator_Star.h"
#include "DungeonGenerator_Looping.h"

#include "SimpleGridRuntime/Public/SG_GridCoordinate.h"
#include "SimpleGridRuntime/Public/SG_GridComponentWithSize.h"

#include <Kismet/GameplayStatics.h>
#include "Runtime/Engine/Classes/Kismet/KismetMathLibrary.h"

// Sets default values
AMineGenerator::AMineGenerator()
{
	GridCmpt = CreateDefaultSubobject<USG_GridComponentWithSize>(TEXT("GridCmpt"));
	GridCmpt->SetTileSize(TileSize);

	MineGenRandomStream = FRandomStream(RandomSeed);
}

// Called when the game starts or when spawned
void AMineGenerator::BeginPlay()
{
	Super::BeginPlay();

	bool bSpawnCorridorRes = SpawnCorridorGenerator();
	ClearRooms();
	bool bGenerateMineRes = GenerateMine();
}

bool AMineGenerator::SpawnCorridorGenerator()
{
	TSubclassOf<ADungeonGenerator_GridBased> SelectedClass = GetSelectedGeneratorClass();

	if (!SelectedClass)
	{
		UE_LOG(LogCityGen, Warning, TEXT("Selected generator class is not set!"));
		return false;
	}

	DungeonGeneratorInstance = GetWorld()->SpawnActor<ADungeonGenerator_GridBased>(SelectedClass);

	if (!DungeonGeneratorInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to spawn DungeonGeneratorInstance."));
		return false;
	}

	return true;
}

void AMineGenerator::ClearRooms()
{
	for (ACityGen_RoomBase* Room : AllSpawnedRooms)
	{
		if (Room)
		{
			Room->Destroy();
		}
	}

	AllSpawnedRooms.Empty();

	if (!DungeonGeneratorInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("DungeonGeneratorInstance is not initialized. Cannot generate mine."));
		return;
	}

	DungeonGeneratorInstance->ClearCorridorMeshes();
}

bool AMineGenerator::GenerateMine()
{
	AllSpawnedRooms.Empty();
	SpawnedCorridors.Empty();
	OccupiedGridCells.Empty();

	if (!DungeonGeneratorInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("DungeonGeneratorInstance is not initialized. Cannot generate mine."));
		return false;
	}

	// for each int in rooms per level, generate int number of rooms randomly from the list of RoomTypes
	for (int32 LevelIndex = 0; LevelIndex < RoomsPerLevel.Num(); ++LevelIndex)
	{
		int32 NumRooms = RoomsPerLevel[LevelIndex];

		int32 MaxAttempts = NumRooms * 10;
		int32 Attempts = 0;
		int32 Spawned = 0;

		while (Spawned < NumRooms && Attempts < MaxAttempts)
		{
			++Attempts;

			int32 RandomRoomTypeIndex = MineGenRandomStream.RandRange(0, RoomTypes.Num() - 1);
			TSubclassOf<ACityGen_RoomBase> SelectedRoomType = RoomTypes[RandomRoomTypeIndex];

			int32 SpawnLocationX = MineGenRandomStream.RandRange(-GridWidth / 2, GridWidth/2); // spawns randomly in the middle of the level
			int32 SpawnLocationY = MineGenRandomStream.RandRange(-GridHeight / 2, GridHeight/2);
			int32 SpawnLocationZ = LevelIndex; // Adjust Z based on level index

			FSG_GridCoordinate Coord(SpawnLocationX, SpawnLocationY, SpawnLocationZ);
			FVector SpawnLocationWS = GridCmpt->GridToWorld(Coord);
			FRotator SpawnRotation = FRotator(0, MineGenRandomStream.RandRange(0, 3) * 90.0f, 0); // randomise rotation based on 90 degree increments only on yaw

			UE_LOG(LogTemp, Warning, TEXT("SpawnLocation ints: X=%d, Y=%d, Z=%d"),
				SpawnLocationX, SpawnLocationY, SpawnLocationZ);
			UE_LOG(LogTemp, Warning, TEXT("SpawnLocation: X=%f, Y=%f, Z=%f"),
				SpawnLocationWS.X, SpawnLocationWS.Y, SpawnLocationWS.Z);

			// ensure rooms are not directly on top of the other z +- 1 (do we need this?)
			bool bTooCloseVertically = false;
			for (const FSG_GridCoordinate& Occupied : OccupiedGridCells)
			{
				if (Occupied.X == Coord.X && Occupied.Y == Coord.Y &&
					FMath::Abs(Occupied.Z - Coord.Z) <= 1)
				{
					bTooCloseVertically = true;
					break;
				}
			}

			if (bTooCloseVertically)
			{
				continue; // Skip spawning this room
			}

			ACityGen_RoomBase* NewRoom = GetWorld()->SpawnActor<ACityGen_RoomBase>(SelectedRoomType, SpawnLocationWS, SpawnRotation);
				
			AllSpawnedRooms.Add(NewRoom);
			OccupiedGridCells.Add(Coord);
			++Spawned;
			
		}
	}

	// After all rooms are spawned, we can now generate corridors between them
	bool bSuccess = false;
	if ((GeneratorType == EGeneratorType::Star) && AllSpawnedRooms.Num() > 0) // we can skip SetRoomsToConnect
	{
		ADungeonGenerator_Star* StarGenerator = Cast<ADungeonGenerator_Star>(DungeonGeneratorInstance);
		// casy central room to ACityGen_RoomBase
		if (StarGenerator)
		{
			ACityGen_RoomBase* CentralRoomActor = SpawnCentralRoom();

			if(CentralRoomActor)
			{
				StarGenerator->CentralRoom = CentralRoomActor;
				StarGenerator->InitFromSpawnedRooms( AllSpawnedRooms );
				bSuccess = StarGenerator->ConnectRoomsInOrder();
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Central room is null"));
				bSuccess = false;
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Central room has issues"));
			bSuccess = false;
		}
	}
	else
	{
		DungeonGeneratorInstance->InitFromSpawnedRooms(AllSpawnedRooms);
		bSuccess = DungeonGeneratorInstance->ConnectRoomsInOrder();
	}
	
	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to connect rooms with corridors."));
		return false;
	}
	return true;
}


ACityGen_RoomBase* AMineGenerator::SpawnCentralRoom()
{
	ACityGen_RoomBase* CentralRoomActor = nullptr;
	if (GeneratorType == EGeneratorType::Star)
	{
		if (StarSettings.CentralRoom)
		{
			FVector CentralRoomLocation = GridCmpt->GridToWorld(StarSettings.CentralRoomGridLocation);
			FRotator CentralRoomRotation = FRotator(0, MineGenRandomStream.RandRange(0, 3) * 90.0f, 0); // randomise rotation based on 90 degree increments only on yaw
			// Spawn the central room actor
			CentralRoomActor = GetWorld()->SpawnActor<ACityGen_RoomBase>(StarSettings.CentralRoom, CentralRoomLocation, CentralRoomRotation);
			
			AllSpawnedRooms.Add(CentralRoomActor);

			if (CentralRoomActor)
			{
				// Get the room's bounding box in world space
				FBox RoomBounds = CentralRoomActor->GetComponentsBoundingBox();

				FVector Min = RoomBounds.Min;
				FVector Max = RoomBounds.Max;

				FSG_GridCoordinate MinGrid = GridCmpt->WorldToGrid(Min);
				FSG_GridCoordinate MaxGrid = GridCmpt->WorldToGrid(Max);

				// Loop through all grid tiles covered by the bounding box and add them
				for (int32 X = MinGrid.X; X <= MaxGrid.X; ++X)
				{
					for (int32 Y = MinGrid.Y; Y <= MaxGrid.Y; ++Y)
					{
						// Use the same Z as CentralRoomGridLocation (or iterate if the room spans multiple Z levels)
						int32 Z = StarSettings.CentralRoomGridLocation.Z;

						FSG_GridCoordinate OccupiedTile(X, Y, Z);
						OccupiedGridCells.Add(OccupiedTile);
					}
				}
				return CentralRoomActor; // Return the spawned central room actor
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to spawn central room actor."));
				return nullptr;
			}
		}
		return nullptr;
	}
	else
	{
		return nullptr; // If not using Star generator, return nullptr
	}
}
