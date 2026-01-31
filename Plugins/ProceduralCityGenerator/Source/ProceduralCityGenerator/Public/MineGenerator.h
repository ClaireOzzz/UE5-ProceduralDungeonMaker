// Copyright Chateau Pageot, Inc. All Rights Reserved.

#pragma once

#include "GridBasedGeneratorBase.h"

#include "SimpleGridRuntime/Public/SG_GridCoordinate.h"

#include "MineGenerator.generated.h"

class USG_GridComponentWithSize;
class ADungeonGenerator_GridBased;

UENUM(BlueprintType)
enum class EGeneratorType : uint8
{
	Default UMETA(DisplayName = "Default"),
	Star    UMETA(DisplayName = "Star"),
	Loop    UMETA(DisplayName = "Loop")
};

USTRUCT(BlueprintType)
struct FStarGeneratorSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star Generator")
	TSubclassOf<ACityGen_RoomBase> CentralRoom;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star Generator")
	FSG_GridCoordinate CentralRoomGridLocation = FSG_GridCoordinate(0, 0, 1);
};

UCLASS()
class PROCEDURALCITYGENERATOR_API AMineGenerator : public AGridBasedGeneratorBase
{
	GENERATED_BODY()
	
public:
	// GENERATOR CLASS

	// Generator Type enum (select which generator to use)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator")
	EGeneratorType GeneratorType = EGeneratorType::Default;

	// Internal: store references to generator classes - set in defaults or constructor, not editable by user
	UPROPERTY(EditDefaultsOnly, Category = "Generator")
	TSubclassOf<ADungeonGenerator_GridBased> DefaultGeneratorClass;

	UPROPERTY(EditDefaultsOnly, Category = "Generator")
	TSubclassOf<ADungeonGenerator_GridBased> StarGeneratorClass;

	UPROPERTY(EditDefaultsOnly, Category = "Generator")
	TSubclassOf<ADungeonGenerator_GridBased> LoopGeneratorClass;

	// Generator-specific settings — only show if corresponding generator is selected
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room Generation", meta = (EditCondition = "GeneratorType == EGeneratorType::Star"))
	FStarGeneratorSettings StarSettings;

	// Returns the class corresponding to selected generator type
	TSubclassOf<ADungeonGenerator_GridBased> GetSelectedGeneratorClass() const
	{
		switch (GeneratorType)
		{
		case EGeneratorType::Star:
			return StarGeneratorClass;

		case EGeneratorType::Loop:
			return LoopGeneratorClass;

		case EGeneratorType::Default:
		default:
			return DefaultGeneratorClass;
		}
	}

	// ROOMS TO SPAWN

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room Generation")
	TArray<TSubclassOf<ACityGen_RoomBase>> RoomTypes;

	// SETTINGS

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room Generation")
	TArray<int32> RoomsPerLevel; // Define how many rooms per level

	// Width of Grid 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings")
	int32 GridWidth = 10;

	// Height of Grid
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Settings")
	int32 GridHeight = 10;

	UPROPERTY();
	TObjectPtr<USG_GridComponentWithSize> GridCmpt;

	// Random seed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
	int32 RandomSeed = 12345;

	// VARIABLES 

	UPROPERTY()
	TArray<ACityGen_RoomBase*> AllSpawnedRooms;

	UPROPERTY()
	TArray<AActor*> SpawnedCorridors;

	UPROPERTY()
	TSet<FSG_GridCoordinate> OccupiedGridCells;

private:
	ADungeonGenerator_GridBased* DungeonGeneratorInstance;

	FRandomStream MineGenRandomStream;

public:
	// Sets default values for this actor's properties
	AMineGenerator();

	// FUNCTIONS

	UFUNCTION(CallInEditor, Category = "Room Generation")
	bool GenerateMine();

	UFUNCTION(CallInEditor, Category = "Room Generation")
	void ClearRooms();

	UFUNCTION(CallInEditor, Category = "Room Generation")
	bool SpawnCorridorGenerator();

	ACityGen_RoomBase* SpawnCentralRoom();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

};
