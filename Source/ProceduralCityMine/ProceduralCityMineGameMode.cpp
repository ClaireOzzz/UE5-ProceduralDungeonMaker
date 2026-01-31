// Copyright Chateau Pageot, Inc. All Rights Reserved.

#include "ProceduralCityMineGameMode.h"
#include "ProceduralCityMineCharacter.h"
#include "UObject/ConstructorHelpers.h"

AProceduralCityMineGameMode::AProceduralCityMineGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
