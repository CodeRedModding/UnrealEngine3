/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionTerrainLayerCoords extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

enum ETerrainCoordMappingType
{
	TCMT_Auto,
	TCMT_XY,
	TCMT_XZ,
	TCMT_YZ
};

/** Determines the mapping place to use on the terrain. */
var() ETerrainCoordMappingType	MappingType;
/** Uniform scale to apply to the mapping. */
var() float					MappingScale;
/** Rotation to apply to the mapping. */
var() float					MappingRotation;
/** Offset to apply to the mapping along U. */
var() float					MappingPanU;
/** Offset to apply to the mapping along V. */
var() float					MappingPanV;


cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual FString GetCaption() const;
}

defaultproperties
{
	MenuCategories(0)="Terrain"
	MenuCategories(1)="WorldPosOffset"
}
