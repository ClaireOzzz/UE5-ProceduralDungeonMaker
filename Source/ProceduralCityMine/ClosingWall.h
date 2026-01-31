// Copyright Chateau Pageot, Inc. All Rights Reserved.

// PART OF OLD GENERATION METHOD, NOT USED ANYMORE

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClosingWall.generated.h"

UCLASS()
class PROCEDURALCITYMINE_API AClosingWall : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AClosingWall();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* DefaultRootComponent;

	/*UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* ClosingWall; */

protected:
	// Called when the game starts or when spawned
	//virtual void BeginPlay() override;	

};
