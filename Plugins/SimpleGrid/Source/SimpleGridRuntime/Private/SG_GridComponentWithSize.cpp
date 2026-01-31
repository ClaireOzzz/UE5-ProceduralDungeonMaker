// Copyright Chateau Pageot, Inc. All Rights Reserved.

#include "SimpleGridRuntime/public/SG_GridComponentWithSize.h"

// 3D check
bool USG_GridComponentWithSize::IsCoordinateValid(const FSG_GridCoordinate& GridIndex) const
{
	return (
		GridIndex.X < Width &&
		GridIndex.Y < Height &&
		GridIndex.Z < Depth &&
		GridIndex.X >= 0 &&
		GridIndex.Y >= 0 &&
		GridIndex.Z >= 0);
}

// 2D check
bool USG_GridComponentWithSize::IsCoordinateValid_NoZCheck(const FSG_GridCoordinate& GridIndex) const
{
	return (
		GridIndex.X < Width &&
		GridIndex.Y < Height &&
		GridIndex.X >= 0 &&
		GridIndex.Y >= 0);
}

// Assume rotation normalize [0; 3]
FSG_GridCoordinate USG_GridComponentWithSize::RotateCoord2D(const FSG_GridCoordinate& GridIndex, int32 rotationNorm) const
{
	return RotateCoord2D(GridIndex, rotationNorm, Width, Height);
}

// Assume rotation normalize [0; 3]
FSG_GridCoordinate USG_GridComponentWithSize::RotateCoord2D(const FSG_GridCoordinate& GridIndex, int32 rotationNorm, int32 width, int32 height)
{
	check((rotationNorm >= 0) && (rotationNorm <= 3));
	switch (rotationNorm)
	{
	case 1:
		return FSG_GridCoordinate(height - GridIndex.Y - 1, GridIndex.X, GridIndex.Z);
		break;
	case 2:
		return FSG_GridCoordinate(width - GridIndex.X - 1, height - GridIndex.Y - 1, GridIndex.Z);
		break;
	case 3:
		return FSG_GridCoordinate(GridIndex.Y, width - GridIndex.X - 1, GridIndex.Z);
		break;
	}
	return GridIndex;
}

// Assume rotation normalize [0; 3]
FIntVector USG_GridComponentWithSize::RotateSize2D(const FIntVector& Size, int32 rotationNorm)
{
	check((rotationNorm >= 0) && (rotationNorm <= 3));
	FIntVector Result = Size;
	if ((rotationNorm == 1) || (rotationNorm == 3))
	{
		Result.X = Size.Y;
		Result.Y = Size.X;
		Result.Z = Size.Z;
	}
	return Result;
}

// Assume rotation normalize [0; 3]
FSG_GridCoordinateFloat USG_GridComponentWithSize::RotateCoord2DFloat(const FSG_GridCoordinateFloat& GridIndex, int32 rotationNorm) const
{
	check((rotationNorm >= 0) && (rotationNorm <= 3));
	switch (rotationNorm)
	{
	case 1:
		return FSG_GridCoordinateFloat(Width - GridIndex.Y, GridIndex.X, GridIndex.Z);
		break;
	case 2:
		return FSG_GridCoordinateFloat(Height - GridIndex.X, Width - GridIndex.Y, GridIndex.Z);
		break;
	case 3:
		return FSG_GridCoordinateFloat(GridIndex.Y, Height - GridIndex.X, GridIndex.Z);
		break;
	}
	return GridIndex;
}

// This does not check if inside the grid or not
FSG_GridCoordinate USG_GridComponentWithSize::RelativeToGrid(const FVector& positionGS) const
{
	return FSG_GridCoordinate(
		FMath::TruncToInt(positionGS.X / TileSize.X + (float)Height / 2.0f),
		FMath::TruncToInt(positionGS.Y / TileSize.Y + (float)Width / 2.0f),
		//positionGS.Z);
		FMath::TruncToInt(positionGS.Z / TileSize.Z));
}

FVector USG_GridComponentWithSize::GridToRelative_TileCentered_Float(const FSG_GridCoordinate& GridIndex) const
{
	return FVector(GridIndex.X * TileSize.X - Height * TileSize.X / 2.0f, GridIndex.Y * TileSize.Y - Width * TileSize.Y / 2.0f, GridIndex.Z * TileSize.Z);
}

FVector USG_GridComponentWithSize::GridToRelative_TileCentered_Float(const FVector& GridIndexFloat) const
{
	return FVector(GridIndexFloat.X * TileSize.Z - Height * TileSize.X / 2.0f, GridIndexFloat.Y * TileSize.Y - Width * TileSize.Y / 2.0f, GridIndexFloat.Z * TileSize.Z);
}